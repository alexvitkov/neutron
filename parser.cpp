#include "parser.h"
#include "sourcefile.h"

#include <signal.h>

#include "keywords.gperf.gen.h"


#define TOK(c) ((TokenType)c)

enum CharTraits : u8 {
	CT_ERROR               = 0x00,
	CT_LETTER              = 0x01,
	CT_DIGIT               = 0x02,
	CT_WHITESPACE          = 0x04,
	CT_SINGLECHAR_OP       = 0x08,
	CT_MULTICHAR_OP_START  = 0x10,
};

CharTraits ctt[128] {
/* 0   - NUL */ CT_ERROR,
/* 1   - SOH */ CT_ERROR,
/* 2   - STX */ CT_ERROR,
/* 3   - ETX */ CT_ERROR,
/* 4   - EOT */ CT_ERROR,
/* 5   - ENQ */ CT_ERROR,
/* 6   - ACK */ CT_ERROR,
/* 7   - BEL */ CT_ERROR,
/* 8   - BS  */ CT_ERROR,
/* 9   - HT  */ CT_WHITESPACE,
/* 10  - LF  */ CT_WHITESPACE,
/* 11  - VT  */ CT_ERROR,
/* 12  - FF  */ CT_ERROR,
/* 13  - CR  */ CT_WHITESPACE,
/* 14  - SO  */ CT_ERROR,
/* 15  - SI  */ CT_ERROR,
/* 16  - DLE */ CT_ERROR,
/* 17  - DC1 */ CT_ERROR,
/* 18  - DC2 */ CT_ERROR,
/* 19  - DC3 */ CT_ERROR,
/* 20  - DC4 */ CT_ERROR,
/* 21  - NAK */ CT_ERROR,
/* 22  - SYN */ CT_ERROR,
/* 23  - ETB */ CT_ERROR,
/* 24  - CAN */ CT_ERROR,
/* 25  - EM  */ CT_ERROR,
/* 26  - SUB */ CT_ERROR,
/* 27  - ESC */ CT_ERROR,
/* 28  - FS  */ CT_ERROR,
/* 29  - GS  */ CT_ERROR,
/* 30  - RS  */ CT_ERROR,
/* 31  - US  */ CT_ERROR,
/* 32  - SPC */ CT_WHITESPACE,
/* 33  - !   */ CT_ERROR,
/* 34  - "   */ CT_ERROR,
/* 35  - #   */ CT_ERROR,
/* 36  - $   */ CT_ERROR,
/* 37  - %   */ CT_ERROR,
/* 38  - &   */ CT_ERROR,
/* 39  - '   */ CT_ERROR,
/* 40  - (   */ CT_SINGLECHAR_OP,
/* 41  - )   */ CT_SINGLECHAR_OP,
/* 42  - *   */ CT_SINGLECHAR_OP,
/* 43  - +   */ CT_SINGLECHAR_OP,
/* 44  - ,   */ CT_SINGLECHAR_OP,
/* 45  - -   */ CT_SINGLECHAR_OP,
/* 46  - .   */ CT_SINGLECHAR_OP,
/* 47  - /   */ CT_ERROR,
/* 48  - 0   */ CT_DIGIT,
/* 49  - 1   */ CT_DIGIT,
/* 50  - 2   */ CT_DIGIT,
/* 51  - 3   */ CT_DIGIT,
/* 52  - 4   */ CT_DIGIT,
/* 53  - 5   */ CT_DIGIT,
/* 54  - 6   */ CT_DIGIT,
/* 55  - 7   */ CT_DIGIT,
/* 56  - 8   */ CT_DIGIT,
/* 57  - 9   */ CT_DIGIT,
/* 58  - :   */ CT_SINGLECHAR_OP,
/* 59  - ;   */ CT_SINGLECHAR_OP,
/* 60  - <   */ CT_ERROR,
/* 61  - =   */ CT_SINGLECHAR_OP,
/* 62  - >   */ CT_ERROR,
/* 63  - ?   */ CT_ERROR,
/* 64  - @   */ CT_ERROR,
/* 65  - A   */ CT_WHITESPACE,
/* 66  - B   */ CT_WHITESPACE,
/* 67  - C   */ CT_WHITESPACE,
/* 68  - D   */ CT_WHITESPACE,
/* 69  - E   */ CT_WHITESPACE,
/* 70  - F   */ CT_WHITESPACE,
/* 71  - G   */ CT_WHITESPACE,
/* 72  - H   */ CT_WHITESPACE,
/* 73  - I   */ CT_WHITESPACE,
/* 74  - J   */ CT_WHITESPACE,
/* 75  - K   */ CT_WHITESPACE,
/* 76  - L   */ CT_WHITESPACE,
/* 77  - M   */ CT_WHITESPACE,
/* 78  - N   */ CT_WHITESPACE,
/* 79  - O   */ CT_WHITESPACE,
/* 80  - P   */ CT_WHITESPACE,
/* 81  - Q   */ CT_WHITESPACE,
/* 82  - R   */ CT_WHITESPACE,
/* 83  - S   */ CT_WHITESPACE,
/* 84  - T   */ CT_WHITESPACE,
/* 85  - U   */ CT_WHITESPACE,
/* 86  - V   */ CT_WHITESPACE,
/* 87  - W   */ CT_WHITESPACE,
/* 88  - X   */ CT_WHITESPACE,
/* 89  - Y   */ CT_WHITESPACE,
/* 90  - Z   */ CT_WHITESPACE,
/* 91  - [   */ CT_ERROR,
/* 92  - \   */ CT_ERROR,
/* 93  - ]   */ CT_ERROR,
/* 94  - ^   */ CT_ERROR,
/* 95  - _   */ CT_LETTER, // yes, _ is a letter
/* 96  - `   */ CT_ERROR,
/* 97  - a   */ CT_LETTER,
/* 98  - b   */ CT_LETTER,
/* 99  - c   */ CT_LETTER,
/* 100 - d   */ CT_LETTER,
/* 101 - e   */ CT_LETTER,
/* 102 - f   */ CT_LETTER,
/* 103 - g   */ CT_LETTER,
/* 104 - h   */ CT_LETTER,
/* 105 - i   */ CT_LETTER,
/* 106 - j   */ CT_LETTER,
/* 107 - k   */ CT_LETTER,
/* 108 - l   */ CT_LETTER,
/* 109 - m   */ CT_LETTER,
/* 110 - n   */ CT_LETTER,
/* 111 - o   */ CT_LETTER,
/* 112 - p   */ CT_LETTER,
/* 113 - q   */ CT_LETTER,
/* 114 - r   */ CT_LETTER,
/* 115 - s   */ CT_LETTER,
/* 116 - t   */ CT_LETTER,
/* 117 - u   */ CT_LETTER,
/* 118 - v   */ CT_LETTER,
/* 119 - w   */ CT_LETTER,
/* 120 - x   */ CT_LETTER,
/* 121 - y   */ CT_LETTER,
/* 122 - z   */ CT_LETTER,
/* 123 - {   */ CT_SINGLECHAR_OP,
/* 124 - |   */ CT_ERROR,
/* 125 - }   */ CT_SINGLECHAR_OP,
/* 126 - ~   */ CT_ERROR,
/* 127 -     */ CT_ERROR,
};

