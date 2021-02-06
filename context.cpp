#include "common.h"
#include "ast.h"
#include "error.h"

AST_Context::AST_Context(AST_Context* parent)
    : AST_Node(AST_BLOCK),
      parent(parent), 
      global(parent ? parent->global : (AST_GlobalContext*)this), 
      fn(parent ? parent->fn : nullptr) 
{ 
    if (parent)
        parent->children.push(this);
}

u32 map_hash(DeclarationKey key) {
    u32 hash = 0;
    if (key.name)
        hash = map_hash(key.name);

    hash ^= map_hash(key.fn_type);

    return hash;
}

u32 map_equals(DeclarationKey lhs, DeclarationKey rhs) {
    if (!lhs.name != !rhs.name)
        return false;

    if (lhs.name && rhs.name && strcmp(lhs.name, rhs.name))
        return false;

    return lhs.fn_type == rhs.fn_type;
}

bool AST_Context::declare(DeclarationKey key, AST_Node* value) {
    // Throw an error if another value with the same name has been declared
    AST_Node* prev_decl;
    if (declarations.find(key, &prev_decl)) {
        error({
            .code = ERR_ALREADY_DEFINED,
            .nodes = { prev_decl, value }
        });
        return false;
    }

    declarations.insert(key, value);
    return true;
}

AST_Node* AST_Context::resolve(DeclarationKey key) {
    AST_Node* node;
    if (!declarations.find(key, &node)) {
        if (parent)
            return parent->resolve(key);
        return nullptr;
    }
    return node;
}

void AST_Context::error(Error err) {
    global->errors.push(err);
}



struct TypeSizeTuple {
    AST_Type* t;
    u64 u;
};

map<TypeSizeTuple, AST_ArrayType*> array_types;

bool validate_type(AST_Context& ctx, AST_Type** type);

u32 map_hash(TypeSizeTuple tst) {
    return map_hash(tst.t) ^ (u32)tst.u;
}

u32 map_equals(TypeSizeTuple lhs, TypeSizeTuple rhs) {
    return lhs.t == rhs.t && lhs.u == rhs.u;
}

u32 hash(AST_Type* returntype, arr<AST_Type*>& param_types) {
    u32 hash = map_hash(returntype);

    for (AST_Type* param : param_types)
        hash ^= map_hash(param);

    return hash;
}

u32 map_hash(AST_FnType* fn_type) {
    return hash(fn_type->returntype, fn_type->param_types);
};

// TODO RESOLUTION we should check if it's a AST_UnresolvedId
AST_PointerType* AST_Context::get_pointer_type(AST_Type* pointed_type) {
    AST_PointerType* pt;
    if (!global->pointer_types.find(pointed_type, &pt)) {
        pt = alloc<AST_PointerType>(pointed_type, global->target.pointer_size);
        global->pointer_types.insert(pointed_type, pt);
    }
    return pt;
}

AST_ArrayType* AST_Context::get_array_type(AST_Type* base_type, u64 size) {
    TypeSizeTuple key = { base_type, size };
    AST_ArrayType* at;
    if (!array_types.find(key, &at)) {
        at = alloc<AST_ArrayType>(base_type, size);
        array_types.insert(key, at);
    }
    return at;
}

AST_FnType* AST_Context::make_function_type_unique(AST_FnType* temp_type) {
    AST_FnType* result;
    if (global->fn_types_hash.find(temp_type, &result)) {
        return result;
    }
    else {
        AST_FnType* newtype = alloc<AST_FnType>(std::move(*temp_type));
        global->fn_types_hash.insert(newtype, newtype);
        return newtype;
    }
}
