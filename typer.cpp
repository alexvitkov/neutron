#include "typer.h"
#include "ast.h"
#include "error.h"

AST_PrimitiveType t_bool (PRIMITIVE_BOOL,     1, "bool");
AST_PrimitiveType t_u8   (PRIMITIVE_UNSIGNED, 1, "u8");
AST_PrimitiveType t_u16  (PRIMITIVE_UNSIGNED, 2, "u16");
AST_PrimitiveType t_u32  (PRIMITIVE_UNSIGNED, 4, "u32");
AST_PrimitiveType t_u64  (PRIMITIVE_UNSIGNED, 8, "u64");
AST_PrimitiveType t_i8   (PRIMITIVE_SIGNED,   1, "i8");
AST_PrimitiveType t_i16  (PRIMITIVE_SIGNED,   2, "i16");
AST_PrimitiveType t_i32  (PRIMITIVE_SIGNED,   4, "i32");
AST_PrimitiveType t_i64  (PRIMITIVE_SIGNED,   8, "i64");
AST_PrimitiveType t_f32  (PRIMITIVE_FLOAT,    4, "f32");
AST_PrimitiveType t_f64  (PRIMITIVE_FLOAT,    8, "f64");

AST_PrimitiveType t_type           (PRIMITIVE_UNIQUE,  0, "type");
AST_PrimitiveType t_void           (PRIMITIVE_UNIQUE,  0, "void");
AST_PrimitiveType t_string_literal (PRIMITIVE_UNIQUE,  0, "string_literal");

// This is a special type that anything with size <= 8 can implicitly cast to
// variadic arguments are assumed to be of type any8
AST_PrimitiveType t_any8 (PRIMITIVE_UNIQUE, 8, "any8");


// TODO This should be a hashset if someone ever makes one
//
// The reason this exists is to make sure function types are unique
// Type pointers MUST be comparable by checking their pointers with ==
// so we need additional logic to make sure we don't create the same
// composite type twice.
map<AST_FnType*, AST_FnType*> fn_types_hash;

map<AST_Type*, AST_PointerType*> pointer_types;

struct TypeSizeTuple {
    AST_Type* t;
    u64 u;
};

map<TypeSizeTuple, AST_ArrayType*> array_types;

bool validate_type(Context& ctx, AST_Type** type);

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
AST_PointerType* get_pointer_type(AST_Type* pointed_type) {
    AST_PointerType* pt;
    if (!pointer_types.find(pointed_type, &pt)) {
        pt = new AST_PointerType(pointed_type);
        pointer_types.insert(pointed_type, pt);
    }
    return pt;
}

AST_ArrayType* get_array_type(AST_Type* base_type, u64 size) {
    TypeSizeTuple key = { base_type, size };
    AST_ArrayType* at;
    if (!array_types.find(key, &at)) {
        at = new AST_ArrayType(base_type, size);
        array_types.insert(key, at);
    }
    return at;
}

bool map_equals(AST_FnType* lhs, AST_FnType* rhs) {
    MUST (lhs->returntype == rhs->returntype);
    MUST (lhs->param_types.size == rhs->param_types.size);
    MUST (lhs->is_variadic == rhs->is_variadic);

    for (u32 i = 0; i < lhs->param_types.size; i++)
        MUST (lhs->param_types[i] == rhs->param_types[i]);

    return true;
}

AST_FnType* make_function_type_unique(AST_FnType* temp_type) {

    AST_FnType* result;
    if (fn_types_hash.find(temp_type, &result)) {
        return result;
    }
    else {
        // TODO ALLOCATION
        AST_FnType* newtype = new AST_FnType(std::move(*temp_type));

        fn_types_hash.insert(newtype, newtype);
        return newtype;
    }
}

