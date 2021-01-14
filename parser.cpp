#include "common.h"
#include "ast.h"
#include "error.h"
#include "typer.h"
#include "cmdargs.h"
#include "typer.h"

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
/* 34  - "   */ CT_HELPERTOKEN,
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
/* 47  - /   */ CT_HELPERTOKEN,
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

Location location_of(Context& ctx, AST_Node** node) {
    Location loc;
    if (ctx.global->node_locations.find(*node, &loc)) {
        return loc;
    } else if (ctx.global->node_ptr_locations.find(node, &loc)) {
        return loc;
    }
    assert(!"AST_Node doesn't have a location");
}

struct TokenReader;
void unexpected_token(Context& ctx, Token actual, TokenType expected);

bool tokenize(Context& global, SourceFile &s) {
	u64 word_start;
	enum { NONE, WORD, NUMBER } state = NONE;

	struct BracketToken {
        TokenType bracket;
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
            Token newTok = s.pushToken(
                { TOK_ERROR },
                { .start = i, .end = i + 1 });

			global.error({
                .code = ERR_UNEXPECTED_TOKEN, 
                .tokens = { newTok }
            });
			return false;
		}

		if (state) {
			if (!(ct & (CT_LETTER | CT_DIGIT))) {

				tok* kw = Perfect_Hash::in_word_set(s.buffer + word_start, i - word_start);
                TokenType tt = kw ? kw->type : (state == WORD ? TOK_ID : TOK_NUMBER);

                char* name = nullptr;

                if (tt == TOK_ID) {
                    // TODO ALLOCATION
                    u64 length = i - word_start;
                    name = (char*)malloc(length + 1);
                    memcpy(name, s.buffer + word_start, length);
                    name[length] = 0;
                }

                 s.pushToken(
                    { .type = tt, .name = name },
                    { .start = word_start, .end = i });

				state = NONE;
			}
		}
		else {
			if (ct & (CT_LETTER | CT_DIGIT)) {
				word_start = i;
				state = (ct & CT_LETTER) ? WORD : NUMBER;
			}
		}

        if (c == '"') {
            word_start = i + 1;

            u64 string_start_line = line;
            // Look for the matching "
            do {
                i++;
                if (s.buffer[i] == '\n') {
                    assert(!"Multiline string Not implemented");
                }
            } while (s.buffer[i] != '"' && i < s.length);

            long length = i - word_start;

            // Unexpected EOF
            // TODO ERROR
            if (i == s.length) {
                global.error({ .code = ERR_UNEXPECTED_TOKEN });
            }

            char* contents = (char*)malloc(length);
            memcpy(contents, s.buffer + word_start, length);
            
            s.pushToken(
                { .type = TOK_STRING_LITERAL, .name = contents },
                { .start = word_start, .end = i });
        }

        else if (c == '/' && i < s.length - 1 && s.buffer[i + 1] == '/') {
            while (s.buffer[i] != '\n' && i < s.length)
                i++;
            // Set the char to '\n', so the line can get incremented a few paragraphs down
            if (i < s.length)
                c = '\n';
        } 

        else if (ct & (CT_OPERATOR | CT_HELPERTOKEN)) {
            u32 length = 1;

            // We used to use this for skimming
            // Right now we don't,
            // but it's still nice to catch unbalanced brakcets during lexing
            u64 match = 0;

            // Look for two and three char-long operators
            tok* t = nullptr;
            if (i + 2 < s.length && (t = Perfect_Hash::in_word_set(s.buffer + i, 3)))
                length = 3;
            else if (i + 1 < s.length && (t = Perfect_Hash::in_word_set(s.buffer + i, 2)))
                length = 2;

            /*
            Token tok =  {
				.type = t ? t->type : TOK(c),
				.match = (u32)match,
				.length = length,
				.start = i,
                .line = line,
                .pos_in_line = (u32)(i - line_start),
            };
            */

			switch (c) {
				case '(': case '[': case '{': {
					bracket_stack.push({ TOK(c), s._tokens.size});
					break;
                }
				case ')': case ']': case '}': {
					if (bracket_stack.size == 0) {
                        // TODO ERROR
						global.error({
                            .code = ERR_UNBALANCED_BRACKETS,
                            // .tokens = { tok },
                        });
						return false;
					}

					BracketToken bt = bracket_stack.pop();
					if ((c == ')' && bt.bracket != '(') || (c == ']' && bt.bracket != '[') || (c == '}' && bt.bracket != '{')) {
                        // TODO ERROR
						global.error({
                            .code = ERR_UNBALANCED_BRACKETS,
                            // .tokens = { bt.tok },
                        });
						return false;
					}
                    match = bt.tokid;
				}
			}

            s.pushToken(
                { t ? t->type : TOK(c) },
                { .start = i, .end = i + length });

            // i is getting incremented by 1 anyways
            // if we have a 3 char token, we need to further increment it by 2
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
            // TODO ERROR
            // .tokens = { bracket_stack[0].tok },
        });
        return false;
    }
	return true;
}


