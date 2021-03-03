#include "common.h"
#include "ast.h"
#include "typer.h"
#include "cast.h"
#include "error.h"

AST_Context::AST_Context(AST_Context* parent)
    : AST_Node(AST_BLOCK),
      parent(parent), 
      global(parent ? parent->global : *(AST_GlobalContext*)this), 
      fn(parent ? parent->fn : nullptr) 
{ 
    if (parent)
        parent->children.push(this);
}

u32 map_hash(DeclarationKey key) {
    u32 hash = 0;
    if (key.name)
        hash = map_hash(key.name);

    // we assume that the fn_type has been made unique, we can hash it as void*
    hash ^= map_hash((void*)key.fn_type);
    hash ^= key.op;

    return hash;
}

u32 map_equals(DeclarationKey &lhs, DeclarationKey &rhs) {
    // either both have a name or neither has a name
    MUST (!lhs.name == !rhs.name);

    if (lhs.name && rhs.name && strcmp(lhs.name, rhs.name))
        return false;

    MUST (lhs.fn_type == rhs.fn_type);
    MUST (lhs.op == rhs.op);

    return true;
}

u32 map_hash(BuiltinCastPair pair) {
    return map_hash(pair.src) ^ map_hash(pair.dst);
}

bool map_equals(BuiltinCastPair a, BuiltinCastPair b) {
    return a.src == b.src && a.dst == b.dst;
}

bool AST_Context::declare(DeclarationKey key, AST_Node* value, bool sendmsg) {
    // Throw an error if another value with the same name has been declared
    AST_Node* prev_decl;
    if (declarations.find(key, &prev_decl)) {
        error({
            .code = ERR_ALREADY_DEFINED,
            .nodes = { prev_decl, value },
            .key = new DeclarationKey(key),
        });
        return false;
    }

    declarations.insert(key, value);

    if (sendmsg) {
        assert(key.name);

        NewDeclarationMessage msg;    
        msg.msgtype = MSG_NEW_DECLARATION;
        msg.context = this;
        msg.key     = key;
        msg.node    = value;

        global.send_message(&msg);
    }

    return true;
}

void AST_Context::error(Error err) {
    global.errors.push(err);
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

u32 hash(AST_Type* returntype, bucketed_arr<AST_Type*>& param_types) {
    u32 hash = map_hash(returntype);

    for (AST_Type* param : param_types) {
        hash ^= map_hash((void*)param);
        hash <<= 1;
    }

    return hash;
}

u32 map_hash(AST_FnType* fntype) {
    return hash(fntype->returntype, fntype->param_types);
};

bool map_equals(AST_FnType *lhs, AST_FnType *rhs) {
    MUST (lhs->returntype == rhs->returntype);
    MUST (lhs->param_types.size == rhs->param_types.size);
    MUST (lhs->is_variadic == rhs->is_variadic);

    for (u32 i = 0; i < lhs->param_types.size; i++)
        MUST (lhs->param_types[i] == rhs->param_types[i]);

    return true;
}

// TODO RESOLUTION we should check if it's a AST_UnresolvedId
AST_PointerType* AST_Context::get_pointer_type(AST_Type* pointed_type) {
    AST_PointerType* pt;
    if (!global.pointer_types.find(pointed_type, &pt)) {
        pt = alloc<AST_PointerType>(pointed_type, global.target.pointer_size);
        global.pointer_types.insert(pointed_type, pt);
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
    if (global.fn_types_hash.find(temp_type, &result)) {
        return result;
    }
    else {
        AST_FnType* newtype = alloc<AST_FnType>(std::move(*temp_type));
        global.fn_types_hash.insert(newtype, newtype);
        return newtype;
    }
}

void AST_Context::decrement_hanging_declarations() {
    hanging_declarations--;
    if (hanging_declarations == 0) {
        close();
    }
}

void AST_Context::close() {
    ScopeClosedMessage msg;
    msg.msgtype = MSG_SCOPE_CLOSED;
    msg.scope = this;
    closed = true;
    global.send_message(&msg);
}

AST_GlobalContext::AST_GlobalContext() : AST_Context(nullptr), subscribers(MESSAGES_COUNT) {
    for (u32 i = 0; i < MESSAGES_COUNT; i++)
        subscribers.push(arr<Job*>());

    builtin_casts.insert({ &t_number_literal, &t_u64 }, { number_literal_to_u64, 100 });
    //builtin_casts.insert({ &t_number_literal, &t_u32 }, 90);
    //builtin_casts.insert({ &t_number_literal, &t_u16 }, 80);
    //builtin_casts.insert({ &t_number_literal, &t_u8 },  70);
}
