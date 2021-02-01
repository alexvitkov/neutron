#include "ast.h"
#include "typer.h"

#define ALWAYS_BRACKETS 0

int indent = 0;

AST_Number::AST_Number(u64 floorabs) : AST_Value(AST_NUMBER, nullptr), floorabs(floorabs) {

    if (floorabs < 0xFF)
        type = &t_u8;
    else if (floorabs < 0xFFFF)
        type = &t_u16;
    else if (floorabs < 0xFFFFFFFF)
        type = &t_u32;
    else
        type = &t_u64;

}

bool AST_Number::fits_in(AST_PrimitiveType* numtype) {

    switch (numtype->kind) {
        case PRIMITIVE_UNSIGNED: {
            switch (numtype->size) {
                case 1: return floorabs < 0xFF;
                case 2: return floorabs < 0xFFFF;
                case 4: return floorabs < 0xFFFFFFFF;
                case 8: return floorabs < 0xFFFFFFFFFFFFFFFF;
            }
        }
        case PRIMITIVE_SIGNED: {
            switch (numtype->size) {
                case 1: return floorabs < 0x80;
                case 2: return floorabs < 0x8000;
                case 4: return floorabs < 0x80000000;
                case 8: return floorabs < 0x8000000000000000;
            }
        }
        default:
            return false;
    }
    
}

AST_StringLiteral::AST_StringLiteral(Token stringToken) : AST_Value(AST_STRING_LITERAL, &t_string_literal) {
    length = strlen(stringToken.name);
    str = stringToken.name;
}

std::wostream& operator<< (std::wostream& o, arr<NamedType>& tl) {
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

std::wostream& operator<< (std::wostream& o, AST_Block* bl) {
    indent ++;
    o << "{ \n";

    for (auto nnp : bl->ctx.declarations) {
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
    switch (node->nodetype) {
        case AST_FN:             print(o, (AST_Fn*)node, decl); break;
        case AST_BINARY_OP:      print(o, (AST_BinaryOp*)node, false); break;
        case AST_ASSIGNMENT:     print(o, (AST_BinaryOp*)node, false); break;
        case AST_VAR:            print(o, (AST_Var*)node, decl); break;

        case AST_PRIMITIVE_TYPE: o << (AST_PrimitiveType*)node; break;
        case AST_RETURN:         o << (AST_Return*)node; break;
        case AST_CAST:           o << (AST_Cast*)node; break;
        case AST_NUMBER:         o << (AST_Number*)node; break;
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


        // TODO RETURNTYPE
        AST_Type* rettype = (AST_Type*)fn->block.ctx.resolve({ "returntype" });
        if (rettype)
            o << ": " << rettype;

        if (fn->is_extern) {
            o << ";\n";
        } else {
            o << ' ' << &fn->block;
        }
    } 
    else {
        o << fn->name;
    }
}

std::wostream& operator<< (std::wostream& o, AST_PrimitiveType* node) {
    o << node->name;
    return o;
}


void print(std::wostream& o, AST_BinaryOp* node, bool brackets = false) {
    if (ALWAYS_BRACKETS || brackets)
        o << '(';

    if (node->lhs->nodetype == AST_BINARY_OP)
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
    if (node->rhs->nodetype == AST_BINARY_OP)
        print(o, (AST_BinaryOp*)node->rhs, PREC(((AST_BinaryOp*)(node->rhs))->op) < PREC(node->op));
    else
        print(o, node->rhs, false);
    if (ALWAYS_BRACKETS || brackets)
        o << ')';
}

void print(std::wostream& o, AST_Var* node, bool decl) {
    if (decl) {
        o << "let " << (node->name ? node->name : "_") << ": " << node->type;
        if (node->initial_value) {
            o << " = "  << node->initial_value;
        }
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

std::wostream& operator<< (std::wostream& o, AST_Cast* node) {
    o << node->type << '(' << node->inner << ')';
    return o;
}

std::wostream& operator<< (std::wostream& o, AST_Number* node) {
    o << node->floorabs;
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
    if (node->ptr->nodetype == AST_BINARY_OP || ALWAYS_BRACKETS) {
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

std::wostream& operator<< (std::wostream& o, AST_StringLiteral* node) {
    o << '"' << node->str << '"';
    return o;
}
