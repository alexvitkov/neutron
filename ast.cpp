#include "ast.h"
#include "typer.h"

#define ALWAYS_BRACKETS 0

int indent = 0;

AST_StringLiteral::AST_StringLiteral(Token stringToken) : AST_Value(AST_STRING_LITERAL, &t_string_literal) {
    length = strlen(stringToken.name);
    str = stringToken.name;
}

std::wostream& operator<< (std::wostream& o, bucketed_arr<StructElement>& tl) {
    for (const auto& entry : tl) {
        o << entry.name << ": " << entry.type << ", ";
    }
    if (tl.size)
        o << "\b\b \b"; // clear the last comma
    return o;
}

void indent_line(std::wostream& o) {
    for (int i = 0; i < indent; i++)
        o << "    ";
}

std::wostream& operator<< (std::wostream& o, AST_Context* bl) {
    indent ++;
    o << "{ \n";

    for (auto nnp : bl->declarations) {
        indent_line(o);

        switch (nnp.value->nodetype) {
        case AST_FN:
        case AST_VAR:
            print(o, nnp.value, true);
            break;
        default:
            o << "const " << nnp.key.name << " = ";
            print(o, nnp.value, true);
            o << ";\n";
        }
    }

    for (const auto& d : bl->statements) {
        indent_line(o);
        print(o, d, false);

        if (d->nodetype != AST_IF && d->nodetype != AST_WHILE)
            o << ";\n";
    }

    indent--;
    indent_line(o);
    o << "}\n";
    return o;
}

void print(std::wostream& o, AST_Node* node, bool decl) {
    assert(node && "Attempting to print a null node");

    switch (node->nodetype) {
        case AST_FN:             print(o, (AST_Fn*)node, decl); break;
        case AST_BINARY_OP:      print(o, (AST_BinaryOp*)node, false); break;
        case AST_ASSIGNMENT:     print(o, (AST_BinaryOp*)node, false); break;
        case AST_UNARY_OP:       print(o, (AST_UnaryOp*)node,  false); break;
        case AST_VAR:            print(o, (AST_Var*)node, decl); break;
        case AST_STRUCT:         print(o, (AST_Struct*)node, decl); break;

        case AST_BLOCK:          o << (AST_Context*)node; break;
        case AST_PRIMITIVE_TYPE: o << (AST_PrimitiveType*)node; break;
        case AST_RETURN:         o << (AST_Return*)node; break;
        // case AST_CAST:           o << (AST_Cast*)node; break;
        case AST_NUMBER:         o << (AST_NumberLiteral*)node; break;
        case AST_SMALL_NUMBER:   o << (AST_SmallNumber*)node; break;
        case AST_FN_TYPE:        o << (AST_FnType*)node; break;
        case AST_IF:             o << (AST_If*)node; break;
        case AST_WHILE:          o << (AST_While*)node; break;
        case AST_FN_CALL:        o << (AST_FnCall*)node; break;
        case AST_MEMBER_ACCESS:  o << (AST_MemberAccess*)node; break;
        case AST_POINTER_TYPE:   o << (AST_PointerType*)node; break;
        case AST_ARRAY_TYPE:     o << (AST_ArrayType*)node; break;
        case AST_DEREFERENCE:    o << (AST_Dereference*)node; break;
        case AST_ADDRESS_OF:     o << (AST_AddressOf*)node; break;
        case AST_UNRESOLVED_ID:  o << (AST_UnresolvedId*)node; break;
        case AST_STRING_LITERAL: o << (AST_StringLiteral*)node; break;
        case AST_TYPEOF:         o << (AST_Typeof*)node; break;
        default:                 o << "NOPRINT[" << (int)node->nodetype << ']'; break;
    }
}

std::wostream& operator<< (std::wostream& o, AST_Node* node) {
    // o.flush();
    print(o, node, false);
    return o;
}

void print(std::wostream& o, AST_Fn* fn, bool decl) {
    if (decl) {
        o << "fn ";
        if (fn->name)
            o << fn->name;

        o << '(';

        AST_FnType* fntype = (AST_FnType*)fn->type;
        for (u32 i = 0; i < fn->argument_names.size; i++)
            o << fn->argument_names[i] << ": " << fntype->param_types[i] << ", ";

        // Delete the last comma
        if (fn->argument_names.size)
            o << "\b\b \b";

        o << ')';

        AST_Type* rettype = ((AST_FnType*)fn->type)->returntype;
        if (rettype)
            o << ": " << rettype;

        if (fn->is_extern) {
            o << ";\n";
        } else {
            o << ' ' << &fn->block;
        }
    } else {
        o << fn->name;
        AST_FnType *fntype = (AST_FnType*)fn->type;
        o << "(";
        for (AST_Type *param : fntype->param_types) {
            o << param << ", ";
        }
        if (fntype->param_types.size > 0)
            o << "\b\b \b";
        o << ")";
    }
}

