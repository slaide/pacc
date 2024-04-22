#pragma once

#include<tokenizer.h>

#include<parser/type.h>

enum SYMBOLKIND{
	SYMBOL_KIND_UNKNOWN=0,

	/// @brief declaration of a symbol, i.e. includes type information
	SYMBOL_KIND_DECLARATION,
	/// @brief reference to a symbol, i.e. does not include type information
	SYMBOL_KIND_REFERENCE,
};
const char* Symbolkind_asString(enum SYMBOLKIND kind);

typedef struct Symbol Symbol;
struct Symbol{
	Token* name;
	enum SYMBOLKIND kind;
	Type* type;
};
enum SYMBOL_PARSE_RESULT{
	/// @brief symbol was parsed successfully
	SYMBOL_PRESENT,
	/// @brief symbol was not parsed successfully (e.g. syntax error)
	SYMBOL_INVALID,
	/// @brief something that looks like a type was present, but no name for the symbol (this _may_ be an error, depending on the context)
	SYMBOL_WITHOUT_NAME,
};
/// @brief  type cannot be parsed separate from symbol, so we parse a symbol as combination of type and name parser
/// @param symbol 
/// @param token_iter 
enum SYMBOL_PARSE_RESULT Symbol_parse(Symbol*symbol,struct TokenIter*token_iter);

bool Symbol_equal(Symbol*a,Symbol*b);
