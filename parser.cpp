#include "common.h"
#include "ast.h"
#include "error.h"
#include "typer.h"
#include "sourcefile.h"

#include "keywords.gperf.gen.h"

// Convert a 'char' to TokenType
// this is not really needed, but it helps avoid stupid compiler warnings
#define TOK(c) ((TokenType)c)

// Each ASCII character has a bitmask associated with them (see ctt)
// Whne the lexer hits a character, it first looks at this bitmask
// to get a rough idea of what that character is
enum CharTraits : u8 {
	CT_ERROR               = 0x00,
	CT_LETTER              = 0x01,
	CT_DIGIT               = 0x02,
	CT_WHITESPACE          = 0x04,
	CT_OPERATOR            = 0x08,
	CT_MULTICHAR_OP_START  = 0x10,
    CT_HELPERTOKEN         = 0x20,
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
/* 33  - !   */ CT_OPERATOR,
/* 34  - "   */ CT_ERROR,
/* 35  - #   */ CT_ERROR,
/* 36  - $   */ CT_ERROR,
/* 37  - %   */ CT_OPERATOR,
/* 38  - &   */ CT_OPERATOR,
/* 39  - '   */ CT_ERROR,
/* 40  - (   */ CT_HELPERTOKEN,
/* 41  - )   */ CT_HELPERTOKEN,
/* 42  - *   */ CT_OPERATOR,
/* 43  - +   */ CT_OPERATOR,
/* 44  - ,   */ CT_HELPERTOKEN,
/* 45  - -   */ CT_OPERATOR,
/* 46  - .   */ CT_HELPERTOKEN,
/* 47  - /   */ CT_ERROR,
/* 48  - 0   */ CT_DIGIT, CT_DIGIT, CT_DIGIT, CT_DIGIT, CT_DIGIT, CT_DIGIT, CT_DIGIT, CT_DIGIT, CT_DIGIT, CT_DIGIT,
/* 58  - :   */ CT_HELPERTOKEN,
/* 59  - ;   */ CT_HELPERTOKEN,
/* 60  - <   */ CT_OPERATOR,
/* 61  - =   */ CT_OPERATOR,
/* 62  - >   */ CT_OPERATOR,
/* 63  - ?   */ CT_ERROR,
/* 64  - @   */ CT_ERROR,
/* 65  - A   */ CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER,
                CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER,
                CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER,
                CT_LETTER, CT_LETTER,
/* 91  - [   */ CT_HELPERTOKEN,
/* 92  - \   */ CT_ERROR,
/* 93  - ]   */ CT_HELPERTOKEN,
/* 94  - ^   */ CT_OPERATOR,
/* 95  - _   */ CT_LETTER, // yes, _ is a letter
/* 96  - `   */ CT_ERROR,
/* 97  - a   */ CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER,
                CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER,
                CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER, CT_LETTER,
                CT_LETTER, CT_LETTER,
/* 123 - {   */ CT_HELPERTOKEN,
/* 124 - |   */ CT_OPERATOR,
/* 125 - }   */ CT_HELPERTOKEN,
/* 126 - ~   */ CT_OPERATOR,
/* 127 -     */ CT_ERROR,
};


// An operator with precedence of 1 is right associative
#define IS_RIGHT_ASSOC(tt) (PREC(tt) == 1)

inline bool is_operator(TokenType tt) {
    // TODO this is not great
    return (tt < 128 && (ctt[tt] & CT_OPERATOR)) || (tt >= 128 && tt < 148);
}

// PRECEDENCE TABLE
// Lowest 4 bits are the precedence of an operator
// For ex. (prec[OP_SHIFTLEFT] & PREC_MASK) is the precedence of the << operator
// There's a macro PREC(OP_SHIFTLEFT) that does just that