std::wostream& operator<< (std::wostream& o, AST_PrimitiveType* node) {
    o << node->name;
    return o;
}

void print(std::wostream& o, AST_UnaryOp* node, bool brackets = false) {
    if (ALWAYS_BRACKETS || brackets)
        o << '(';

    switch (node->op) {
        case '+': o << '+' << node->inner; break;
        case '-': o << '-' << node->inner; break;
        case '*': o << node->inner << '*'; break;
        case '&': o << node->inner << '*'; break;
        default:
            NOT_IMPLEMENTED();
    }

    if (ALWAYS_BRACKETS || brackets)
        o << ')';
}

void print(std::wostream& o, AST_BinaryOp* node, bool brackets = false) {
    if (ALWAYS_BRACKETS || brackets)
        o << '(';

    if (node->lhs IS AST_BINARY_OP)
        print(o, (AST_BinaryOp*)node->lhs, PREC(((AST_BinaryOp*)(node->lhs))->op) < PREC(node->op));
    else
        print(o, node->lhs, false);
    o << ' ';

    switch (node->op) {
        case OP_PLUSPLUS: o << "++"; break;
        case OP_MINUSMINUS: o << "--"; break;
        case OP_SHIFTLEFT: o << "<<"; break;
        case OP_SHIFTRIGHT: o << ">>"; break;
        case OP_LESSEREQUALS: o << "<="; break;
        case OP_GREATEREQUALS: o << ">="; break;
        case OP_DOUBLEEQUALS: o << "=="; break;
        case OP_AND: o << "&&"; break;
        case OP_OR: o << "||"; break;
        case OP_NOTEQUALS: o << "!="; break;
        case OP_ADDASSIGN: o << "+="; break;
        case OP_SUBASSIGN: o << "-="; break;
        case OP_MULASSIGN: o << "*="; break;
        case OP_DIVASSIGN: o << "/="; break;
        case OP_MODASSIGN: o << "%="; break;
        case OP_SHIFTLEFTASSIGN: o << "<<="; break;
        case OP_SHIFTRIGHTASSIGN: o << ">>="; break;
        case OP_BITANDASSIGN: o << "&="; break;
        case OP_BITXORASSIGN: o << "^="; break;
        case OP_BITORASSIGN: o << "|="; break;
        case OP_ADD_PTR_INT: o << "+"; break;
        case OP_SUB_PTR_INT: o << "-"; break;
        case OP_SUB_PTR_PTR: o << "-"; break;
        default: {
            if (node->op < 128)
                o << (char)node->op;
            else
                o << "TOK(" << (int)node->op << ")";
        }
    }

    o << ' ';
    if (node->rhs IS AST_BINARY_OP)
        print(o, (AST_BinaryOp*)node->rhs, PREC(((AST_BinaryOp*)(node->rhs))->op) < PREC(node->op));
    else
        print(o, node->rhs, false);
    if (ALWAYS_BRACKETS || brackets)
        o << ')';
}

void print(std::wostream& o, AST_Var* node, bool decl) {
    if (decl) {
        o << "let " << (node->name ? node->name : "_") << ": " << node->type;
        //if (node->initial_value)
        //  o << " = "  << node->initial_value;
        o << ";\n";
    }
    else {
        if (node->name)
            o << node->name;
        else
            o << "_";
    }
}

void print(std::wostream& o, AST_Struct* node, bool decl) {
    if (decl) {
        AST_Struct* str = (AST_Struct*)node;
        o << "struct ";
        if (str->name)
            o << str->name;
        o << '{' << str->members << '}';
    }
    else {
        o << node->name;
    }
}

std::wostream& operator<< (std::wostream& o, AST_Return* node) {
    if (node->value) {
        o << "return " << node->value;
    } else {
        o << "return";
    }
    return o;
}

std::wostream& operator<< (std::wostream& o, AST_NumberLiteral* node) {
    o << &node->number_data;
    return o;
}

std::wostream& operator<< (std::wostream& o, AST_SmallNumber* node) {
    AST_PrimitiveType *prim = (AST_PrimitiveType*)node->type;
    switch (prim->kind) {
        case PRIMITIVE_SIGNED:
            o << node->i64_val; 
            break;
        case PRIMITIVE_UNSIGNED: 
            o << node->u64_val; 
            break;
        case PRIMITIVE_FLOAT: {
            switch (prim->size) {
                case 8: 
                    o << node->f64_val; 
                    break;
                case 4: 
                    o << node->f32_val; 
                    break;
                default:
                    UNREACHABLE;
            }
        }
        default:
            UNREACHABLE;
    }
    return o;
}