struct TokenReader {
	u64 pos = 0;
    u64 pos_in_file;

	SourceFile& sf;
	Context& ctx;

	SmallToken peek() {
		if (pos >= sf._tokens.size)
			return  {};
		return sf._tokens[pos]; 
	}
    
	Token peek_full() {
		if (pos >= sf._tokens.size)
			return  {};
		return sf.getToken(pos); 
	}

	SmallToken pop() {
		if (pos >= sf._tokens.size)
			return  {};
        pos_in_file = sf._token_locations[pos].end;
		return sf._tokens[pos++]; 
	}

	Token pop_full() {
		if (pos >= sf._tokens.size)
			return  {};
		Token t = sf.getToken(pos++); 
        pos_in_file = t.loc.end;
        return t;
	}

    SmallToken expect(TokenType tt) {
		SmallToken t = pop();

		if (t.type != tt) {
            Token t = sf.getToken(pos - 1);
			unexpected_token(ctx, t, tt);
			return {};
		}
        return t;
    }

    Token expect_full(TokenType tt) {
		Token t = pop_full();

		if (t.type != tt) {
			unexpected_token(ctx, t, tt);
			return {};
		}
        return t;
    }

};


void unexpected_token(Context& ctx, Token actual, TokenType expected) {
    // The virtual token gives more info about what was expected in place of to
    Token virt = actual;
    virt.virt = true;
    virt.type = expected;

    SourceFile& sf = sources[actual.file_id];

    // Find the token before 'actual', place the virtual token after it


    // ERR_UNEXPECTED_TOKEN expects exactly two tokens - the regular and the virtual
    // TODO ERROR
    ctx.error({
        .code = ERR_UNEXPECTED_TOKEN,
        // .tokens = { tok, virt }
    });
}


bool parse_type(Context& ctx, TokenReader& r, AST_Type** out, bool save_range = true) {
    Token t = r.pop_full();

    switch (t.type) {
        case KW_U8:   *out = &t_u8;   break;
        case KW_U16:  *out = &t_u16;  break;
        case KW_U32:  *out = &t_u32;  break;
        case KW_U64:  *out = &t_u64;  break;
        case KW_I8:   *out = &t_i8;   break;
        case KW_I16:  *out = &t_i16;  break;
        case KW_I32:  *out = &t_i32;  break;
        case KW_I64:  *out = &t_i64;  break;
        case KW_F32:  *out = &t_f32;  break;
        case KW_F64:  *out = &t_f64;  break;
        case KW_BOOL: *out = &t_bool; break;

        case '*': {
            AST_Type* pointed_type;
            MUST (parse_type(ctx, r, &pointed_type, false));

            *out = get_pointer_type(pointed_type);
            break;
        }

        case TOK_ID: {
            AST_UnresolvedId* id = ctx.alloc_temp<AST_UnresolvedId>(t.name, ctx);
            break;
        }

        default:
            unexpected_token(ctx, t, TOK(':'));
            return false;
    }

    if (save_range) {
        ctx.global->node_ptr_locations[(AST_Node**)out] = {
            .file_id = r.sf.id,
            .loc = {
                .start = t.loc.start,
                .end = r.pos_in_file,
            }
        };
    }
    return true;
}