bool implicit_cast(Context& ctx, AST_Value** dst, AST_Type* type) {
    AST_Type* dstt = gettype(ctx, *dst);
    MUST (dstt);

    if (dstt == type) 
        return true;

    if (dstt->nodetype == AST_PRIMITIVE_TYPE 
            && type->nodetype == AST_PRIMITIVE_TYPE
            && ((AST_PrimitiveType*)type)->kind != PRIMITIVE_UNIQUE)
    {
        AST_PrimitiveType* l = (AST_PrimitiveType*)dstt;
        AST_PrimitiveType* r = (AST_PrimitiveType*)type;

        if ((*dst)->nodetype == AST_NUMBER) {
            AST_Number* num = (AST_Number*)*dst;
            if (num->fits_in((AST_PrimitiveType*)type)) {
                num->type = type;
                return true;
            } else {
                return false;
            }
        }
        else {
            // We allow very conservative implicit casting
            // bool -> u8 -> u16 -> u32 -> u64
            //    \     \       \      \
            //     i8 -> i16 -> i32 -> i64
            if (l == &t_bool
                    || (l->kind == r->kind && l->size < r->size)
                    || (l->kind == PRIMITIVE_UNSIGNED && r->kind == PRIMITIVE_SIGNED && l->size < r->size))
            {
                *dst = ctx.alloc<AST_Cast>(r, *dst);
                return true;
            }
        }
    }

    else if (dstt == &t_string_literal) {

        if (type == get_pointer_type(&t_i8) || type == &t_any8) {

            AST_StringLiteral* str = (AST_StringLiteral*)*dst;

            // If the same string literal has been seen before
            // There's already a static variable defined for it
            AST_Var* string_static_var = (AST_Var*)ctx.resolve({ .string_literal = str });

            if (!string_static_var) {
                // Get an array type that's just big enough to fit the C string 
                AST_ArrayType* string_array_type = get_array_type(&t_i8, str->length + 1);

                string_static_var = ctx.alloc<AST_Var>(nullptr, 0);
                string_static_var->type = string_array_type;
                string_static_var->initial_value = str;
                string_static_var->is_global = true;

                MUST (ctx.global->declare({ .string_literal = str }, string_static_var));
            }

            *dst =  ctx.alloc<AST_Cast>(get_pointer_type(&t_i8), string_static_var);
            return true;
        }

        return false;
    }

    else if (type == &t_any8) {
        return true;
    }

    return false;
}

bool is_valid_lvalue(AST_Node* expr) {
    switch (expr->nodetype) {
        case AST_VAR:
            return true;
        case AST_MEMBER_ACCESS:
            return true;
        case AST_DEREFERENCE:
            return true;
        default:
            return false;
    }
}

bool resolve_unresolved_references(GlobalContext& global, AST_Node** nodeptr);

bool resolve_unresolved_references(GlobalContext& global, AST_Block* block) {
    for (const auto& decl : block->ctx.declarations)
        MUST (resolve_unresolved_references(global, (AST_Node**)&decl.value));

    for (auto& stmt : block->statements)
        MUST (resolve_unresolved_references(global, &stmt));
    return true;
}

