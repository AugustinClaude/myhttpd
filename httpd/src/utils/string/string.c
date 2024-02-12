#include "string.h"

#include <stdio.h>
#include <stdlib.h>

struct string *string_create(const char *str, size_t size)
{
    struct string *string = malloc(sizeof(struct string));
    string->size = size;
    if (size == 0)
    {
        string->data = NULL;
        return string;
    }

    string->data = malloc(sizeof(char) * size);
    for (size_t i = 0; i < size; i++)
        string->data[i] = str[i];

    return string;
}

int string_compare_n_str(const struct string *str1, const char *str2, size_t n)
{
    if (n == 0)
        return 0;

    size_t i = 0;
    while (i < n)
    {
        if (str1->data[i] != str2[i])
            return str1->data[i] - str2[i];
        if (i >= str1->size)
            return 0;

        i++;
    }

    return 0;
}

void string_concat_str(struct string *str, const char *to_concat, size_t size)
{
    str->data = realloc(str->data, str->size + size);
    for (size_t i = 0; i < size; i++)
        str->data[i + str->size] = to_concat[i];

    str->size += size;
}

char *get_str(struct string *str)
{
    char *s = malloc(str->size + 1);

    for (size_t i = 0; i < str->size; i++)
        s[i] = str->data[i];

    s[str->size] = '\0';
    return s;
}

void string_print(struct string *str)
{
    if (!str)
        return;

    for (size_t i = 0; i < str->size; i++)
    {
        if (str->data[i] == '\n')
            printf("\\n");
        else if (str->data[i] == '\r')
            printf("\\r");
        else
            printf("%c", str->data[i]);
    }
}

void string_destroy(struct string *str)
{
    if (!str)
        return;

    free(str->data);
    free(str);
}
