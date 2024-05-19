#pragma once

#include<stdint.h>
#include<file.h>

enum TOKEN_TAG{
	TOKEN_TAG_UNDEFINED=0,
	TOKEN_TAG_COMMENT=1,
	TOKEN_TAG_KEYWORD=2,
	
	TOKEN_TAG_SYMBOL=3,

	TOKEN_TAG_LITERAL_INTEGER=0x10,
	TOKEN_TAG_LITERAL_FLOAT=0x11,
	TOKEN_TAG_LITERAL_CHAR=0x12,
	TOKEN_TAG_LITERAL_STRING=0x13,

	TOKEN_TAG_PREP_INCLUDE_ARGUMENT=0x20,
};
typedef struct Token{
	enum TOKEN_TAG tag;
	const char* filename;
	int line;
	int col;
	int len;
	const char*p;
	/// @brief for numeric literals, this contains some metainformation
	struct{
		bool hasLeadingSign;
		uint8_t base;
		bool hasPrefix;
		bool hasLeadingDigits;
		bool hasDecimalPoint;
		bool hasTrailingDigits;
		bool hasExponent;
		bool hasExponentSign;
		bool hasExponentDigits;
		bool hasSuffix;
	}num_info;
}Token;

/* return token location as null-terminated string */
char*Token_loc(Token*token);
/* return token (incl. location) as null-terminated string */
char*Token_print(Token*token);

// check of two tokens point to strings with the same content
bool Token_equalToken(Token*,Token*);
bool Token_equalString(Token*,char*);

typedef struct Tokenizer{
	int num_tokens;
    Token*tokens;
	const char*token_src;
}Tokenizer;

int Tokenizer_init(Tokenizer*tokenizer,File*file);

struct TokenIter{
    Tokenizer*tokenizer;
    int next_token_index;

    struct TokenIterConfig{
        bool skip_comments;
    }config;
};
void TokenIter_init(struct TokenIter*token_iter,Tokenizer*tokenizer,struct TokenIterConfig config);
// returns true if token was returned
int TokenIter_nextToken(struct TokenIter*iter,Token*out);
// returns true if there are tokens left, i.e. next token index is within valid range of token indices
bool TokenIter_isEmpty(struct TokenIter*iter);
// returns true if token was returned
int TokenIter_lastToken(struct TokenIter*iter,Token*out);

/// @brief map token to keyword
void Token_map(Token*token);
static const char*KEYWORD_SWITCH="switch";
static const char*KEYWORD_CASE="case";
static const char*KEYWORD_RETURN="return";
static const char*KEYWORD_BREAK="break";
static const char*KEYWORD_CONTINUE="continue";
static const char*KEYWORD_GOTO="goto";
static const char*KEYWORD_TYPEDEF="typedef";

static const char*KEYWORD_STRUCT="struct";
static const char*KEYWORD_UNION="union";
static const char*KEYWORD_ENUM="enum";

static const char*KEYWORD_INCLUDE="include";
static const char*KEYWORD_DEFINE="define";
static const char*KEYWORD_IFDEF="ifdef";
static const char*KEYWORD_IFNDEF="ifndef";
static const char*KEYWORD_UNDEF="undef";
static const char*KEYWORD_PRAGMA="pragma";

static const char*KEYWORD_IF="if";
static const char*KEYWORD_ELSE="else";
static const char*KEYWORD_FOR="for";
static const char*KEYWORD_WHILE="while";

static const char*KEYWORD_VOID="void";
static const char*KEYWORD_INT="int";
static const char*KEYWORD_FLOAT="float";
static const char*KEYWORD_DOUBLE="double";
static const char*KEYWORD_CHAR="char";

static const char*KEYWORD_LONG="long";
static const char*KEYWORD_UNSIGNED="unsigned";

static const char*KEYWORD_ASTERISK="*";
static const char*KEYWORD_PLUS="+";
static const char*KEYWORD_MINUS="-";
static const char*KEYWORD_EQUAL="=";
static const char*KEYWORD_QUESTIONMARK="?";
static const char*KEYWORD_BANG="!";
static const char*KEYWORD_TILDE="~";
static const char*KEYWORD_HASH="#";
static const char*KEYWORD_PARENS_OPEN="(";
static const char*KEYWORD_PARENS_CLOSE=")";
static const char*KEYWORD_SQUARE_BRACKETS_OPEN="[";
static const char*KEYWORD_SQUARE_BRACKETS_CLOSE="]";
static const char*KEYWORD_CURLY_BRACES_OPEN="{";
static const char*KEYWORD_CURLY_BRACES_CLOSE="}";
static const char*KEYWORD_COLON=":";
static const char*KEYWORD_SEMICOLON=";";
static const char*KEYWORD_COMMA=",";
static const char*KEYWORD_DOT=".";
static const char*KEYWORD_AMPERSAND="&";

static int highlight_token_kind=-1;
/// print tokenizer contents with appropriate spacing, and line number at start of each line
void Tokenizer_print(Tokenizer*tokenizer);
