#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <string>

#define u8  uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t
#define i8  int8_t
#define i16 int16_t
#define i32 int32_t
#define i64 int64_t

#define PREC(tt) (prec[tt] & PREC_MASK)

#define MUST(v) if (!(v)) return 0;

enum TokenType : u8 {
	TOK_NONE = 0,

    // VOLATILE - if you reorder the operators, you have to change 
    // the precedence table in parser.cpp
    OP_PLUSPLUS = 128,
    OP_MINUSMINUS,
    OP_SHIFTLEFT,
    OP_SHIFTRIGHT,
    OP_LESSEREQUALS,
    OP_GREATEREQUALS,
    OP_DOUBLEEQUALS,
    OP_AND,
    OP_OR,
    OP_NOTEQUALS,
    OP_ADDASSIGN,
    OP_SUBASSIGN,
    OP_MULASSIGN,
    OP_DIVASSIGN,
    OP_MODASSIGN,
    OP_SHIFTLEFTASSIGN,
    OP_SHIFTRIGHTASSIGN,
    OP_BITANDASSIGN,
    OP_BITXORASSIGN,
    OP_BITORASSIGN,

	KW_U8,
	KW_U16,
	KW_U32,
	KW_U64,
	KW_I8,
	KW_I16,
	KW_I32,
	KW_I64,
	KW_BOOL,
	KW_F32,
	KW_F64,
	KW_FN,
	KW_LET,
	KW_STRUCT,
    KW_RETURN,
	KW_TRUE,
	KW_FALSE,
    KW_IF,
    KW_WHILE,

	TOK_ID,
	TOK_NUMBER,
};

struct ASTNode;
struct Error;

enum OpTraits : u8 {
    PREFIX     = 0x10,
    POSTFIX    = 0x20,
    ASSIGNMENT = 0x40,
    PREC_MASK  = 0x0F,
};

extern u8 prec[148];

struct Token {
	TokenType type;

    // A virtual token doesn't exist in the source code
    // It's an additional piece of metadata that the 'unexpected_token' function
    // adds to the error, so when we print it we have information about what token
    // was expected
    u8 virt; 

    i16 file;

    // If the token is a bracket, 'match' is the index of the matching bracket token
	u32 match;

	u32 length;
	u64 start;

    u32 line;
    u32 pos_in_line;

    inline bool operator<(const Token& other) {
        if (line < other.line)
            return true;
        if (line == other.line)
             return this->virt;
        return false;
    }
};


#endif // guard
