// Host tests for the Chinese record of the wordlist (kiss_zhwords.h). The
// property that matters is the one the module exists to keep: a Chinese
// sentence always comes back as the ENGLISH sentence it stands for, so the
// seed made from it is the English seed.
// Build: sim/build_test.sh -> /tmp/kisstest
#include <stdio.h>
#include <string.h>
#include "kiss_lastword.h"
#include "kiss_seed.h"
#include "kiss_zhwords.h"

static int zfails;
static void ok(const char *n, int c)
{
    if (c) printf("PASS: %s\n", n);
    else { printf("FAIL: %s\n", n); zfails++; }
}

#define ZH_DE  "\xE7\x9A\x84"   // index 0, abandon
#define ZH_SHI "\xE6\x98\xAF"   // index 2, able
#define ZH_ZAI "\xE5\x9C\xA8"   // index 3, about
#define ZH_XIE "\xE6\xAD\x87"   // index 2047, zoo
#define IDEO_SP "\xE3\x80\x80"  // U+3000

int test_zhwords(void)
{
    zfails = 0;
    printf("\n-- Chinese wordlist record --\n");

    char zc[4];
    ok("index 0 is the first character",
       kiss_zh_word(0, zc) == 0 && strcmp(zc, ZH_DE) == 0);
    ok("index 2047 is the last character",
       kiss_zh_word(2047, zc) == 0 && strcmp(zc, ZH_XIE) == 0);
    ok("out of range refused",
       kiss_zh_word(-1, zc) != 0 && zc[0] == 0 && kiss_zh_word(2048, zc) != 0);

    // Every character maps back to its own index, and so is unique.
    {
        int bad = 0;
        for (int i = 0; i < KISS_ZH_WORDS; i++) {
            int used = 0;
            if (kiss_zh_word(i, zc) != 0 || kiss_zh_index(zc, &used) != i || used != 3)
                bad++;
        }
        ok("all 2048 characters round trip", bad == 0);
    }
    ok("ASCII is not a character on the list", kiss_zh_index("ab", NULL) < 0);

    // Every reading is lower case a-z and fits the typed field.
    {
        int bad = 0;
        for (int i = 0; i < KISS_ZH_WORDS; i++) {
            const char *p = kiss_zh_pinyin[i];
            int run = 0;
            if (!*p) bad++;
            for (; *p; p++) {
                if (*p == '|') { if (run == 0) bad++; run = 0; continue; }
                if (*p < 'a' || *p > 'z') bad++;
                if (++run >= KISS_ZH_PY_MAX) bad++;
            }
        }
        ok("every reading is a-z and fits KISS_ZH_PY_MAX", bad == 0);
    }

    // ---- the search ----
    uint16_t c[64];
    ok("'de' puts the primary reading first",
       kiss_zh_suggest("de", 0, c, 6) >= 1 && c[0] == 0);
    ok("'shi' starts at index 2",
       kiss_zh_suggest("shi", 0, c, 6) >= 1 && c[0] == 2);
    {
        int all = kiss_zh_suggest("shi", 0, NULL, 0);
        ok("'shi' has many homophones", all >= 20 && all <= 64);
        int got = kiss_zh_suggest("shi", 0, c, 64);
        int dup = 0, order = 1;
        for (int i = 0; i < got; i++)
            for (int j = i + 1; j < got; j++)
                if (c[i] == c[j]) dup++;
        // pages are slices of the one ranked list
        uint16_t pg[6];
        int pn = kiss_zh_suggest("shi", 6, pg, 6);
        for (int i = 0; i < pn; i++)
            if (pg[i] != c[6 + i]) order = 0;
        ok("'shi': count matches, no repeats, pages slice the list",
           got == all && dup == 0 && order && pn == 6);
    }
    {
        // a prefix reaches every exact match and more
        int exact = kiss_zh_suggest("zhi", 0, NULL, 0);
        int pref  = kiss_zh_suggest("zh", 0, NULL, 0);
        ok("'zh' finds more than 'zhi'", exact > 0 && pref > exact);
    }
    ok("no reading starts with 'xq'", kiss_zh_suggest("xq", 0, c, 6) == 0);
    ok("upper case, digits, empty and overlong refused",
       kiss_zh_suggest("SHI", 0, c, 6) == 0 && kiss_zh_suggest("sh1", 0, c, 6) == 0 &&
       kiss_zh_suggest("", 0, c, 6) == 0 &&
       kiss_zh_suggest("zhuangz", 0, c, 6) == 0);

    char py[KISS_ZH_PY_MAX];
    ok("primary reading of index 2 is shi",
       kiss_zh_pinyin_primary(2, py, sizeof py) == 0 && strcmp(py, "shi") == 0);

    // ---- sentences ----
    static const char *MN =
        "abandon abandon abandon abandon abandon abandon "
        "abandon abandon abandon abandon abandon about";
    char en[WSEED_MAX_MNEMONIC], zh[WSEED_MAX_MNEMONIC];
    ok("run-together Chinese -> English",
       kiss_zh_to_english(ZH_DE ZH_DE ZH_DE ZH_DE ZH_DE ZH_DE
                          ZH_DE ZH_DE ZH_DE ZH_DE ZH_DE ZH_ZAI,
                          en, sizeof en) == 0 && strcmp(en, MN) == 0);
    ok("spaced Chinese (ASCII and ideographic) -> English",
       kiss_zh_to_english(" " ZH_DE " " ZH_DE IDEO_SP ZH_DE "\n" ZH_DE " " ZH_DE
                          " " ZH_DE " " ZH_DE " " ZH_DE " " ZH_DE " " ZH_DE
                          " " ZH_DE " " ZH_ZAI "\r\n", en, sizeof en) == 0 &&
       strcmp(en, MN) == 0);
    ok("eleven characters refused, out cleared",
       kiss_zh_to_english(ZH_DE ZH_DE ZH_DE ZH_DE ZH_DE ZH_DE
                          ZH_DE ZH_DE ZH_DE ZH_DE ZH_ZAI, en, sizeof en) != 0 &&
       en[0] == 0);
    ok("a letter in the sentence refused",
       kiss_zh_to_english(ZH_DE ZH_DE ZH_DE ZH_DE ZH_DE "a" ZH_DE
                          ZH_DE ZH_DE ZH_DE ZH_DE ZH_DE ZH_ZAI, en, sizeof en) != 0);
    ok("English -> Chinese record",
       kiss_zh_from_english(MN, zh, sizeof zh) == 0 &&
       strncmp(zh, ZH_DE " " ZH_DE " ", 8) == 0 &&
       strcmp(zh + strlen(zh) - 3, ZH_ZAI) == 0);
    ok("a non list English word refused",
       kiss_zh_from_english("abandon bitcoin", zh, sizeof zh) != 0 && zh[0] == 0);

    // A real, non degenerate seed, built from entropy the way test_fuzz does.
    uint8_t ent[32];
    char good[WSEED_MAX_MNEMONIC];
    for (size_t i = 0; i < sizeof ent; i++) ent[i] = (uint8_t)(i * 37 + 11);
    ok("built a 24 word seed",
       kiss_seed_from_entropy(ent, sizeof ent, good, sizeof good) == 0);
    ok("24 words round trip through Chinese",
       kiss_zh_from_english(good, zh, sizeof zh) == 0 &&
       kiss_zh_to_english(zh, en, sizeof en) == 0 && strcmp(en, good) == 0);

    // ---- import: a sealed Chinese sentence opens the ENGLISH wallet ----
    char so[WSEED_MAX_MNEMONIC];
    ok("plaintext: Chinese sentence imported as its English",
       kiss_seed_from_plaintext(zh, strlen(zh), so, sizeof so) == 0 &&
       strcmp(so, good) == 0);
    {
        // one character swapped: the checksum still decides
        char bad[WSEED_MAX_MNEMONIC];
        snprintf(bad, sizeof bad, "%s", zh);
        int i0 = kiss_zh_index(bad, NULL);
        kiss_zh_word(i0 == 0 ? 1 : 0, zc);
        memcpy(bad, zc, 3);
        ok("plaintext: a Chinese sentence with a bad checksum refused",
           kiss_seed_from_plaintext(bad, strlen(bad), so, sizeof so) != 0 &&
           so[0] == 0);
    }

    return zfails;
}
