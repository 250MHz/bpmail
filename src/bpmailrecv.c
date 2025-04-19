#include "bpmailrecv.h"

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ares.h"
#include "bp.h"
#include "dtpc.h"
#include "gmime/gmime.h"
#include "zlib.h"

static struct sdrv_str *sdr = NULL;
static struct dtpcsap_st *sap = NULL;
static int allow_invalid_mime = 0;
static int verify_ipn = 1;
static ares_channel_t *channel = NULL;

struct ipn_verify {
    unsigned long long node_nbr;
    int success;
};

static void usage(void) {
    (void)fprintf(
        stderr,
        "%s\n",
        "usage: bpmailrecv [--allow-invalid-mime] [--no-verify-ipn |"
        " -s dns_server_list] [-t topic_id]"
    );
    exit(EXIT_FAILURE);
}

static struct option longopts[] = {
    {"allow-invalid-mime", no_argument, &allow_invalid_mime, 1},
    {"no-verify-ipn", no_argument, &verify_ipn, 0},
    {NULL, 0, NULL, 0},
};

/*
 * Decompress zlib data dynamically using the inflate API.
 * On success, returns a Bytef * pointing to data allocated by malloc(). The
 * caller is responsible for calling free().
 * Returns NULL on failure.
 */
static Bytef *inflate_dynamic(const Bytef *in, size_t in_len, size_t *out_len) {
    int ret;
    z_stream strm;
    Bytef *out = NULL;
    size_t out_capacity = 16384; /* Start with 16KB */
    size_t out_size = 0;

    out = malloc(out_capacity);
    if (out == NULL) {
        return NULL;
    }

    memset(&strm, 0, sizeof(strm));

    /* zlib only accepts uInt for avail_in, so we must check and cast safely */
    if (in_len > UINT_MAX) {
        free(out);
        return NULL; /* input too large for zlib */
    }

    strm.next_in = (Bytef *)(uintptr_t)in; /* safe cast from const */
    strm.avail_in = (uInt)in_len;

    if (inflateInit(&strm) != Z_OK) {
        free(out);
        return NULL;
    }

    do {
        size_t remaining = out_capacity - out_size;

        /* zlib requires avail_out to be uInt */
        if (remaining > UINT_MAX) {
            inflateEnd(&strm);
            free(out);
            return NULL; /* chunk too big for zlib */
        }

        strm.avail_out = (uInt)remaining;
        strm.next_out = out + out_size;

        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR)
        {
            inflateEnd(&strm);
            free(out);
            return NULL;
        }

        out_size = strm.total_out;

        /* If output buffer is full, grow it */
        if (ret != Z_STREAM_END && strm.avail_out == 0) {
            out_capacity *= 2;
            Bytef *tmp = realloc(out, out_capacity);
            if (tmp == NULL) {
                inflateEnd(&strm);
                free(out);
                return NULL;
            }
            out = tmp;
        }
    } while (ret != Z_STREAM_END);

    *out_len = out_size;

    inflateEnd(&strm);
    return out;
}

static void dnsrec_cb(
    void *arg,
    ares_status_t status,
    size_t timeouts,
    const ares_dns_record_t *dnsrec
) {
    (void)timeouts;
    if (dnsrec == NULL || status != ARES_SUCCESS) {
        return;
    }
    struct ipn_verify *res = arg;

    res->success = 0;
    size_t rr_cnt = ares_dns_record_rr_cnt(dnsrec, ARES_SECTION_ANSWER);
    for (size_t i = 0; i < rr_cnt; i++) {
        const ares_dns_rr_t *rr =
            ares_dns_record_rr_get_const(dnsrec, ARES_SECTION_ANSWER, i);
        if (rr == NULL) {
            (void)fprintf(stderr, "ares_dns_record_rr_get_const: misuse\n");
            return;
        }

        if (ares_dns_rr_get_type(rr) != ARES_REC_TYPE_RAW_RR) {
            continue;
        }

        size_t len; /* TODO: do we need to keep track of len? */
        const unsigned char *data =
            ares_dns_rr_get_bin(rr, ARES_RR_RAW_RR_DATA, &len);
        uint64_t r_node_nbr = 0;
        /* data is big endian */
        for (size_t j = 0; j < sizeof(uint64_t); j++) {
            r_node_nbr = (r_node_nbr << 8) | data[j];
        }
        if (res->node_nbr == r_node_nbr) {
            res->success = 1;
            return;
        }
    }
}