bool tokenize(Context& global, SourceFile &s) {
	u64 word_start;
	enum { NONE, WORD, NUMBER } state = NONE;

	struct BracketToken {
		char tok;
		u64 tokid;
	};
	std::vector<BracketToken> bracket_stack;

	for (u64 i = 0; i < s.length; i++) {
		char c = s.buffer[i];
		CharTraits ct = ctt[c];

		if (!ct) {
			char* err_msg = (char*)malloc(256);
			sprintf(err_msg, "unrecognized character 0x%x, or '%c'", c, c);
			global.errors.push_back({Error::TOKENIZER, Error::ERROR, err_msg});
			return false;
		}

		if (state) {
			if ((ct & (CT_LETTER | CT_DIGIT)) == 0 && state) {
				tok* kw = Perfect_Hash::in_word_set(s.buffer + word_start, i - word_start);

				s.tokens.push_back({
					.type = kw ? kw->type : (state == WORD ? TOK_ID : TOK_NUM),
					.length = (u32)(i - word_start),
					.start = word_start
				});
				state = NONE;
			}
		}
		else {
			if (ct & (CT_LETTER | CT_DIGIT)) {
				word_start = i;
				state = (ct & CT_LETTER) ? WORD : NUMBER;
			}
		}

		if (ct & CT_SINGLECHAR_OP) {
			u64 match = 0;
			switch (c) {
				case '(':
				case '[':
				case '{':
					bracket_stack.push_back({ c, s.tokens.size()});
					break;
				case ')':
				case ']':
				case '}': {
					if (bracket_stack.size() == 0) {
						global.errors.push_back({Error::TOKENIZER, Error::ERROR, "unbalanced brackets"});
						return false;
					}

					BracketToken bt = bracket_stack.back();
					bracket_stack.pop_back();
					if ((c == ')' && bt.tok != '(') || (c == ']' && bt.tok != '[') || (c == '}' && bt.tok != '{')) {
						global.errors.push_back({Error::TOKENIZER, Error::ERROR, "unbalanced brackets"});
						return false;
					}
                    match = bt.tokid;
                    s.tokens[match].match = s.tokens.size();
				}
			}

			s.tokens.push_back({
				.type = TOK(c),
				.match = (u32)match,
				.length = 1,
				.start = i
			});
		}
	}

    if (bracket_stack.size() != 0)
		global.errors.push_back({Error::TOKENIZER, Error::ERROR, "unbalanced brackets"});
	return true;
}

