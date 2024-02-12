#ifndef DAEMON_H
#define DAEMON_H

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../config/config.h"
#include "../server/server.h"

int daemon(struct config *config, int a_val);

#endif /* ! DAEMON_H */
