#include "bpmailrecv.h"

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "bp.h"
#include "dtpc.h"

static struct sdrv_str *sdr = NULL;
static struct dtpcsap_st *sap = NULL;

static void usage(void) {
    (void)fprintf(stderr, "usage: bpmailrecv [ -t topic_id ]\n");
    exit(EXIT_FAILURE);
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

    /* TODO: handle case when bundle sizes are larger than 2^63-1 */
    uint64_t buffer_len = 1024;
    char buffer[buffer_len];

    uint64_t bundle_len_remaining = dlv.length;

    while (bundle_len_remaining > 0) {
        uint64_t read_len = bundle_len_remaining < buffer_len
            ? bundle_len_remaining
            : buffer_len;
        sdr_read(sdr, buffer, dlv.item, read_len);
        size_t ret = fwrite(buffer, sizeof(*buffer), read_len, stdout);
        if (ret != read_len) {
            (void)fprintf(stderr, "error writing data to stdout\n");
        }
        bundle_len_remaining -= read_len;
    }
    (void)fflush(stdout);

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
    while ((ch = getopt(argc, argv, "t:")) != -1) {
        switch (ch) {
            case 't':
                errno = 0;
                unsigned long tflag = strtoul(optarg, NULL, 0);
                if (errno != 0) {
                    perror("strtoul()");
                    exit(EXIT_FAILURE);
                }
                if (tflag > UINT_MAX) {
                    (void)fprintf(stderr, "topic_id out of range\n");
                    exit(EXIT_FAILURE);
                }
                topic_id = (unsigned int)tflag;
                break;
            default:
                usage();
        }
    }
    argc -= optind;
    argv += optind;

    if (argc != 0) {
        usage();
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
    return retval;
}

