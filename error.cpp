#include "error.h"
#include "context.h"
#include "ast.h"
#include <algorithm>


enum VirtualTokens {
    VIRT_MISSING_TYPE_SPECIFIER = 2,
};

void print_line(Context& global, SourceFile& sf, int line, arr<Location>& red_tokens) {
    if (line < 0 || line > sf.line_start.size)
        return;

    std::cout << dim;
    printf(" %*d │ ", 5, line + 1);
    std::cout << resetstyle;

    u64 line_start = sf.line_start[line];

    for (u64 i = line_start; i < sf.length && sf.buffer[i] != '\n'; i++) {

        for (Location& loc : red_tokens) {
            if (loc.file_id == sf.id && loc.loc.start == i) {
                std::cout << red;
            }
            if (loc.file_id ==sf.id && loc.loc.end == i)
                std::cout << resetstyle;
        }

        putc(sf.buffer[i], stdout);
    }
    std::cout << resetstyle << '\n';
}

int get_line(SourceFile& sf, LocationInFile loc) {
    for (int i = 0; i < sf.line_start.size; i++) {
        if (loc.start < sf.line_start[i])
            return i - 1;
    }
    return sf.line_start.size - 1;
}

void print_code_segment(Context& global, arr<Token>* tokens, arr<AST_Node*>* nodes, arr<AST_Node**>* node_ptrs) {
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

            if (!grouped.find(sf, nullptr)) {
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
        printf("%s:\n", kvp.key->filename);
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
                std::cout << dim << "...\n" << resetstyle;
            }

            for (u64 i = start; i < end; i++)
                print_line(global, *kvp.key, i, toks);
            last = end;
        }
    }
}

std::ostream& operator<< (std::ostream& o, TokenType& tt) {
    switch (tt) {
        case ':':      o << "type"; break;
        case ';':      o << "semicolon"; break;
        case TOK_NONE: o << "end of file"; break; 
        case TOK_ID:   o << "identifier";  break;
        default: {
            o << "TOK(" << (int)tt << ")";
            break;
        }
    }
    return o;
}

void th(u64 n) {
    static const char* first_10[10] = {
         "zeroth", "first", "second", "third", "fourth", 
         "fifth", "sixth", "seventh", "eighth", "ninth", 
    };

    if (n < 10) {
        std::cout << first_10[n];
        return;
    } 
    else {
        std::cout << n;

        switch (n % 10) {
            case 1:  std::cout << "st"; break;
            case 2:  std::cout << "nd"; break;
            case 3:  std::cout << "rd"; break;
            default: std::cout << "th"; break;
        }
    }
}

void print_err(Context& global, Error& err) {
    std::cout << "Fatal: ";

    switch (err.code) {

        case ERR_ALREADY_DEFINED: {
            std::cout << red << err.tokens[0].name << resetstyle << " is defined multiple times:\n";
            print_code_segment(global, &err.tokens, &err.nodes, &err.node_ptrs);
            break;
        }

        case ERR_BAD_FN_CALL: {
            AST_Value* src = (AST_Value*)*err.node_ptrs[0];

            auto arg = err.args[0];

            std::cout << "Expected the ";
            th(arg.arg_index + 1);
            std::cout << " argument to be of type " 
                << red << *arg.arg_type_ptr << resetstyle 
                << " but instead got "
                << red << src << dim << " (of type " << src->type << ')' << resetstyle << ":\n";

            print_code_segment(global, nullptr, nullptr, &err.node_ptrs);

            break;
            
        }

        case ERR_INVALID_ASSIGNMENT: {
            AST_Value* src = (AST_Value*)*err.node_ptrs[0];
            AST_Value* dst = (AST_Value*)*err.node_ptrs[1];

            std::cout << "Cannot assign " 
                << red << dst << dim << " (of type " << dst->type << ')' << resetstyle << " to "
                << red << src << dim << " (of type " << src->type << ')' << resetstyle << ":\n";

            arr<AST_Node*> nodes = { err.nodes[0] };

            print_code_segment(global, nullptr, &nodes, nullptr);
            break;
        }

        case ERR_UNEXPECTED_TOKEN: {
            Token actual = err.tokens[0];
            Token expected = err.tokens[1];

            std::cout << "Unexpected "         << red << actual.type << resetstyle
                      << " while looking for " << red << expected.type << resetstyle << ":\n";

            arr<Token> toks = { actual };
            print_code_segment(global, &toks, nullptr, nullptr);
            break;

            break;
        }

        case ERR_NOT_DEFINED: {
            std::cout << red;
            print(std::cout, err.nodes[0], false);
            std::cout << resetstyle << " is not defined.\n";
            print_code_segment(global, nullptr, &err.nodes, nullptr);
            break;
        }

        case ERR_RETURN_TYPE_MISSING: {
            AST_Return* ret = (AST_Return*)err.nodes[0];
            AST_Fn* fn = (AST_Fn*)err.nodes[1];
            AST_FnType* fntype = (AST_FnType*)fn->type;
            std::cout << "Missing return value - expected a value of type " 
                << red << fntype->returntype << resetstyle << ":\n";

            arr<AST_Node*> nodes_to_print = { ret };
            print_code_segment(global, nullptr, &nodes_to_print, nullptr);
            break;
        }

        case ERR_RETURN_TYPE_INVALID: {
            AST_Return* ret = (AST_Return*)err.nodes[0];
            AST_Fn* fn = (AST_Fn*)err.nodes[1];
            AST_FnType* fntype = (AST_FnType*)fn->type;

            std::cout << "Cannot return " 
                << red << ret->value << dim << " (of type " << ret->value->type << ')' << resetstyle 
                << ", the function's return type is ";

            if (fntype->returntype)
                std::cout << fntype->returntype << ":\n";
            else
                std::cout << "void:\n";

            arr<AST_Node*> nodes_to_print = { ret };
            print_code_segment(global, nullptr, &nodes_to_print, nullptr);
            break;
        }

        default: {
            printf("Error %d.\n", err.code);
            print_code_segment(global, &err.tokens, &err.nodes, &err.node_ptrs);
        }

    } 
}