struct TokenReader;
void unexpected_token(TokenReader& r, Token tok, TokenType expected);

struct TokenReader {
	int pos = 0;
	const SourceFile& sf;
	Context& ctx;

	Token peek() {
		if (pos >= sf.tokens.size())
			return  {};
		return sf.tokens[pos]; 
	}
	Token pop() {
		Token t = peek();
		pos++;
		return t;
	}
    Token expect(TokenType tt) {
		Token t = pop();
		if (t.type != tt) {
			unexpected_token(*this, t, tt);
			return {};
		}
        return t;
    }
    Token pop_until(TokenType tt) {
        Token t;
        do {
            t = pop();
        } while (t.type && t.type != tt);
        if (!t.type)
            unexpected_token(*this, t, TOK(';')); // unexpected eof
        return t;
    }
};

void print_tokens(const SourceFile& sf) {
	for (const Token& t : sf.tokens) {
		printf("%d\t%.*s\n", t.type, (int)t.length, sf.buffer + t.start);
	}
}

void unexpected_token(TokenReader& r, Token tok, TokenType expected) {
    // raise(SIGINT);
	char* err_msg = (char*)malloc(256);
    char* w = err_msg;

    w += sprintf(w, "unexpected ");

    if (tok.type)
	    w += sprintf(w, "'%.*s'", (int)tok.length, r.sf.buffer + tok.start);
    else
	    w += sprintf(err_msg, "eof");

    if (expected) {
        if (expected < 128)
            w += sprintf(w, " while looking for '%c'", (char)expected);
        else
            w += sprintf(w, " while looking for TOK(%d)", (int)expected);
    }
    r.ctx.global->errors.push_back({Error::TOKENIZER, Error::ERROR, err_msg});
}

void print_token(TokenReader& r, Token t) {
    if (!t.type)
        printf("[EOF]");
    else
		printf("[%d-%.*s]", t.type, (int)t.length, r.sf.buffer + t.start);
}

char* malloc_token_name(TokenReader& r, Token tok) {
    char* buf = (char*)malloc(tok.length + 1);
    memcpy(buf, r.sf.buffer + tok.start, tok.length);
    buf[tok.length] = 0;
    return buf;
}

bool skim(Context& ctx, TokenReader& r) {
	int start_pos = r.pos;

    while (true) {
        Token next = r.pop();
        switch (next.type) {

            case KW_FN: {
                Token name = r.expect(TOK_ID);
                if (!name.type)
                    return false;
                Token openBracket = r.expect(TOK('('));
                if (!openBracket.type)
                    return false;
                r.pos = openBracket.match + 1;
                Token openCurly = r.expect(TOK('{'));
                if (!openCurly.type)
                    return false;
                r.pos = openCurly.match + 1;

                ASTFn* decl = (ASTFn*)malloc(sizeof(ASTFn));
                decl->nodetype = AST_FN;
                decl->name = malloc_token_name(r, name);

                if (!ctx.define(decl->name, (ASTNode*)decl))
                    return false;
                break;
            }

            case KW_LET: {
                Token name = r.expect(TOK_ID);
                if (!name.type)
                    return false;
                Token semicolon = r.pop_until(TOK(';'));
                if (!semicolon.type)
                    return false;

                ASTVar* decl = (ASTVar*)malloc(sizeof(ASTVar));
                decl->nodetype = AST_VAR;
                decl->type = nullptr;
                decl->value = nullptr;
                decl->name = malloc_token_name(r, name);
                if (!ctx.define(decl->name, (ASTNode*)decl))
                    return false;

                break;
            }

            case TOK('}'): {
                if (ctx.is_global()) {
                    unexpected_token(r, next, TOK_NONE);
                    return false;
                } else
                    r.pos = start_pos;
                    return true;
            }

            case TOK_NONE: {
                if (ctx.is_global()) {
                    r.pos = start_pos;
                    return true;
                } else {
                    unexpected_token(r, next, TOK('}'));
                    return false;
                }
            }

            default: {
                if (ctx.is_global()) {
                    unexpected_token(r, next, TOK_NONE);
                    return false;
                }
                else {
                    Token semicolon = r.pop_until(TOK(';'));
                    if (!semicolon.type)
                        return false;
                }
            }

        }
    }
}

