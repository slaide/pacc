#pragma once

#include<tokenizer.h>
#include<parser/module.h>

#include<parser/type.h>

enum SYMBOLKIND{
	SYMBOL_KIND_UNKNOWN=0,

	/// @brief declaration of a symbol, i.e. includes type information
	SYMBOL_KIND_DECLARATION,
	/// @brief reference to a symbol, i.e. does not include type information
	SYMBOL_KIND_REFERENCE,
	SYMBOL_KIND_VARARG,
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
};
/// @brief  type cannot be parsed separate from symbol, so we parse a symbol as combination of type and name parser
/// @param module module that provides the context for parsing
/// @param num_symbols number of symbols returned
/// @param symbols_out pointer to number of symbols (to be freed by the caller)
/// @param token_iter_in iterator over tokens to be consumed for symbol parsing, iterator is mutated only on success
enum SYMBOL_PARSE_RESULT Symbol_parse(Module*module,int*num_symbols,Symbol**symbols_out,struct TokenIter*token_iter_in);

bool Symbol_equal(Symbol*a,Symbol*b);
