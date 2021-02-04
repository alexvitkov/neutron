/* C++ code produced by gperf version 3.1 */
/* Command-line: gperf --output-file=/home/alex/src/neutron/keywords.gperf.gen.h /home/alex/src/neutron/keywords.gperf  */
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

#line 1 "/home/alex/src/neutron/keywords.gperf"

#include "common.h"
#line 7 "/home/alex/src/neutron/keywords.gperf"
struct tok { const char* name; TokenType type; };

#define TOTAL_KEYWORDS 40
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 6
#define MIN_HASH_VALUE 2
#define MAX_HASH_VALUE 92
/* maximum key range = 91, duplicates = 0 */

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
      93, 93, 93, 93, 93, 93, 93, 93, 93, 93,
      93, 93, 93, 93, 93, 93, 93, 93, 93, 93,
      93, 93, 93, 93, 93, 93, 93, 93, 93, 93,
      93, 93, 93, 17, 93, 93, 93, 12, 40, 93,
      93, 93,  7, 35, 93, 30, 30, 51, 93, 30,
      93, 15, 93, 93,  0, 93, 55, 93, 93, 93,
      10,  5,  0, 93, 93, 93, 93, 93, 93, 93,
      93, 93, 93, 93, 93, 93, 93, 93, 93, 93,
      93, 93, 93, 93, 93, 93, 93, 93, 93, 93,
      93, 93, 93, 93, 41, 93, 93,  0,  5, 93,
      93,  5, 15, 93,  0, 10, 93, 93,  0, 93,
      40,  0, 93, 93,  0,  0,  0, 35, 93,  0,
      93, 93, 93, 93, 25, 93, 93, 93, 93, 93,
      93, 93, 93, 93, 93, 93, 93, 93, 93, 93,
      93, 93, 93, 93, 93, 93, 93, 93, 93, 93,
      93, 93, 93, 93, 93, 93, 93, 93, 93, 93,
      93, 93, 93, 93, 93, 93, 93, 93, 93, 93,
      93, 93, 93, 93, 93, 93, 93, 93, 93, 93,
      93, 93, 93, 93, 93, 93, 93, 93, 93, 93,
      93, 93, 93, 93, 93, 93, 93, 93, 93, 93,
      93, 93, 93, 93, 93, 93, 93, 93, 93, 93,
      93, 93, 93, 93, 93, 93, 93, 93, 93, 93,
      93, 93, 93, 93, 93, 93, 93, 93, 93, 93,
      93, 93, 93, 93, 93, 93, 93, 93, 93, 93,
      93, 93, 93, 93, 93, 93, 93, 93, 93, 93,
      93, 93, 93, 93, 93, 93
    };
  return len + asso_values[static_cast<unsigned char>(str[1])] + asso_values[static_cast<unsigned char>(str[0])];
}

