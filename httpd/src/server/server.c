#define _POSIX_C_SOURCE 200112

#include "server.h"

static int sig = 1;

static void handler(int signum)
{
    if (signum == SIGUSR1)
        sig = 2;
    else if (signum == SIGUSR2)
        sig = 3;
    else if (signum == SIGINT)
        sig = 0;
}

static int handle_signals(void)
{
    struct sigaction sa;
    sa.sa_flags = 0;
    sa.sa_handler = handler;

    if (sigemptyset(&sa.sa_mask) < 0)
        return 1;
    if (sigaction(SIGUSR1, &sa, NULL) < 0)
        return 1;
    if (sigaction(SIGUSR2, &sa, NULL) < 0)
        return 1;
    if (sigaction(SIGINT, &sa, NULL) < 0)
        return 1;

    return 0;
}

static void concat_request(char buf[], struct string *request, int r)
{
    for (int i = 0; i < r; i++)
    {
        if ((i > 0 && buf[i] == '\n' && buf[i - 1] != '\r')
            || (i == 0 && buf[i] == '\n'))
            string_concat_str(request, "\r", 1);
        string_concat_str(request, &buf[i], 1);
    }
}

static struct string *get_request(int client_fd)
{
    struct string *request = string_create(NULL, 0);
    char buf[BUF_SIZE] = { '\0' };
    ssize_t r = 1;
    int body = 0;
    while (r > 0 && !body)
    {
        r = recv(client_fd, buf, BUF_SIZE, MSG_NOSIGNAL);
        if (r <= 0)
            break;

        concat_request(buf, request, r);

        int i = request->size - 4;
        if (i > 0 && request->data[i] == '\r' && request->data[i + 1] == '\n'
            && request->data[i + 2] == '\r' && request->data[i + 3] == '\n')
            body = 1;
    }

    r = 1;
    while (r > 0 && body)
    {
        r = recv(client_fd, buf, BUF_SIZE, MSG_NOSIGNAL | MSG_DONTWAIT);
        if (r <= 0)
            break;

        concat_request(buf, request, r);
    }

    return request;
}

static int send_body(int client_fd, struct string *body, struct string *path)
{
    char *str_path = get_str(path);
    int body_fd = open(str_path, O_RDONLY);
    if (body_fd == -1)
        return 1;
    free(str_path);

    size_t total = 0;
    while (total < body->size)
    {
        ssize_t s = sendfile(client_fd, body_fd, NULL, body->size);
        if (s == -1)
        {
            close(body_fd);
            fprintf(stderr, "Sendfile failed.\n");
            return 1;
        }

        total += s;
    }

    close(body_fd);
    return 0;
}

static int communicate_msg(int client_fd, struct string **params[2],
                           struct http_msg **msg, struct config *config)
{
    struct string **request = params[0];
    struct string **to_send = params[1];

    // Parse request
    *request = get_request(client_fd);
    *msg = parse_request(*request, config);

    if (!sig)
        return 0;

    // Send response
    struct string *body = NULL;
    struct string *path = NULL;
    *to_send = send_response(*msg, config, &body, &path);

    size_t total = 0;
    while (total < (*to_send)->size)
    {
        if (!sig)
        {
            string_destroy(body);
            string_destroy(path);
            return 0;
        }

        ssize_t s = send(client_fd, (*to_send)->data, (*to_send)->size, 0);
        if (s == -1)
        {
            string_destroy(body);
            string_destroy(path);
            fprintf(stderr, "Send failed.\n");
            return 1;
        }

        total += s;
    }

    int err = 0;
    if (*msg && (*msg)->method == 0 && strcmp((*msg)->status, "200") == 0)
        err = send_body(client_fd, body, path);

    string_destroy(body);
    string_destroy(path);
    return err;
}

static int client_connection(int socket_fd, struct config *config)
{
    int client_fd = 0;
    struct string *request = NULL;
    struct http_msg *msg = NULL;
    struct string *to_send = NULL;
    int err = 0;

    while (sig)
    {
        client_fd = accept(socket_fd, NULL, NULL);
        if (client_fd == -1)
            continue;

        if (!sig)
        {
            close(client_fd);
            break;
        }

        struct string **params[2] = {
            &request,
            &to_send,
        };

        err = communicate_msg(client_fd, params, &msg, config);

        destroy_msg(msg);
        string_destroy(to_send);
        string_destroy(request);
        close(client_fd);
    }

    return err;
}

static int create_socket(struct addrinfo *tmp)
{
    int socket_fd = -1;
    while (tmp)
    {
        socket_fd = socket(tmp->ai_family, tmp->ai_socktype, tmp->ai_protocol);
        if (socket_fd == -1)
            continue;

        int sock = 1;
        setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &sock, sizeof(sock));

        int b = bind(socket_fd, tmp->ai_addr, tmp->ai_addrlen);
        if (b == 0)
            break;

        close(socket_fd);
        socket_fd = -1;
        tmp = tmp->ai_next;
    }

    return socket_fd;
}

static int initiate_connection(int socket_fd, struct config *config)
{
    int err = handle_signals();
    if (err)
        return 1;

    err = client_connection(socket_fd, config);
    if (err)
        return 1;

    return 0;
}

int launch_server(struct config *config)
{
    char *node = config->servers->ip;
    char *port = config->servers->port;

    struct addrinfo hints = { 0 };
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *res = NULL;
    int info = getaddrinfo(node, port, &hints, &res);
    if (info != 0)
    {
        fprintf(stderr, "getaddrinfo: %s.\n", gai_strerror(info));
        return 1;
    }

    struct addrinfo *tmp = res;
    int socket_fd = create_socket(tmp);

    if (!tmp)
    {
        close(socket_fd);
        fprintf(stderr, "Coup dur pour le serveur.\n");
        return 1;
    }
    freeaddrinfo(res);

    int l = listen(socket_fd, 1);
    if (l == -1)
    {
        close(socket_fd);
        fprintf(stderr, "Listen error.\n");
        return 1;
    }

    int err = initiate_connection(socket_fd, config);
    if (err)
    {
        close(socket_fd);
        return 1;
    }

    close(socket_fd);
    return sig;
}
