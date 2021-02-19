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
/* 47  - /   */ (CharTraits)(CT_OPERATOR | CT_HELPERTOKEN),
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
/* + */  9  | PREFIX,
0, // ,
/* - */  9  | PREFIX,
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
/* << */  10,
/* >> */  10,
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

Location location_of(AST_Context& ctx, AST_Node** node) {
    Location loc;
    if (ctx.global->reference_locations.find(node, &loc)) {
        return loc;
    } else if (ctx.global->definition_locations.find(*node, &loc)) {
        return loc;
    } 
    DIE("The AST_Node doesn't have a location");
}

struct TokenReader;
void unexpected_token(AST_Context& ctx, Token actual, TokenType expected);



NumberData* parse_number(AST_Context& ctx, char* start, char* end) {
    // TODO ALLOCATION
    NumberData* data = new NumberData();

    u64 acc[2] = { 0, 0 };
    u8 acc_index = 0;

    for (char* i = start; i != end; i++) {
        char ch = *i;

        if (ch >= '0' && ch <= '9') {
            if (acc[acc_index])
                acc[acc_index] *= 10;

            acc[acc_index] += ch - '0';
        }
        else if (ch == '.') {
            // Multiple decimal places
            if (acc_index) {
                ctx.error({
                    .code = ERR_INVALID_NUMBER_FORMAT
                });
                return nullptr;
            }
            acc_index ++;
        }
    }

    if (acc_index) {
        data->is_float = true;
        NOT_IMPLEMENTED();
        //data->f64_data = (double)data0] / (double)acc_index[1];
    }
    else {
        data->is_float = false;
        data->u64_data = acc[0];
    }

    return data;
}


