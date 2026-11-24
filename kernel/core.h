#ifndef SIGIL_OS_CORE_H
#define SIGIL_OS_CORE_H

#include <stdint.h>
#include <stddef.h>

struct sigil_list {
    int64_t *items;
    uint64_t len;
    uint64_t cap;
};

struct sigil_string {
    char *data;
    uint64_t len;
    uint64_t cap;
};

struct sigil_list *sigil_list_new(void);
int sigil_list_push(struct sigil_list *list, int64_t val);
int64_t sigil_list_get(const struct sigil_list *list, uint64_t index);
uint64_t sigil_list_len(const struct sigil_list *list);
void sigil_list_free(struct sigil_list *list);

struct sigil_string *sigil_string_new(void);
int sigil_string_append(struct sigil_string *str, const char *text);
const char *sigil_string_cstr(const struct sigil_string *str);
uint64_t sigil_string_len(const struct sigil_string *str);
void sigil_string_free(struct sigil_string *str);

void test_sigil_core_list(void);
void test_sigil_core_string(void);

#endif
