#include "parser.h"
#include "sourcefile.h"

#include "keywords.gperf.gen.h"

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
/* 58  - :   */ CT_ERROR,
/* 59  - ;   */ CT_SINGLECHAR_OP,
/* 60  - <   */ CT_ERROR,
/* 61  - =   */ CT_ERROR,
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

void tokenize(SourceFile &s) {
	u64 word_start;
	enum { NONE, WORD, NUMBER } state = NONE;

	for (u64 i = 0; i < s.length; i++) {
		char c = s.buffer[i];
		CharTraits ct = ctt[c];

		if (!ct) {
			printf("unrecognized character %d: '%c'\n", c, c);
			exit(1);
		}

		if (state) {
			if ((ct & (CT_LETTER | CT_DIGIT)) == 0 && state) {
				tok* kw = Perfect_Hash::in_word_set(s.buffer + word_start, i - word_start);

				s.tokens.push_back({
					.type = kw ? kw->type : (state == WORD ? TOK_ID : TOK_NUM),
					.length = i - word_start,
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
			s.tokens.push_back({
				.type = (TokenType)c,
				.length = 1,
				.start = i
			});
		}
	}
}

struct TokenReader {
	int pos = 0;
	const SourceFile& sf;

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
};

void print_tokens(const SourceFile& sf) {
	for (const Token& t : sf.tokens) {
		printf("%d\t%.*s\n", t.type, (int)t.length, sf.buffer + t.start);
	}
}

void parse(int sf, Context* c) {
	SourceFile& s = sources[sf];
	tokenize(s);

	//TokenReader r = { .sf = s };
	print_tokens(s);

}

/*
void parse_top_level_decl(TokenReader& r) {
	Token t = r.pop();
	switch (t.type) {
		case TOK_NONE:
			return;
	}
}
*/
