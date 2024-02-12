#define _POSIX_C_SOURCE 200809L

#include "http.h"

void print_msg(struct http_msg *msg)
{
    if (!msg)
        return;

    printf("Method: %d\n", msg->method);
    printf("Path: ");
    string_print(msg->path);
    printf("\n");
    printf("Version: ");
    string_print(msg->version);
    printf("\n");

    printf("Headers:\n");
    struct header *tmp = msg->headers;
    while (tmp)
    {
        string_print(tmp->key);
        printf(": ");
        string_print(tmp->value);
        printf("\n");
        tmp = tmp->next;
    }

    printf("Body:\n");
    string_print(msg->body);
    printf("\n");
}

void *destroy_msg(struct http_msg *msg)
{
    if (!msg)
        return NULL;

    string_destroy(msg->path);
    string_destroy(msg->version);
    string_destroy(msg->body);

    struct header *tmp = msg->headers;
    while (tmp)
    {
        struct header *tmp2 = tmp;
        tmp = tmp->next;

        string_destroy(tmp2->key);
        string_destroy(tmp2->value);
        free(tmp2);
    }

    free(msg);
    return NULL;
}

static void destroy_headers(struct header *headers)
{
    struct header *tmp = headers;
    while (tmp)
    {
        struct header *tmp2 = tmp;
        tmp = tmp->next;

        string_destroy(tmp2->key);
        string_destroy(tmp2->value);
        free(tmp2);
    }
}

static int overflowing(void *ptr, struct string *request)
{
    char *data = ptr;
    int i = data - request->data;
    int size = request->size;
    return i >= size;
}

static void *my_memchr(void *ptr, char c, struct string *request)
{
    if (!ptr)
        return NULL;

    char *data = ptr;
    while (!overflowing(data, request) && *data != c)
        data++;

    if (overflowing(data, request))
        return NULL;
    return data;
}

static int my_memlen(void *ptr, struct string *request)
{
    if (!ptr)
        return 0;

    int len = 0;
    char *data = ptr;
    while (!overflowing(data, request))
    {
        len++;
        data++;
    }

    return len;
}

static int my_atoi(struct string *str)
{
    char *data = malloc(str->size + 1);
    memcpy(data, str->data, str->size);
    data[str->size] = '\0';

    int n = atoi(data);
    free(data);
    return n;
}

static int space_in(char *str)
{
    for (int i = 0; str[i] != '\0'; i++)
    {
        if (isspace(str[i]))
            return 1;
    }

    return 0;
}

static int char_in(char *str, int ind)
{
    for (int i = ind; str[i] != '\0'; i++)
    {
        if (!isspace(str[i]))
            return 1;
    }

    return 0;
}

static char *my_tolower(struct string *str)
{
    for (size_t i = 0; i < str->size; i++)
    {
        if (str->data[i] >= 'A' && str->data[i] <= 'Z')
            str->data[i] += 'a' - 'A';
    }

    return str->data;
}

static struct header *add_header(struct header *headers, struct header *elm)
{
    if (!headers)
    {
        headers = elm;
        return headers;
    }

    struct header *tmp = headers;
    while (tmp->next)
        tmp = tmp->next;

    tmp->next = elm;
    return headers;
}

static int host_valid(struct header *header, struct config *config)
{
    struct string *s1 = header->value;
    struct string *s2 = config->servers->server_name;
    char *s3 = config->servers->ip;
    struct string *s4 = string_create(s3, strlen(s3));
    string_concat_str(s4, ":", 1);
    string_concat_str(s4, config->servers->port, strlen(config->servers->port));

    if (s1->size == s2->size && memcmp(s1->data, s2->data, s1->size) == 0)
    {
        string_destroy(s4);
        return 1;
    }
    if (s1->size == strlen(s3) && memcmp(s1->data, s3, s1->size) == 0)
    {
        string_destroy(s4);
        return 1;
    }
    if (s1->size == s4->size && memcmp(s1->data, s4->data, s4->size) == 0)
    {
        string_destroy(s4);
        return 1;
    }

    string_destroy(s4);
    return 0;
}