// if the PREFIX or POSTFIX bits is set, the operator can also be postfix/prefix
u8 prec[148] = {
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* ! */        PREFIX, 
0, 0, 0,
/* % */  11,
/* & */  8   | POSTFIX,
0, 0, 0,
/* * */  11,
/* + */  10  | PREFIX,
0, // ,
/* - */  10  | PREFIX,
/* . */        POSTFIX,
/* / */  11,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* < */  5,
/* = */  1   | ASSIGNMENT,
/* > */  5,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
/* [ */        POSTFIX,
0,
/* ] */  0,
/* ^ */  7,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* | */ 6,
0, 0, 0,
/* ++ */  0,
/* -- */  0,
/* << */  9,
/* >> */  9,
/* <= */  5,
/* >= */  5,
/* == */  4,
/* && */  3,
/* || */  2,
/* != */  4,
/* += */  1 | ASSIGNMENT,
/* -= */  1 | ASSIGNMENT,
/* *= */  1 | ASSIGNMENT,
/* /=  */ 1 | ASSIGNMENT,
/* \%= */ 1 | ASSIGNMENT,
/* <<= */ 1 | ASSIGNMENT,
/* >>= */ 1 | ASSIGNMENT,
/* &= */  1 | ASSIGNMENT,
/* ^= */  1 | ASSIGNMENT,
/* |= */  1 | ASSIGNMENT,
};


