#include "core.h"
#include "alloc.h"
#include "serial.h"

static void mem_copy(void *dest, const void *src, uint64_t count)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (uint64_t i = 0; i < count; ++i)
        d[i] = s[i];
}

static uint64_t str_len(const char *s)
{
    uint64_t len = 0;
    while (s[len])
        len++;
    return len;
}

static int str_equal(const char *a, const char *b)
{
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return (*a == *b);
}

/* --- Dynamic List --- */
struct sigil_list *sigil_list_new(void)
{
    struct sigil_list *list = (struct sigil_list *)sigil_alloc(sizeof(struct sigil_list), 8);
    if (!list)
        return 0;
    list->cap = 4;
    list->len = 0;
    list->items = (int64_t *)sigil_alloc(list->cap * sizeof(int64_t), 8);
    if (!list->items) {
        sigil_free(list, sizeof(struct sigil_list), 8);
        return 0;
    }
    return list;
}

int sigil_list_push(struct sigil_list *list, int64_t val)
{
    if (!list)
        return 0;
    if (list->len >= list->cap) {
        uint64_t new_cap = list->cap * 2;
        int64_t *new_items = (int64_t *)sigil_alloc(new_cap * sizeof(int64_t), 8);
        if (!new_items)
            return 0;
        mem_copy(new_items, list->items, list->len * sizeof(int64_t));
        sigil_free(list->items, list->cap * sizeof(int64_t), 8);
        list->items = new_items;
        list->cap = new_cap;
    }
    list->items[list->len++] = val;
    return 1;
}

int64_t sigil_list_get(const struct sigil_list *list, uint64_t index)
{
    if (!list || index >= list->len)
        boot_stop("STOP list index out of bounds\r\n");
    return list->items[index];
}

uint64_t sigil_list_len(const struct sigil_list *list)
{
    return list ? list->len : 0;
}

void sigil_list_free(struct sigil_list *list)
{
    if (!list)
        return;
    if (list->items) {
        sigil_free(list->items, list->cap * sizeof(int64_t), 8);
    }
    sigil_free(list, sizeof(struct sigil_list), 8);
}

/* --- Dynamic String --- */
struct sigil_string *sigil_string_new(void)
{
    struct sigil_string *str = (struct sigil_string *)sigil_alloc(sizeof(struct sigil_string), 8);
    if (!str)
        return 0;
    str->cap = 16;
    str->len = 0;
    str->data = (char *)sigil_alloc(str->cap, 8);
    if (!str->data) {
        sigil_free(str, sizeof(struct sigil_string), 8);
        return 0;
    }
    str->data[0] = '\0';
    return str;
}

int sigil_string_append(struct sigil_string *str, const char *text)
{
    if (!str || !text)
        return 0;
    uint64_t tlen = str_len(text);
    if (str->len + tlen + 1 > str->cap) {
        uint64_t new_cap = (str->len + tlen + 1) * 2;
        char *new_data = (char *)sigil_alloc(new_cap, 8);
        if (!new_data)
            return 0;
        mem_copy(new_data, str->data, str->len);
        sigil_free(str->data, str->cap, 8);
        str->data = new_data;
        str->cap = new_cap;
    }
    mem_copy(str->data + str->len, text, tlen);
    str->len += tlen;
    str->data[str->len] = '\0';
    return 1;
}

const char *sigil_string_cstr(const struct sigil_string *str)
{
    return str ? str->data : "";
}

uint64_t sigil_string_len(const struct sigil_string *str)
{
    return str ? str->len : 0;
}

void sigil_string_free(struct sigil_string *str)
{
    if (!str)
        return;
    if (str->data) {
        sigil_free(str->data, str->cap, 8);
    }
    sigil_free(str, sizeof(struct sigil_string), 8);
}

/* --- Tests --- */
void test_sigil_core_list(void)
{
    uint64_t initial_blocks = alloc_live_blocks();

    struct sigil_list *list = sigil_list_new();
    if (!list)
        boot_stop("STOP list_new failed\r\n");

    /* Push 64 elements, triggering capacity doubling (4 -> 8 -> 16 -> 32 -> 64) */
    for (int64_t i = 0; i < 64; ++i) {
        if (!sigil_list_push(list, i * 10))
            boot_stop("STOP list_push failed\r\n");
    }

    if (sigil_list_len(list) != 64)
        boot_stop("STOP list len mismatch\r\n");

    for (int64_t i = 0; i < 64; ++i) {
        if (sigil_list_get(list, (uint64_t)i) != i * 10)
            boot_stop("STOP list element mismatch\r\n");
    }

    sigil_list_free(list);

    if (alloc_live_blocks() != initial_blocks)
        boot_stop("STOP list memory leak\r\n");

    serial_write("allocator core list test=OK\r\n");
}

void test_sigil_core_string(void)
{
    uint64_t initial_blocks = alloc_live_blocks();

    struct sigil_string *str = sigil_string_new();
    if (!str)
        boot_stop("STOP string_new failed\r\n");

    sigil_string_append(str, "Sigil ");
    sigil_string_append(str, "P6-2 ");
    sigil_string_append(str, "Kernel");

    if (sigil_string_len(str) != 17)
        boot_stop("STOP string len mismatch\r\n");

    if (!str_equal(sigil_string_cstr(str), "Sigil P6-2 Kernel"))
        boot_stop("STOP string content mismatch\r\n");

    sigil_string_free(str);

    if (alloc_live_blocks() != initial_blocks)
        boot_stop("STOP string memory leak\r\n");

    serial_write("allocator core string test=OK\r\n");
}