static int check_headers(struct header *headers, struct config *config)
{
    int host = 0;
    struct header *tmp = headers;
    while (tmp)
    {
        size_t min = tmp->key->size < 4 ? tmp->key->size : 4;
        if (!host && memcmp(my_tolower(tmp->key), "host", min) == 0)
        {
            if (!host_valid(tmp, config))
                return 0;
            host = 1;
        }

        struct header *tmp2 = tmp->next;
        while (tmp2)
        {
            size_t size1 = tmp->key->size;
            size_t size2 = tmp2->key->size;
            min = size1 < size2 ? size1 : size2;
            if (memcmp(my_tolower(tmp->key), my_tolower(tmp2->key), min) == 0)
                return 0;

            tmp2 = tmp2->next;
        }

        tmp = tmp->next;
    }

    return host;
}

static char *get_method(struct string *request, struct http_msg *msg)
{
    char *end_method = my_memchr(request->data, ' ', request);
    if (!end_method)
        return NULL;

    if (memcmp(request->data, "GET", strlen("GET")) == 0)
        msg->method = 0;
    else if (memcmp(request->data, "HEAD", strlen("HEAD")) == 0)
        msg->method = 1;
    else
        msg->method = 2;

    return end_method;
}

static char *get_path(struct string *request, struct http_msg *msg,
                      char *end_method)
{
    end_method++;
    char *end_path = my_memchr(end_method, ' ', request);
    if (!end_path)
        return NULL;

    int path_len = end_path - end_method;
    char *path = malloc(sizeof(char) * (path_len + 1));
    memcpy(path, end_method, path_len);
    path[path_len] = '\0';
    if (path[0] != '/')
        return NULL;

    int i = 0;
    while (i < path_len && path[i] != '?')
        i++;
    if (i < path_len)
        path_len = i;

    msg->path = string_create(path, path_len);
    free(path);

    return end_path;
}

static char *get_version(struct string *request, struct http_msg *msg,
                         char *end_path)
{
    end_path++;
    char *end_version = my_memchr(end_path, '\r', request);
    if (!end_version)
        return NULL;

    int version_len = end_version - end_path;
    char *version = malloc(sizeof(char) * (version_len + 1));
    memcpy(version, end_path, version_len);
    version[version_len] = '\0';
    msg->version = string_create(version, version_len);
    free(version);

    return end_version;
}

static char *get_header_key(struct string *request, struct header *curr,
                            char *end_version)
{
    char *end_key = my_memchr(end_version, ':', request);
    if (!end_key)
        return NULL;

    int key_len = end_key - end_version;
    char *key = malloc(sizeof(char) * (key_len + 1));
    memcpy(key, end_version, key_len);
    key[key_len] = '\0';
    if (space_in(key))
    {
        free(key);
        return NULL;
    }
    curr->key = string_create(key, key_len);
    free(key);

    end_key++;
    if (overflowing(end_key, request))
        return NULL;

    return end_key;
}

static char *get_header_value(struct string *request, struct header *curr,
                              char *end_key)
{
    char *end_value = my_memchr(end_key, '\r', request);
    if (!end_value)
        return NULL;

    int value_len = end_value - end_key;
    char *value = malloc(sizeof(char) * (value_len + 1));
    memcpy(value, end_key, value_len);
    value[value_len] = '\0';

    // Ignore whitespaces
    int i = 0;
    char *tmp = value;
    while (i < value_len && isspace(value[i]))
        i++;

    value += i;
    value_len -= i;
    i = 0;
    while (i < value_len && !isspace(value[i]))
        i++;
    value_len = i;

    if (value_len == 0 || char_in(value, i))
    {
        free(tmp);
        return NULL;
    }

    curr->value = string_create(value, value_len);
    free(tmp);

    end_value++;
    if (overflowing(end_value, request) || *end_value != '\n')
        return NULL;

    return end_value;
}

static char *get_headers(struct string *request, struct http_msg *msg,
                         struct config *config, char *end_version)
{
    end_version++;
    if (overflowing(end_version, request) || *end_version != '\n')
        return NULL;

    end_version++;
    if (overflowing(end_version, request))
        return NULL;

