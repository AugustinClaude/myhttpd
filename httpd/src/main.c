#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "config/config.h"
#include "daemon/daemon.h"
#include "server/server.h"

static int option(char *opt)
{
    int value = -1;
    if (strcmp(opt, "start") == 0)
        value = 0;
    else if (strcmp(opt, "stop") == 0)
        value = 1;
    else if (strcmp(opt, "reload") == 0)
        value = 2;
    else if (strcmp(opt, "restart") == 0)
        value = 3;

    return value;
}

static int start_all(char **argv, int conf_ind, int dryrun_opt, int a_val)
{
    struct config *config = parse_configuration(argv[conf_ind]);
    if (!config)
        errx(2, "Configuration file is invalid.");
    if (dryrun_opt)
    {
        config_destroy(config);
        return 0;
    }

    /*
    char *log = NULL;
    if (config->log)
    {
        if (config->log_file)
            log = config->log_file;
        if (a_val != -1)
            log = "HTTPd.log";
    }
    */

    int err = 0;
    if (a_val != -1)
        err = daemon(config, a_val);
    else
        err = launch_server(config);

    config_destroy(config);
    return err;
}

int main(int argc, char **argv)
{
    if (argc < 2)
        errx(1, "Not enough arguments.");

    int dryrun_opt = 0;
    int a_opt = 0;
    int a_val = -1;
    int conf_file = 0;
    int conf_ind = -1;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--dry-run") == 0)
        {
            if (dryrun_opt)
                errx(1, "--dry-run option specified multiple times.");
            dryrun_opt = 1;
        }
        else if (strcmp(argv[i], "-a") == 0)
        {
            if (a_opt)
                errx(1, "-a option specified multiple times.");
            if (++i >= argc)
                errx(1, "Missing option after -a");

            a_opt = 1;
            a_val = option(argv[i]);
            if (a_val == -1)
                errx(1, "Unrecognized option after -a.");
        }
        else
        {
            if (conf_file)
                errx(1, "Configuration file specified multiple times.");
            conf_ind = i;
            conf_file = 1;
        }
    }

    if (!conf_file)
        errx(1, "Missing configuration file.");

    return start_all(argv, conf_ind, dryrun_opt, a_val);
}
