#ifndef HTTP_H
#define HTTP_H

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "../config/config.h"
#include "../server/server.h"
#include "../utils/string/string.h"

struct header
{
    struct string *key;
    struct string *value;
    struct header *next;
};

struct http_msg
{
    int method;
    struct string *path;
    struct string *version;
    struct header *headers;
    struct string *body;
    char *status;
    char *reason;
};

void print_msg(struct http_msg *msg);
void *destroy_msg(struct http_msg *msg);
struct http_msg *parse_request(struct string *request, struct config *config);
struct string *send_response(struct http_msg *msg, struct config *config,
                             struct string **body, struct string **path);

#endif /* ! HTTP_H */
