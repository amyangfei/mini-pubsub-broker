#ifndef __TRIE_UTIL_H
#define __TRIE_UTIL_H

#include <datrie/trie.h>
#include <iconv.h>

#define TRIE_DATA_DFLT  1

typedef struct _DictRec DictRec;
struct _DictRec {
    AlphaChar *key;
    TrieData  data;
};

/* iconv encoding name for AlphaChar string */
#define ALPHA_ENC   "UCS-4LE"

#define HAVE_LANGINFO_CODESET 1
#if defined(HAVE_LOCALE_CHARSET)
# include <localcharset.h>
#elif defined (HAVE_LANGINFO_CODESET)
# include <langinfo.h>
# define locale_charset()  nl_langinfo(CODESET)
#endif

Trie *trie_create();
void init_conv(iconv_t *to_code);
size_t conv_to_alpha(iconv_t to_code, const char *in, AlphaChar *out,
        size_t out_size);
int trie_walker(TrieState *s, AlphaChar *alpha, int len, int cur);

#endif
