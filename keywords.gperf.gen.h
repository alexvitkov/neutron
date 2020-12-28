/* C++ code produced by gperf version 3.1 */
/* Command-line: gperf --output-file=keywords.gperf.gen.h keywords.gperf  */
/* Computed positions: -k'1-2' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gperf@gnu.org>."
#endif

#line 1 "keywords.gperf"

#include "common.h"
#line 7 "keywords.gperf"
struct tok { const char* name; TokenType type; };

#define TOTAL_KEYWORDS 36
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 6
#define MIN_HASH_VALUE 2
#define MAX_HASH_VALUE 82
/* maximum key range = 81, duplicates = 0 */

class Perfect_Hash
{
private:
  static inline unsigned int hash (const char *str, size_t len);
public:
  static struct tok *in_word_set (const char *str, size_t len);
};

inline unsigned int
Perfect_Hash::hash (const char *str, size_t len)
{
  static unsigned char asso_values[] =
    {
      83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
      83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
      83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
      83, 83, 83, 12, 83, 83, 83, 35, 40, 83,
      83, 83,  7, 30, 83, 25, 83, 61, 83, 50,
      83, 25, 83, 83,  5, 83, 41, 83, 83, 83,
      10,  5,  0, 83, 83, 83, 83, 83, 83, 83,
      83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
      83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
      83, 83, 35, 83, 60, 83, 83,  0,  5, 83,
      83, 30, 10, 83, 83,  5, 83, 83, 30, 83,
      45,  0, 83, 83,  0,  0,  0,  0, 83, 83,
      83, 83, 83, 83, 20, 83, 83, 83, 83, 83,
      83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
      83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
      83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
      83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
      83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
      83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
      83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
      83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
      83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
      83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
      83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
      83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
      83, 83, 83, 83, 83, 83
    };
  return len + asso_values[static_cast<unsigned char>(str[1])] + asso_values[static_cast<unsigned char>(str[0])];
}

struct tok *
Perfect_Hash::in_word_set (const char *str, size_t len)
{
  static struct tok wordlist[] =
    {
      {""}, {""},
#line 28 "keywords.gperf"
      {">>",     OP_SHIFTRIGHT},
#line 41 "keywords.gperf"
      {">>=",    OP_SHIFTRIGHTASSIGN},
#line 23 "keywords.gperf"
      {"true",   KW_TRUE},
      {""},
#line 21 "keywords.gperf"
      {"struct", KW_STRUCT},
#line 30 "keywords.gperf"
      {">=",     OP_GREATEREQUALS},
#line 12 "keywords.gperf"
      {"u64",    KW_U64},
#line 17 "keywords.gperf"
      {"bool",   KW_BOOL},
      {""}, {""},
#line 31 "keywords.gperf"
      {"==",     OP_DOUBLEEQUALS},
#line 16 "keywords.gperf"
      {"i64",    KW_I64},
#line 37 "keywords.gperf"
      {"*=",     OP_MULASSIGN},
#line 24 "keywords.gperf"
      {"false",  KW_FALSE},
      {""},
#line 29 "keywords.gperf"
      {"<=",     OP_LESSEREQUALS},
#line 19 "keywords.gperf"
      {"f64",    KW_F64},
#line 34 "keywords.gperf"
      {"!=",     OP_NOTEQUALS},
      {""}, {""},
#line 27 "keywords.gperf"
      {"<<",     OP_SHIFTLEFT},
#line 40 "keywords.gperf"
      {"<<=",    OP_SHIFTLEFTASSIGN},
      {""}, {""}, {""},
#line 44 "keywords.gperf"
      {"|=",     OP_BITORASSIGN},
#line 11 "keywords.gperf"
      {"u32",    KW_U32 },
      {""}, {""}, {""},
#line 36 "keywords.gperf"
      {"-=",     OP_SUBASSIGN},
#line 15 "keywords.gperf"
      {"i32",    KW_I32 },
      {""}, {""}, {""},
#line 35 "keywords.gperf"
      {"+=",     OP_ADDASSIGN},
#line 18 "keywords.gperf"
      {"f32",    KW_F32},
      {""}, {""}, {""},
#line 33 "keywords.gperf"
      {"||",     OP_OR},
#line 9 "keywords.gperf"
      {"u8",     KW_U8},
      {""}, {""}, {""},
#line 42 "keywords.gperf"
      {"&=",     OP_BITANDASSIGN},
#line 13 "keywords.gperf"
      {"i8",     KW_I8},
      {""}, {""}, {""},
#line 26 "keywords.gperf"
      {"--",     OP_MINUSMINUS},
#line 10 "keywords.gperf"
      {"u16",    KW_U16 },
      {""}, {""}, {""},
#line 20 "keywords.gperf"
      {"fn",     KW_FN},
#line 14 "keywords.gperf"
      {"i16",    KW_I16 },
      {""}, {""}, {""},
#line 25 "keywords.gperf"
      {"++",     OP_PLUSPLUS},
#line 22 "keywords.gperf"
      {"let",    KW_LET},
      {""}, {""}, {""},
#line 43 "keywords.gperf"
      {"^=",     OP_BITXORASSIGN},
#line 38 "keywords.gperf"
      {"/=",     OP_DIVASSIGN},
      {""}, {""}, {""}, {""},
#line 39 "keywords.gperf"
      {"\\%=",    OP_MODASSIGN},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 32 "keywords.gperf"
      {"&&",     OP_AND}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      unsigned int key = hash (str, len);

      if (key <= MAX_HASH_VALUE)
        {
          const char *s = wordlist[key].name;

          if (*str == *s && !strncmp (str + 1, s + 1, len - 1) && s[len] == '\0')
            return &wordlist[key];
        }
    }
  return 0;
}