    struct header *headers = NULL;
    char *ptr = end_version;
    while (!overflowing(ptr, request) && ptr[0] != '\r' && ptr[1] != '\n')
    {
        struct header *curr = malloc(sizeof(struct header));
        memset(curr, 0, sizeof(struct header));

        // Get Header Key
        char *end_key = get_header_key(request, curr, end_version);
        if (!end_key)
        {
            destroy_headers(headers);
            return NULL;
        }

        // Get Header Value
        char *end_value = get_header_value(request, curr, end_key);
        if (!end_value)
        {
            destroy_headers(headers);
            return NULL;
        }

        ptr = ++end_value;
        end_version = ptr;
        headers = add_header(headers, curr);
    }

    msg->headers = headers;
    if (!headers || overflowing(ptr, request) || ptr[0] != '\r'
        || ptr[1] != '\n' || !check_headers(msg->headers, config))
        return NULL;

    return ptr;
}

static struct http_msg *get_request_body(struct string *request,
                                         struct http_msg *msg, char *ptr)
{
    ptr++;
    if (overflowing(ptr, request) || *ptr != '\n')
        return NULL;
    ptr++;

    struct header *tmp = msg->headers;
    int len = memcmp(my_tolower(tmp->key), "content-length", tmp->key->size);
    while (tmp && len != 0)
    {
        tmp = tmp->next;
        if (!tmp)
            break;

        len = memcmp(my_tolower(tmp->key), "content-length", tmp->key->size);
    }

    if (overflowing(ptr, request))
    {
        if (len == 0)
            return NULL;

        msg->body = string_create(NULL, 0);
        return msg;
    }

    int body_len = my_memlen(ptr, request);
    if (len == 0)
    {
        int l = my_atoi(tmp->value);
        if (l <= 0 || l > body_len)
            return NULL;
        if (l <= body_len)
            body_len = l;
    }

    char *body = malloc(sizeof(char) * (body_len + 1));
    memcpy(body, ptr, body_len);
    body[body_len] = '\0';
    msg->body = string_create(body, body_len);
    free(body);

    return msg;
}

struct http_msg *parse_request(struct string *request, struct config *config)
{
    if (!request || !request->data || request->size == 0)
        return NULL;

    struct http_msg *msg = malloc(sizeof(struct http_msg));
    memset(msg, 0, sizeof(struct http_msg));

    // Get Method
    char *end_method = get_method(request, msg);
    if (!end_method)
        return destroy_msg(msg);

    // Get Path
    char *end_path = get_path(request, msg, end_method);
    if (!end_path)
        return destroy_msg(msg);

    // Get HTTP Version
    char *end_version = get_version(request, msg, end_path);
    if (!end_version)
        return destroy_msg(msg);

    // Get Headers
    char *ptr = get_headers(request, msg, config, end_version);
    if (!ptr)
        return destroy_msg(msg);

    // Get Body
    struct http_msg *tmp = get_request_body(request, msg, ptr);
    if (!tmp)
        return destroy_msg(msg);

    return msg;
}

static int check_http_version(struct http_msg *msg)
{
    char *begin = msg->version->data;
    char *version = my_memchr(begin, '/', msg->version);
    if (!version)
        return 0;

    int len = version - begin;
    if (memcmp(begin, "HTTP", len) != 0)
        return 0;

    int ver_len = msg->version->size - len - 1;
    if (ver_len != 3)
        return 0;

    version++;
    if (overflowing(version, msg->version))
        return 0;

    if (!isdigit(version[0]) || version[1] != '.' || !isdigit(version[2]))
        return 0;
    if (version[0] != '1' || version[2] != '1')
        return 2;

    return 1;
}

static void get_status(struct http_msg *msg)
{
    if (msg->method == 2)
    {
        msg->status = "405";
        msg->reason = "Method Not Allowed";
    }
    else
    {
        int check_ver = check_http_version(msg);
        if (check_ver == 0)
        {
            msg->status = "400";
            msg->reason = "Bad Request";
        }
        else if (check_ver == 2)
        {
            msg->status = "505";
            msg->reason = "HTTP Version Not Supported";
        }
    }
}

static size_t get_size_length(size_t size)
{
    if (size < 10)
        return 1;

    return 1 + get_size_length(size / 10);
}

