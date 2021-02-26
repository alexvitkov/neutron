#include "error.h"
#include "context.h"
#include "ast.h"
#include <algorithm>
#include <iomanip>


enum VirtualTokens {
    VIRT_MISSING_TYPE_SPECIFIER = 2,
};

void print_line(AST_Context &global, SourceFile& sf, i64 line, arr<Location>& red_tokens) {
    if (line < 0 || line > sf.line_start.size)
        return;

    wcout << dim << std::setw(6) << line + 1 << L" │ " << resetstyle;

    u64 line_start = sf.line_start[(u32)line];

    for (u64 i = line_start; i < sf.length && sf.buffer[i] != '\n'; i++) {

        for (Location& loc : red_tokens) {
            if (loc.file_id == sf.id && loc.loc.start == i)
                wcout << red;
            if (loc.file_id ==sf.id && loc.loc.end == i)
                wcout << resetstyle;
        }

        wcout << sf.buffer[i];
    }
    wcout << resetstyle << '\n';
}

int get_line(SourceFile& sf, LocationInFile loc) {
    for (u32 i = 0; i < sf.line_start.size; i++) {
        if (loc.start < sf.line_start[i])
            return i - 1;
    }
    return sf.line_start.size - 1;
}

struct ERR_OfType {
    AST_Value* val;
};

std::wostream& operator<< (std::wostream& o, ERR_OfType rhs) {
    o << red << rhs.val << resetstyle << " (of type ";
    if (rhs.val->type) {
        o << rhs.val->type;
    } else {
        o << "NULL";
    }
    o << ")";
    return o; 
}

std::wostream& operator<< (std::wostream& o, TokenType tt) {
    switch (tt) {
        case ':':      o << "a type"; break;
        case ';':      o << "a semicolon"; break;
        case TOK_NONE: o << "end of file"; break; 
        case TOK_ID:   o << "an identifier";  break;
        default: {
            o << "TOK(" << (int)tt << ")";
            break;
        }
    }
    return o;
}

ERR_OfType oftype(AST_Node* node) {
    return ERR_OfType { (AST_Value*)node };
}

void print_code_segment(AST_Context& global, arr<Token>* tokens, arr<AST_Node*>* nodes, arr<AST_Node**>* node_ptrs) {
    // Group the tokens by file
    map<SourceFile*, arr<Location>> grouped;

    if (tokens) {
        for (Token& t : *tokens) {
            SourceFile* sf = &sources[t.file_id];
            if (!grouped.find(sf, nullptr)) {
                grouped.insert(sf, arr<Location>());
            }
            grouped[sf].push({ .file_id = sf->id, .loc = t.loc });
        }
    }

    if (nodes) {
        for (AST_Node* n : *nodes) {
            Location loc = location_of(global, &n);
            assert((loc.file_id || loc.loc.end || loc.loc.end) 
                    && "The AST Node doesn't have a defined location");

            SourceFile* sf = &sources[loc.file_id];

            if (!grouped.find2(sf)) {
                grouped.insert(sf, arr<Location>());
            }
            grouped[sf].push(loc);
        }
    }

    if (node_ptrs) {
        for (AST_Node** n : *node_ptrs) {
            Location loc = location_of(global, n);
            assert((loc.file_id || loc.loc.end || loc.loc.end) 
                    && "The AST Node doesn't have a defined location");

            SourceFile* sf = &sources[loc.file_id];

            if (!grouped.find(sf, nullptr)) {
                grouped.insert(sf, arr<Location>());
            }
            grouped[sf].push(loc);
        }
    }

    for (auto& kvp : grouped) {
        std::wcout << kvp.key->filename << ":\n";
        arr<Location>& toks = kvp.value;

        arr<u64> lines;
        for (Location& t : toks)
            lines.push_unique(get_line(sources[t.file_id], t.loc));

        std::sort(lines.begin(), lines.end());

        u64 last = 0;

        for (u64 l : lines) {
            u64 start = 0;
            if (l > 2) start = l - 2;

            start = std::max(last, start);

            u64 end = std::min(l + 3, (u64)kvp.key->line_start.size);

            if (start != last && last != 0) {
                wcout << dim << "...\n" << resetstyle;
            }

            for (u64 i = start; i < end; i++)
                print_line(global, *kvp.key, (i64)i, toks);
            last = end;
        }
    }
}


void th(u64 n) {
    static const char* first_10[10] = {
         "zeroth", "first", "second", "third", "fourth", 
         "fifth", "sixth", "seventh", "eighth", "ninth", 
    };

    if (n < 10) {
        wcout << first_10[n];
        return;
    } 
    else {
        wcout << n;

        switch (n % 10) {
            case 1:  wcout << "st"; break;
            case 2:  wcout << "nd"; break;
            case 3:  wcout << "rd"; break;
            default: wcout << "th"; break;
        }
    }
}

