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

/*
 * Decompress zlib data dynamically using the inflate API.
 * This avoids imposing a hard limit like 1MB on message size.
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

    if ((ret = inflateInit(&strm)) != Z_OK) {
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
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
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

    if (dlv.length > 4 && strncmp(received_data, "ZLIB", 4) == 0) {
        /*
         * Use dynamic decompression instead of fixed 1MB buffer.
         * This allows receiving messages larger than 1MB.
         */
        size_t decompressed_size = 0;
        Bytef *decompressed = inflate_dynamic(
            (const Bytef *)(received_data + 4), dlv.length - 4, &decompressed_size);

        if (decompressed == NULL) {
            (void)fprintf(stderr, "decompression failed\n");
            free(received_data);
            dtpc_release_delivery(&dlv);
            return EXIT_FAILURE;
        }

        (void)fwrite(decompressed, 1, decompressed_size, stdout);
        free(decompressed);
    } else {
        /* Fallback in case content wasn't compressed (e.g., legacy or test data) */
        (void)fwrite(received_data, 1, dlv.length, stdout);
    }

    (void)fflush(stdout);
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
            case 't': {
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
            }
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