bool tokenize(AST_Context& global, SourceFile &s) {
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

            unexpected_token(global, newTok, TOK_NONE);
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
                else if (tt == TOK_NUMBER) {
                    name = (char*)parse_number(global, s.buffer + word_start, s.buffer + i);
                    if (!name)
                        return false;
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

        arr<char> string_literals;

        if (c == '"') {
            word_start = i + 1;

            u64 string_start_line = line;
            // Look for the matching "
            while (true) {
                i++;
                if (i == s.length) {
                    NOT_IMPLEMENTED("TODO ERROR");
                }

                switch (s.buffer[i]) {
                    case '\\': {
                        i++;
                        if (i == s.length) NOT_IMPLEMENTED("TODO ERROR");
                        
                        switch (s.buffer[i]) {
                            case 'a':  string_literals.push('\a'); break;
                            case 'b':  string_literals.push('\b'); break;
                            case 'f':  string_literals.push('\f'); break;
                            case 'n':  string_literals.push('\n'); break;
                            case 'r':  string_literals.push('\r'); break;
                            case 't':  string_literals.push('\t'); break;
                            case 'v':  string_literals.push('\v'); break;
                            default:
                                string_literals.push(s.buffer[i]);
                                break;
                        }

                        break;
                    }
                    case '"': {
                        goto Done;
                    }
                    default: {
                        string_literals.push(s.buffer[i]);
                        break;
                    }
                }
            } while (true);

Done:
            // TODO remove this
            string_literals.push(0);
            s.pushToken(
                { .type = TOK_STRING_LITERAL, .name = string_literals.release().buffer },
                { .start = word_start, .end = i + 1 });
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
			switch (c) {
				case '(': case '[': case '{': {
					bracket_stack.push({ TOK(c), s._tokens.size});
					break;
                }
				case ')': case ']': case '}': {
					if (bracket_stack.size == 0) {
                        // TODO ERROR
						global.error({ .code = ERR_UNBALANCED_BRACKETS, });
						return false;
					}

					BracketToken bt = bracket_stack.pop();
					if ((c == ')' && bt.bracket != '(') 
                            || (c == ']' && bt.bracket != '[') 
                            || (c == '}' && bt.bracket != '{')) 
                    {
                        // TODO ERROR
						global.error({ .code = ERR_UNBALANCED_BRACKETS, });
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
            s.line_start.push((u32)(i + 1));
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
	AST_Context& ctx;

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


void unexpected_token(AST_Context& ctx, Token actual, TokenType expected_tt) {
    Token expected = actual;
    expected.type = expected_tt;

    // ERR_UNEXPECTED_TOKEN expects exactly two tokens - the actual token and the expected
    ctx.error({
        .code = ERR_UNEXPECTED_TOKEN,
        .tokens = { actual, expected }
    });
}

bool parse_decl_statement(AST_Context& ctx, TokenReader& r, bool* error);
AST_Value* parse_expr(AST_Context& ctx, TokenReader& r, TokenType delim = TOK_NONE);

bool parse_type_list(AST_Context& ctx, TokenReader& r, TokenType delim, arr<StructElement>* tl) {
    SmallToken t = r.peek();
    if (t.type == delim) {
        r.pop();
        return true;
    }

    while (true) {
        SmallToken nameToken = r.expect(TOK_ID);
        MUST (nameToken.type);

        MUST (r.expect(TOK(':')).type);

        AST_Value* type = parse_expr(ctx, r);
        MUST (type);

        auto& ref = tl->push({ .name = nameToken.name, .type = (AST_Type*)type });

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

bool parse_block(AST_Context& block, TokenReader& r);

bool parse_scope(AST_Context& block, TokenReader& r, TokenType delim) {
    while (true) {
        bool err = false;
        if (parse_decl_statement(block, r, &err))
            continue;
        MUST (!err);

        switch(r.peek().type) {
            case KW_RETURN: {
                Token return_tok = r.pop_full();

                AST_Return *ret;
                if (r.peek().type == TOK(';')) {
                    ret = block.alloc<AST_Return>(nullptr);
                }
                else {
                    ret = block.alloc<AST_Return>(parse_expr(block, r));
                    MUST (ret->value);
                }
                block.statements.push((AST_Node*)ret);

                r.ctx.global->definition_locations[ret] = {
                    .file_id = r.sf.id,
                    .loc = {
                        .start = return_tok.loc.start,
                        .end = r.pos_in_file,
                    }
                };

                MUST (r.expect(TOK(';')).type);
                break;
            }
            case KW_IF: {
                Token if_tok = r.pop_full();

                AST_If* ifs = block.alloc<AST_If>(&block);
                ifs->condition = parse_expr(block, r);
                MUST (ifs->condition);
                MUST (parse_block(ifs->then_block, r));
                block.statements.push(ifs);

                r.ctx.global->definition_locations[ifs] = {
                    .file_id = r.sf.id,
                    .loc = {
                        .start = if_tok.loc.start,
                        .end = r.pos_in_file,
                    }
                };

                break;
            }
            case KW_WHILE: {
                Token while_tok = r.pop_full();

                AST_While* whiles = block.alloc<AST_While>(&block);
                whiles->condition = parse_expr(block, r);
                MUST (whiles->condition);
                MUST (parse_block(whiles->block, r));
                block.statements.push(whiles);

                r.ctx.global->definition_locations[whiles] = {
                    .file_id = r.sf.id,
                    .loc = {
                        .start = while_tok.loc.start,
                        .end = r.pos_in_file,
                    }
                };

                break;
            }
            default: {
                if (delim == r.peek().type) {
                    r.pop();
                    // TODO this is wrong
                    // First of all it breaks on multiple files because global scope
                    // is closed when it shouldnt be
                    // And it won't work for macroexpand alter on
                    ScopeClosedMessage msg;
                    msg.msgtype = MSG_SCOPE_CLOSED;
                    msg.scope = &block;
                    block.global->send_message(&msg);
                    return true;
                }
                AST_Node* expr = parse_expr(block, r);
                MUST (expr);
                MUST (r.expect(TOK(';')).type);
                block.statements.push(expr);
                break;
            }
        }
    }
}

bool parse_block(AST_Context& block, TokenReader& r) {
    MUST (r.expect(TOK('{')).type);
    MUST (parse_scope(block, r, TOK('}')));
    return true;
}

AST_Fn* parse_fn(AST_Context& ctx, TokenReader& r, bool decl) {
    Token fn_kw = r.expect_full(KW_FN);

    // This only ever gets called if the 'struct' keyword
    // has been peeked, no need for MUST check
    assert (fn_kw.type);

    Token nameToken;
    const char* name = nullptr;

    if (decl) {
        nameToken = r.expect_full(TOK_ID);
        name = nameToken.name;
        MUST (nameToken.type);
    }

    AST_Fn* fn = ctx.alloc<AST_Fn>(&ctx, name);
    AST_FnType* temp_fn_type = ctx.alloc_temp<AST_FnType>(ctx.global->target.pointer_size);

    fn->type = temp_fn_type;
    fn->block.fn = fn;

    if (decl) {
        MUST (ctx.declare({ name }, fn, true));
    }

    MUST (r.expect(TOK('(')).type);

    // Parse the function arguments
    while (r.peek().type != TOK_CLOSEBRACKET) {

        if (r.peek().type == OP_VARARGS) {
            r.pop();
            temp_fn_type->is_variadic = true;
            break;
        }

        SmallToken nameToken = r.expect(TOK_ID);
        MUST (nameToken.type);

        MUST (r.expect(TOK(':')).type);

        AST_Value* type = parse_expr(ctx, r);
        MUST (type);

        temp_fn_type->param_types.push((AST_Type*)type);
        fn->argument_names.push(nameToken.name);
        
        TokenType p = r.peek().type;

        if (p == TOK_CLOSEBRACKET) {
            break;
        } else if (p == TOK(',')) {
            r.pop();
        } else {
            unexpected_token(ctx, r.pop_full(), TOK(','));
            return nullptr;
        }
    }

    MUST (r.expect(TOK(')')).type);

    // Parse the return type
    if (r.peek().type == TOK(':')) {
        r.pop();
        MUST (temp_fn_type->returntype = (AST_Type*)parse_expr(ctx, r));
    }

    // If it's a declaration without body, it's implictly extern
    if (decl) {
        if (r.peek().type == ';') {
            r.pop();
            fn->is_extern = true;
        }
    }

    if (!fn->is_extern) {
        MUST (parse_block(fn->block, r));
    }

    ctx.global->definition_locations[fn] = {
        .file_id = r.sf.id,
        .loc = {
           .start = fn_kw.loc.start,
           .end = r.pos_in_file,
        }
    };

    return fn;
}

struct ParseExprValue {
    AST_Value *val;
    Location   loc;
};

struct ParseExprState {
    AST_Context &ctx;
    u32 brackets = 0;
    arr<ParseExprValue> _output;
    arr<TokenType> stack;

    ParseExprValue pop_into(AST_Value **out) {
        ParseExprValue val = _output.pop();
        *out = val.val;

        if (val.val IS AST_UNRESOLVED_ID) {
            ResolveJob *resolve_job = new ResolveJob((AST_UnresolvedId**)out, &ctx);
            resolve_job->subscribe(MSG_NEW_DECLARATION);
            resolve_job->subscribe(MSG_SCOPE_CLOSED);
        }

        return val;
    };
};


bool pop_operator(ParseExprState& state) {
    if (state._output.size < 2) {
        state.ctx.error({
            .code = ERR_INVALID_EXPRESSION
        });
        return false;
    }

    TokenType op = state.stack.pop();

    AST_BinaryOp* bin;
    ParseExprValue lhs, rhs;

    if (prec[op] & ASSIGNMENT) {
        if (op == '=') {
            bin = state.ctx.alloc<AST_BinaryOp>(op, nullptr, nullptr);
            rhs = state.pop_into(&bin->rhs);
            lhs = state.pop_into(&bin->lhs);
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
                    UNREACHABLE;
            }

            NOT_IMPLEMENTED();
            //rhs.val = state.ctx.alloc<AST_BinaryOp>(op, lhs.val, rhs.val);
            //bin = state.ctx.alloc<AST_BinaryOp>(TOK('='), lhs.val, rhs.val);
            //bin->nodetype = AST_ASSIGNMENT;
        }
    }
    else {
        bin = state.ctx.alloc<AST_BinaryOp>(op, nullptr, nullptr);
        rhs = state.pop_into(&bin->rhs);
        lhs = state.pop_into(&bin->lhs);
    }

    Location loc = {
        .file_id = lhs.loc.file_id,
        .loc = {
            lhs.loc.loc.start,
            rhs.loc.loc.end,
        }
    };
    state.ctx.global->definition_locations[bin] = loc;
    state._output.push({ bin, loc });

    return true;
}

bool _token_is_value(TokenType t) {
    if (IS_TYPE_KW(t))
        return true;
    if (t == TOK_ID || t == TOK_NUMBER || t == TOK_STRING_LITERAL || t == '(')
        return true;
    return false;
}

AST_Value* parse_expr(AST_Context& ctx, TokenReader& r, TokenType delim) {
    ParseExprState state { .ctx = ctx };

    Token t;
    bool prev_was_value = false;

    while (true) {
        t = r.pop_full();

        Location kwloc = {
            .file_id = r.sf.id,
            .loc = t.loc,
        };

        switch (t.type) {
            case TOK_ID:
            case TOK_NUMBER:
            case TOK_STRING_LITERAL: 
            {
                if (prev_was_value) {
                    unexpected_token(r.ctx, t, TOK_NONE);
                    return nullptr;
                }
                prev_was_value = true;

                AST_Value* val = nullptr;

                switch (t.type) {
                    case TOK_ID: {
                        if (!ctx.declarations.find({ .name = t.name }, (AST_Node**)&val))
                            val = ctx.alloc_temp<AST_UnresolvedId>(t.name, ctx);
                        break;
                    }
                    case TOK_NUMBER: {
                        val = ctx.alloc<AST_Number>(t.number_data->u64_data);
                        break;
                    }
                    case TOK_STRING_LITERAL: {
                        if (!ctx.global->literals.find(t.name, (AST_StringLiteral**)&val)) {
                            val = ctx.alloc<AST_StringLiteral>(t);
                            ctx.global->literals.insert(t.name, (AST_StringLiteral*)val);
                        }
                        break;
                    }
                    default:
                        UNREACHABLE;
                }

                Location loc = {
                    .file_id = r.sf.id,
                    .loc = t.loc,
                };

                // TODO ALLOCATION we should not be storing the node locations
                // for the unresolved IDs here, as they'll get discarded
                if (!ctx.global->definition_locations.find(val, nullptr))
                    ctx.global->definition_locations[val] = loc;

                state._output.push({val, loc });
                break;
            }

            case KW_TYPEOF: {
                MUST (r.expect(TOK('(')).type);
                AST_Value *inner = parse_expr(ctx, r, TOK(')'));
                MUST (r.expect(TOK(')')).type);

                Location loc = {
                    .file_id = t.file_id,
                    .loc = {
                        .start = t.loc.start,
                        .end = r.pos_in_file,
                    }
                };

                AST_Typeof* ast_typeof = ctx.alloc<AST_Typeof>(inner);
                ctx.global->definition_locations[ast_typeof] = loc;

                state._output.push({ ast_typeof, loc });
                break;
            }

            case TOK_OPENBRACKET: {
                // Value followed by a open brackets means function call
                if (prev_was_value) {
                    if (state._output.size == 0) {
                        state.ctx.error({ .code = ERR_INVALID_EXPRESSION });
                        return nullptr;
                    }

                    AST_FnCall* fncall = ctx.alloc<AST_FnCall>(nullptr);
                    ParseExprValue fn = state.pop_into(&fncall->fn);

                    arr<Location> arg_locs;

                    while (r.peek().type != TOK(')')) {
                        AST_Value* arg = parse_expr(ctx, r);
                        MUST (arg);
                        fncall->args.push(arg);

                        Location argloc = location_of(ctx, (AST_Node**)&arg);
                        arg_locs.push(argloc);

                        if (r.peek().type != TOK(')'))
                            MUST (r.expect(TOK(',')).type);
                    }
                    r.pop(); // discard the closing bracket

                    for (u32 i = 0; i < arg_locs.size; i++) {
                        ctx.global->reference_locations[(AST_Node**)&fncall->args[i]] = arg_locs[i];
                    }

                    Location loc = {
                        .file_id = fn.loc.file_id,
                        .loc = {
                            .start = fn.loc.loc.start,
                            .end = r.pos_in_file,
                        }
                    };

                    r.ctx.global->definition_locations[fncall] = loc;
                    state._output.push({ fncall, loc });
                }
                
                // If previous isn't a value, do the shunting yard thing
                else {
                    prev_was_value = false;
                    state.stack.push(t.type);
                    state.brackets++;
                }
                break;
            }

            case TOK_CLOSEBRACKET: {
                if (state.brackets == 0) {
                    // This should only happen when we're the last argument of a fn call
                    // We rewind the bracket so the parent parse_expr can consume it
                    r.pos--;
                    goto Done;
                }
                while (state.stack.last() != TOK('('))
                    MUST (pop_operator(state));
                state.brackets--;
                state.stack.pop(); // disacrd the (
                prev_was_value = true;
                break;
            }

            case TOK_DOT: {
                Token nameToken = r.expect_full(TOK_ID);
                MUST (nameToken.type);

                if (state._output.size == 0) {
                    state.ctx.error({ .code = ERR_INVALID_EXPRESSION });
                    return nullptr;
                }

                AST_MemberAccess* ma = ctx.alloc<AST_MemberAccess>(nullptr, nameToken.name);
                ParseExprValue lhs = state.pop_into(&ma->lhs);

                lhs.loc.loc.end = r.pos_in_file;

                state._output.push({ ma, lhs.loc });
                break;
            }

            case TOK_OPENSQUARE: { AST_Value* index = parse_expr(ctx, r);
                MUST (index);
                MUST (r.expect(TOK(']')).type);

                if (state._output.size == 0) {
                    state.ctx.error({ .code = ERR_INVALID_EXPRESSION });
                    return nullptr;
                }


                // When we see foo[bar] we output (foo + bar)*
                AST_BinaryOp* add = ctx.alloc<AST_BinaryOp>(TOK('+'), nullptr, index);
                ParseExprValue inner = state.pop_into(&add->lhs);
                AST_Dereference* deref = ctx.alloc<AST_Dereference>(add);

                Location loc = {
                    .file_id = r.sf.id,
                    .loc = {
                        .start = inner.loc.loc.start,
                        .end = r.pos_in_file,
                    }
                };

                state._output.push({ deref, loc });
                ctx.global->definition_locations[deref] = loc;
                break;
            }
            
            case TOK_AMPERSAND: {
                if (state._output.size == 0) {
                    state.ctx.error({ .code = ERR_INVALID_EXPRESSION });
                    return nullptr;
                }

                AST_AddressOf *addrof = ctx.alloc<AST_AddressOf>(nullptr);
                ParseExprValue syval = state.pop_into(&addrof->inner);

                Location loc = {
                    .file_id = syval.loc.file_id,
                    .loc = {
                        .start = syval.loc.loc.start,
                        .end = r.pos_in_file,
                    }
                };

                state._output.push({addrof, loc});
                ctx.global->definition_locations[addrof] = loc;

                break;
            }

            case KW_U8:   state._output.push({ &t_u8,   kwloc }); prev_was_value = true; break;
            case KW_U16:  state._output.push({ &t_u16,  kwloc }); prev_was_value = true; break;
            case KW_U32:  state._output.push({ &t_u32,  kwloc }); prev_was_value = true; break;
            case KW_U64:  state._output.push({ &t_u64,  kwloc }); prev_was_value = true; break;
            case KW_I8:   state._output.push({ &t_i8,   kwloc }); prev_was_value = true; break;
            case KW_I16:  state._output.push({ &t_i16,  kwloc }); prev_was_value = true; break;
            case KW_I32:  state._output.push({ &t_i32,  kwloc }); prev_was_value = true; break;
            case KW_I64:  state._output.push({ &t_i64,  kwloc }); prev_was_value = true; break;
            case KW_BOOL: state._output.push({ &t_bool, kwloc }); prev_was_value = true; break;
            case KW_F32:  state._output.push({ &t_f32,  kwloc }); prev_was_value = true; break;
            case KW_F64:  state._output.push({ &t_f64,  kwloc }); prev_was_value = true; break;

            default: {
                if (delim == t.type) {
                    r.pos --;
                    goto Done;
                }

                if (is_operator(t.type)) {
                    // * can be either postfix, in which case it means dereference
                    // or be infix, in which case it means multiplication
                    if (t.type == '*') {
                        TokenType next = r.peek().type;

                        // If the next token type is value, the * is infix
                        // if it's an operator, the * is postfix
                        if (!_token_is_value(next)) {
                            if (state._output.size == 0) {
                                state.ctx.error({ .code = ERR_INVALID_EXPRESSION });
                                return nullptr;
                            }

                            AST_Dereference *deref = ctx.alloc<AST_Dereference>(nullptr);
                            ParseExprValue last = state.pop_into(&deref->ptr);
                            last.loc.loc.end = r.pos_in_file;

                            state._output.push({
                                deref,
                                last.loc,
                            });
                            state.ctx.global->definition_locations[deref] = last.loc;
                            prev_was_value = true;
                            break;
                        }
                    }

                    while (state.stack.size && state.stack.last() != TOK('(') 
                            && PREC(state.stack.last()) + !IS_RIGHT_ASSOC(t.type) > PREC(t.type)) 
                    {
                        MUST (pop_operator(state));
                    }

                    prev_was_value = false;
                    state.stack.push(t.type);
                }
                else {
                    if (prev_was_value) {
                        // The expression is over.
                        // This token is part fo whatever comes next - don't consume it
                        r.pos --;
                        goto Done;
                    }
                    else {
                        state.ctx.error({ .code = ERR_INVALID_EXPRESSION });
                        return nullptr;
                    }
                }
            }

        }
    }

Done:
    while (state.stack.size) 
        MUST (pop_operator(state));

    if (state._output.size != 1) {
        state.ctx.error({
            .code = ERR_INVALID_EXPRESSION
        });
        return nullptr;
    }
    return state._output[0].val;
}

bool parse_let(AST_Context& ctx, TokenReader& r) {
    Token let_tok = r.expect_full(KW_LET);
    MUST (let_tok.type);

    Token nameToken = r.expect_full(TOK_ID);
    MUST (nameToken.type);

    AST_Var* var = ctx.alloc<AST_Var>(nameToken.name, -1);

    MUST (r.expect(TOK(':')).type);
    MUST (var->type = (AST_Type*)parse_expr(ctx, r, TOK('=')));

    if (r.peek().type == TOK('=')) {
        r.pop();
        AST_Value *initial_value = (AST_Value*)parse_expr(ctx, r);
        MUST (initial_value);

        Location loc = {
            .file_id = let_tok.file_id,
            .loc = {
                .start = let_tok.loc.start,
                .end = r.pos_in_file,
            }
        };

        AST_BinaryOp *assignment = ctx.alloc<AST_BinaryOp>(TOK('='), var, initial_value);
        ctx.global->definition_locations[assignment] = loc;
        assignment->nodetype = AST_ASSIGNMENT;
        ctx.statements.push(assignment);
    };

    var->is_global = ctx.global == &ctx;

    MUST (ctx.declare({ nameToken.name }, var, &ctx == ctx.global));

    MUST (r.expect(TOK(';')).type);

    ctx.global->definition_locations[var] = {
        .file_id = r.sf.id,
        .loc = {
            .start = let_tok.loc.start,
            .end = r.pos_in_file,
        }
    };

    return true;
}

AST_Struct *parse_struct(AST_Context& ctx, TokenReader& r, bool decl) {
    AST_Struct *st;

    Token struct_kw = r.expect_full(KW_STRUCT);

    // parse_struct only ever gets called if the 'struct' keyword has been peeked
    assert (struct_kw.type);

    if (decl) {
        Token nameToken = r.expect_full(TOK_ID);
        MUST (nameToken.type); 

        st = ctx.alloc<AST_Struct>(nameToken.name);
        MUST (ctx.declare({ .name = st->name }, st, true));
    }
    else {
        st = ctx.alloc<AST_Struct>(nullptr);
    }

    MUST (r.expect(TOK('{')).type);
    MUST (parse_type_list(ctx, r, TOK('}'), &st->members));

    return st;
}

bool parse_decl_statement(AST_Context& ctx, TokenReader& r, bool* error) {
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

bool parse_source_file(AST_Context& global, SourceFile& sf) {
    TokenReader r { .sf = sf, .ctx = global };
    MUST (tokenize(global, sf));
    MUST (parse_scope(global, r, TOK_NONE));
    return true;
}

bool parse_all(AST_Context& global) {
    for (SourceFile& sf : sources)
        MUST (parse_source_file(global, sf));
    return true;
}
