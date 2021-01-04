#include "ast.h"
#include "typer.h"

#define ALWAYS_BRACKETS 0

int indent = 0;

ASTNumber::ASTNumber(u64 floorabs) : ASTValue(AST_NUMBER, nullptr), floorabs(floorabs) {
    if (floorabs < 0xFF)
        type = &t_u8;
    else if (floorabs < 0xFFFF)
        type = &t_u16;
    else if (floorabs < 0xFFFFFFFF)
        type = &t_u32;
    else
        type = &t_u64;
}

void print(std::ostream& o, TypeList& tl) {
    for (const auto& entry : tl.entries) {
        print(o, entry.type, false);
        o << " " << entry.name;
        o << ", ";
    }
    if (tl.entries.size)
        o << "\b\b \b"; // clear the last comma
}

void indent_line(std::ostream& o) {
    for (int i = 0; i < indent; i++)
        o << "    ";
}

void print(std::ostream& o, ASTBlock* bl) {
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

void print(std::ostream& o, ASTNode* node, bool decl) {
    switch (node->nodetype) {
        case AST_FN:             print(o, (ASTFn*)node, decl); break;
        case AST_PRIMITIVE_TYPE: print(o, (ASTPrimitiveType*)node); break;
        case AST_BINARY_OP:      print(o, (ASTBinaryOp*)node, false); break;
        case AST_ASSIGNMENT:     print(o, (ASTBinaryOp*)node, false); break;
        case AST_VAR:            print(o, (ASTVar*)node, decl); break;
        case AST_RETURN:         print(o, (ASTReturn*)node); break;
        case AST_CAST:           print(o, (ASTCast*)node); break;
        case AST_NUMBER:         print(o, (ASTNumber*)node); break;
        case AST_IF:             print(o, (ASTIf*)node); break;
        case AST_WHILE:          print(o, (ASTWhile*)node); break;
        case AST_FN_CALL:        print(o, (ASTFnCall*)node); break;
        case AST_STRUCT:         print(o, (ASTStruct*)node, decl); break;
        case AST_MEMBER_ACCESS:  print(o, (ASTMemberAccess*)node); break;
        default:                 o << "NOPRINT[" << node->nodetype << ']'; break;
    }
}

void print(std::ostream& o, ASTFn* node, bool decl) {
    if (decl) {
        ASTFn* fn = (ASTFn*)node;
        o << "fn ";
        if (fn->name)
            o << fn->name;
        o << '(';
        print(o, fn->args);
        o << ")";

        ASTType* rettype = (ASTType*)node->block.ctx.resolve("returntype");
        if (rettype) {
            o << ": ";
            print(o, rettype, false);
        }
        o << ' ';
        print(o, &node->block);
    }
    else {
        o << node->name;
    }
}

void print(std::ostream& o, ASTPrimitiveType* node) {
    o << node->name;
}


void print(std::ostream& o, ASTBinaryOp* node, bool brackets = false) {
    if (ALWAYS_BRACKETS || brackets)
        o << '(';

    if (node->lhs->nodetype == AST_BINARY_OP)
        print(o, (ASTBinaryOp*)node->lhs, PREC(((ASTBinaryOp*)(node->lhs))->op) < PREC(node->op));
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
        print(o, (ASTBinaryOp*)node->rhs, PREC(((ASTBinaryOp*)(node->rhs))->op) < PREC(node->op));
    else
        print(o, node->rhs, false);
    if (ALWAYS_BRACKETS || brackets)
        o << ')';
    // << node->lhs << " # " << node->rhs << ')';
}

void print(std::ostream& o, ASTVar* node, bool decl) {
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

void print(std::ostream& o, ASTReturn* node) {
    if (node->value) {
        o << "return ";
        print(o, node->value, false);
    }
    else {
        o << "return";
    }
}

void print(std::ostream& o, ASTCast* node) {
    print(o, node->type, false);
    o << '(';
    print(o, node->inner, false);
    o << ')';
}

void print(std::ostream& o, ASTNumber* node) {
    o << node->floorabs;
}

void print(std::ostream& o, ASTIf* node) {
    o << "if ";
    print(o, node->condition, false);
    o << " ";
    print(o, &node->block);
}

void print(std::ostream& o, ASTWhile* node) {
    o << "while ";
    print(o, node->condition, false);
    o << " ";
    print(o, &node->block);
}

void print(std::ostream& o, ASTFnCall* node) {
    print(o, node->fn, false);
    o << "(";
    for (ASTNode*& a : node->args) {
        print(o, a, false);
        o << ", ";
    }
    if (node->args.size)
        o << "\b\b \b"; // clear the last comma
    o << ")";
}

void print(std::ostream& o, ASTStruct* node, bool decl) {
    if (decl) {
        ASTStruct* fn = (ASTStruct*)node;
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

void print(std::ostream& o, ASTMemberAccess* node) {
    if (ALWAYS_BRACKETS) o << '(';
    print(o, node->lhs, false);
    o << '.' << node->member_name;
    if (ALWAYS_BRACKETS) o << ')';
}
