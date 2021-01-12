#include "common.h"
#include "ast.h"
#include "error.h"
#include "typer.h"
#include "cmdargs.h"

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
/* 46  - .   */ CT_OPERATOR,
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
/* . */  12,
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
	arr<BracketToken> bracket_stack;

    u32 line = 0;
    u64 line_start = 0;
    s.line_start.push(0);

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

                TokenType tt = kw ? kw->type : (state == WORD ? TOK_ID : TOK_NUMBER);

				Token t = {
					.type = tt,
					.length = (u32)(i - word_start),
					.start = word_start,
                    .line = line,
                    .pos_in_line = (u32)(word_start - line_start),
				};

                if (tt == TOK_ID) {
                    // TODO a more sane allocation is needed here
                    u64 length = i - word_start;
                    char* word = (char*)malloc(length + 1);
                    memcpy(word, s.buffer + word_start, length);
                    word[length] = 0;
                    t.name = word;
                }

                s.tokens.push(t);
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
            tok* t = nullptr;
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
					bracket_stack.push({ tok, s.tokens.size});
					break;
                }
				case ')': case ']': case '}': {
					if (bracket_stack.size == 0) {
						global.error({
                            .code = ERR_UNBALANCED_BRACKETS,
                            .tokens = { tok },
                        });
						return false;
					}

					BracketToken bt = bracket_stack.pop();
					if ((c == ')' && bt.tok.type != '(') || (c == ']' && bt.tok.type != '[') || (c == '}' && bt.tok.type != '{')) {
						global.error({
                            .code = ERR_UNBALANCED_BRACKETS,
                            .tokens = { bt.tok },
                        });
						return false;
					}
                    match = bt.tokid;
                    s.tokens[match].match = s.tokens.size;
				}
			}

            s.tokens.push(tok);
            i += length - 1;
		}

        if (c == '\n') {
            line++;
            s.line_start.push(i + 1);
            line_start = i + 1;
        }
	}

    if (bracket_stack.size != 0) {
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
	SourceFile& sf;
	Context& ctx;

	Token peek() {
		if (pos >= sf.tokens.size)
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

void print_tokens(SourceFile& sf) {
	for (Token& t : sf.tokens) {
		printf("%d\t%.*s\n", t.type, (int)t.length, sf.buffer + t.start);
	}
}

void unexpected_token(TokenReader& r, Token tok, TokenType expected) {
    // The virtual token gives more info about what was expected in place of to
    Token virt = tok;
    virt.virt = true;
    virt.type = expected;

    // Find the token before 'tok', place the virtual token after it
    for (int i = 1; i < r.sf.tokens.size; i++) {
        if (r.sf.tokens[i].start == tok.start) {
            Token prev = r.sf.tokens[i - 1];
            virt.start = prev.start + prev.length;
            virt.pos_in_line = prev.pos_in_line + prev.length;
            break;
        }
    }

    // ERR_UNEXPECTED_TOKEN expects exactly two tokens - the regular and the virtual
    r.ctx.error({
        .code = ERR_UNEXPECTED_TOKEN,
        .tokens = { tok, virt }
    });
}

void print_token(TokenReader& r, Token t) {
    if (!t.type)
        printf("[EOF]");
    else
		printf("[%d-%.*s]", t.type, (int)t.length, r.sf.buffer + t.start);
}


AST_Type* parse_type(Context& ctx, TokenReader& r) {
    Token t = r.pop();

    switch (t.type) {
        case KW_U8:   return &t_u8;
        case KW_U16:  return &t_u16;
        case KW_U32:  return &t_u32;
        case KW_U64:  return &t_u64;
        case KW_I8:   return &t_i8;
        case KW_I16:  return &t_i16;
        case KW_I32:  return &t_i32;
        case KW_I64:  return &t_i64;
        case KW_F32:  return &t_f32;
        case KW_F64:  return &t_f64;
        case KW_BOOL: return &t_bool;
        case TOK_ID:
            return (AST_Type*)ctx.try_resolve(t.name);
        default:
            unexpected_token(r, t, TOK(':'));
            return nullptr;
    }
}

bool parse_type_list(Context& ctx, TokenReader& r, TokenType delim, arr<NamedType>* tl) {
    Token t = r.peek();
    if (t.type == delim) {
        r.pop();
        return true;
    }

    while (true) {
        AST_Type* type;
        MUST (type = parse_type(ctx, r));

        Token nameToken = r.expect(TOK_ID);
        MUST (nameToken.type);

        auto& ref = tl->push({ .name = nameToken.name, .type = type });

        t = r.peek();
        if (t.type == delim) {
            r.pop();
            return true;
        } else {
            MUST (r.expect(TOK(',')).type);
        }
    }
    return true;
}

bool parse_decl_statement(Context& ctx, TokenReader& r, bool* error);
AST_Node* parse_expr(Context& ctx, TokenReader& r);

bool parse_block(AST_Block& block, TokenReader& r) {
    MUST (r.expect(TOK('{')).type);

    while (true) {
        bool err = false;
        if (parse_decl_statement(block.ctx, r, &err))
            continue;
        MUST (!err);

        switch(r.peek().type) {
            case KW_RETURN: {
                r.pop();
                AST_Return *ret;
                if (r.peek().type == TOK(';')) {
                    ret = block.ctx.alloc<AST_Return>(nullptr);
                }
                else {
                    ret = block.ctx.alloc<AST_Return>(parse_expr(block.ctx, r));
                    MUST (ret->value);
                }
                block.statements.push((AST_Node*)ret);
                MUST (r.expect(TOK(';')).type);
                break;
            }
            case KW_IF: {
                AST_If* ifs = block.ctx.alloc<AST_If>(block.ctx);
                ifs->condition = parse_expr(block.ctx, r);
                MUST (ifs->condition);
                MUST (parse_block(ifs->block, r));
                block.statements.push(ifs);
                break;
            }
            case KW_WHILE: {
                AST_While* whiles = block.ctx.alloc<AST_While>(block.ctx);
                whiles->condition = parse_expr(block.ctx, r);
                MUST (whiles->condition);
                MUST (parse_block(whiles->block, r));
                block.statements.push(whiles);
                break;
            }
            case TOK('}'): {
                r.pop();
                return true;
            }
            default: {
                AST_Node* expr = parse_expr(block.ctx, r);
                MUST (expr);
                MUST (r.expect(TOK(';')).type);
                block.statements.push(expr);
                break;
            }
        }
    }
}

AST_Fn* parse_fn(Context& ctx, TokenReader& r, bool decl) {
    assert (r.expect(KW_FN).type);

    AST_Fn* fn;
    Token nameToken;
    const char* name = nullptr;
    if (decl) {
        nameToken = r.expect(TOK_ID);
        name = nameToken.name;
        MUST (nameToken.type);
    }
    fn = ctx.alloc<AST_Fn>(ctx, name);
    if (decl) {
        MUST (ctx.declare(name, fn, nameToken));
    }

    MUST (r.expect(TOK('(')).type);
    MUST (parse_type_list(ctx, r, TOK(')'), &fn->args));

    AST_Type* rettype = nullptr;
    Token rettype_token;

    if (r.peek().type == TOK(':')) {
        r.pop();
        rettype_token = r.peek();
        MUST (rettype = parse_type(ctx, r));
    }

    if (rettype)
        fn->block.ctx.declare("returntype", rettype, rettype_token);

    int argid = 0;
    for (const auto& entry : fn->args) {
        AST_Var* decl = ctx.alloc<AST_Var>(entry.name, entry.type, nullptr, argid++);
        fn->block.ctx.declare(entry.name, (AST_Node*)decl, {});
    }

    MUST (parse_block(fn->block, r));
    return fn;
}

struct SYState {
    Context& ctx;
    u32 brackets = 0;
    arr<AST_Node*> output;
    arr<TokenType> stack;
};

bool pop(SYState& s) {
    if (s.output.size < 2) {
        s.ctx.error({
            .code = ERR_INVALID_EXPRESSION
        });
        return false;
    }

    TokenType op = s.stack.pop();

    AST_BinaryOp* bin;
    AST_Node* lhs = s.output[s.output.size - 2];
    AST_Node* rhs = s.output[s.output.size - 1];

    if (prec[op] & ASSIGNMENT) {
        if (op == '=') {
            bin = s.ctx.alloc<AST_BinaryOp>(op, lhs, rhs);
            bin->nodetype = AST_ASSIGNMENT;
        }
        else {
            switch (op) {
                case OP_ADDASSIGN:        op = TOK('+');      break;
                case OP_SUBASSIGN:        op = TOK('-');      break;
                case OP_MULASSIGN:        op = TOK('*');      break;
                case OP_DIVASSIGN:        op = TOK('/');      break;
                case OP_MODASSIGN:        op = TOK('%');      break;
                case OP_SHIFTLEFTASSIGN:  op = OP_SHIFTLEFT;  break;
                case OP_SHIFTRIGHTASSIGN: op = OP_SHIFTRIGHT; break;
                case OP_BITANDASSIGN:     op = TOK('&');      break;
                case OP_BITXORASSIGN:     op = TOK('|');      break;
                case OP_BITORASSIGN:      op = TOK('^');      break;
                default:
                                          assert(0);
            }
            rhs = s.ctx.alloc<AST_BinaryOp>(op, lhs, rhs);
            bin = s.ctx.alloc<AST_BinaryOp>(TOK('='), lhs, rhs);
            bin->nodetype = AST_ASSIGNMENT;
        }
    }
    else {
        bin = s.ctx.alloc<AST_BinaryOp>(op, lhs, rhs);
    }
    s.output.pop();
    s.output[s.output.size - 1] = (AST_Node*)bin;
    return true;
}

AST_Node* parse_expr(Context& ctx, TokenReader& r) {
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

                AST_Node* resolved = ctx.try_resolve(t.name);

                s.output.push(resolved);
                prev_was_value = true;
                break;
            }

            case TOK_NUMBER: {
                if (prev_was_value) {
                    unexpected_token(r, t, TOK_NONE);
                    return nullptr;
                }
                prev_was_value = true;

                u64 acc = 0;
                for (int i = 0; i < t.length; i++) {
                    char c = r.sf.buffer[t.start + i];
                    if (c >= '0' && c <= '9') {
                        acc += c - '0';
                        acc *= 10;
                    }
                }
                acc /= 10;
                s.output.push(ctx.alloc<AST_Number>(acc));
                break;
            }
            case TOK_OPENBRACKET: {
                if (prev_was_value) {
                    if (s.output.size == 0) {
                        s.ctx.error({ .code = ERR_INVALID_EXPRESSION });
                        return nullptr;
                    }
                    AST_FnCall* fncall = ctx.alloc<AST_FnCall>(s.output.pop());

                    while (r.peek().type != TOK(')')) {
                        AST_Node* arg = parse_expr(ctx, r);
                        MUST (arg);
                        fncall->args.push(arg);
                        if (r.peek().type != TOK(')'))
                            MUST (r.expect(TOK(',')).type);
                    }
                    r.pop(); // discard the bracket
                    s.output.push(fncall);
                } else {
                    s.stack.push(t.type);
                    prev_was_value = false;
                    s.brackets++;
                }
                break;
            }
            case TOK_CLOSEBRACKET: {
                if (s.brackets == 0) {
                    // This should only happen when we're the last argument of a fn call
                    // We rewind the bracket so the parent parse_expr can consume it
                    r.pos--;
                    goto Done;
                }
                while (s.stack.last() != TOK('('))
                    MUST (pop(s));
                s.brackets--;
                s.stack.pop(); // disacrd the (
                prev_was_value = true;
                break;
            }
            case TOK_DOT: {
                Token nameToken = r.expect(TOK_ID);
                MUST (nameToken.type);
                if (!s.output.size) {
                    s.ctx.error({ .code = ERR_INVALID_EXPRESSION });
                    return nullptr;
                }
                AST_Node* lhs = s.output.last();
                AST_MemberAccess* ma = ctx.alloc<AST_MemberAccess>(lhs, nameToken.name);
                s.output.last() = ma;
                break;
            }
            default: {
                if (is_operator(t.type)) {
                    while (s.stack.size && s.stack.last() != TOK('(') && PREC(s.stack.last()) + !IS_RIGHT_ASSOC(t.type) > PREC(t.type)) 
                        MUST (pop(s));
                    s.stack.push(t.type);
                    prev_was_value = false;
                }
                else {
                    if (prev_was_value) {
                        // Our expression is over, this token is part fo whatever comes next - don't consume it
                        r.pos --;
                        goto Done;
                    }
                    else {
                        s.ctx.error({ .code = ERR_INVALID_EXPRESSION });
                        return nullptr;
                    }
                }
            }
        }
    }

