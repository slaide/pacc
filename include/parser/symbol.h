#pragma once

#include<tokenizer.h>

typedef struct Symbol Symbol;
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

struct Symbol{
	enum SYMBOLKIND kind;
	Token* name;
	Type* type;
};
struct SymbolDefinition{
	Symbol symbol;
	Value* initializer;
};
enum SYMBOL_PARSE_RESULT{
	/// @brief symbol was parsed successfully
	SYMBOL_PRESENT,
	/// @brief symbol was not parsed successfully (e.g. syntax error)
	SYMBOL_INVALID,
};
struct Symbol_parse_options{
	bool forbid_multiple;
	bool allow_initializers;
};

bool Symbol_equal(Symbol*a,Symbol*b);