bool resolve_unresolved_references(GlobalContext& global, AST_Node** nodeptr) {
    AST_Node* node = *nodeptr;
    if (!node)
        return true;

    switch (node->nodetype) {
        case AST_UNRESOLVED_ID: {
            AST_UnresolvedId* id = (AST_UnresolvedId*)node;
            AST_Node* resolved = id->ctx.resolve({ id->name });
            if (!resolved) {
                id->ctx.error({
                    .code = ERR_NOT_DEFINED,
                    .nodes = { id }
                });
                return false;
            }

            *nodeptr = resolved;
            return true;
        }

        case AST_VAR: {
            AST_Var* var = (AST_Var*)node; MUST (resolve_unresolved_references(global, (AST_Node**)&var->initial_value)); 
            MUST (resolve_unresolved_references(global, (AST_Node**)&var->type));
            break;
        }

        case AST_CAST: {
            AST_Cast* cast = (AST_Cast*)node;
            MUST (resolve_unresolved_references(global, (AST_Node**)&cast->inner));
            MUST (resolve_unresolved_references(global, (AST_Node**)&cast->type));
            break;
        }

        case AST_FN_CALL: {
            AST_FnCall* fncall = (AST_FnCall*)node;
            MUST (resolve_unresolved_references(global, (AST_Node**)&fncall->fn));
            for (auto& arg : fncall->args)
                MUST (resolve_unresolved_references(global, (AST_Node**)&arg));
            break;
        }

        case AST_FN: {
            AST_Fn* fn = (AST_Fn*)node;
            AST_FnType* fntype = (AST_FnType*)fn->type;

            for (auto& param_type : fntype->param_types)
                MUST (resolve_unresolved_references(global, (AST_Node**)&param_type));

            for (u32 i = 0; i < fn->argument_names.size; i++) {
                AST_Var* argvar = global.alloc<AST_Var>(fn->argument_names[i], i);
                argvar->type = fntype->param_types[i];

                MUST (fn->block.ctx.declare({ .name = argvar->name }, argvar));
            }

            MUST (resolve_unresolved_references(global, &fn->block));
            break;
        }

        case AST_ASSIGNMENT:
        case AST_BINARY_OP: 
        {
            AST_BinaryOp* op = (AST_BinaryOp*)node;
            MUST (resolve_unresolved_references(global, (AST_Node**)&op->lhs));
            MUST (resolve_unresolved_references(global, (AST_Node**)&op->rhs));
            break;
        }

        case AST_DEREFERENCE: {
            AST_Dereference* deref = (AST_Dereference*)node;
            MUST (resolve_unresolved_references(global, (AST_Node**)&deref->ptr));
            return true;
        }

        case AST_ADDRESS_OF: {
            AST_AddressOf* addrof = (AST_AddressOf*)node;
            MUST (resolve_unresolved_references(global, (AST_Node**)&addrof->inner));
            return true;
        }

        case AST_RETURN: {
            AST_Return* ret = (AST_Return*)node;
            MUST (resolve_unresolved_references(global, (AST_Node**)&ret->value));
            break;
        }

        case AST_IF: {
            AST_If* ifs = (AST_If*)node;
            MUST (resolve_unresolved_references(global, &ifs->then_block));
            MUST (resolve_unresolved_references(global, &ifs->condition));
            break;
        }

        case AST_WHILE: {
            AST_While* whiles = (AST_While*)node;
            MUST (resolve_unresolved_references(global, &whiles->block));
            MUST (resolve_unresolved_references(global, &whiles->condition));
            break;
        }

        case AST_BLOCK: {
            MUST (resolve_unresolved_references(global, (AST_Block*)node));
            break;
        }
        case AST_STRUCT: {
            AST_Struct* str = (AST_Struct*)node;
            for (auto& mem : str->members)
                MUST (resolve_unresolved_references(global, (AST_Node**)&mem.type));
            break;
        }

        case AST_MEMBER_ACCESS: {
            AST_MemberAccess* access = (AST_MemberAccess*)node;
            MUST (resolve_unresolved_references(global, (AST_Node**)&access->lhs));
            break;
        }

        case AST_POINTER_TYPE: {
            AST_PointerType* pt = (AST_PointerType*)node;
            if (pt->pointed_type->nodetype == AST_UNRESOLVED_ID) {
                MUST (resolve_unresolved_references(global, (AST_Node**)&pt->pointed_type));

                *nodeptr = get_pointer_type(pt->pointed_type);
                // TODO ALLOCATION free the temp pointer type here
            }
            break;
        }

        case AST_NONE:
        case AST_TEMP_REF:
            assert(!"Not supported");

        case AST_NUMBER:
        case AST_PRIMITIVE_TYPE:
        case AST_STRING_LITERAL:
            break;


        default:
            NOT_IMPLEMENTED();
    }
    return true;
}

bool is_integral_type(AST_Type* type) {
    switch (type->nodetype) {
        case AST_PRIMITIVE_TYPE: {
            AST_PrimitiveType* prim = (AST_PrimitiveType*)type;
            return prim->kind == PRIMITIVE_BOOL
                || prim->kind == PRIMITIVE_SIGNED
                || prim->kind == PRIMITIVE_UNSIGNED;
        }
        default: 
            return false;
    }
}


bool validate_fn_type(Context& ctx, AST_Fn* fn) {
    AST_FnType *fntype = (AST_FnType*)fn->type;
    
    for (auto& p : fntype->param_types) {
        // we must check if the parameters are valid AST_Types
        MUST (validate_type(ctx, &p));
    }

    if (!fntype->returntype)
        fntype->returntype = &t_void;

    MUST (validate_type(ctx, &fntype->returntype));
    
    fntype = make_function_type_unique(fntype);
    fn->type = fntype;
    
    return fn->type;
}


