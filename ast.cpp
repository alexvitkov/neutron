#include "ast.h"

int indent = 0;

void print(std::ostream& o, TypeList& tl) {
    for (const auto& entry : tl.entries) {
        o << entry.name << ": ";
        print(o, entry.type, false);
        o << ", ";
    }
    if (!tl.entries.empty())
        o << "\b\b \b"; // clear the last comma
}

void indent_line(std::ostream& o) {
    for (int i = 0; i < indent; i++)
        o << "    ";
}

void print(std::ostream& o, Block& bl) {
    indent ++;
    o << "{ \n";
    for (const auto& x : bl.ctx->defines) {
        indent_line(o);
        print(o, x.second, true);
    }

    for (const auto& d : bl.statements) {
        indent_line(o);
        print(o, d, false);
    }

    indent--;
    indent_line(o);
    o << "}\n";
}

void print(std::ostream& o, ASTNode* node, bool decl) {
    switch (node->nodetype) {
        case AST_FN:             print(o, (ASTFn*)node, decl); break;
        case AST_PRIMITIVE_TYPE: print(o, (ASTPrimitiveType*)node); break;
        case AST_BINARY_OP:      print(o, (ASTBinaryOp*)node); break;
        case AST_VAR:            print(o, (ASTVar*)node, decl); break;
        case AST_RETURN:         print(o, (ASTReturn*)node); break;
        case AST_CAST:           print(o, (ASTCast*)node); break;
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

        ASTType* rettype = (ASTType*)node->block->ctx->resolve("returntype");
        if (rettype) {
            o << ": ";
            print(o, rettype, false);
        }
        o << ' ';
        print(o, *node->block);
    }
    else {
        o << node->name;
    }
}

void print(std::ostream& o, ASTPrimitiveType* node) {
    o << node->name;
}

void print(std::ostream& o, ASTBinaryOp* node) {
    o << '(';
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
    print(o, node->rhs, false);
    o << ')';
    // << node->lhs << " # " << node->rhs << ')';
}

void print(std::ostream& o, ASTVar* node, bool decl) {
    if (decl) {
        o << "let " << node->name << ": ";
        print(o, node->type, false);
        if (node->value) {
            o << " = " ;
            print(o, node->value, false);
        }

        o << ";\n";
    }
    else {
        o << node->name;
    }
}

void print(std::ostream& o, ASTReturn* node) {
    o << "return ";
    print(o, node->value, false);
    o << ";\n";
}

void print(std::ostream& o, ASTCast* node) {
    o << '(';
    print(o, node->newtype, false);
    o << ')';
    print(o, node->inner, false);
}
