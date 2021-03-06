#ifndef TYPER_H
#define TYPER_H

#include "common.h"
#include "ast.h"

extern AST_PrimitiveType t_bool;
extern AST_PrimitiveType t_u8;
extern AST_PrimitiveType t_u16;
extern AST_PrimitiveType t_u32;
extern AST_PrimitiveType t_u64;
extern AST_PrimitiveType t_i8;
extern AST_PrimitiveType t_i16;
extern AST_PrimitiveType t_i32;
extern AST_PrimitiveType t_i64;
extern AST_PrimitiveType t_f32;
extern AST_PrimitiveType t_f64;
extern AST_PrimitiveType t_type;
extern AST_PrimitiveType t_void;
extern AST_PrimitiveType t_string_literal;
extern AST_PrimitiveType t_number_literal;
extern AST_PrimitiveType t_any8;

extern arr<AST_Type*> unsigned_types;
extern arr<AST_Type*>   signed_types;

//AST_PointerType* get_pointer_type(AST_Type* pointed_type);
//AST_ArrayType* get_array_type(AST_Type* base_type, u64 length);
struct TypeCheckJob : Job {
    AST_Context &ctx; 
    AST_Node    *node;

    bool declared = false;

    TypeCheckJob(AST_Context& ctx, AST_Node* node);

    std::wstring get_name() override;
    bool run(Message *msg) override;
};

#endif // guard