AST_Type* gettype(Context& ctx, AST_Value* node) {
    if (node->type)
        return node->type;

    switch (node->nodetype) {
        case AST_PRIMITIVE_TYPE: 
        case AST_FN_TYPE:
        case AST_POINTER_TYPE:
        {
            return &t_type;
        }

        case AST_FN: {
            NOT_IMPLEMENTED();
        }

        case AST_FN_CALL: {
            AST_FnCall *fncall = (AST_FnCall*)node;

            // The parser can't tell apart function calls from casts
            // We are smarter than the parser though so we can
            if (fncall->fn->nodetype == AST_FN) {
                AST_Fn *fn = (AST_Fn*)fncall->fn;
                AST_FnType *fntype = (AST_FnType*)fn->type;

                u64 num_args = fncall->args.size;
                u64 num_params = fntype->param_types.size;

                if ((!fntype->is_variadic && num_args != num_params) || (fntype->is_variadic && num_args < num_params)) {
                    // TODO ERROR
                    ctx.error({ .code = ERR_BAD_FN_CALL });
                    return nullptr;
                }

                for (int i = 0; i < num_args; i++) {

                    AST_Type* param_type = i < num_params ? fntype->param_types[i] : &t_any8;

                    if (!implicit_cast(ctx, &fncall->args[i], param_type)) {
                        ctx.error({
                            .code = ERR_BAD_FN_CALL,
                            .node_ptrs = { 
                                (AST_Node**)&fncall->args[i] 
                            },
                            .args = {{  
                                .arg_name = fn->argument_names[i],
                                .arg_type_ptr = (AST_Node**)&param_type,
                                .arg_index = (u64)i
                            }}
                        });
                        return nullptr;
                    }
                }

                fncall->type = fntype->returntype;
                return fntype->returntype;
            }
            else {
                
                MUST (validate_type(ctx, (AST_Type**)(&fncall->fn)));

                // If the user is trying to 'call' a type then that's a cast.
                // Here the node is patched up from a AST_FnCall to a AST_Cast
                
                // There  must be exactly one 'argument' to the function call -
                // the value the user is trying to cast
                
                if (fncall->args.size != 1) {
                    // TODO ERROR
                    ctx.error({ .code = ERR_BAD_FN_CALL });
                    return nullptr;
                }
                
                AST_Cast* cast = (AST_Cast*)fncall;
                
                cast->nodetype = AST_CAST;
                cast->type = (AST_Type*)fncall->fn;
                cast->inner = fncall->args[0];
                
                MUST (validate_type(ctx, &cast->type));
                MUST (gettype(ctx, cast->inner));
                
                // Right now we only support pointer->pointer casts
                if ((cast->type->nodetype == AST_POINTER_TYPE || cast->type->nodetype == AST_ARRAY_TYPE)
                    && (cast->inner->type->nodetype == AST_POINTER_TYPE || cast->inner->type->nodetype == AST_ARRAY_TYPE))
                {
                    return cast->type;
                }
                else 
                {
                    // TODO ERROR
                    ctx.error({ .code = ERR_INVALID_CAST });
                    return nullptr;;
                }
            }
        }

        case AST_ASSIGNMENT: {
            AST_BinaryOp* bin = (AST_BinaryOp*)node;

            AST_Type* lhst = gettype(ctx, bin->lhs);
            AST_Type* rhst = gettype(ctx, bin->rhs);
            MUST (lhst && rhst);

            if (!is_valid_lvalue(bin->lhs)) {
                ctx.error({ .code = ERR_NOT_AN_LVALUE, .nodes = { lhst, } });
                return nullptr;
            }

            if (implicit_cast(ctx, &bin->rhs, lhst)) {
                bin->type = lhst;
                return bin->type;
            }

            ctx.error({ 
                .code = ERR_INVALID_ASSIGNMENT, 
                .nodes = {
                    bin
                },
                .node_ptrs = { 
                    (AST_Node**)&bin->lhs, 
                    (AST_Node**)&bin->rhs, 
                }
            });

            return nullptr;;
        }

        case AST_BINARY_OP: {
            AST_BinaryOp* bin = (AST_BinaryOp*)node;

            AST_Type* lhst = gettype(ctx, bin->lhs);
            AST_Type* rhst = gettype(ctx, bin->rhs);
            MUST (lhst && rhst);

            // Handle pointer arithmetic
            if (lhst->nodetype == AST_POINTER_TYPE || rhst->nodetype == AST_POINTER_TYPE) {
                if (bin->op != '+' && bin->op != '-') {
                    ctx.error({
                        .code = ERR_INVALID_ASSIGNMENT,
                        .nodes = { bin->lhs, bin-> rhs }
                    });
                    return nullptr;
                }

                // There are 6 cases here
                // Pointer  + Integral -- valid
                // Integral + Pointer  -- valid
                // Pointer  + Pointer  -- invalid
                
                // Pointer  - Integral -- valid
                // Pointer  - Pointer  -- valid
                // Integral - Pointer  -- invalid

                if (lhst->nodetype == AST_POINTER_TYPE) {
                    // If LHS is a pointer and operator is +, RHS can only be an integral type
                    if (bin->op == '+' && !is_integral_type(rhst))
                        goto Error;

                    if (bin->op == '-') {
                        // Pointer Pointer subtraction is valid if the pointers the same type
                        if (rhst->nodetype == AST_POINTER_TYPE) {
                            if (lhst != rhst)
                                goto Error;
                            bin->op = OP_SUB_PTR_PTR;
                        }
                        // The other valid case is Pointer - Integral
                        else {
                            if (!is_integral_type(rhst))
                                goto Error;
                            bin->op = OP_SUB_PTR_INT;
                        }
                    }
                    else {
                        bin->op = OP_ADD_PTR_INT;
                    }


                } else {
                    // LHS is not a pointer => the only valid case is Integral + Pointer
                    if (!is_integral_type(lhst) || bin->op != '+')
                        goto Error;

                    // In order to simplify code generation
                    // we will swap the arguments so that the pointer is LHS
                    std::swap(bin->lhs, bin->rhs);
                    bin->op = OP_ADD_PTR_INT;
                }

                // After this is all done, the type of the binary expression
                // is the same as the pointer type
                bin->type = lhst->nodetype == AST_POINTER_TYPE ? lhst : rhst;
                return bin->type;

                return nullptr;
            }

            // Handle regular human arithmetic between numbers
            else {
                if (implicit_cast(ctx, &bin->rhs, lhst))
                    return (bin->type = lhst);

                if (implicit_cast(ctx, &bin->lhs, rhst))
                    return (bin->type = rhst);
            }

Error:
            ctx.error({
                .code = ERR_BAD_BINARY_OP,
                .nodes = { bin->lhs, bin->rhs }
            });
            return nullptr;
        }

        case AST_MEMBER_ACCESS: {
            AST_MemberAccess* ma = (AST_MemberAccess*)node;

            AST_Struct* s = (AST_Struct*)gettype(ctx, ma->lhs);
            MUST (s);
            if (s->nodetype != AST_STRUCT) {
                ctx.error({ .code = ERR_NO_SUCH_MEMBER, });
                return nullptr;
            }

            for (u32 i = 0; i < s->members.size; i++) {
                auto& member = s->members[i];
                if (!strcmp(member.name, ma->member_name)) {
                    ma->index = i;
                    ma->type = member.type;
                    return member.type;
                }
            }
            ctx.error({ .code = ERR_NO_SUCH_MEMBER, });
            return nullptr;
        }

        case AST_DEREFERENCE: {
            AST_Dereference* deref = (AST_Dereference*)node;

            AST_Type* inner_type = gettype(ctx, deref->ptr);
            MUST (inner_type);

            if (inner_type->nodetype != AST_POINTER_TYPE) {
                ctx.error({
                    .code = ERR_INVALID_DEREFERENCE,
                    .nodes = { deref },
                });
            }

            deref->type = ((AST_PointerType*)inner_type)->pointed_type;
            return deref->type;
        }

        case AST_ADDRESS_OF: {
            AST_AddressOf* addrof = (AST_AddressOf*)node;

            AST_Type* inner_type = gettype(ctx, addrof->inner);
            MUST (inner_type);

            addrof->type = get_pointer_type(inner_type);

            if (addrof->inner->nodetype == AST_VAR) {
                ((AST_Var*)addrof->inner)->always_on_stack = true;
            }
            return addrof->type;
        }

        default:
            NOT_IMPLEMENTED();
    }

    UNREACHABLE;
}

