#include <stdio.h>
#include <unistd.h>

#include "gmime/gmime-parser.h"
#include "gmime/gmime.h"
#include "gmime/internet-address.h"
int main(int argc, char **argv) {
    char *content = NULL;
    size_t content_cap = 0;
    /* content_size includes the trailing '\0' */
    ssize_t content_size = getdelim(&content, &content_cap, '\0', stdin);

    /* initialize GMime */
    g_mime_init();

    GMimeStream *istream =
        g_mime_stream_mem_new_with_buffer(content, (size_t)content_size);
    GMimeParser *parser = g_mime_parser_new_with_stream(istream);
    g_object_unref(istream);

    GMimeMessage *message = g_mime_parser_construct_message(parser, NULL);
    g_object_unref(parser);
    if (message == NULL) {
        fprintf(stderr, "could not parse message\n");
        (void)fwrite(content, 1, (unsigned long)content_size, stdout);
        free(content);
        (void)fflush(stdout);
        exit(1);
    }

    printf("reached\n");
    // printf("%s\n", message->subject);
    // printf("%s\n", message->message_id);
    printf("reached\n");
    InternetAddressList *list = g_mime_message_get_from(message);
    printf("%s\n", internet_address_list_to_string(list, NULL, FALSE));
    for (int i = 0; i < internet_address_list_length(list); i++) {
        InternetAddress *ia = internet_address_list_get_address(list, i);
        InternetAddressMailbox *mb = (InternetAddressMailbox *)ia;
        printf("%d\n", mb->addr != NULL);
        printf("%d\n", mb->idn_addr != NULL);
        if (mb->addr == NULL) {
            /* create a stream around stdout */
            // GMimeStream *ostream = g_mime_stream_fs_new(dup(fileno(stdout)));
            GMimeStream *ostream = g_mime_stream_pipe_new(STDOUT_FILENO);
            g_mime_stream_pipe_set_owner((GMimeStreamPipe *)ostream, FALSE);

            /* 'printf' */
            g_mime_object_write_to_stream(
                (GMimeObject *)message,
                NULL,
                ostream
            );

            /* flush stdout */
            g_mime_stream_flush(ostream);

            /* free/close the stream */
            g_object_unref(ostream);
            g_object_unref(message);

            return 0;
        }

        printf("%s\n", ia->name);
        printf("%s\n", ia->charset);
        printf("%s\n", mb->addr);
        printf("%s\n", mb->idn_addr);
        printf("%s\n", mb->idn_addr + mb->at + 1);
        printf(
            "%s\n",
            internet_address_mailbox_get_idn_addr((InternetAddressMailbox *)ia)
        );
        printf("%s\n", internet_address_to_string(ia, NULL, FALSE));

        printf("-----\n");
    }

    g_mime_header_list_remove(message->parent_object.headers, "Return-Path");

    /* create a stream around stdout */
    GMimeStream *ostream = g_mime_stream_fs_new(dup(fileno(stdout)));

    /* 'printf' */
    g_mime_object_write_to_stream((GMimeObject *)message, NULL, ostream);

    /* flush stdout */
    g_mime_stream_flush(ostream);

    /* free/close the stream */
    g_object_unref(ostream);
    g_object_unref(message);

    return 0;
}

