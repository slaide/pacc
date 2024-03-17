#pragma once

#include<file.h>

typedef struct Token{
	enum{
		TOKEN_TAG_UNDEFINED=0,
		TOKEN_TAG_KEYWORD,
		TOKEN_TAG_SYMBOL,
		TOKEN_TAG_COMMENT,
		TOKEN_TAG_LITERAL_INTEGER,
		TOKEN_TAG_LITERAL_FLOAT,
		TOKEN_TAG_LITERAL_CHAR,
		TOKEN_TAG_LITERAL_STRING,
        TOKEN_TAG_PREP_INCLUDE_ARGUMENT,
	}tag;
	const char* filename;
	int line;
	int col;
	int len;
	const char*p;
}Token;

// check of two tokens point to strings with the same content
bool Token_symbolEqual(Token*,Token*);

typedef struct Tokenizer{
	int num_tokens;
    Token*tokens;
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
static const char*KEYWORD_HASH="#";
static const char*KEYWORD_PARENS_OPEN="(";
static const char*KEYWORD_PARENS_CLOSE=")";
static const char*KEYWORD_SQUARE_BRACKETS_OPEN="[";
static const char*KEYWORD_SQUARE_BRACKETS_CLOSE="]";
static const char*KEYWORD_CURLY_BRACES_OPEN="{";
static const char*KEYWORD_CURLY_BRACES_CLOSE="}";
static const char*KEYWORD_COLON=":";
static const char*KEYWORD_SEMICOLON=";";
