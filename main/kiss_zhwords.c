// See kiss_zhwords.h. Index arithmetic and string copying, nothing else: the
// Chinese never reaches a hash, and the English it turns into is judged by
// kiss_seed_validate like any other typed sentence.
#include "kiss_zhwords.h"

#include <string.h>

#include "kiss_lastword.h"   // English word <-> index, both directions
#include "kiss_wipe.h"

int kiss_zh_word(int index, char out[4])
{
    if (!out) return -1;
    out[0] = 0;
    if (index < 0 || index >= KISS_ZH_WORDS) return -1;
    memcpy(out, &kiss_zh_chars[index * 3], 3);
    out[3] = 0;
    return 0;
}

int kiss_zh_index(const char *s, int *used)
{
    if (used) *used = 0;
    // Every character on the list is three bytes of UTF-8 (the generator
    // refuses anything else), so a lead byte outside E0..EF cannot be one.
    if (!s || ((unsigned char)s[0] & 0xF0) != 0xE0 || !s[1] || !s[2])
        return -1;
    for (int i = 0; i < KISS_ZH_WORDS; i++) {
        if (memcmp(&kiss_zh_chars[i * 3], s, 3) == 0) {
            if (used) *used = 3;
            return i;
        }
    }
    return -1;
}

int kiss_zh_pinyin_primary(int index, char *out, size_t out_len)
{
    if (!out || out_len == 0) return -1;
    out[0] = 0;
    if (index < 0 || index >= KISS_ZH_WORDS) return -1;
    const char *p = kiss_zh_pinyin[index];
    size_t n = 0;
    while (p[n] && p[n] != '|') n++;
    if (n + 1 > out_len) return -1;
    memcpy(out, p, n);
    out[n] = 0;
    return 0;
}

// The best of the four ranks in kiss_zh_suggest that `py` earns for typed[0..tl),
// or -1 when no reading matches at all.
static int py_rank(const char *py, const char *typed, size_t tl)
{
    int best = -1;
    for (int r = 0; *py; r++) {
        size_t n = 0;
        while (py[n] && py[n] != '|') n++;
        int rank = -1;
        if (n == tl && memcmp(py, typed, tl) == 0)
            rank = r == 0 ? 0 : 1;
        else if (n > tl && memcmp(py, typed, tl) == 0)
            rank = r == 0 ? 2 : 3;
        if (rank >= 0 && (best < 0 || rank < best)) best = rank;
        py += n;
        if (*py == '|') py++;
    }
    return best;
}

int kiss_zh_suggest(const char *typed, int skip, uint16_t *out, int n)
{
    if (!typed) return 0;
    size_t tl = strlen(typed);
    if (tl == 0 || tl >= KISS_ZH_PY_MAX) return 0;
    for (size_t i = 0; i < tl; i++)
        if (typed[i] < 'a' || typed[i] > 'z') return 0;
    if (skip < 0) skip = 0;

    int seen = 0, wrote = 0;
    for (int rank = 0; rank < 4; rank++) {
        for (int i = 0; i < KISS_ZH_WORDS; i++) {
            if (py_rank(kiss_zh_pinyin[i], typed, tl) != rank) continue;
            if (n == 0) { seen++; continue; }      // counting only
            if (seen++ < skip) continue;
            if (!out || wrote >= n) return wrote;
            out[wrote++] = (uint16_t)i;
        }
    }
    return n == 0 ? seen : wrote;
}

int kiss_zh_looks_chinese(const char *s, size_t len)
{
    if (!s) return 0;
    for (size_t i = 0; i < len; i++)
        if ((unsigned char)s[i] >= 0x80) return 1;
    return 0;
}

static int count_ok(int words)
{
    return words == 12 || words == 15 || words == 18 || words == 21 || words == 24;
}

int kiss_zh_to_english(const char *zh, char *out, size_t out_len)
{
    if (!out || out_len == 0) return -1;
    out[0] = 0;
    if (!zh) return -1;

    size_t at = 0;
    int words = 0;
    const char *p = zh;
    while (*p) {
        // ASCII whitespace, and the ideographic space a Chinese keyboard puts
        // between characters. Nothing else is skipped: a stray letter or
        // punctuation mark is a sentence this module cannot vouch for.
        if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') { p++; continue; }
        if ((unsigned char)p[0] == 0xE3 && (unsigned char)p[1] == 0x80 &&
            (unsigned char)p[2] == 0x80) { p += 3; continue; }

        int used = 0;
        int idx = kiss_zh_index(p, &used);
        const char *w = idx >= 0 ? kiss_lastword_word((uint16_t)idx) : NULL;
        if (!w || words >= 24) goto fail;
        size_t wl = strlen(w);
        if (at + (words ? 1 : 0) + wl + 1 > out_len) goto fail;
        if (words) out[at++] = ' ';
        memcpy(out + at, w, wl);
        at += wl;
        out[at] = 0;
        words++;
        p += used;
    }
    if (!count_ok(words)) goto fail;
    return 0;
fail:
    kiss_wipe(out, out_len);
    return -1;
}

int kiss_zh_from_english(const char *en, char *out, size_t out_len)
{
    if (!out || out_len == 0) return -1;
    out[0] = 0;
    if (!en) return -1;

    size_t at = 0;
    int words = 0;
    const char *p = en;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        char w[12];
        size_t wl = 0;
        while (p[wl] && p[wl] != ' ') wl++;
        if (wl >= sizeof w) goto fail;
        memcpy(w, p, wl);
        w[wl] = 0;
        p += wl;

        char zc[4];
        int idx = kiss_lastword_index(w);
        kiss_wipe(w, sizeof w);
        if (idx < 0 || kiss_zh_word(idx, zc) != 0) goto fail;
        if (at + (words ? 1 : 0) + 3 + 1 > out_len) goto fail;
        if (words) out[at++] = ' ';
        memcpy(out + at, zc, 3);
        at += 3;
        out[at] = 0;
        words++;
    }
    if (words == 0) goto fail;
    return 0;
fail:
    kiss_wipe(out, out_len);
    return -1;
}
