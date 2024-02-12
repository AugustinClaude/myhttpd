#define _POSIX_C_SOURCE 200809L

#include "config.h"

#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

static void init_server_config(struct config *config, size_t i)
{
    config->servers[i].server_name = NULL;
    config->servers[i].port = NULL;
    config->servers[i].ip = NULL;
    config->servers[i].root_dir = NULL;
    config->servers[i].default_file = NULL;
}

static struct config *init_config(void)
{
    struct config *config = malloc(sizeof(struct config));
    config->pid_file = NULL;
    config->log_file = NULL;
    config->log = true;

    config->nb_servers = 0;
    config->servers = malloc(sizeof(struct server_config));
    init_server_config(config, 0);

    return config;
}

static char *copy_str(char *str)
{
    char *res = malloc(sizeof(char) * (strlen(str) + 1));

    size_t i;
    for (i = 0; str[i] != '\0'; i++)
        res[i] = str[i];

    res[i] = '\0';
    return res;
}

static bool config_invalid(struct config *config)
{
    bool res = !config->pid_file;
    for (size_t i = 0; i < config->nb_servers; i++)
    {
        if (res)
            break;

        res |= !config->servers[i].server_name || !config->servers[i].port
            || !config->servers[i].ip || !config->servers[i].root_dir;
    }

    return res;
}

static void compute_tag(struct config *config, char *line, int tag_glob,
                        int tag_vhosts)
{
    if (tag_glob)
    {
        char *value = strchr(line, '=') + 1;
        value[strlen(value) - 1] = '\0';
        if (value[0] == ' ')
            value++;

        if (fnmatch("pid_file*", line, 0) == 0)
            config->pid_file = copy_str(value);
        else if (fnmatch("log_file*", line, 0) == 0)
            config->log_file = copy_str(value);
        else if (fnmatch("log*", line, 0) == 0)
        {
            if (strcmp(value, "") == 0 || strcmp(value, "true") == 0)
                config->log = true;
            else if (strcmp(value, "false") == 0)
                config->log = false;
        }
    }
    else if (tag_vhosts)
    {
        char *value = strchr(line, '=') + 1;
        value[strlen(value) - 1] = '\0';
        if (value[0] == ' ')
            value++;

        size_t i = config->nb_servers - 1;

        if (fnmatch("server_name*", line, 0) == 0)
            config->servers[i].server_name =
                string_create(value, strlen(value));
        else if (fnmatch("port*", line, 0) == 0)
            config->servers[i].port = copy_str(value);
        else if (fnmatch("ip*", line, 0) == 0)
            config->servers[i].ip = copy_str(value);
        else if (fnmatch("root_dir*", line, 0) == 0)
            config->servers[i].root_dir = copy_str(value);
        else if (fnmatch("default_file*", line, 0) == 0)
            config->servers[i].default_file = copy_str(value);
    }
}

struct config *parse_configuration(const char *path)
{
    struct config *config = init_config();
    FILE *file = fopen(path, "r");
    if (!file)
    {
        config_destroy(config);
        return NULL;
    }

    char *line = NULL;
    size_t n = 0;
    ssize_t l = 1;

    int tag_glob = 0;
    int tag_vhosts = 0;
    while (l != -1)
    {
        l = getline(&line, &n, file);
        if (l == -1)
            break;

        // END TAG
        if (strcmp(line, "\n") == 0)
        {
            tag_glob = 0;
            tag_vhosts = 0;
        }

        // COMPUTE TAG
        compute_tag(config, line, tag_glob, tag_vhosts);

        // BEGIN TAG
        if (strcmp(line, "[global]\n") == 0)
            tag_glob = 1;
        if (strcmp(line, "[[vhosts]]\n") == 0)
        {
            tag_vhosts = 1;
            config->nb_servers++;
            config->servers =
                realloc(config->servers,
                        sizeof(struct server_config) * config->nb_servers);
            init_server_config(config, config->nb_servers - 1);
        }
    }

    free(line);

    if (config->nb_servers == 0 || config_invalid(config) || fclose(file) == -1)
    {
        config_destroy(config);
        return NULL;
    }

    return config;
}

void config_destroy(struct config *config)
{
    if (!config)
        return;

    for (size_t i = 0; i < config->nb_servers; i++)
    {
        string_destroy(config->servers[i].server_name);
        free(config->servers[i].port);
        free(config->servers[i].ip);
        free(config->servers[i].root_dir);
        free(config->servers[i].default_file);
    }

    free(config->pid_file);
    free(config->log_file);
    free(config->servers);
    free(config);
}