bool parse_type_list(Context& ctx, TokenReader& r, TokenType delim, arr<NamedType>* tl) {
    SmallToken t = r.peek();
    if (t.type == delim) {
        r.pop();
        return true;
    }

    while (true) {
        AST_Type* type;
        MUST (parse_type(ctx, r, &type, false));

        SmallToken nameToken = r.expect(TOK_ID);
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
AST_Value* parse_expr(Context& ctx, TokenReader& r);

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
                r.pop();
                AST_If* ifs = block.ctx.alloc<AST_If>(block.ctx);
                ifs->condition = parse_expr(block.ctx, r);
                MUST (ifs->condition);
                MUST (parse_block(ifs->then_block, r));
                block.statements.push(ifs);
                break;
            }
            case KW_WHILE: {
                r.pop();
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
    Token fn_kw = r.expect_full(KW_FN);

    // This only ever gets called if the 'struct' keyword
    // has been peeked, no need for MUST check
    assert (fn_kw.type);

    AST_Fn* fn;
    Token nameToken;
    const char* name = nullptr;

    if (decl) {
        nameToken = r.expect_full(TOK_ID);
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
        rettype_token = r.peek_full();
        MUST (parse_type(ctx, r, &rettype));
    }

    if (rettype)
        fn->block.ctx.declare("returntype", rettype, rettype_token);

    int argid = 0;
    for (const auto& entry : fn->args) {
        AST_Var* decl = ctx.alloc<AST_Var>(entry.name, argid++);
        decl->type = entry.type;

        fn->block.ctx.declare(entry.name, (AST_Node*)decl, {});
    }

    // If it's a declaration without body, it's an extern fn
    if (decl) {
        if (r.peek().type == ';') {
            r.pop();
            fn->is_extern = true;
        }
    }

    if (!fn->is_extern) {
        MUST (parse_block(fn->block, r));
    }

    ctx.global->node_locations[fn] = {
        .file_id = r.sf.id,
        .loc = {
           .start = fn_kw.loc.start,
           .end = r.pos_in_file,
        }
    };

    return fn;
}

struct SYState {
    Context& ctx;
    u32 brackets = 0;
    arr<AST_Value*> output;
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
    s.output[s.output.size - 1] = bin;

    Location lhsloc = location_of(s.ctx, &bin->lhs);
    Location rhsloc = location_of(s.ctx, &bin->rhs);

    s.ctx.global->node_locations[bin] = {
        .file_id = lhsloc.file_id,
        .loc = {
            lhsloc.loc.start,
            rhsloc.loc.end,
        }
    };

    return true;
}

AST_Value* parse_expr(Context& ctx, TokenReader& r) {
    SYState s { .ctx = ctx };
    bool prev_was_value = false;

    Token t;
    while (true) {
        t = r.pop_full();

        switch (t.type) {
            case TOK_ID: {
                if (prev_was_value) {
                    unexpected_token(r.ctx, t, TOK_NONE);
                    return nullptr;
                }
                prev_was_value = true;

                AST_UnresolvedId* id = ctx.alloc_temp<AST_UnresolvedId>(t.name, ctx);
                ctx.global->node_locations[id] = {
                    .file_id = r.sf.id,
                    .loc = t.loc,
                };
                s.output.push(id);
                break;
            }

            case TOK_NUMBER: {
                if (prev_was_value) {
                    unexpected_token(r.ctx, t, TOK_NONE);
                    return nullptr;
                }
                prev_was_value = true;

                s.output.push(ctx.alloc<AST_Number>(t.u64_val));
                break;
            }

            case TOK_OPENBRACKET: {
                // Value followed by a open brackets means function call
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
                } 
                
                // If previous isn't a value, do the shunting yard thing
                else {
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
                Token nameToken = r.expect_full(TOK_ID);
                MUST (nameToken.type);

                if (s.output.size == 0) {
                    s.ctx.error({ .code = ERR_INVALID_EXPRESSION });
                    return nullptr;
                }
                AST_Node* lhs = s.output.last();
                AST_MemberAccess* ma = ctx.alloc<AST_MemberAccess>(lhs, nameToken.name);
                s.output.last() = ma;
                break;
            }

            case '[': {
                AST_Node* index = parse_expr(ctx, r);
                MUST (index);
                MUST (r.expect(TOK(']')).type);

                if (s.output.size == 0) {
                    s.ctx.error({ .code = ERR_INVALID_EXPRESSION });
                    return nullptr;
                }

                u64 loc_start = location_of(s.ctx, (AST_Node**)&s.output.last()).loc.start;

                s.output.last() = ctx.alloc<AST_BinaryOp>(TOK('+'), s.output.last(), index);
                s.output.last() = ctx.alloc<AST_Dereference>((AST_Value*)s.output.last());

                ctx.global->node_locations[s.output.last()] = {
                    .file_id = r.sf.id,
                    .loc = {
                        .start = loc_start,
                        .end = r.pos_in_file,
                    }
                };
                break;
            }
            
            case '&': {
                if (s.output.size == 0) {
                    s.ctx.error({ .code = ERR_INVALID_EXPRESSION });
                    return nullptr;
                }

                AST_Node* top = s.output.last();

                if (top->nodetype == AST_DEREFERENCE) {
                    // TODO ALLOCATION FREE we're leaking the Dereference node here

                    // foo*& is the same as x
                    // we hadnle this here because foo[123] is expanded to (foo + 123)* (look the '[' case on this switch)
                    // and without this foo[123]& would become (foo + 123)*& which is ridiculous
                    s.output.last() = ((AST_Dereference*)top)->ptr;
                } else {
                    u64 loc_start = location_of(ctx, (AST_Node**)&s.output.last()).loc.start;

                    s.output.last() = ctx.alloc<AST_AddressOf>((AST_Value*)top);

                    ctx.global->node_locations[s.output.last()] = {
                        .file_id = r.sf.id,
                        .loc = {
                            .start = loc_start,
                            .end = r.pos_in_file
                        }
                    };
                }

                break;
            }

            default: {
                if (is_operator(t.type)) {

                    // * can be either postfix, in which case it means dereference
                    // or be infix, in which case it means multiplication
                    if (t.type == '*') {
                        TokenType next = r.peek().type;

                        // If the next token type is id/number/open bracket, the * is infix
                        // in all other cases it is postfix
                        if (next != TOK_OPENBRACKET && next != TOK_ID && next != TOK_NUMBER) {
                            if (s.output.size == 0) {
                                s.ctx.error({ .code = ERR_INVALID_EXPRESSION });
                                return nullptr;
                            }
                            s.output.last() = ctx.alloc<AST_Dereference>((AST_Value*)s.output.last());
                            break;
                        }
                    }

                    while (s.stack.size && s.stack.last() != TOK('(') 
                            && PREC(s.stack.last()) + !IS_RIGHT_ASSOC(t.type) > PREC(t.type)) 
                    {
                        MUST (pop(s));
                    }

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
    Token let_tok = r.expect_full(KW_LET);
    MUST (let_tok.type);

    Token nameToken = r.expect_full(TOK_ID);
    MUST (nameToken.type);

    AST_Var* var = ctx.alloc<AST_Var>(nameToken.name, -1);

    MUST (r.expect(TOK(':')).type);
    MUST (parse_type(ctx, r, &var->type));

    if (r.peek().type == TOK('=')) {
        r.pop();
        MUST (var->initial_value = (AST_Value*)parse_expr(ctx, r));
    };

    MUST (ctx.declare(nameToken.name, var, nameToken));

    MUST (r.expect(TOK(';')).type);

    ctx.global->node_locations[var] = {
        .file_id = r.sf.id,
        .loc = {
            .start = let_tok.loc.start,
            .end = r.pos_in_file,
        }
    };

    return true;
}

AST_Struct* parse_struct(Context& ctx, TokenReader& r, bool decl) {
    Token struct_kw = r.expect_full(KW_STRUCT);

    // This only ever gets called if the 'struct' keyword
    // has been peeked, no need for MUST check
    assert (struct_kw.type);

    AST_Struct* st;
    if (decl) {
        // TODO this is broken since skim was removed
        Token nameToken = r.expect_full(TOK_ID);
        MUST (nameToken.type); 

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
        unexpected_token(r.ctx, r.peek_full(), TOK_NONE);
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
