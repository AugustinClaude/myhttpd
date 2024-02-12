#ifndef SERVER_H
#define SERVER_H

#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "../config/config.h"
#include "../http/http.h"
#include "../utils/string/string.h"

#define BUF_SIZE 1024

int launch_server(struct config *config);

#endif /* ! SERVER_H */
