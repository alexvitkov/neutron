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

void print(std::ostream& o, arr<NamedType>& tl) {
    for (const auto& entry : tl) {
        print(o, entry.type, false);
        o << " " << entry.name;
        o << ", ";
    }
    if (tl.size)
        o << "\b\b \b"; // clear the last comma
}

void indent_line(std::ostream& o) {
    for (int i = 0; i < indent; i++)
        o << "    ";
}

void print(std::ostream& o, AST_Block* bl) {
    indent ++;
    o << "{ \n";

    for (auto nnp : bl->ctx.declarations_arr) {
        indent_line(o);

        switch (nnp.node->nodetype) {
        case AST_FN:
        case AST_VAR:
            print(o, nnp.node, true);
            break;
        default:
            o << "const " << nnp.name << " = ";
            print(o, nnp.node, true);
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
}

void print(std::ostream& o, AST_Node* node, bool decl) {
    switch (node->nodetype) {
        case AST_FN:             print(o, (AST_Fn*)node, decl); break;
        case AST_PRIMITIVE_TYPE: print(o, (AST_PrimitiveType*)node); break;
        case AST_BINARY_OP:      print(o, (AST_BinaryOp*)node, false); break;
        case AST_ASSIGNMENT:     print(o, (AST_BinaryOp*)node, false); break;
        case AST_VAR:            print(o, (AST_Var*)node, decl); break;
        case AST_RETURN:         print(o, (AST_Return*)node); break;
        case AST_CAST:           print(o, (AST_Cast*)node); break;
        case AST_NUMBER:         print(o, (AST_Number*)node); break;
        case AST_IF:             print(o, (AST_If*)node); break;
        case AST_WHILE:          print(o, (AST_While*)node); break;
        case AST_FN_CALL:        print(o, (AST_FnCall*)node); break;
        case AST_STRUCT:         print(o, (AST_Struct*)node, decl); break;
        case AST_MEMBER_ACCESS:  print(o, (AST_MemberAccess*)node); break;
        default:                 o << "NOPRINT[" << node->nodetype << ']'; break;
    }
}

void print(std::ostream& o, AST_Fn* node, bool decl) {
    if (decl) {
        AST_Fn* fn = (AST_Fn*)node;
        o << "fn ";
        if (fn->name)
            o << fn->name;
        o << '(';
        print(o, fn->args);
        o << ")";

        AST_Type* rettype = (AST_Type*)node->block.ctx.resolve("returntype");
        if (rettype) {
            o << ": ";
            print(o, rettype, false);
        }
        o << ' ';

        if (fn->is_extern) {
            o << ";\n";
        } else {
            print(o, &node->block);
        }
    }
    else {
        o << node->name;
    }
}

void print(std::ostream& o, AST_PrimitiveType* node) {
    o << node->name;
}


void print(std::ostream& o, AST_BinaryOp* node, bool brackets = false) {
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
        case OP_MODASSIGN: o << "\%="; break;
        case OP_SHIFTLEFTASSIGN: o << "<<="; break;
        case OP_SHIFTRIGHTASSIGN: o << ">>="; break;
        case OP_BITANDASSIGN: o << "&="; break;
        case OP_BITXORASSIGN: o << "^="; break;
        case OP_BITORASSIGN: o << "|="; break;
        default:
            o << (char)node->op;
    }

    o << ' ';
    if (node->rhs->nodetype == AST_BINARY_OP)
        print(o, (AST_BinaryOp*)node->rhs, PREC(((AST_BinaryOp*)(node->rhs))->op) < PREC(node->op));
    else
        print(o, node->rhs, false);
    if (ALWAYS_BRACKETS || brackets)
        o << ')';
    // << node->lhs << " # " << node->rhs << ')';
}

void print(std::ostream& o, AST_Var* node, bool decl) {
    if (decl) {
        o << "let " << node->name << ": ";
        print(o, node->type, false);
        if (node->initial_value) {
            o << " = " ;
            print(o, node->initial_value, false);
        }

        o << ";\n";
    }
    else {
        o << node->name;
    }
}

void print(std::ostream& o, AST_Return* node) {
    if (node->value) {
        o << "return ";
        print(o, node->value, false);
    }
    else {
        o << "return";
    }
}

void print(std::ostream& o, AST_Cast* node) {
    print(o, node->type, false);
    o << '(';
    print(o, node->inner, false);
    o << ')';
}

void print(std::ostream& o, AST_Number* node) {
    o << node->floorabs;
}

void print(std::ostream& o, AST_If* node) {
    o << "if ";
    print(o, node->condition, false);
    o << " ";
    print(o, &node->block);
}

void print(std::ostream& o, AST_While* node) {
    o << "while ";
    print(o, node->condition, false);
    o << " ";
    print(o, &node->block);
}

void print(std::ostream& o, AST_FnCall* node) {
    print(o, node->fn, false);
    o << "(";
    for (AST_Node*& a : node->args) {
        print(o, a, false);
        o << ", ";
    }
    if (node->args.size)
        o << "\b\b \b"; // clear the last comma
    o << ")";
}

void print(std::ostream& o, AST_Struct* node, bool decl) {
    if (decl) {
        AST_Struct* fn = (AST_Struct*)node;
        o << "struct ";
        if (fn->name)
            o << fn->name;
        o << '{';
        print(o, fn->members);
        o << '}';
    }
    else {
        o << node->name;
    }
}

void print(std::ostream& o, AST_MemberAccess* node) {
    if (ALWAYS_BRACKETS) o << '(';
    print(o, node->lhs, false);
    o << '.' << node->member_name;
    if (ALWAYS_BRACKETS) o << ')';
}