// Types are parsed by the parse_expr function
// When parse_expr sees foo* it doesn't know it it's a dereference 
// or a pointer type, so it just assumes it's a dereference
//
// Same goes for foo[123], is it a index operator or array type?
//
// This is called on a AST_Value outputted by parse_expr
// to patch up the node into a valid type
//
// TODO ALLOCATION this function leaks from all sides
bool validate_type(Context& ctx, AST_Type** type) {
    
    AST_Value* val = (AST_Value*)*type;

    if (val->nodetype & AST_TYPE_BIT)
        return true;

    switch (val->nodetype) {
        case AST_DEREFERENCE: {
            AST_Dereference* deref = (AST_Dereference*)val;

            MUST (validate_type(ctx, (AST_Type**)&deref->ptr));

            Location loc = location_of(ctx, (AST_Node**)&deref);

            *type = get_pointer_type((AST_Type*)deref->ptr);
            ctx.global->reference_locations[(AST_Node**)type] = loc;

            return true;
        }
        default: {
            ctx.error({
                .code = ERR_INVALID_TYPE,
                .nodes = { val },
            });
            return false;
        }
    }

    return true;
};

bool typecheck(Context& ctx, AST_Node* node) {

    switch (node->nodetype) {
        case AST_BLOCK: {
            AST_Block* block = (AST_Block*)node;

            for (const auto& decl : block->ctx.declarations) {
                if (decl.value->nodetype == AST_VAR) {
                    AST_Var* var = (AST_Var*)decl.value;

                    MUST (typecheck(block->ctx, var));
                    if (var->initial_value)
                        MUST (typecheck(block->ctx, var->initial_value));
                }
            }

            for (const auto& stmt : block->statements)
                MUST (typecheck(block->ctx, stmt));
            return true;
        }

        case AST_FN: {
            AST_Fn* fn = (AST_Fn*)node;

            // MUST (gettype(ctx, fn)); // This is done in a prepass

            if (!fn->is_extern) {
                MUST (typecheck(fn->block.ctx, &fn->block));
            }
            return true;
        }

        // TODO INITIAL_VALUE
        // There should be a prepass for variables outside the global scope
        // that converts initial values into assignments
        // Way too many checks have to be written twice, for assignments and init values
        case AST_VAR: {
            AST_Var* var = (AST_Var*)node;

            MUST (validate_type(ctx, &var->type));

            if (var->initial_value) {
                MUST (gettype(ctx, var->initial_value));

                MUST (validate_type(ctx, &var->initial_value->type));

                if (!implicit_cast(ctx, &var->initial_value, var->type)) {
                    ctx.error({
                        .code = ERR_INVALID_INITIAL_VALUE,
                        .nodes = {
                            var,
                            var->initial_value,
                        },
                    });
                    return false;
                }

                return true;
            }

            return true;
        }

        case AST_RETURN: {
            AST_Return *ret = (AST_Return*)node;
            AST_Type *rettype = ((AST_FnType*)ctx.fn->type)->returntype;

            if (rettype != &t_void && !ret->value) {
                ctx.error({
                    .code = ERR_RETURN_TYPE_MISSING,
                    .nodes = {
                        ret,
                        ctx.fn,
                    }
                });
                return false;
            }
            if ((rettype != &t_void && !implicit_cast(ctx, &ret->value, rettype))
                || (rettype == &t_void && ret->value))
            {
                ctx.error({
                    .code = ERR_RETURN_TYPE_INVALID,
                    .nodes = {
                        ret,
                        ctx.fn,
                    }
                });
                return false;
            }
            return true;
        }

        case AST_IF: {
            AST_If* ifs = (AST_If*)node;
            MUST (typecheck(ctx, ifs->condition));
            MUST (typecheck(ctx, &ifs->then_block));
            return true;
        }

        case AST_WHILE: {
            AST_While* whiles = (AST_While*)node;
            MUST (typecheck(ctx, whiles->condition));
            MUST (typecheck(ctx, &whiles->block));
            return true;
        }

        case AST_STRUCT: {
            //AST_Struct* s = (AST_Struct*)node;
            //for (auto& member : s->members.entries) {
                // MUST(typecheck(ctx, member.type));
            //}
            return true;
        }

        default: {
            if (node->nodetype & AST_VALUE_BIT) {
                return gettype(ctx, (AST_Value*)node);
            }
            NOT_IMPLEMENTED();
        }

    }


}

bool typecheck_all(GlobalContext& global) {
    for (auto& decl : global.declarations)
        MUST (resolve_unresolved_references(global, &decl.value));

    // During typechecking we can declare more stuff
    // so it's not safe to iterate over global.declarations as it may relocate
    // Right now we copy the original declartions into an array and iterate over those
    // This means that the new declarations WON'T BE TYPECHECKED
    arr<AST_Node*> declarations_copy;
    for (auto& decl : global.declarations)
        declarations_copy.push(decl.value);

    // First resolve the function types, we need their signatures for everything else
    for (auto& decl : declarations_copy) {
        if (decl->nodetype == AST_FN) {
            MUST (validate_fn_type(global, (AST_Fn*)decl));
        }
        else if (decl->nodetype == AST_VAR) {
            MUST (typecheck(global, decl));
        }
    }

    global.temp_allocator.free_all();

    for (auto& decl : declarations_copy)
        MUST (typecheck(global, decl));

    return true;
}