Done:
    while (s.stack.size) 
        MUST (pop(s));

    if (s.output.size != 1) {
        s.ctx.error({
            .code = ERR_INVALID_EXPRESSION
        });
        return nullptr;
    }
    return s.output[0];
}

bool parse_let(Context& ctx, TokenReader& r) {
    assert (r.expect(KW_LET).type);

    Token nameToken = r.expect(TOK_ID);
    AST_Type* type;
    AST_Value* initial_value;

    MUST (r.expect(TOK(':')).type);
    MUST (type = parse_type(ctx, r));

    if (r.peek().type == TOK('=')) {
        r.pop();
        MUST (initial_value = (AST_Value*)parse_expr(ctx, r));
    };

    AST_Var* var = ctx.alloc<AST_Var>(nameToken.name, type, initial_value, -1);
    MUST (ctx.declare(nameToken.name, var, nameToken));

    MUST (r.expect(TOK(';')).type);
    return true;
}

// TODO this is broken since skim was removed
AST_Struct* parse_struct(Context& ctx, TokenReader& r, bool decl) {
    assert (r.expect(KW_STRUCT).type);

    AST_Struct* st;
    if (decl) {
        Token nameToken = r.expect(TOK_ID);
        // we've already skimmed the scope, so we know there's an identifier here
        assert(nameToken.type); 

        st = (AST_Struct*)ctx.resolve(nameToken.name);
    }
    else {
        st = ctx.alloc<AST_Struct>(nullptr);
    }

    MUST (r.expect(TOK('{')).type);
    MUST (parse_type_list(ctx, r, TOK('}'), &st->members));

    return st;
}


