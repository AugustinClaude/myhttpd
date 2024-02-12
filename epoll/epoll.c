#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#define MAX_EV 10

static void wait_epoll(int epoll_fd, int fd, struct epoll_event events[MAX_EV])
{
    while (1)
    {
        int w = epoll_wait(epoll_fd, events, MAX_EV, -1);
        if (w == -1)
            break;

        int q = 0;
        char buf[1024];
        ssize_t r = 1;
        while (r > 0 && !q)
        {
            r = read(fd, buf, 1024);
            if (r <= 0)
                break;

            buf[r] = '\0';
            if (strcmp(buf, "ping") == 0)
                printf("pong!\n");
            else if (strcmp(buf, "pong") == 0)
                printf("ping!\n");
            else if (strcmp(buf, "quit") == 0)
            {
                printf("quit\n");
                q = 1;
            }
            else
                printf("Unknown: %s\n", buf);
        }

        if (q)
            break;
    }
}

int main(int argc, char **argv)
{
    if (argc != 2)
        errx(1, "wrong arguments");

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
        errx(1, "create error");

    int fd = open(argv[1], O_RDWR);
    if (fd == -1)
        errx(1, "fifo open error");
    int f = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    if (f == -1)
        errx(1, "fcntl error");

    struct epoll_event ev;
    struct epoll_event events[MAX_EV];
    ev.data.fd = fd;
    ev.events = EPOLLIN | EPOLLET;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1)
        errx(1, "ctl error");

    wait_epoll(epoll_fd, fd, events);

    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &ev) == -1)
        errx(1, "ctl error");
    if (close(fd) == -1)
        errx(1, "error closing fd");
    if (close(epoll_fd) == -1)
        errx(1, "error closing epoll fd");

    return 0;
}