ASTType* parse_type(Context& ctx, TokenReader& r) {
    Token t = r.pop();

    switch (t.type) {
        case KW_U8:  return &t_u8;
        case KW_U16: return &t_u16;
        case KW_U32: return &t_u32;
        case KW_U64: return &t_u64;
        case KW_I8:  return &t_i8;
        case KW_I16: return &t_i16;
        case KW_I32: return &t_i32;
        case KW_I64: return &t_i64;
        case KW_F32: return &t_f32;
        case KW_F64: return &t_f64;
        default:
            unexpected_token(r, t, TOK_NONE);
            return nullptr;
    }
}

bool parse_type_list(Context& ctx, TokenReader& r, TokenType delim, TypeList* tl) {
    new (tl) TypeList ();

    Token t = r.peek();
    if (t.type == delim) {
        r.pop();
        return true;
    }

    while (true) {
        Token name = r.expect(TOK_ID);
        if (!t.type)
            return false;
        if (!r.expect(TOK(':')).type)
            return false;

        ASTType* type = parse_type(ctx, r);
        if (!type)
            return false;

        tl->entries.push_back(TypeList::Entry { .name = malloc_token_name(r, name), .value = type });

        t = r.peek();
        if (t.type == delim) {
            r.pop();
            return true;
        } else {
            if (!r.expect(TOK(',')).type)
                return false;
        }
    }
    return true;
}

bool parse_decl_statement(Context& ctx, TokenReader& r);

ASTBlock* parse_block(Context& parent, TokenReader& r, Context* ctx = nullptr) {

    if (!r.expect(TOK('{')).type)
        return nullptr;

    if (!ctx) {
        ctx = new Context();
        ctx->parent = &parent;
        ctx->global = parent.global;
    }
    
    if (!skim(*ctx, r))
        return nullptr;

    ASTBlock* block = new ASTBlock();
    block->nodetype = AST_BLOCK;
    block->ctx = ctx;

    while (true) {
        if (parse_decl_statement(*ctx, r))
            continue;

        if (r.peek().type == TOK('}')) {
            r.pop();
            break;
        }
        else {
            r.expect(TOK('}'));
            return nullptr;
        }
    }
    return block;
}

ASTFn* parse_fn(Context& ctx, TokenReader& r, bool decl) {
    Token name;
    if (!r.expect(KW_FN).type)
        return nullptr;

    ASTFn* fn;
    if (decl) {
        // we've already skimmed the scope, we know there's an identifier here
        name = r.expect(TOK_ID);
        char* name_ = malloc_token_name(r, name);
        fn = (ASTFn*)ctx.resolve(name_);
        free(name_);
    }
    else {
        ASTFn* decl = (ASTFn*)malloc(sizeof(ASTFn));
        decl->nodetype = AST_FN;
        decl->name = nullptr;
    }

    if (!r.expect(TOK('(')).type)
        return nullptr;

    if (!parse_type_list(ctx, r, TOK(')'), &fn->args)) {
        return nullptr;
    }

    Context* fn_ctx = new Context();
    fn_ctx->parent = &ctx;
    fn_ctx->global = ctx.global;

    fn->block = parse_block(ctx, r, fn_ctx);
    if (!fn->block)
        return nullptr;

    return fn;
}

ASTNode* parse_expr(Context& ctx, TokenReader& r, TokenType delim) {
    r.pop_until(delim);
    return (ASTNode*)&t_u32;
}

bool parse_let(Context& ctx, TokenReader& r) {
    if (!r.expect(KW_LET).type)
        return false;

    Token name = r.expect(TOK_ID);
    char* name_ = malloc_token_name(r, name);
    ASTVar* var = (ASTVar*)ctx.resolve(name_);
    free(name_);

    if (r.peek().type == TOK(':')) {
        r.pop();
        var->type = parse_type(ctx, r);
        if (!var->type)
            return false;
    }

    if (r.peek().type == TOK('='))
        return (var->value = parse_expr(ctx, r, TOK(';')));
    else 
        return r.expect(TOK(';')).type;
}

bool parse_decl_statement(Context& ctx, TokenReader& r) {
    switch (r.peek().type) {
        case KW_FN: {
            ASTFn* fn = parse_fn(ctx, r, true);
            if (!fn)
                return false;
            std::cout << fn << "\n";
            break;
        }
        case KW_LET: {
            if (!parse_let(ctx, r))
                return false;
            break;
        }
        default:
            return false;
    }
    return true;
}

bool parse_top_level(Context& ctx, TokenReader r) {
    while (parse_decl_statement(ctx, r));
    return r.peek().type == TOK_NONE;
}

bool parse_all_files(Context& global) {
    for (int i = 0; i < sources.size(); i++) {
        if (!tokenize(global, sources[i]))
            return false;
        TokenReader r { .sf = sources[i], .ctx = global };
        if (!skim(global, r))
            return false;
    }
    for (int i = 0; i < sources.size(); i++) {
        TokenReader r { .sf = sources[i], .ctx = global };
        if (!parse_top_level(global, r))
            return false;
    }
    return true;
}
