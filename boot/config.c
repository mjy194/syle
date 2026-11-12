#include "../include/syle/boot_platform.h"

static void copy(char *to, const char *from, size_t size) {
    for (size_t i = 0; i < size; ++i) to[i] = from[i];
    to[size] = 0;
}
static int equal(const char *p, size_t n, const char *s) {
    size_t i = 0;
    while (s[i]) {
        if (i == n || p[i] != s[i]) return 0;
        ++i;
    }
    return i == n;
}
static int space(char c) { return c == ' ' || c == '\t'; }
void syle_boot_defaults(struct syle_boot_config *out) {
    for (size_t i = 0; i < sizeof *out; ++i) ((unsigned char *)out)[i] = 0;
    out->mode = SYLE_BOOT_NORMAL;
    out->timeout_seconds = 3;
    copy(out->title, "Syle", 4);
}
int syle_boot_parse(const char *text, size_t size, struct syle_boot_config *out) {
    if (!out || (!text && size) || size > SYLE_BOOT_CONFIG_LIMIT) return -1;
    struct syle_boot_config parsed;
    syle_boot_defaults(&parsed);
    unsigned seen = 0;
    for (size_t at = 0; at < size;) {
        size_t start = at;
        while (at < size && text[at] != '\n') {
            unsigned char c = (unsigned char)text[at];
            if ((c < 32 && c != '\t' && c != '\r') || c > 126) return -1;
            ++at;
        }
        size_t end = at;
        if (at < size) ++at;
        if (end > start && text[end - 1] == '\r') --end;
        while (start < end && space(text[start])) ++start;
        while (end > start && space(text[end - 1])) --end;
        if (start == end || text[start] == '#') continue;
        size_t split = start;
        while (split < end && text[split] != '=') ++split;
        if (split == end) return -1;
        size_t key_end = split, val = split + 1;
        while (key_end > start && space(text[key_end - 1])) --key_end;
        while (val < end && space(text[val])) ++val;
        unsigned bit;
        if (equal(text + start, key_end - start, "default")) {
            bit = 1;
            if (equal(text + val, end - val, "normal")) parsed.mode = SYLE_BOOT_NORMAL;
            else if (equal(text + val, end - val, "diagnostic")) parsed.mode = SYLE_BOOT_DIAGNOSTIC;
            else return -1;
        } else if (equal(text + start, key_end - start, "timeout")) {
            bit = 2;
            if (val == end) return -1;
            unsigned n = 0;
            for (size_t i = val; i < end; ++i) {
                if (text[i] < '0' || text[i] > '9' || n > 30) return -1;
                n = n * 10 + (unsigned)(text[i] - '0');
            }
            if (n > 30) return -1;
            parsed.timeout_seconds = n;
        } else if (equal(text + start, key_end - start, "title")) {
            bit = 4;
            if (end == val || end - val > SYLE_BOOT_TITLE_LIMIT) return -1;
            for (size_t i = val; i < end; ++i)
                if ((unsigned char)text[i] < 32 || (unsigned char)text[i] > 126) return -1;
            copy(parsed.title, text + val, end - val);
        } else return -1;
        if (seen & bit) return -1;
        seen |= bit;
    }
    *out = parsed;
    return 0;
}
