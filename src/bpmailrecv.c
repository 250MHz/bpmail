#include "bpmailrecv.h"

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

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

    char *received_data = malloc(dlv.length);
    if (!received_data) {
        fprintf(stderr, "malloc failed\n");
        dtpc_release_delivery(&dlv);
        return EXIT_FAILURE;
    }

    sdr_read(sdr, received_data, dlv.item, dlv.length);

    if (dlv.length > 4 && strncmp(received_data, "ZLIB", 4) == 0) {
        Bytef *decompressed = malloc(1024 * 1024);  // 1MB buffer
        uLongf decompressed_size = 1024 * 1024;
        if (uncompress(decompressed, &decompressed_size,
                       (Bytef *)(received_data + 4), (uLong)(dlv.length - 4)) != Z_OK) {
            fprintf(stderr, "decompression failed\n");
            free(received_data);
            free(decompressed);
            dtpc_release_delivery(&dlv);
            return EXIT_FAILURE;
        }
        fwrite(decompressed, 1, decompressed_size, stdout);
        free(decompressed);
    } else {
        fwrite(received_data, 1, dlv.length, stdout);
    }

    fflush(stdout);
    free(received_data);
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
                char *endptr;
                unsigned long tflag = strtoul(optarg, &endptr, 0);
                if (optarg == endptr) {
                    errno = EINVAL;
                }
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