std::wostream& operator<< (std::wostream& o, AST_If* node) {
    o << "if " << node->condition << " " << &node->then_block;
    return o;
}

std::wostream& operator<< (std::wostream& o, AST_While* node) {
    o << "while " << node->condition << " " << &node->block;
    return o;
}

std::wostream& operator<< (std::wostream& o, AST_FnCall* node) {
    o << node->fn << "(";

    for (AST_Value*& a : node->args) {
        o << a << ", ";
    }
    if (node->args.size)
        o << "\b\b \b"; // clear the last comma
    o << ")";
    return o;
}

std::wostream& operator<< (std::wostream& o, AST_PointerType* node) {
    o << node->pointed_type << '*';
    return o;
}

std::wostream& operator<< (std::wostream& o, AST_ArrayType* node) {
    o << node->base_type << '[' << node->array_length << ']';
    return o;
}

std::wostream& operator<< (std::wostream& o, AST_MemberAccess* node) {
    if (ALWAYS_BRACKETS) o << '(';
    o << node->lhs << '.' << node->member_name;
    if (ALWAYS_BRACKETS) o << ')';
    return o;
}

std::wostream& operator<< (std::wostream& o, AST_Dereference* node) {
    if (node->ptr IS AST_BINARY_OP || ALWAYS_BRACKETS) {
        o << '(' << node->ptr << ")*";
    } else {
        o << node->ptr << '*';
    }
    return o;
}

std::wostream& operator<< (std::wostream& o, AST_AddressOf* node) {
    o << node->inner << '&';
    return o;
}

std::wostream& operator<< (std::wostream& o, AST_UnresolvedId* node) {
    o << node->name;
    return o;
}

void print_string(std::wostream& o, const char* s) {
    o << '"';
    for (const char *c = s; *c; c++) {
        switch (*c) {
            case '"':  o << "\\\""; break;
            case '\\': o << '\\';   break;
            case '\n': o << "\\n";  break;
            default:
                o << *c;
                break;
        }
    }
    o <<  '"';
}

std::wostream& operator<< (std::wostream& o, AST_StringLiteral* node) {
    print_string(o, node->str);
    return o;
}

bool postparse_tree_compare(AST_Node *lhs, AST_Node *rhs) {
    if (lhs->nodetype != rhs->nodetype) {
        return false;
    }

    switch (lhs->nodetype) {
        case AST_FN: {
            AST_Fn *fn1 = (AST_Fn*)lhs;
            AST_Fn *fn2 = (AST_Fn*)rhs;

            return postparse_tree_compare(&fn1->block, &fn2->block);
        }

        case AST_BLOCK: {
            AST_Context *bl1 = (AST_Context*)lhs;
            AST_Context *bl2 = (AST_Context*)rhs;

            MUST (bl1->statements.size == bl2->statements.size);

            for (u32 i = 0; i < bl1->statements.size; i++)
                MUST (postparse_tree_compare(bl1->statements[i], bl2->statements[i]));

            return true;
        }

        case AST_ASSIGNMENT:
        case AST_BINARY_OP: 
        {
            AST_BinaryOp *op1 = (AST_BinaryOp*)lhs;
            AST_BinaryOp *op2 = (AST_BinaryOp*)rhs;

            MUST(op1->op == op2->op);
            MUST(postparse_tree_compare(op1->lhs, op2->lhs));
            MUST(postparse_tree_compare(op1->rhs, op2->rhs));

            return true;
        }

        case AST_NUMBER: {
            NOT_IMPLEMENTED();
            /*
            AST_Number *num1 = (AST_Number*)lhs;
            AST_Number *num2 = (AST_Number*)rhs;

            return num1->floorabs == num2->floorabs;
            */
        }

        case AST_UNRESOLVED_ID: {
            AST_UnresolvedId *id1 = (AST_UnresolvedId*)lhs;
            AST_UnresolvedId *id2 = (AST_UnresolvedId*)rhs;

            return !strcmp(id1->name, id2->name);
        }

        default: {
            NOT_IMPLEMENTED();
        }
    }
}

std::wostream& operator<<(std::wostream& o, AST_Typeof* node) {
    o << "typeof(" << node->inner << ")";
    return o;
}

std::wostream& operator<<(std::wostream& o, AST_FnType* node) {
    // wcout << "(" << 
    if (node->param_types.size == 0)
        o << "()";
    else if (node->param_types.size > 0)
        o << "(";

    for (int i = 0; i < node->param_types.size; i++) {
        o << node->param_types[i];
        if (i != node->param_types.size - 1)
            o << ", ";
    }
    if (node->param_types.size > 0)
        o << ")";
    o << "->" << node->returntype;
    return o;
}

AST_NumberLiteral::AST_NumberLiteral(NumberData *data) 
        : AST_Value(AST_NUMBER, &t_number_literal), number_data(*data) {}