static void handle_dir(struct config *config, struct string *path,
                       struct stat buf)
{
    if (S_ISDIR(buf.st_mode))
    {
        string_concat_str(path, "/", 1);
        if (!config->servers->default_file)
            string_concat_str(path, "index.html", strlen("index.html"));
        else
            string_concat_str(path, config->servers->default_file,
                              strlen(config->servers->default_file));
    }
}

static FILE *open_file(struct http_msg *msg, struct config *config,
                       struct string **path)
{
    *path = string_create(config->servers->root_dir,
                          strlen(config->servers->root_dir));
    string_concat_str(*path, msg->path->data, msg->path->size);

    char *str_path = get_str(*path);
    struct stat buf;
    if (stat(str_path, &buf) == -1)
    {
        msg->status = "404";
        msg->reason = "Not Found";
        free(str_path);
        return NULL;
    }
    free(str_path);

    handle_dir(config, *path, buf);

    str_path = get_str(*path);
    errno = 0;
    FILE *file = fopen(str_path, "r");
    if (!file)
    {
        if (errno == EACCES)
        {
            msg->status = "403";
            msg->reason = "Forbidden";
        }
        else
        {
            msg->status = "404";
            msg->reason = "Not Found";
        }

        free(str_path);
        return NULL;
    }

    free(str_path);
    return file;
}

static struct string *get_body(struct http_msg *msg, struct config *config,
                               struct string **path)
{
    struct string *body = string_create(NULL, 0);
    if (!msg)
        return body;

    FILE *file = open_file(msg, config, path);
    if (!file)
        return body;

    char *line = NULL;
    size_t n = 0;
    ssize_t l = 1;
    while (l != -1)
    {
        l = getline(&line, &n, file);
        if (l == -1)
            break;

        string_concat_str(body, line, l);
    }

    free(line);
    return body;
}

static void add_date(struct string *response)
{
    char date[1024];
    time_t now = time(0);
    struct tm *tmp = gmtime(&now);
    size_t date_len =
        strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S %Z", tmp);
    string_concat_str(response, "Date: ", strlen("Date: "));
    string_concat_str(response, date, date_len);
    string_concat_str(response, "\r\n", 2);
}

static struct string *send_badrequest(void)
{
    char *status = "400";
    char *reason = "Bad Request";

    struct string *response = string_create("HTTP/1.1", strlen("HTTP/1.1"));
    string_concat_str(response, " ", 1);
    string_concat_str(response, status, strlen(status));
    string_concat_str(response, " ", 1);
    string_concat_str(response, reason, strlen(reason));
    string_concat_str(response, "\r\n", 2);

    add_date(response);

    char *connection_close = "Connection: close";
    string_concat_str(response, connection_close, strlen(connection_close));
    string_concat_str(response, "\r\n", 2);
    string_concat_str(response, "\r\n", 2);

    return response;
}

struct string *send_response(struct http_msg *msg, struct config *config,
                             struct string **body, struct string **path)
{
    if (!msg)
        return send_badrequest();

    // Update Status
    msg->status = "200";
    msg->reason = "OK";
    get_status(msg);
    *body = get_body(msg, config, path);

    // Construct Status Line
    struct string *response = string_create("HTTP/1.1", strlen("HTTP/1.1"));
    string_concat_str(response, " ", 1);
    string_concat_str(response, msg->status, strlen(msg->status));
    string_concat_str(response, " ", 1);
    string_concat_str(response, msg->reason, strlen(msg->reason));
    string_concat_str(response, "\r\n", 2);

    // Construct Date
    add_date(response);

    // Construct Content-Length
    if (strcmp(msg->status, "200") == 0)
    {
        size_t bodysize_len = get_size_length((*body)->size);
        char *bodysize = malloc(bodysize_len + 1);
        snprintf(bodysize, bodysize_len + 1, "%ld", (*body)->size);

        char *content = "Content-Length: ";
        string_concat_str(response, content, strlen(content));
        string_concat_str(response, bodysize, bodysize_len);

        string_concat_str(response, "\r\n", 2);
        free(bodysize);
    }

    // Construct Connection
    char *connection_close = "Connection: close";
    string_concat_str(response, connection_close, strlen(connection_close));
    string_concat_str(response, "\r\n", 2);
    string_concat_str(response, "\r\n", 2);

    return response;
}
