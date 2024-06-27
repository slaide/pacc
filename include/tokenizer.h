#pragma once

#include<stdint.h>
#include<wchar.h>

#include"file.h"

enum TOKEN_TAG{
	TOKEN_TAG_UNDEFINED=0,
	TOKEN_TAG_COMMENT=1,
	TOKEN_TAG_KEYWORD=2,
	
	TOKEN_TAG_SYMBOL=3,

	TOKEN_TAG_LITERAL=4,

	TOKEN_TAG_PREP_INCLUDE_ARGUMENT=5,
};
enum Token_LiteralTag{
	TOKEN_LITERAL_TAG_UNDEFINED=0,
	TOKEN_LITERAL_TAG_NUMERIC,
	TOKEN_LITERAL_TAG_STRING,
};
enum Token_LiteralNumeric_Tag{
	TOKEN_LITERAL_NUMERIC_TAG_UNDEFINED=0,

	TOKEN_LITERAL_NUMERIC_TAG_INTEGER,
	TOKEN_LITERAL_NUMERIC_TAG_UNSIGNED,
	TOKEN_LITERAL_NUMERIC_TAG_LONG,
	TOKEN_LITERAL_NUMERIC_TAG_UNSIGNED_LONG,
	TOKEN_LITERAL_NUMERIC_TAG_LONG_LONG,
	TOKEN_LITERAL_NUMERIC_TAG_UNSIGNED_LONG_LONG,

	TOKEN_LITERAL_NUMERIC_TAG_FLOAT,
	TOKEN_LITERAL_NUMERIC_TAG_DOUBLE,

	TOKEN_LITERAL_NUMERIC_TAG_CHAR,
	TOKEN_LITERAL_NUMERIC_TAG_UNSIGNED_CHAR,
	TOKEN_LITERAL_NUMERIC_TAG_WCHAR,
};

typedef struct Token{
	enum TOKEN_TAG tag;

	// token length
	int len;
	// token string (NOT zero terminated)
	const char*p;

	/* name of file containing this char */
	const char* filename;
	// line number in file
	int line;
	// column number in file
	int col;

	// slightly out of place indicator if this token has already been expanded by the preprocessor and hence should not be expanded again
	bool alreadyExpanded;

	struct{
		enum Token_LiteralTag tag;

		union{
			struct{
				int len;
				char*str;
			}string;

			// numeric literals (include integers, floats, chars of any size)
			struct{
				enum Token_LiteralNumeric_Tag tag;

				union{
					int int_;
					unsigned int uint;
					long long_;
					unsigned long ulong;
					long long llong_;
					unsigned long long ullong;
					float float_;
					double double_;
					char char_;
					unsigned char uchar;
					wchar_t wchar;
				}value;

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
			}numeric;
		}/* data */;
	}literal;
}Token;

/* return token location as null-terminated string */
char*Token_loc(const Token*token);
/* return token (incl. location) as null-terminated string */
char*Token_print(const Token*token);
Token*Token_fromString(const char*str);

// check of two tokens point to strings with the same content
bool Token_equalToken(const Token*,const Token*);
bool Token_equalString(const Token*,const char*);

typedef struct Tokenizer{
	int num_tokens;
    Token*tokens;
	const char*token_src;
}Tokenizer;

int Tokenizer_init(Tokenizer*tokenizer,const File*file);

struct TokenIter{
    Tokenizer*tokenizer;
    int next_token_index;

    struct TokenIterConfig{
        bool skip_comments;
    }config;
};
void TokenIter_init(struct TokenIter*token_iter,Tokenizer*tokenizer,struct TokenIterConfig config);
// returns true if token was returned
bool TokenIter_nextToken(struct TokenIter*iter,Token*out);
// returns true if there are tokens left, i.e. next token index is within valid range of token indices
bool TokenIter_isEmpty(const struct TokenIter*iter);
// returns true if token was returned
bool TokenIter_lastToken(const struct TokenIter*iter,Token*out);

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

/*
return value of numeric literal token at appropriate size

if token is not a numeric literal, fatal

only unsigned long long is returned into u
all other non-float values are expanded into i
all floating point values are expanded into d
*/
void TokenLiteral_getNumericValue(struct Token*token,uint64_t*u,int64_t*i,double*d);