static int bpmailrecv(void) {
    DtpcDelivery dlv;

    if (dtpc_receive(sap, &dlv, BP_BLOCKING) != 0) {
        (void)fprintf(stderr, "could not receive DTPC application data unit\n");
        return EXIT_FAILURE;
    }

    if (dlv.result != PayloadPresent) {
        dtpc_release_delivery(&dlv);
        return EXIT_SUCCESS;
    }

    char *received_data = malloc(dlv.length);
    if (received_data == NULL) {
        (void)fprintf(stderr, "malloc failed\n");
        dtpc_release_delivery(&dlv);
        return EXIT_FAILURE;
    }

    sdr_read(sdr, received_data, dlv.item, dlv.length);

    size_t decompressed_size = 0;
    Bytef *decompressed = inflate_dynamic(
        (const Bytef *)(received_data),
        dlv.length,
        &decompressed_size
    );
    free(received_data);
    if (decompressed == NULL) {
        (void)fprintf(stderr, "decompression failed\n");
        dtpc_release_delivery(&dlv);
        return EXIT_FAILURE;
    }

    g_mime_init();

    GMimeStream *istream = g_mime_stream_mem_new_with_buffer(
        (char *)decompressed,
        decompressed_size
    );
    if (istream == NULL) {
        (void)fprintf(stderr, "could not create new GMime memory stream\n");
        free(decompressed);
        dtpc_release_delivery(&dlv);
        return EXIT_FAILURE;
    }

    /* GObject construction can never fail; parser should never be NULL */
    GMimeParser *parser = g_mime_parser_new_with_stream(istream);
    g_object_unref(istream);

    GMimeMessage *message = g_mime_parser_construct_message(parser, NULL);
    g_object_unref(parser);
    if (message == NULL) {
        (void)fprintf(stderr, "could not parse MIME message\n");
        if (allow_invalid_mime) {
            (void)fwrite(decompressed, 1, decompressed_size, stdout);
            (void)fflush(stdout);
        }
        free(decompressed);
        dtpc_release_delivery(&dlv);
        if (allow_invalid_mime) {
            return EXIT_SUCCESS;
        }
        return EXIT_FAILURE;
    }
    free(decompressed);

    /*
     * From this point, we assume `message` is a valid MIME message.
     * (GMime does not accept some values that should be valid, such as
     * From: Managing Partners:ben@example.com,carol@example.com;
     * (taken from Section 4 of RFC 6854).)
     * So ignore `allow_invalid_mime` and treat a failure to parse a header
     * as a failure to verify the source EID.
     */

    if (verify_ipn) {
        /* Parse source EID */
        if (strncmp("ipn:", dlv.srcEid, 4)) {
            (void)fprintf(stderr, "source EID does not use ipn URI scheme\n");
            g_object_unref(message);
            dtpc_release_delivery(&dlv);
            return EXIT_FAILURE;
        }
        char *node_nbr_str = malloc(strlen(dlv.srcEid) - 4);
        if (node_nbr_str == NULL) {
            perror("malloc");
            g_object_unref(message);
            dtpc_release_delivery(&dlv);
            return EXIT_FAILURE;
        }
        *node_nbr_str = '\0';
        const char *start = dlv.srcEid + 4;
        strncat(node_nbr_str, start, strcspn(start, "."));
        errno = 0;
        char *endptr;
        unsigned long long node_nbr = strtoull(node_nbr_str, &endptr, 0);
        if (node_nbr_str == endptr) {
            errno = EINVAL;
        }
        if (errno != 0) {
            perror("strtoull");
            g_object_unref(message);
            dtpc_release_delivery(&dlv);
            return EXIT_FAILURE;
        }
        free(node_nbr_str);

        InternetAddressList *list = g_mime_message_get_from(message);
        if (list == NULL) {
            (void)fprintf(
                stderr,
                "could not extract mailbox-list from RFC5322.From header\n"
            );
            g_object_unref(message);
            dtpc_release_delivery(&dlv);
            return EXIT_FAILURE;
        }
        for (int i = 0; i < internet_address_list_length(list); i++) {
            InternetAddressMailbox *mb = (InternetAddressMailbox *)
                internet_address_list_get_address(list, i);
            if (mb == NULL) {
                (void)fprintf(
                    stderr,
                    "could not extract mailbox from mailbox-list\n"
                );
                g_object_unref(message);
                dtpc_release_delivery(&dlv);
                return EXIT_FAILURE;
            }
            if (mb->addr == NULL) {
                (void)fprintf(stderr, "could not extract addr from mailbox\n");
                g_object_unref(message);
                dtpc_release_delivery(&dlv);
                return EXIT_FAILURE;
            }
            const char *idn_addr = internet_address_mailbox_get_idn_addr(mb);
            if (idn_addr == NULL) {
                (void)fprintf(stderr, "could not get IDN encoded addr-spec\n");
                g_object_unref(message);
                dtpc_release_delivery(&dlv);
                return EXIT_FAILURE;
            }
            const char *domain = idn_addr + mb->at + 1;

            struct ipn_verify res = {node_nbr, 0};

            /* Perform query */
            ares_status_t status = ares_query_dnsrec(
                channel,
                domain,
                ARES_CLASS_IN,
                264, /* IPN RRTYPE value */
                dnsrec_cb,
                &res,
                NULL
            );
            if (status != ARES_SUCCESS) {
                (void)fprintf(
                    stderr,
                    "failed to enqueue query: %s\n",
                    ares_strerror((int)status)
                );
                g_object_unref(message);
                dtpc_release_delivery(&dlv);
                return EXIT_FAILURE;
            }

            /* Wait until no more requests are left to be processed */
            ares_queue_wait_empty(channel, -1);

            if (!res.success) {
                (void)fprintf(stderr, "IPN verification failed\n");
                g_object_unref(message);
                dtpc_release_delivery(&dlv);
                return EXIT_FAILURE;
            }
        }
    }

    while (g_mime_header_list_contains(
        message->parent_object.headers,
        "Return-Path"
    ))
    {
        g_mime_header_list_remove(
            message->parent_object.headers,
            "Return-Path"
        );
    }

    GMimeStream *ostream = g_mime_stream_pipe_new(fileno(stdout));
    if (ostream == NULL) {
        (void)fprintf(stderr, "could not create stream pipe around stdout\n");
        g_object_unref(ostream);
        g_object_unref(message);
        dtpc_release_delivery(&dlv);
        return EXIT_FAILURE;
    }
    g_mime_stream_pipe_set_owner((GMimeStreamPipe *)ostream, FALSE);
    /*
     * g_mime_format_options_new() uses g_slice_new() which can never return
     * NULL.
     */
    GMimeFormatOptions *format = g_mime_format_options_new();
    g_mime_format_options_set_newline_format(format, GMIME_NEWLINE_FORMAT_DOS);
    if (g_mime_object_write_to_stream((GMimeObject *)message, format, ostream)
        == -1)
    {
        (void)fprintf(stderr, "could not write data to stdout\n");
        g_object_unref(ostream);
        g_object_unref(message);
        dtpc_release_delivery(&dlv);
        return EXIT_FAILURE;
    }
    if (g_mime_stream_flush(ostream) == -1) {
        (void)fprintf(stderr, "could not sync stream to disk\n");
        g_object_unref(ostream);
        g_object_unref(message);
        dtpc_release_delivery(&dlv);
        return EXIT_FAILURE;
    }
    g_object_unref(ostream);
    g_object_unref(message);

    dtpc_release_delivery(&dlv);
    return EXIT_SUCCESS;
}

