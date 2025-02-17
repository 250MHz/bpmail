#include "bpmailrecv.h"

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "bp.h"

static struct sdrv_str *sdr = NULL;
static struct bpsap_st *sap = NULL;

static void usage(void) {
    (void)fprintf(stderr, "usage: bpmailrecv own_eid\n");
    exit(EXIT_FAILURE);
}

static int bpmailrecv(void) {
    BpDelivery dlv;

    if (bp_receive(sap, &dlv, BP_BLOCKING) != 0) {
        (void)fprintf(stderr, "could not receive bundle\n");
        return EXIT_FAILURE;
    }

    if (dlv.result != BpPayloadPresent) {
        bp_release_delivery(&dlv, 1);
        return EXIT_SUCCESS;
    }

    /* TODO: handle case when bundle sizes are larger than 2^63-1 */
    ZcoReader reader;
    int64_t buffer_len = 1024;
    char buffer[buffer_len];

    if (sdr_begin_xn(sdr) == 0) {
        (void)fprintf(stderr, "could not initiate a SDR transaction\n");
        bp_release_delivery(&dlv, 1);
        return EXIT_FAILURE;
    }
    int64_t bundle_len_remaining = zco_source_data_length(sdr, dlv.adu);
    sdr_exit_xn(sdr);

    zco_start_receiving(dlv.adu, &reader);
    while (bundle_len_remaining > 0) {
        int64_t read_len = bundle_len_remaining < buffer_len
            ? bundle_len_remaining
            : buffer_len;
        if (sdr_begin_xn(sdr) == 0) {
            (void)fprintf(stderr, "could not initiate a SDR transaction\n");
            break;
        }
        int64_t rc = zco_receive_source(sdr, &reader, read_len, buffer);
        if (sdr_end_xn(sdr) != 0 || rc <= 0) {
            (void)fprintf(stderr, "error extracting data from ZCO\n");
            break;
        }
        size_t ret = fwrite(buffer, sizeof(*buffer), (uint64_t)rc, stdout);
        if (ret != (size_t)rc) {
            (void)fprintf(stderr, "error writing data to stdout\n");
        }
        bundle_len_remaining -= rc;
    }
    (void)fflush(stdout);

    bp_release_delivery(&dlv, 1);
    return EXIT_SUCCESS;
}

static void handle_interrupt(int sig) {
    (void)sig;
    bp_interrupt(sap);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        usage();
    }
    char *own_eid = argv[1];

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

    struct sigaction act = {0};
    act.sa_handler = &handle_interrupt;
    if (sigaction(SIGINT, &act, NULL) == -1) {
        perror("sigaction");
        bp_close(sap);
        bp_detach();
        exit(EXIT_FAILURE);
    }

    int retval = bpmailrecv();

    bp_close(sap);
    bp_detach();
    return retval;
}

