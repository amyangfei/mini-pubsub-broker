#include <string.h>
#include <datrie/trie.h>

#include <locale.h>
#include "trie_util.h"

static AlphaMap *en_alpha_map_new()
{
    AlphaMap *en_map;

    en_map = alpha_map_new();
    if (!en_map) {
        return NULL;
    }

    /* a-z */
    if (alpha_map_add_range (en_map, 0x0061, 0x007a) != 0) {
        alpha_map_free (en_map);
        return NULL;
    }
    /* A-Z */
    if (alpha_map_add_range (en_map, 0x0041, 0x005a) != 0) {
        alpha_map_free (en_map);
        return NULL;
    }

    return en_map;
}

Trie *trie_create()
{
    AlphaMap *en_map;
    Trie     *en_trie;

    en_map = en_alpha_map_new();
    if (!en_map) {
        return NULL;
    }

    en_trie = trie_new(en_map);

    alpha_map_free(en_map);
    return en_trie ? en_trie : NULL;

}

void init_conv(iconv_t *to_code)
{
    const char *prev_locale;
    const char *locale_codeset;

    prev_locale = setlocale (LC_CTYPE, "");
    locale_codeset = locale_charset();
    setlocale (LC_CTYPE, prev_locale);

    *to_code = iconv_open (ALPHA_ENC, locale_codeset);
}

size_t conv_to_alpha (iconv_t to_code, const char *in, AlphaChar *out,
        size_t out_size)
{
    char   *in_p = (char *) in;
    char   *out_p = (char *) out;
    size_t  in_left = strlen (in);
    size_t  out_left = out_size * sizeof (AlphaChar);
    size_t  res;
    const unsigned char *byte_p;

    /* convert to UCS-4LE */
    res = iconv (to_code, (char **) &in_p, &in_left, &out_p, &out_left);

    /* convert UCS-4LE to AlphaChar string */
    res = 0;
    for (byte_p = (const unsigned char *) out;
        res < out_size && byte_p + 3 < (unsigned char*) out_p;
        byte_p += 4)
    {
        out[res++] = byte_p[0]
                    | (byte_p[1] << 8)
                    | (byte_p[2] << 16)
                    | (byte_p[3] << 24);
    }
    if (res < out_size) {
        out[res] = 0;
    }

    return res;
}

int trie_walker(TrieState *s, AlphaChar *alpha, int len, int cur)
{
    int i;
    for (i = cur; i < len; i++) {
        if (trie_state_is_walkable(s, alpha[i])) {
            trie_state_walk(s, alpha[i]);
            if (trie_state_is_terminal(s)) {
                return i;
            }
        } else {
            return -1;
        }
    }
    return -1;
}
