#include "bpmailsend.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "bp.h"

static struct sdrv_str *sdr = NULL;
static struct bpsap_st *sap = NULL;
static char *dest_eid = NULL;

static void usage(void) {
    (void)fprintf(stderr, "usage: bpmailsend own_eid dest_eid\n");
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

    if (sdr_begin_xn(sdr) == 0) {
        (void)fprintf(stderr, "could not initiate a SDR transaction\n");
        free(content);
        return EXIT_FAILURE;
    }
    /* TODO: verify if we need this check */
    /*if (sdr_heap_depleted(sdr) != 0) {*/
    /*    sdr_exit_xn(sdr);*/
    /*    (void)fprintf(stderr, "could not send mail; SDR low on heap space\n");*/
    /*    return EXIT_FAILURE;*/
    /*}*/

    SdrObject bundle_payload = sdr_insert(sdr, content, (size_t)content_size);
    if (sdr_end_xn(sdr) != 0) {
        (void)fprintf(stderr, "could not copy mail content into a bundle\n");
        free(content);
        return EXIT_FAILURE;
    }

    SdrObject bundle_zco = ionCreateZco(
        ZcoSdrSource,
        bundle_payload,
        0,
        content_size,
        BP_STD_PRIORITY,
        0,
        ZcoOutbound,
        NULL /* TODO: make the call to ionCreateZco blocking */
    );
    if (bundle_zco == 0 || bundle_zco == (SdrObject)ERROR) {
        (void)fprintf(stderr, "could not create ZCO\n");
        free(content);
        return EXIT_FAILURE;
    }

    /* TODO: do not hard code lifespan, classOfService, custodySwitch */
    /* TODO: consider finding a use for srrFlags and ackRequested */
    if (bp_send(
            sap,
            dest_eid,
            NULL,
            86400,
            BP_STD_PRIORITY,
            NoCustodyRequested,
            0,
            0,
            NULL,
            bundle_zco,
            NULL
        )
        <= 0)
    {
        (void)fprintf(stderr, "could not send bundle\n");
        free(content);
        return EXIT_FAILURE;
    }

    free(content);
    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        usage();
    }
    char *own_eid = argv[1];
    dest_eid = argv[2];

    if (bp_attach() != 0) {
        (void)fprintf(stderr, "could not attach to BP\n");
        exit(EXIT_FAILURE);
    }

    if (bp_open(own_eid, &sap) != 0) {
        (void)fprintf(stderr, "could not open own endpoint %s\n", own_eid);
        bp_detach();
        exit(EXIT_FAILURE);
    }

    sdr = bp_get_sdr();
    if (sdr == NULL) {
        (void)fprintf(stderr, "could not obtain handle for SDR\n");
        bp_close(sap);
        bp_detach();
        exit(EXIT_FAILURE);
    }

    int retval = bpmailsend();

    bp_close(sap);
    bp_detach();
    exit(retval);
}