bool parse_decl_statement(Context& ctx, TokenReader& r, bool* error) {
    switch (r.peek().type) {
        case KW_FN: {
            if (!parse_fn(ctx, r, true)) {
                *error = true;
                return false;
            }
            return true;
        }
        case KW_LET: {
            if (!parse_let(ctx, r)) {
                *error = true;
                return false;
            }
            return true;
        }
        case KW_STRUCT: {
            if (!parse_struct(ctx, r, true)) {
                *error = true;
                return false;
            }
            return true;
        }
        default:
            return false;
    }
}

bool parse_top_level(Context& ctx, TokenReader r) {
    bool err = false;
    while (parse_decl_statement(ctx, r, &err));
    MUST (!err);

    // parse_decl_statement should consume all tokens in the file
    // If there's anyhting left, it's an unexpected token
    if (r.peek().type) {
        unexpected_token(r, r.peek(), TOK_NONE);
        return false;
    }

    return true;
}

bool parse_all(Context& global) {
    for (int i = 0; i < sources.size; i++) {
        MUST (tokenize(global, sources[i]));
        TokenReader r { .sf = sources[i], .ctx = global };
    }
    for (int i = 0; i < sources.size; i++) {
        TokenReader r { .sf = sources[i], .ctx = global };
        MUST (parse_top_level(global, r));
    }

    return true;
}
