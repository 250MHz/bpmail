#include <ares.h>
#include <ares_dns_record.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <inttypes.h>

static void hexdump(const void *data, size_t size) {
    const unsigned char *bytes = data;
    size_t i, j;

    for (i = 0; i < size; i += 16) {
        printf("%08x  ", (unsigned int)i); // Print address offset

        // Print hexadecimal bytes
        for (j = 0; j < 16; j++) {
            if (i + j < size) {
                printf("%02x ", bytes[i + j]);
            } else {
                printf("   "); // Pad with spaces if less than 16 bytes
            }
        }

        printf(" |");

        // Print ASCII representation
        for (j = 0; j < 16; j++) {
            if (i + j < size) {
                if (bytes[i + j] >= 32 && bytes[i + j] <= 126) {
                    printf("%c", bytes[i + j]);
                } else {
                    printf("."); // Replace non-printable characters with '.'
                }
            } else {
                printf(" ");
            }
        }
        printf("|\n");
    }
}

static void dnsrec_cb(
    void *arg,
    ares_status_t status,
    size_t timeouts,
    const ares_dns_record_t *dnsrec
) {
    size_t i;

    (void)arg; /* Example does not use user context */

    printf("Result: %s, timeouts: %zu\n", ares_strerror(status), timeouts);

    if (dnsrec == NULL) {
        return;
    }
    printf(
        "rr cnt: %zu\n",
        ares_dns_record_rr_cnt(dnsrec, ARES_SECTION_ANSWER)
    );
    for (i = 0; i < ares_dns_record_rr_cnt(dnsrec, ARES_SECTION_ANSWER); i++) {
        const ares_dns_rr_t *rr = NULL;

        rr = ares_dns_record_rr_get_const(dnsrec, ARES_SECTION_ANSWER, i);
        printf("type: %d\n", ares_dns_rr_get_type(rr));
        if (ares_dns_rr_get_type(rr) != ARES_REC_TYPE_RAW_RR) {
            continue;
        }

        size_t len;
        const unsigned char *data =
            ares_dns_rr_get_bin(rr, ARES_RR_RAW_RR_DATA, &len);
        printf("len: %zu\n", len);
        printf("data: %s\n", data);
        hexdump(data, len);
        uint64_t res = 0;
        /* data is big endian */
        for (size_t i = 0; i < sizeof(uint64_t); i++) {
            res = (res << 8) | data[i];
        }
        printf("%" PRIu64 "\n", res);
        continue;

        printf(
            "SRV Priority: %u\n",
            ares_dns_rr_get_u16(rr, ARES_RR_SRV_PRIORITY)
        );
        printf("SRV Weight: %u\n", ares_dns_rr_get_u16(rr, ARES_RR_SRV_WEIGHT));
        printf("SRV Port: %u\n", ares_dns_rr_get_u16(rr, ARES_RR_SRV_PORT));
        printf(
            "SRV Target: %s.\n",
            ares_dns_rr_get_str(rr, ARES_RR_SRV_TARGET)
        );
    }
}

int main(int argc, char **argv) {
    ares_channel_t *channel = NULL;
    struct ares_options options;
    int optmask = 0;
    ares_status_t status;

    if (argc != 2) {
        printf("Usage: %s domain\n", argv[0]);
        return 1;
    }

    /* Initialize library */
    ares_library_init(ARES_LIB_INIT_ALL);

    if (!ares_threadsafety()) {
        printf("c-ares not compiled with thread support\n");
        return 1;
    }


    /* Enable event thread so we don't have to monitor file descriptors */
    memset(&options, 0, sizeof(options));
    optmask |= ARES_OPT_EVENT_THREAD;
    options.evsys = ARES_EVSYS_DEFAULT;
    // optmask |= ARES_OPT_UDP_PORT;
    // // c-ares asks for unsigned short for this
    // options.udp_port = 5300;
    // optmask |= ARES_OPT_SERVERS;
    // // struct in_addr local = { 0x0100007fUL };
    // uint32_t dst;
    // ares_inet_pton(AF_INET, "127.0.0.1", &dst);
    // printf("%ud\n", dst);
    // struct in_addr local = { dst };
    // printf("%ud\n", local.s_addr);
    // options.servers = &local;
    // options.nservers = 1;

    /* Initialize channel to run queries, a single channel can accept unlimited
   * queries */
    status = ares_init_options(&channel, &options, optmask);
    if (status != ARES_SUCCESS) {
        printf("c-ares initialization issue: %s\n", ares_strerror(status));
        return 1;
    }
    ares_set_servers_ports_csv(channel, "127.0.0.1:5300");
    printf("%s\n", ares_get_servers_csv(channel));

    /* Perform query */
    status = ares_query_dnsrec(
        channel,
        argv[1],
        ARES_CLASS_IN,
        // ARES_REC_TYPE_SRV,
        264, // IPN
        // 53, // SMIMEA
        // 1, // A
        dnsrec_cb,
        NULL /* user context */,
        NULL /* qid */
    );
    if (status != ARES_SUCCESS) {
        printf("failed to enqueue query: %s\n", ares_strerror(status));
        return 1;
    }

    /* Wait until no more requests are left to be processed */
    ares_queue_wait_empty(channel, -1);

    /* Cleanup */
    ares_destroy(channel);

    ares_library_cleanup();
    return 0;
}