static void handle_interrupt(int sig) {
    (void)sig;
    dtpc_interrupt(sap);
}

int main(int argc, char **argv) {
    int ch;
    unsigned int topic_id = 25;
    char *servers = NULL;

    while ((ch = getopt_long(argc, argv, "t:s:", longopts, NULL)) != -1) {
        switch (ch) {
            case 't': {
                errno = 0;
                char *endptr;
                unsigned long tflag = strtoul(optarg, &endptr, 0);
                if (optarg == endptr) {
                    errno = EINVAL;
                }
                if (errno != 0) {
                    perror("strtoul");
                    free(servers);
                    exit(EXIT_FAILURE);
                }
                if (tflag > UINT_MAX) {
                    (void)fprintf(stderr, "topic_id out of range\n");
                    free(servers);
                    exit(EXIT_FAILURE);
                }
                topic_id = (unsigned int)tflag;
                break;
            }
            case 's':
                servers = strdup(optarg);
                break;
            case 0:
                break;
            default:
                free(servers);
                usage();
        }
    }
    argc -= optind;
    argv += optind;

    if (argc != 0) {
        free(servers);
        usage();
    }

    if (verify_ipn) {
        int status;
        status = ares_library_init(ARES_LIB_INIT_ALL);
        if (status != ARES_SUCCESS) {
            (void)fprintf(
                stderr,
                "c-ares library initialization issue: %s\n",
                ares_strerror(status)
            );
            free(servers);
            exit(EXIT_FAILURE);
        }
        if (!ares_threadsafety()) {
            (void)fprintf(stderr, "c-ares not compiled with thread support\n");
            free(servers);
            exit(EXIT_FAILURE);
        }

        struct ares_options options = {0};
        int optmask = 0;
        optmask |= ARES_OPT_EVENT_THREAD;
        options.evsys = ARES_EVSYS_DEFAULT;

        status = ares_init_options(&channel, &options, optmask);
        if (status != ARES_SUCCESS) {
            (void)fprintf(
                stderr,
                "c-ares initialization issue: %s\n",
                ares_strerror(status)
            );
            free(servers);
            exit(EXIT_FAILURE);
        }

        if (servers != NULL) {
            status = ares_set_servers_csv(channel, servers);
            free(servers);
            if (status != ARES_SUCCESS) {
                (void)fprintf(stderr, "invalid format for list of servers\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    if (dtpc_attach() != 0) {
        (void)fprintf(stderr, "could not attach to DTPC\n");
        exit(EXIT_FAILURE);
    }

    if (dtpc_open(topic_id, NULL, &sap) != 0) {
        (void)fprintf(stderr, "could not open topic %u\n", topic_id);
        dtpc_detach();
        exit(EXIT_FAILURE);
    }

    sdr = bp_get_sdr();
    if (sdr == NULL) {
        (void)fprintf(stderr, "could not obtain handle for SDR\n");
        dtpc_close(sap);
        dtpc_detach();
        exit(EXIT_FAILURE);
    }

    struct sigaction act = {0};
    act.sa_handler = &handle_interrupt;
    if (sigaction(SIGINT, &act, NULL) == -1) {
        perror("sigaction");
        dtpc_close(sap);
        dtpc_detach();
        exit(EXIT_FAILURE);
    }

    int retval = bpmailrecv();

    dtpc_close(sap);
    dtpc_detach();
    if (verify_ipn) {
        ares_destroy(channel);
        ares_library_cleanup();
    }
    return retval;
}