struct tok *
Perfect_Hash::in_word_set (const char *str, size_t len)
{
  static struct tok wordlist[] =
    {
      {""}, {""},
#line 31 "/home/alex/src/neutron/keywords.gperf"
      {">>",     OP_SHIFTRIGHT},
#line 44 "/home/alex/src/neutron/keywords.gperf"
      {">>=",    OP_SHIFTRIGHTASSIGN},
#line 24 "/home/alex/src/neutron/keywords.gperf"
      {"true",   KW_TRUE},
#line 27 "/home/alex/src/neutron/keywords.gperf"
      {"while",  KW_WHILE},
#line 21 "/home/alex/src/neutron/keywords.gperf"
      {"struct", KW_STRUCT},
#line 33 "/home/alex/src/neutron/keywords.gperf"
      {">=",     OP_GREATEREQUALS},
#line 22 "/home/alex/src/neutron/keywords.gperf"
      {"let",    KW_LET},
#line 17 "/home/alex/src/neutron/keywords.gperf"
      {"bool",   KW_BOOL},
      {""},
#line 23 "/home/alex/src/neutron/keywords.gperf"
      {"return", KW_RETURN},
#line 34 "/home/alex/src/neutron/keywords.gperf"
      {"==",     OP_DOUBLEEQUALS},
#line 16 "/home/alex/src/neutron/keywords.gperf"
      {"i64",    KW_I64},
#line 40 "/home/alex/src/neutron/keywords.gperf"
      {"*=",     OP_MULASSIGN},
      {""}, {""},
#line 32 "/home/alex/src/neutron/keywords.gperf"
      {"<=",     OP_LESSEREQUALS},
#line 19 "/home/alex/src/neutron/keywords.gperf"
      {"f64",    KW_F64},
#line 42 "/home/alex/src/neutron/keywords.gperf"
      {"%=",    OP_MODASSIGN},
#line 25 "/home/alex/src/neutron/keywords.gperf"
      {"false",  KW_FALSE},
      {""},
#line 30 "/home/alex/src/neutron/keywords.gperf"
      {"<<",     OP_SHIFTLEFT},
#line 43 "/home/alex/src/neutron/keywords.gperf"
      {"<<=",    OP_SHIFTLEFTASSIGN},
#line 37 "/home/alex/src/neutron/keywords.gperf"
      {"!=",     OP_NOTEQUALS},
      {""}, {""},
#line 26 "/home/alex/src/neutron/keywords.gperf"
      {"if",     KW_IF},
#line 15 "/home/alex/src/neutron/keywords.gperf"
      {"i32",    KW_I32 },
      {""}, {""}, {""},
#line 47 "/home/alex/src/neutron/keywords.gperf"
      {"|=",     OP_BITORASSIGN},
#line 18 "/home/alex/src/neutron/keywords.gperf"
      {"f32",    KW_F32},
      {""}, {""}, {""},
#line 39 "/home/alex/src/neutron/keywords.gperf"
      {"-=",     OP_SUBASSIGN},
#line 12 "/home/alex/src/neutron/keywords.gperf"
      {"u64",    KW_U64},
      {""}, {""}, {""},
#line 38 "/home/alex/src/neutron/keywords.gperf"
      {"+=",     OP_ADDASSIGN},
#line 14 "/home/alex/src/neutron/keywords.gperf"
      {"i16",    KW_I16 },
      {""}, {""}, {""},
#line 45 "/home/alex/src/neutron/keywords.gperf"
      {"&=",     OP_BITANDASSIGN},
#line 46 "/home/alex/src/neutron/keywords.gperf"
      {"^=",     OP_BITXORASSIGN},
      {""}, {""}, {""},
#line 36 "/home/alex/src/neutron/keywords.gperf"
      {"||",     OP_OR},
#line 11 "/home/alex/src/neutron/keywords.gperf"
      {"u32",    KW_U32 },
      {""}, {""}, {""},
#line 20 "/home/alex/src/neutron/keywords.gperf"
      {"fn",     KW_FN},
#line 41 "/home/alex/src/neutron/keywords.gperf"
      {"/=",     OP_DIVASSIGN},
      {""}, {""}, {""},
#line 29 "/home/alex/src/neutron/keywords.gperf"
      {"--",     OP_MINUSMINUS},
#line 48 "/home/alex/src/neutron/keywords.gperf"
      {"...",    OP_VARARGS},
      {""}, {""}, {""},
#line 13 "/home/alex/src/neutron/keywords.gperf"
      {"i8",     KW_I8},
#line 10 "/home/alex/src/neutron/keywords.gperf"
      {"u16",    KW_U16 },
      {""}, {""}, {""},
#line 28 "/home/alex/src/neutron/keywords.gperf"
      {"++",     OP_PLUSPLUS},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 35 "/home/alex/src/neutron/keywords.gperf"
      {"&&",     OP_AND},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 9 "/home/alex/src/neutron/keywords.gperf"
      {"u8",     KW_U8}
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
