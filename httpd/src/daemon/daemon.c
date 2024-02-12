#define _POSIX_C_SOURCE 200809L

#include "daemon.h"

static int get_pid(char *pid_file)
{
    FILE *file = fopen(pid_file, "r");
    if (!file)
    {
        fprintf(stderr, "File '%s' couldn't be opened.\n", pid_file);
        return -1;
    }

    char *line = NULL;
    size_t n = 0;
    getline(&line, &n, file);

    int res = atoi(line);
    free(line);

    if (fclose(file) == -1)
    {
        fprintf(stderr, "File '%s' couldn't be closed.\n", pid_file);
        return -1;
    }

    return res;
}

static int daemon_start(struct config *config)
{
    char *pid_file = config->pid_file;

    struct stat buf;
    int exists = stat(pid_file, &buf) == 0;
    if (exists)
    {
        int pid = get_pid(pid_file);
        if (pid == -1)
            return 1;

        if (pid != 0 && kill(pid, 0) == 0)
            return 1;
    }

    pid_t cpid = fork();
    if (cpid == -1)
    {
        fprintf(stderr, "Fork failed.\n");
        return 1;
    }
    else if (cpid)
    {
        FILE *file = fopen(pid_file, "w");
        if (!file)
        {
            fprintf(stderr, "File '%s' couldn't be opened.\n", pid_file);
            return 1;
        }

        fprintf(file, "%d", cpid);

        if (fclose(file) == -1)
        {
            fprintf(stderr, "File '%s' couldn't be closed.\n", pid_file);
            return 1;
        }

        return 0;
    }

    int c = launch_server(config);
    return c;
}

static int daemon_stop(char *pid_file)
{
    struct stat buf;
    int exists = stat(pid_file, &buf) == 0;
    if (exists)
    {
        int pid = get_pid(pid_file);
        if (pid != 0)
            kill(pid, SIGINT);

        if (remove(pid_file) == -1)
        {
            fprintf(stderr, "Couldn't delete file '%s'.\n", pid_file);
            return 1;
        }
    }

    return 0;
}

static int daemon_reload(void)
{
    return 0;
}

static int daemon_restart(struct config *config)
{
    int c = daemon_stop(config->pid_file);
    if (c == -1)
        return 1;

    c = daemon_start(config);
    return c;
}

int daemon(struct config *config, int a_val)
{
    int err = 0;
    if (a_val == 0)
        err = daemon_start(config);
    else if (a_val == 1)
        err = daemon_stop(config->pid_file);
    else if (a_val == 2)
        err = daemon_reload();
    else if (a_val == 3)
        err = daemon_restart(config);

    return err;
}
