#include "bpmailsend.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "bp.h"
#include "dtpc.h"
#include "zlib.h"

static char *dest_eid = NULL;
static unsigned int profile_id = 0;
static struct dtpcsap_st *sap = NULL;
static struct sdrv_str *sdr = NULL;

static void usage(void) {
    (void)fprintf(
        stderr,
        "%s\n",
        "usage: bpmailsend [ -t topic_id ] profile_id dest_eid"
    );
    exit(EXIT_FAILURE);
}

static int bpmailsend(void) {
    /*
     * TODO: we're currently reading from stdin, storing it in a malloc'd
     * buffer, and then copying that buffer into the SDR for our bundle's
     * payload. It would be better if we skipped the second step and just wrote
     * from stdin to the SDR, but SDR doesn't have a realloc() function so I
     * don't know how we'd do that.
     */
    char *content = NULL;
    size_t content_cap = 0;
    /* content_size includes the trailing '\0' */
    ssize_t content_size = getdelim(&content, &content_cap, '\0', stdin);

    if (content_size == -1) {
        (void)fprintf(
            stderr,
            "error or nothing to read from stdin; nothing to send\n"
        );
        return EXIT_FAILURE;
    }

    /*
     * TODO: DTPC API uses unsigned int to specify content size, so we can't
     * send more than UINT_MAX at once. We should allocate and send multiple
     * times rather than just once.
     */
    if (content_size > UINT_MAX) {
        (void)fprintf(stderr, "content too large to send\n");
        free(content);
        return EXIT_FAILURE;
    }

    /* Compress content using zlib */
    uLong compressed_size = compressBound((uLong)content_size);
    Bytef *compressed = malloc(compressed_size);
    if (compressed == NULL) {
        fprintf(stderr, "malloc failed\n");
        free(content);
        return EXIT_FAILURE;
    }

    if (compress(
            compressed,
            &compressed_size,
            (Bytef *)content,
            (uLong)content_size
        )
        != Z_OK)
    {
        fprintf(stderr, "compression failed\n");
        free(content);
        free(compressed);
        return EXIT_FAILURE;
    }
    free(content); /* free original data once compressed */

    if (sdr_begin_xn(sdr) == 0) {
        (void)fprintf(stderr, "could not initiate a SDR transaction\n");
        free(compressed);
        return EXIT_FAILURE;
    }
    /*
     * TODO: verify if we need this check
     * if (sdr_heap_depleted(sdr) != 0) {
     *     sdr_exit_xn(sdr);
     *     (void)fprintf(stderr, "could not send mail; SDR low on heap space\n");
     *     return EXIT_FAILURE;
     * }
     */

    SdrObject adu_payload =
        sdr_insert(sdr, (char *)compressed, (unsigned long)compressed_size);
    if (sdr_end_xn(sdr) != 0) {
        (void)fprintf(stderr, "could not copy data into SDR\n");
        free(compressed);
        return EXIT_FAILURE;
    }

    switch (dtpc_send(
        profile_id,
        sap,
        dest_eid,
        0,
        0,
        0,
        0,
        NULL,
        0,
        NoCustodyRequested,
        NULL,
        BP_STD_PRIORITY,
        adu_payload,
        (unsigned int)compressed_size
    ))
    {
        case -1:
            (void)fprintf(stderr, "system failure from dtpc_send\n");
            free(compressed);
            return EXIT_FAILURE;
        case 0:
            (void)fprintf(stderr, "could not send payload\n");
            free(compressed);
            if (sdr_begin_xn(sdr) == 0) {
                (void)fprintf(stderr, "could not initiate a SDR transaction\n");
                return EXIT_FAILURE;
            }
            sdr_free(sdr, adu_payload);
            if (sdr_end_xn(sdr) != 0) {
                (void)fprintf(stderr, "could not free ADU memory from SDR\n");
            }
            return EXIT_FAILURE;
        case 1:
            /* Fall through */
        default:
            break;
    }

    free(compressed);
    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    int ch;
    char *endptr;
    unsigned int topic_id = 25;

    while ((ch = getopt(argc, argv, "t:")) != -1) {
        switch (ch) {
            case 't': {
                errno = 0;
                unsigned long tflag = strtoul(optarg, &endptr, 0);
                if (optarg == endptr) {
                    errno = EINVAL;
                }
                if (errno != 0) {
                    perror("strtoul");
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

    if (argc != 2) {
        usage();
    }

    errno = 0;
    unsigned long _profile_id = strtoul(argv[0], &endptr, 0);
    if (argv[0] == endptr) {
        errno = EINVAL;
    }
    if (errno != 0) {
        perror("strtoul");
        exit(EXIT_FAILURE);
    }
    if (_profile_id > UINT_MAX) {
        (void)fprintf(stderr, "profile_id out of range\n");
        exit(EXIT_FAILURE);
    }
    profile_id = (unsigned int)_profile_id;

    dest_eid = argv[1];

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

    int retval = bpmailsend();

    dtpc_close(sap);
    dtpc_detach();
    return retval;
}