void print_err(AST_Context &global, Error& err) {
    wcout << "Fatal: ";

    switch (err.code) {

        case ERR_ALREADY_DEFINED: {
            wcout << red << err.key->name << resetstyle << " is defined multiple times:\n";
            print_code_segment(global, nullptr, &err.nodes, nullptr);
            break;
        }

        case ERR_BAD_FN_CALL: {
            AST_Value* src = (AST_Value*)*err.node_ptrs[0];

            auto arg = err.args[0];

            wcout << "Expected the ";
            th(arg.arg_index + 1);
            wcout << " argument to be of type " 
                << red << arg.arg_type << resetstyle 
                << " but instead got " << oftype(src) << ":\n";

            print_code_segment(global, nullptr, nullptr, &err.node_ptrs);

            break;
            
        }

        case ERR_INVALID_NUMBER_OF_ARGUMENTS: {
            AST_FnCall *fncall = (AST_FnCall*)err.nodes[0];
            AST_Fn     *fn     = (AST_Fn*)fncall->fn;
            AST_FnType *fntype = (AST_FnType*)fn->type;

            wcout << fn << " accepts ";
            if (fntype->is_variadic)
                wcout << "at least ";
            wcout << fntype->param_types.size << " arguments, but was called with ";
            if (fncall->args.size < fntype->param_types.size)
                wcout << "only ";
            wcout << fncall->args.size << ":\n";

            arr<AST_Node*> nodes = { fncall, fn };
            print_code_segment(global, nullptr, &nodes, nullptr);
            break;
        }

        // TODO ERR_INVALID_ASSIGNMENT and ERR_INVALID_INITIAL_VALUE can be merged
        case ERR_INVALID_ASSIGNMENT: {
            AST_Value* dst = (AST_Value*)*err.node_ptrs[0];
            AST_Value* src = (AST_Value*)*err.node_ptrs[1];

            wcout << "Cannot assign " << oftype(src) << " to " << oftype(dst) << ":\n";

            arr<AST_Node*> nodes = { err.nodes[0] };

            print_code_segment(global, nullptr, &nodes, nullptr);
            break;
        }

        case ERR_CANNOT_IMPLICIT_CAST: {
            AST_Node* dsttype     = (AST_Value*)err.nodes[0];
            AST_Node* the_value   = (AST_Value*)err.nodes[1];
            AST_Node* parent_stmt = (AST_Value*)err.nodes[2];

            switch (parent_stmt->nodetype) {
                case AST_RETURN:
                    wcout << "Cannot return " << oftype(the_value) << ", the return type is " << dsttype << ":\n";
                    break;
                default:
                    wcout << "Cannot implicitly cast " << oftype(the_value) << " to " << dsttype << ":\n";
                    break;
            }

            arr<AST_Node*> nodes = { parent_stmt };
            print_code_segment(global, nullptr, &nodes, nullptr);
            break;
        }

        case ERR_UNEXPECTED_TOKEN: {
            Token actual = err.tokens[0];
            Token expected = err.tokens[1];

            wcout << "Unexpected "         << red << actual.type   << resetstyle
                  << " while looking for " << red << expected.type << resetstyle << ":\n";

            arr<Token> toks = { actual };
            print_code_segment(global, &toks, nullptr, nullptr);
            break;

            break;
        }

        case ERR_NOT_DEFINED: {
            wcout << red << *err.node_ptrs[0] << resetstyle << " is not defined.\n";
            print_code_segment(global, nullptr, nullptr, &err.node_ptrs);
            break;
        }

        case ERR_RETURN_TYPE_MISSING: {
            AST_Return* ret = (AST_Return*)err.nodes[0];
            AST_Fn* fn = (AST_Fn*)err.nodes[1];
            AST_FnType* fntype = (AST_FnType*)fn->type;
            wcout << "Missing return value - expected a value of type " 
                << red << fntype->returntype << resetstyle << ":\n";

            arr<AST_Node*> nodes_to_print = { ret };
            print_code_segment(global, nullptr, &nodes_to_print, nullptr);
            break;
        }

        case ERR_RETURN_TYPE_INVALID: {
            AST_Return* ret = (AST_Return*)err.nodes[0];
            AST_Fn* fn = (AST_Fn*)err.nodes[1];
            AST_FnType* fntype = (AST_FnType*)fn->type;

            wcout << "Cannot return " << oftype(ret->value) << ", the function's return type is ";

            if (fntype->returntype)
                wcout << fntype->returntype << ":\n";
            else
                wcout << "void:\n";

            arr<AST_Node*> nodes_to_print = { ret };
            print_code_segment(global, nullptr, &nodes_to_print, nullptr);
            break;
        }

        case ERR_BAD_BINARY_OP: {
            wcout << "Cannot do a binary operation on " << oftype(err.nodes[0]) << " and " << oftype(err.nodes[1]) << ":\n";
            print_code_segment(global, &err.tokens, &err.nodes, &err.node_ptrs);
        }

        default: {
            wcout << "Error " << err.code << std::endl;
            print_code_segment(global, &err.tokens, &err.nodes, &err.node_ptrs);
        }

    } 
}
