#include "error.h"
#include "context.h"
#include "ast.h"
#include <algorithm>


enum VirtualTokens {
    VIRT_MISSING_TYPE_SPECIFIER = 2,
};

const char* virt_tokens_text[] = {
    nullptr,
    nullptr,
    ": TYPE",
};

const char* error_names[] = {
    "Generic error",

    // Tokenizer errors
    "Unbalanced brackets",

    // Parser errors
    "Unexpected token",
    "Already defined",
    "Not defined",
    "Invalid expression",
    "Invalid number format",

    // Typer errors
    "Incompatible types",
    "Invalid return",
    "Not an Lvalue",
    "No such member",
    "Bad function call",
    "Invalid type",
    "Invalid derefernece",
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

void print_err(Context& global, Error& err) {
    std::cout << "Fatal: ";

    switch (err.code) {

        case ERR_ALREADY_DEFINED: {
            std::cout << red << err.tokens[0].name << resetstyle << " is defined multiple times:\n";
            print_code_segment(global, &err.tokens, &err.nodes, &err.node_ptrs);
            break;
        }

        case ERR_INCOMPATIBLE_TYPES: {
            AST_Value* src = (AST_Value*)*err.node_ptrs[0];
            AST_Value* dst = (AST_Value*)*err.node_ptrs[1];

            std::cout << "Cannot implicitly cast " 
                << red << dst << dim << " (" << dst->type << ')' << resetstyle << " to "
                << red << src << dim << " (" << src->type << ')' << resetstyle << ":\n";

            // print(std::cout, src_type, false);
            // std::cout << resetstyle << ":\n";

            arr<AST_Node*> nodes = { err.nodes[0] };

            print_code_segment(global, nullptr, &nodes, nullptr);
            break;
        }

        case ERR_NOT_DEFINED: {
            std::cout << red;
            print(std::cout, err.nodes[0], false);
            std::cout << resetstyle << " is not defined.\n";
            print_code_segment(global, nullptr, &err.nodes, nullptr);
            break;
        }

        default: {
            printf("%s.\n", error_names[err.code]);
            print_code_segment(global, &err.tokens, &err.nodes, &err.node_ptrs);
        }

    } 
}
