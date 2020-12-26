#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <string.h>

#define u8  uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t
#define i8  int8_t
#define i16 int16_t
#define i32 int32_t
#define i64 int64_t

enum ASTNodeType {
	AST_NONE = 0,
	AST_TYPE,
};

struct ASTNode {
	ASTNodeType nodetype;
};

enum TokenType : u8 {
	TOK_NONE = 0,
	TOK_ID  = 128,
	TOK_NUM,

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
	KW_STRUCT,
	KW_TRUE,
	KW_FALSE,
};

#endif // guard