bool tokenize(Context& global, SourceFile &s) {
	u64 word_start;
	enum { NONE, WORD, NUMBER } state = NONE;

	struct BracketToken {
		Token tok;
		u64 tokid;
	};
	std::vector<BracketToken> bracket_stack;

    u32 line = 0;
    u64 line_start = 0;

	for (u64 i = 0; i < s.length; i++) {
		char c = s.buffer[i];
		CharTraits ct = ctt[c];

		if (ct == CT_ERROR) {
			global.error({
                .code = ERR_UNRECOGNIZED_CHARACTER, 
                .tokens = {{
                    .length = 1,
                    .start = i,
                    .line = line,
                    .pos_in_line = (u32)(i - line_start),
                }}
            });
			return false;
		}

		if (state) {
			if (!(ct & (CT_LETTER | CT_DIGIT))) {
				tok* kw = Perfect_Hash::in_word_set(s.buffer + word_start, i - word_start);

				s.tokens.push_back({
					.type = kw ? kw->type : (state == WORD ? TOK_ID : TOK_NUMBER),
					.length = (u32)(i - word_start),
					.start = word_start,
                    .line = line,
                    .pos_in_line = (u32)(i - line_start),
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

		if (ct & (CT_OPERATOR | CT_HELPERTOKEN)) {
			u64 match = 0;

            u32 length = 1;
            tok* t;
            if (i + 2 < s.length && (t = Perfect_Hash::in_word_set(s.buffer + i, 3)))
                length = 3;
            else if (i + 1 < s.length && (t = Perfect_Hash::in_word_set(s.buffer + i, 2)))
                length = 2;

            Token tok =  {
				.type = t ? t->type : TOK(c),
				.match = (u32)match,
				.length = length,
				.start = i,
                .line = line,
                .pos_in_line = (u32)(i - line_start),
            };

			switch (c) {
				case '(': case '[': case '{': {
					bracket_stack.push_back({ tok, s.tokens.size()});
					break;
                }
				case ')': case ']': case '}': {
					if (bracket_stack.size() == 0) {
						global.error({
                            .code = ERR_UNBALANCED_BRACKETS,
                            .tokens = { tok },
                        });
						return false;
					}

					BracketToken bt = bracket_stack.back();
					bracket_stack.pop_back();
					if ((c == ')' && bt.tok.type != '(') || (c == ']' && bt.tok.type != '[') || (c == '}' && bt.tok.type != '{')) {
						global.error({
                            .code = ERR_UNBALANCED_BRACKETS,
                            .tokens = { bt.tok, tok },
                        });
						return false;
					}
                    match = bt.tokid;
                    s.tokens[match].match = s.tokens.size();
				}
			}

            s.tokens.push_back(tok);
            i += length - 1;
		}

        if (c == '\n') {
            line++;
            line_start = i;
        }
	}

    if (bracket_stack.size() != 0) {
        global.error({
            .code = ERR_UNBALANCED_BRACKETS,
            .tokens = { bracket_stack[0].tok },
        });
        return false;
    }
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
    r.ctx.error({
        .code = ERR_UNEXPECTED_TOKEN,
        .tokens = {
            tok,
            { .type = expected },
        }
    });
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
    // TODO a solution to the start_pos meme may be to pass TokenReader by value
	int start_pos = r.pos;
    int depth = 0;

    while (true) {
        Token t = r.pop();
        switch (t.type) {
            case TOK('{'):
                depth++;
                break;
            case TOK('}'):
                if (depth-- == 0) {
                    r.pos = start_pos;
                    return true;
                }
                break;
            case KW_FN: {
                if (depth == 0) {
                    // TODO we're leaking
                    Token namet = r.peek();
                    if (namet.type == TOK_ID) {
                        char* name = malloc_token_name(r, namet);
                        if (!ctx.declare(name, ctx.alloc<ASTFn>(ctx, name)))
                            return false;
                    }
                    break;
                }
            }
            case KW_LET: {
                if (depth == 0) {
                    Token namet = r.peek();
                    if (namet.type == TOK_ID) {
                        char* name = malloc_token_name(r, namet);
                        if (!ctx.declare(name, ctx.alloc<ASTVar>(name, nullptr, nullptr, 0)))
                            return false;
                    }
                }
                break;
            }
            case TOK_NONE: {
                r.pos = start_pos;
                return true;
            }
            default:
                continue;
        }

    }
    return true;
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
        case KW_BOOL: return &t_bool;
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

        tl->entries.push_back(TypeList::Entry { .name = malloc_token_name(r, name), .type = type });

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

enum trool { MORE, DONE, FAIL };
trool parse_decl_statement(Context& ctx, TokenReader& r);
ASTNode* parse_expr(Context& ctx, TokenReader& r);

bool parse_block(ASTBlock& block, TokenReader& r) {
    if (!r.expect(TOK('{')).type)
        return false;

    if (!skim(block.ctx, r))
        return false;

    while (true) {
X:
        switch (parse_decl_statement(block.ctx, r)) {
            case FAIL:
                return false;
            case DONE:
                break;
            case MORE:
                goto X;
        }
        
        switch(r.peek().type) {
            case KW_RETURN: {
                r.pop();
                ASTReturn *ret;
                if (r.peek().type == TOK(';')) {
                    ret = block.ctx.alloc<ASTReturn>(nullptr);
                }
                else {
                    ret = block.ctx.alloc<ASTReturn>(parse_expr(block.ctx, r));
                    if (!ret->value)
                        return false;
                }
                block.statements.push_back((ASTNode*)ret);
                if (!r.expect(TOK(';')).type)
                    return false;
                break;
            }
            case KW_IF: {
                ASTIf* ifs = block.ctx.alloc<ASTIf>(block.ctx);
                ifs->condition = parse_expr(block.ctx, r);
                if (!ifs->condition)
                    return false;
                if (!parse_block(ifs->block, r))
                    return false;
                block.statements.push_back(ifs);
                break;
            }
            case TOK('}'): {
                r.pop();
                return true;
            }
            default: {
                ASTNode* expr = parse_expr(block.ctx, r);
                if (!expr)
                    return false;
                if (!r.expect(TOK(';')).type)
                    return false;
                block.statements.push_back(expr);
                break;
            }
        }
    }
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
        ASTFn* decl = ctx.alloc<ASTFn>(ctx, nullptr);
    }

    if (!r.expect(TOK('(')).type)
        return nullptr;

    if (!parse_type_list(ctx, r, TOK(')'), &fn->args))
        return nullptr;

    ASTType* rettype = nullptr;

    if (r.peek().type == TOK(':')) {
        r.pop();
        rettype = parse_type(ctx, r);
        if (!rettype)
            return nullptr;
    }

    if (rettype)
        fn->block.ctx.declare("returntype", rettype);

    int argid = 0;
    for (const auto& entry : fn->args.entries) {
        ASTVar* decl = ctx.alloc<ASTVar>(entry.name, entry.type, nullptr, argid++);
        fn->block.ctx.declare(entry.name, (ASTNode*)decl);
    }

    if (!parse_block(fn->block, r))
        return nullptr;

    return fn;
}

struct SYState {
    Context& ctx;
    std::vector<ASTNode*> output;
    std::vector<TokenType> stack;
};

bool pop(SYState& s) {
    if (s.output.size() < 2) {
        s.ctx.error({
            .code = ERR_INVALID_EXPRESSION
        });
        return false;
    }
    ASTBinaryOp* bin = s.ctx.alloc<ASTBinaryOp>(
            s.stack.back(), 
            s.output[s.output.size() - 2], 
            s.output[s.output.size() - 1]);
    s.stack.pop_back();
    s.output.pop_back();
    s.output[s.output.size() - 1] = (ASTNode*)bin;
    return true;
}

ASTNode* parse_expr(Context& ctx, TokenReader& r) {
    SYState s { .ctx = ctx };
    bool prev_was_value = false;

    Token t;
    while (true) {
        t = r.pop();

        switch (t.type) {

            case TOK_ID: {
                if (prev_was_value) {
                    unexpected_token(r, t, TOK_NONE);
                    return nullptr;
                }

                ASTNode* resolved = ctx.resolve(r.sf.buffer + t.start, t.length);
                if (!resolved) {
                    s.ctx.error({
                        .code = ERR_NOT_DEFINED,
                        .tokens = { t },
                    });
                    return nullptr;
                }
                s.output.push_back(resolved);
                prev_was_value = true;
                break;
            }

            case TOK_NUMBER: {
                if (prev_was_value) {
                    unexpected_token(r, t, TOK_NONE);
                    return nullptr;
                }

                u64 acc = 0;
                for (int i = 0; i < t.length; i++) {
                    char c = r.sf.buffer[t.start + i];
                    if (c >= '0' && c <= '9') {
                        acc += c - '0';
                        acc *= 10;
                    }
                }
                acc /= 10;
                s.output.push_back(ctx.alloc<ASTNumber>(acc));
                break;
            }
            case TOK('('): {
                if (prev_was_value) {
                    // TODO Function call
                    assert(!"not impl");
                } else {
                    s.stack.push_back(t.type);
                    prev_was_value = false;
                }
                break;
            }
            case TOK(')'): {
                while (s.stack.back() != TOK('('))
                    if (!pop(s))
                        return nullptr;
                s.stack.pop_back(); // disacrd the (
                prev_was_value = true;
                break;
            }
            // TODO this is not a sane exit condition
            case TOK(';'):
            case TOK('{'):
            case TOK(','): {
                // s.ctx.global->errors.push_back({Error::PARSER, Error::ERROR, "invalid expression 1"});
                r.pos--;
                goto Done;
            }

            default: {
                prev_was_value = false;
                if (is_operator(t.type)) {
                    while (!s.stack.empty() && s.stack.back() != TOK('(') && PREC(s.stack.back()) + !IS_RIGHT_ASSOC(t.type) > PREC(t.type)) 
                        if (!pop(s))
                            return nullptr;
                    s.stack.push_back(t.type);
                    prev_was_value = false;
                }
            }
        }
    }

Done:
    while (!s.stack.empty()) 
        if (!pop(s))
            return nullptr;

    if (s.output.size() != 1) {
        s.ctx.error({
            .code = ERR_INVALID_EXPRESSION
        });
        return nullptr;
    }
    return s.output[0];
}

bool parse_let(Context& ctx, TokenReader& r) {
    if (!r.expect(KW_LET).type)
        return false;

    Token name = r.expect(TOK_ID);
    char* name_ = malloc_token_name(r, name);
    ASTVar* var = (ASTVar*)ctx.resolve(name_);
    free(name_);

    if (!r.expect(TOK(':')).type)
        return false;
    var->type = parse_type(ctx, r);
    if (!var->type)
        return false;

    if (r.peek().type == TOK('=')) {
        r.pop();
        return (var->initial_value = parse_expr(ctx, r));
    };
    return r.expect(TOK(';')).type;
}


trool parse_decl_statement(Context& ctx, TokenReader& r) {
    switch (r.peek().type) {
        case KW_FN: {
            ASTFn* fn = parse_fn(ctx, r, true);
            if (!fn)
                return FAIL;
            break;
        }
        case KW_LET: {
            if (!parse_let(ctx, r))
                return FAIL;
            break;
        }
        default:
            return DONE;
    }
    return MORE;
}

bool parse_top_level(Context& ctx, TokenReader r) {
    trool res = MORE;
    while (res == MORE)
        res = parse_decl_statement(ctx, r);
    return res == DONE;
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
