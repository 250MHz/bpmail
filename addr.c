#include <stddef.h>
#include <stdio.h>

#include "third_party/dovecot-parser.h"

int main(void) {
    char *content = NULL;
    size_t content_cap = 0;
    /* content_size includes the trailing '\0' */
    ssize_t content_size = getdelim(&content, &content_cap, '\0', stdin);

    struct message_address *a =
        message_address_parse(content, (size_t)content_size, 64, 0);
    for (struct message_address *p = a; p != NULL; p = p->next) {
        printf("is null: %d\n", p == NULL);
        printf("%p\n", (void *)p);
        printf("%p\n", (void *)p->next);
        printf("errors: %d\n", p->invalid_syntax);
        printf("display-name: %s\n", p->name);
        printf("route: %s\n", p->route);
        printf("domain: %s\n", p->domain);
        printf("original: %s\n", p->original);
    }

    return 0;
}

