#include "error.h"

#include <algorithm>

void dim() {
    printf("\e[2m");
}

void red() {
    printf("\e[31;1m");
}

void resetstyle() {
    printf("\e[0m");
}

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

    "Unrecognized character",
    "Unbalanced brackets",

    "Unexpected token",
    "Already defined",
    "Not defined",
    "Invalid expression",

    "Incompatible types",
    "Invalid return",
    "Not an lvalue",
    "No such member",
};

void print_line(Context& global, SourceFile& sf, int line, arr<Token>& red_tokens) {
    if (line < 0 || line > sf.line_start.size)
        return;

    dim();
    printf(" %*d │ ", 5, line + 1);
    resetstyle();

    u64 line_start = sf.line_start[line];

    for (u64 i = line_start; i < sf.length && sf.buffer[i] != '\n'; i++) {
        for (Token& t : red_tokens) {
            if (t.file == sf.id && t.line == line && t.pos_in_line == i - line_start) {
                if (t.virt) {
                    red();
                    printf("%s", virt_tokens_text[t.virt]);
                    resetstyle();
                }
                else {
                    red();
                }
            }
            if (t.file == sf.id && t.line == line && (t.pos_in_line == i  - t.length - line_start) && !t.virt)
                resetstyle();
        }
        putc(sf.buffer[i], stdout);
    }
    resetstyle();
    putc('\n', stdout);
}


void print_code_segment(Context& global, arr<Token>& tokens) {
    // Group the tokens by file
    map<SourceFile*, arr<Token>> grouped;

    for (Token t : tokens) {
        SourceFile* sf = &sources[t.file];
        if (!grouped.find(sf, nullptr)) {
            grouped.insert(sf, arr<Token>());
        }
        grouped[sf].push(t);
    }

    for (auto& kvp : grouped) {
        printf("%s:\n", kvp.key->filename);
        arr<Token>& toks = kvp.value;
        std::sort(toks.begin(), toks.end());

        u32 l = 0;
        for (u32 i = 0; i < toks.size; i++) {
            for (l = std::max(l, toks[i].line - 1); l < toks[i].line + 2; l++) 
                print_line(global, *kvp.key, l, toks);
        }
    }
}

void print_err(Context& global, Error& err) {
    red();
    printf("Fatal: ");
    resetstyle();

    switch (err.code) {

        case ERR_ALREADY_DEFINED: {
            printf("%s is defined multiple times:\n", "asdf");
            print_code_segment(global, err.tokens);
            break;
        }

        case ERR_UNEXPECTED_TOKEN: {

            Token real = err.tokens[0];
            Token virt = err.tokens[1];

            // print_code_segment expects an array of red tokens
            // For ERR_UNEXPECTED_TOKEN we'll either give it the virtual token
            // which will result in an additional chunk of hint code being outputted
            // or we'll pass it the real token to highlight it in red
            arr<Token> t;

            switch (virt.type) {
                case TOK_ID:
                    printf("Expected an identifier:\n");
                    t.push(real);
                    break;
                case ':':
                    // If the expected token is a ':', then we're missing a type specifier
                    // The virtual token will print a ': TYPE' where the type siecifier is missing
                    printf("Missing type specifier:\n");
                    virt.virt = VIRT_MISSING_TYPE_SPECIFIER;
                    t.push(virt);
                    break;
                default:
                    printf("Unexpected token\n");
                    t.push(real);
                    break;
            }

            print_code_segment(global, t);
            break;
        }

        default: {
            printf("%s.\n", error_names[err.code]);
            print_code_segment(global, err.tokens);
        }

    } 
}
