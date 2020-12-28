#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <unordered_map>

#define u8  uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t
#define i8  int8_t
#define i16 int16_t
#define i32 int32_t
#define i64 int64_t

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

	TOK_ID  = 128,
	TOK_NUM,
};

struct Error {
	enum {
		TOKENIZER,
		PARSER,
		TYPER,
	} kind;

	enum {
		WARNING,
		ERROR,
	} severity;

	const char* message;

	void print();
};

struct ASTNode;

struct Context {
	std::unordered_map<std::string, ASTNode*> defines;
	std::vector<Error> errors;
    Context* global;
    Context* parent;

	bool ok();
    bool is_global();

    ASTNode* resolve(const char* name);
    ASTNode* resolve(char* name, int length);
    bool declare(const char* name, ASTNode* value);

    template <typename T, typename ... Ts>
    T* alloc(Ts ...args) {
        T* buf = (T*)malloc(sizeof(T));
        new (buf) T (args...);
        return buf;
    }
};

#endif // guard
