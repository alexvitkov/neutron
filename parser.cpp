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
    if (ctx.global.reference_locations.find(node, &loc)) {
        return loc;
    } else if (ctx.global.definition_locations.find(*node, &loc)) {
        return loc;
    } 
    DIE("The AST_Node doesn't have a location");
}

struct TokenReader;
void unexpected_token(AST_Context& ctx, Token actual, TokenType expected);

AST_Fn* parse_fn(AST_Context& ctx, TokenReader& r, bool decl);
AST_Macro* parse_macro(AST_Context& ctx, TokenReader& r);
AST_Struct *parse_struct(AST_Context& ctx, TokenReader& r, bool decl);


NumberData* parse_number(AST_Context& ctx, char** pos)  {
    NumberData *num_data = new NumberData();
    char *&p = *pos;
    num_data->base = 10;

    if (p[0] == '0') {
        switch (p[1]) {
            case 'b': { num_data->base = 2;  p += 2; break; }
            case 'o': { num_data->base = 8;  p += 2; break; }
            case 'x': { num_data->base = 16; p += 2; break; }
            default: break;
        }
    }

    // skip leading zeroes
    while (*p == '0')
        p++;

    for ( ;; p++) {
        char digit = *p;

        if (digit >= '0' && digit <= '9') {
            digit -= '0';
        } else if (digit >= 'A' && digit <= 'F') {
            digit -= 'A';
        } else if (digit >= 'a' && digit <= 'f') {
            digit -= 'a';
        } 

        else if (digit == '.') {
            if (num_data->decimal_point >= 0)
                assert(!"TODO ERROR");
            num_data->decimal_point = num_data->digits.size;
        } 

        else {
            break;
        }

        if (digit > num_data->base) {
            assert(!"TODO ERROR");
        }
        num_data->digits.push(digit);

    }

    return num_data;
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

                char       *name     = nullptr;
                NumberData *num_data = nullptr;

                if (tt == TOK_ID) {
                    // TODO ALLOCATION
                    u64 length = i - word_start;
                    name = (char*)malloc(length + 1);
                    memcpy(name, s.buffer + word_start, length);
                    name[length] = 0;
                } else if (tt == TOK_NUMBER) {
                    char *pos = s.buffer + word_start;
                    num_data = parse_number(global, &pos);
                }

                if (name) {
                    s.pushToken(
                        { .type = tt, .name = name },
                        { .start = word_start, .end = i });
                } else {
                    s.pushToken(
                        { .type = tt, .number_data = num_data },
                        { .start = word_start, .end = i });
                }

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
bool parse_expr(AST_Context& ctx, AST_Value **out, TokenReader& r, TokenType delim);

bool parse_type_list(AST_Context& ctx, TokenReader& r, TokenType delim, bucketed_arr<StructElement>* tl) {
    SmallToken t = r.peek();
    if (t.type == delim) {
        r.pop();
        return true;
    }

    while (true) {
        SmallToken nameToken = r.expect(TOK_ID);
        MUST (nameToken.type);

        MUST (r.expect(TOK(':')).type);


        auto& ref = tl->push({ .name = nameToken.name, .type = nullptr });
        MUST (parse_expr(ctx, (AST_Value**)&ref.type, r, {}));

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


enum ParseNodeRet {
    PARSE_NODE_ERROR = 0,
    PARSE_NODE_DONE,
    PARSE_NODE_STMT,
    PARSE_NODE_DECL,
};

ParseNodeRet parse_node(AST_Context &ctx, AST_Node **out, TokenReader &r, TokenType delim) {
    switch (r.peek().type) {
        case KW_FN: {
            *out = parse_fn(ctx, r, true);
            return PARSE_NODE_DECL;
        }
        case KW_MACRO: {
            *out = parse_macro(ctx, r);
            return PARSE_NODE_DECL;
        }
        case KW_STRUCT: {
            *out = parse_struct(ctx, r, true);
            return PARSE_NODE_DECL;
        }
        case KW_EMIT: {
            if (!ctx.fn || !(ctx.fn IS AST_MACRO)) {
                NOT_IMPLEMENTED("TODO ERROR");
            }
            r.pop();
            AST_Emit *emit = ctx.alloc<AST_Emit>();
            ParseNodeRet ret = parse_node(ctx, &emit->node_to_emit, r, TOK_NONE);

            if (ret == PARSE_NODE_ERROR || ret == PARSE_NODE_DONE)
                return PARSE_NODE_ERROR;

            *out = emit;
            return PARSE_NODE_STMT;
        }
        case KW_RETURN: {
            Token return_tok = r.pop_full();

            AST_Return *ret;
            if (r.peek().type == TOK(';')) {
                ret = ctx.alloc<AST_Return>(nullptr);
            } else {
                ret = ctx.alloc<AST_Return>(nullptr);
                if (!parse_expr(ctx, &ret->value, r, {}))
                    return PARSE_NODE_ERROR;
            }

            r.ctx.global.definition_locations[ret] = {
                .file_id = r.sf.id,
                .loc = {
                    .start = return_tok.loc.start,
                    .end = r.pos_in_file,
                }
            };

            if (!r.expect(TOK(';')).type)
                return PARSE_NODE_ERROR;
            *out = ret;

            return PARSE_NODE_STMT;
        }
        case KW_IF: {
            Token if_tok = r.pop_full();

            AST_If* ifs = ctx.alloc<AST_If>(&ctx);
            if (!parse_expr(ctx, &ifs->condition, r, {}))
                return PARSE_NODE_ERROR;

            if (!parse_block(ifs->then_block, r))
                return PARSE_NODE_ERROR;

            r.ctx.global.definition_locations[ifs] = {
                .file_id = r.sf.id,
                .loc = {
                    .start = if_tok.loc.start,
                    .end = r.pos_in_file,
                }
            };

            *out = ifs;
            return PARSE_NODE_STMT;
        }

        case KW_WHILE: {
            Token while_tok = r.pop_full();

            AST_While* whiles = ctx.alloc<AST_While>(&ctx);
            if (!parse_expr(ctx, &whiles->condition, r, {}))
                return PARSE_NODE_ERROR;
            if (!parse_block(whiles->block, r))
                return PARSE_NODE_ERROR;

            r.ctx.global.definition_locations[whiles] = {
                .file_id = r.sf.id,
                .loc = {
                    .start = while_tok.loc.start,
                    .end = r.pos_in_file,
                }
            };

            *out = whiles;
            return PARSE_NODE_STMT;
        }

        default: {
            if (r.peek().type == delim) {
                r.pop();
                return PARSE_NODE_DONE;
            }

            if (!parse_expr(ctx, (AST_Value**)out, r, {}))
                return PARSE_NODE_ERROR;
            if (!r.expect(TOK(';')).type)
                return PARSE_NODE_ERROR;

            return PARSE_NODE_STMT;
        }
    }
}


bool parse_scope(AST_Context& block, TokenReader& r, TokenType delim) {
    bool done;
    while (true) {
        AST_Node *node;
        switch (parse_node(block, &node, r, delim)) {
            case PARSE_NODE_DONE: {
                if (block.hanging_declarations == 0)
                    block.close();
                return true;
            }
            case PARSE_NODE_ERROR: {
                return false;
            }
            case PARSE_NODE_STMT: {
                block.statements.push(node);
                break;
            }
            case PARSE_NODE_DECL: {
                break;
            }
            default:
                UNREACHABLE;
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

    // parse_fn only ever gets called if the 'fn' keyword
    // has been peeked, no need for MUST check
    assert (fn_kw.type);

    Token nameToken = {};
    if (decl) {
        nameToken = r.expect_full(TOK_ID);
        MUST (nameToken.type);
    }

    AST_Fn *fn = ctx.alloc<AST_Fn>(&ctx, nameToken.name);
    AST_FnType* temp_fn_type = ctx.alloc_temp<AST_FnType>(ctx.global.target.pointer_size);
    
    if (decl) {
        ctx.global.fns_to_declare.push({ ctx, fn });
        ctx.hanging_declarations ++;
    }

    fn->type = temp_fn_type;

    MUST (r.expect(TOK('(')).type);

    // Parse the function arguments
    i64 argindex = 0;
    while (r.peek().type != TOK_CLOSEBRACKET) {

        if (r.peek().type == OP_VARARGS) {
            r.pop();
            temp_fn_type->is_variadic = true;
            break;
        }

        Token nameToken = r.expect_full(TOK_ID);
        MUST (nameToken.type);

        MUST (r.expect(TOK(':')).type);

        AST_Type *& type = temp_fn_type->param_types.push(nullptr);
        MUST (parse_expr(ctx, (AST_Value**)&type, r, {}));
        fn->argument_names.push(nameToken.name);
        
        TokenType p = r.peek().type;

        AST_Var* var = ctx.alloc<AST_Var>(nameToken.name, argindex++);
        ctx.global.definition_locations[var] = {
            .file_id = r.sf.id,
            .loc = {
               .start = nameToken.loc.start,
               .end = r.pos_in_file,
            }
        };

        // the type of the AST_Var is set by the typer
        MUST (fn->block.declare({ nameToken.name }, var, false));

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
        MUST (parse_expr(ctx, (AST_Value**)&temp_fn_type->returntype, r, {}));
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

    ctx.global.definition_locations[fn] = {
        .file_id = r.sf.id,
        .loc = {
           .start = fn_kw.loc.start,
           .end = r.pos_in_file,
        }
    };

    return fn;
}


AST_Macro* parse_macro(AST_Context& ctx, TokenReader& r) {
    Token macro_kw = r.expect_full(KW_MACRO);

    // parse_macro only ever gets called if the 'macro' keyword
    // has been peeked, no need for MUST check
    assert (macro_kw.type);

    Token nameToken = r.expect_full(TOK_ID);
    MUST (nameToken.type);

    AST_Macro *macro = ctx.alloc<AST_Macro>(&ctx, nameToken.name);

    MUST(parse_block(macro->block, r));

    MUST(ctx.global.declare({ .name = macro->name }, macro, true));

    ctx.global.definition_locations[macro] = {
        .file_id = r.sf.id,
        .loc = {
           .start = macro_kw.loc.start,
           .end = r.pos_in_file,
        }
    };

    return macro;
}





struct ParseExprValue {
    AST_Value *val;
    Location   loc;
};

struct OperatorOnStack {
    Token     tok;
    bool      is_unary;
};

struct ParseExprState {
    AST_Context &ctx;
    u32 brackets = 0;
    arr<ParseExprValue> _output;
    arr<OperatorOnStack> stack;

    ParseExprValue pop_into(AST_Value **out) {
        ParseExprValue val = _output.pop();
        *out = val.val;

        if (val.val IS AST_UNRESOLVED_ID) {
            IdResolveJob _resolve_job(ctx, (AST_UnresolvedId**)out);
            HeapJob *resolve_job = _resolve_job.heapify<CallResolveJob>();

            ((AST_UnresolvedId*)val.val)->job = resolve_job;

            ctx.subscribers.push(resolve_job);
            ctx.global.add_job(resolve_job);
        }

        return val;
    };

};


bool pop_operator(ParseExprState& state) {

    OperatorOnStack op = state.stack.pop();

    if (state._output.size < (op.is_unary ? 1 : 2)) {
        state.ctx.error({
            .code = ERR_INVALID_EXPRESSION
        });
        return false;
    }

    if (op.is_unary) {
        AST_Call *un = state.ctx.alloc<AST_Call>(FNCALL_UNARY_OP, op.tok.type, nullptr, 1);
        un->args.size = 1;
        ParseExprValue inner = state.pop_into(&un->args[0]);

        Location loc = {
            .file_id = inner.loc.file_id,
            .loc = {
                inner.loc.loc.start,
                inner.loc.loc.end,
            }
        };
        state.ctx.global.definition_locations[un] = loc;
        state._output.push({ un, loc });
        
        return true;

    } else {
        AST_Call *bin = state.ctx.alloc<AST_Call>(FNCALL_BINARY_OP, op.tok.type, nullptr, 2);
        bin->args.size = 2;
        ParseExprValue lhs, rhs;
        rhs = state.pop_into(&bin->args[1]);
        lhs = state.pop_into(&bin->args[0]);
        
        Location loc = {
            .file_id = lhs.loc.file_id,
            .loc = {
                lhs.loc.loc.start,
                rhs.loc.loc.end,
            }
        };
        state.ctx.global.definition_locations[bin] = loc;
        state._output.push({ bin, loc });
        
        return true;
    }
}

bool _token_is_value(TokenType t) {
    if (IS_TYPE_KW(t))
        return true;
    if (t == TOK_ID || t == TOK_NUMBER || t == TOK_STRING_LITERAL || t == '(')
        return true;
    return false;
}

AST_Var *parse_var_decl(AST_Context& ctx, TokenReader& r);

bool parse_expr(AST_Context& ctx, AST_Value **out, TokenReader& r, TokenType delim) {
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
                    return false;
                }
                prev_was_value = true;

                AST_Value* val = nullptr;

                switch (t.type) {
                    case TOK_ID: {
                        if (r.peek().type == TOK_COLON) {
                            r.pos--;
                            val = parse_var_decl(ctx, r);
                            MUST (val);
                        }
                        else {
                            if (!ctx.declarations.find({ .name = t.name }, (AST_Node**)&val))
                                val = ctx.alloc_temp<AST_UnresolvedId>(t.name, ctx);
                        }
                        break;
                    }
                    case TOK_NUMBER: {
                        val = ctx.alloc<AST_NumberLiteral>(t.number_data);
                        break;
                    }
                    case TOK_STRING_LITERAL: {
                        if (!ctx.global.literals.find(t.name, (AST_StringLiteral**)&val)) {
                            val = ctx.alloc<AST_StringLiteral>(t);
                            ctx.global.literals.insert(t.name, (AST_StringLiteral*)val);
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
                if (!ctx.global.definition_locations.find2(val))
                    ctx.global.definition_locations[val] = loc;

                state._output.push({val, loc });
                break;
            }

            case TOK_OPENBRACKET: {
                // Value followed by a open brackets means function call
                if (prev_was_value) {
                    if (state._output.size == 0) {
                        state.ctx.error({ .code = ERR_INVALID_EXPRESSION });
                        return false;
                    }

                    AST_Call* fncall = ctx.alloc<AST_Call>(FNCALL_REGULAR_FN, TOK(0), nullptr, 4);
                    ParseExprValue fn = state._output.pop();

                    assert(fn.val IS AST_UNRESOLVED_ID);
                    fncall->fn = fn.val;

                    arr<Location> arg_locs;

                    while (r.peek().type != TOK(')')) {
                        AST_Value *& arg = fncall->args.push(nullptr);
                        MUST (parse_expr(ctx, &arg, r, {}));

                        Location argloc = location_of(ctx, (AST_Node**)&arg);
                        arg_locs.push(argloc);

                        if (r.peek().type != TOK(')'))
                            MUST (r.expect(TOK(',')).type);
                    }
                    r.pop(); // discard the closing bracket

                    for (u32 i = 0; i < arg_locs.size; i++) {
                        ctx.global.reference_locations[(AST_Node**)&fncall->args[i]] = arg_locs[i];
                    }

                    Location loc = {
                        .file_id = fn.loc.file_id,
                        .loc = {
                            .start = fn.loc.loc.start,
                            .end = r.pos_in_file,
                        }
                    };

                    r.ctx.global.definition_locations[fncall] = loc;
                    state._output.push({ fncall, loc });
                }
                
                // If previous isn't a value, do the shunting yard thing
                else {
                    prev_was_value = false;
                    state.stack.push({ t, true });
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
                while (state.stack.last().tok.type != TOK('('))
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
                    return false;
                }

                AST_MemberAccess* ma = ctx.alloc<AST_MemberAccess>(nullptr, nameToken.name);
                ParseExprValue lhs = state.pop_into(&ma->lhs);

                lhs.loc.loc.end = r.pos_in_file;

                state._output.push({ ma, lhs.loc });
                break;
            }

            case TOK_AMPERSAND: {
                if (state._output.size == 0) {
                    state.ctx.error({ .code = ERR_INVALID_EXPRESSION });
                    return false;
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
                ctx.global.definition_locations[addrof] = loc;

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
                                return false;
                            }

                            AST_Dereference *deref = ctx.alloc<AST_Dereference>(nullptr);
                            ParseExprValue last = state.pop_into(&deref->ptr);
                            last.loc.loc.end = r.pos_in_file;

                            state._output.push({
                                deref,
                                last.loc,
                            });
                            state.ctx.global.definition_locations[deref] = last.loc;
                            prev_was_value = true;
                            break;
                        }
                    }

                    if (prev_was_value) {
                        while (state.stack.size && state.stack.last().tok.type != TOK('(') 
                                && PREC(state.stack.last().tok.type) + !IS_RIGHT_ASSOC(t.type) > PREC(t.type)) 
                        {
                            MUST (pop_operator(state));
                        }
                        prev_was_value = false;
                        state.stack.push({ t, false });
                    } else {
                        if ((prec[t.type] & PREFIX) == PREFIX) {
                            prev_was_value = false;
                            state.stack.push({ t, true });
                        } else {
                            NOT_IMPLEMENTED("ERrror");
                        }
                    }

                }
                else {
                    if (prev_was_value) {
                        // The expression is over.
                        // the current token is part of whatever comes next - don't consume it
                        r.pos --;
                        goto Done;
                    }
                    else {
                        state.ctx.error({ .code = ERR_INVALID_EXPRESSION });
                        return false;
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
        return false;
    }

    state.pop_into(out);
    return true;
}

AST_Var *parse_var_decl(AST_Context& ctx, TokenReader& r) {
    Token nameToken = r.expect_full(TOK_ID);
    MUST (nameToken.type);

    AST_Var* var = ctx.alloc<AST_Var>(nameToken.name, -1);

    MUST (r.expect(TOK(':')).type);
    MUST (parse_expr(ctx, (AST_Value**)&var->type, r, TOK('=')));

    var->is_global = &ctx.global == &ctx;

    ctx.global.definition_locations[var] = {
        .file_id = r.sf.id,
        .loc = {
            .start = nameToken.loc.start,
            .end = r.pos_in_file,
        }
    };

    MUST (ctx.declare({ nameToken.name }, var, &ctx == &ctx.global));
    return var;
}

AST_Struct *parse_struct(AST_Context& ctx, TokenReader& r, bool decl) {
    AST_Struct *st;

    Token struct_kw = r.expect_full(KW_STRUCT);

    // parse_struct only ever gets called if the 'struct' keyword has been peeked
    assert (struct_kw.type);

    bool declare_succeeded = true;

    if (decl) {
        Token nameToken = r.expect_full(TOK_ID);
        MUST (nameToken.type); 

        st = ctx.alloc<AST_Struct>(nameToken.name);
        declare_succeeded = ctx.declare({ .name = st->name }, st, true);
    }
    else {
        st = ctx.alloc<AST_Struct>(nullptr);
    }

    MUST (r.expect(TOK('{')).type);
    MUST (parse_type_list(ctx, r, TOK('}'), &st->members));

    return declare_succeeded ? st : nullptr;
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

