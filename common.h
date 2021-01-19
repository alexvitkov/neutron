#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
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

#define IS_TYPE_KW(t) ((t) >= KW_U8 && (t) <= KW_F64)

// This can be extended to u16 if needed,
// right now there's unused space in the Token struct
enum TokenType : u8 {
	TOK_NONE = 0,

    TOK_ADD = '+',
    TOK_SUBTRACT = '-',

    TOK_AMPERSAND = '&',
    TOK_OPENSQUARE = '[',
    TOK_CLOSESQUARE = ']',
    TOK_OPENCURLY = '{',
    TOK_CLOSECURLY = '}',
    TOK_OPENBRACKET = '(',
    TOK_CLOSEBRACKET = ')',
    TOK_DOT = '.',

    // VOLATILE
    // If you reorder the operators, you have to change 
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
    OP_VARARGS,


    // VOLATILE 
    // IS_TYPE_KW checks if a keyword is a type by doing t >= KW_U8 && t <= KW_F64
    // If you add a new type keyword update IS_TYPE_KW accordingly
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



	TOK_ERROR,

	TOK_ID,
	TOK_NUMBER,
    TOK_STRING_LITERAL,

    // Those get generated by the typer
    OP_ADD_PTR_INT,
    OP_SUB_PTR_INT,
    OP_SUB_PTR_PTR,

};

struct AST_Node;
struct Error;

enum OpTraits : u8 {
    PREFIX     = 0x10,
    POSTFIX    = 0x20,
    ASSIGNMENT = 0x40,
    PREC_MASK  = 0x0F,
};

extern u8 prec[148];


// Each token and AST Node has one of these associated with them
// It's not stored inside the token/node itself, 
// because we only need it when handling errors
struct LocationInFile {
    u64 start;
    u64 end;
};

struct Location {
    u64 file_id;
    LocationInFile loc;
};

struct NumberData {
    bool is_float;
    union {
        u64 u64_data;
        double f64_data;
    };
};

struct SmallToken {
    TokenType type;
    
    union {
        const char* name;
        NumberData* number_data;
    };
};

struct Token : SmallToken {
    u64 file_id;

    // The first token in the file has ID 0, second has ID 1, etc...
    u64 id;

    LocationInFile loc;
};

#endif // guard
