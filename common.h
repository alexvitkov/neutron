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

#define MUST(v)             { if (!(v))                     return 0;       }
#define MUST_OR_FAIL_JOB(v) { if (!(v)) { set_error_flag(); return false; } }

#define IS_TYPE_KW(t) ((t) >= KW_U8 && (t) <= KW_F64)

// This can be extended to u16 if needed,
// right now there's unused space in the Token struct
enum TokenType : u8 {
	TOK_NONE = 0,

    OP_ADD = '+',
    OP_SUB = '-',
    OP_MUL = '*',
    OP_DIV = '/',
    OP_MOD = '%',

    OP_GREATERTHAN = '>',
    OP_LESSTHAN = '<',

    TOK_COLON = ':',
    TOK_SEMICOLON = ';',

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
	KW_MACRO,
	KW_EMIT,
	KW_UNQUOTE,
	KW_STRUCT,
    KW_RETURN,
	KW_TRUE,
	KW_FALSE,
    KW_IF,
    KW_WHILE,
    KW_FOR,
    KW_TYPEOF,

	TOK_ERROR,

	TOK_ID,
	TOK_NUMBER,
    TOK_STRING_LITERAL,
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
    u32 file_id;
    LocationInFile loc;
};


struct NumberData;

struct SmallToken {
    enum TokenType type;
    
    union {
        const char* name;
        NumberData* number_data;
    };
};

struct Token : SmallToken {
    u32 file_id;

    // The first token in the file has ID 0, second has ID 1, etc...
    u64 id;

    LocationInFile loc;
};


#define AST_TYPE_BIT  0x80
#define AST_VALUE_BIT 0x40

enum AST_NodeType : u8 {
	AST_NONE = 0,

    AST_RETURN        = 0x01,
    AST_IF            = 0x02,
    AST_WHILE         = 0x03,
    AST_BLOCK         = 0x04,
    AST_EMIT          = 0x05,

    AST_FN            = 0x00 | AST_VALUE_BIT,
    AST_VAR           = 0x01 | AST_VALUE_BIT,
    AST_ASSIGNMENT    = 0x02 | AST_VALUE_BIT,
    AST_NUMBER        = 0x03 | AST_VALUE_BIT,
    AST_FN_CALL       = 0x04 | AST_VALUE_BIT,
    AST_MEMBER_ACCESS = 0x05 | AST_VALUE_BIT,
    AST_TEMP_REF      = 0x06 | AST_VALUE_BIT,
    AST_UNRESOLVED_ID = 0x07 | AST_VALUE_BIT,
    AST_DEREFERENCE   = 0x08 | AST_VALUE_BIT,
    AST_ADDRESS_OF    = 0x09 | AST_VALUE_BIT,
    AST_STRING_LITERAL= 0x0a | AST_VALUE_BIT,
    AST_TYPEOF        = 0x0b | AST_VALUE_BIT,               
    AST_SMALL_NUMBER  = 0x0c | AST_VALUE_BIT,
    AST_MACRO         = 0x0d | AST_VALUE_BIT,
    AST_UNQUOTE       = 0x0e | AST_VALUE_BIT,
    AST_ZEROEXTEND    = 0x0f | AST_VALUE_BIT,

    AST_STRUCT         = 0x00 | AST_VALUE_BIT | AST_TYPE_BIT,
	AST_PRIMITIVE_TYPE = 0x01 | AST_VALUE_BIT | AST_TYPE_BIT,
    AST_FN_TYPE        = 0x02 | AST_VALUE_BIT | AST_TYPE_BIT,
    AST_POINTER_TYPE   = 0x03 | AST_VALUE_BIT | AST_TYPE_BIT,
    AST_ARRAY_TYPE     = 0x04 | AST_VALUE_BIT | AST_TYPE_BIT,
};

struct AST_Node {
	AST_NodeType nodetype;
    inline AST_Node(AST_NodeType nt) : nodetype(nt) {}
};

#endif // guard
