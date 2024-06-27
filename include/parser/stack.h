#pragma once

typedef struct Stack Stack;

#include<util/array.h>

#include<tokenizer.h>
#include<parser/symbol.h>
#include<parser/statement.h>

struct Stack{
    /* list of symbols defined in this stack */
    array symbols;
    /* parent stack, if any */
    Stack*parent;
    /*
    list of types defined in this stack
    
    i.e. item type is Type
    */
    array types;
    /* list of statements made in this stack, which may create temporary values that live on the stack
    
    item type is Statement
    */
    array statements;
};
// initialize a stack
void Stack_init(Stack*stack,Stack*parent);

enum STACK_PARSE_RESULT{
    /// @brief  stack was parsed successfully
    STACK_PRESENT,
    /// @brief  stack was not parsed successfully (e.g. syntax error)
    STACK_INVALID,
};
enum STACK_PARSE_RESULT Stack_parse(Stack*stack,struct TokenIter*token_iter);

/*
find a symbol by name, returns nullptr if none is found

returned pointer is valid until the module is destroyed (does not get invalidated from module changes)
*/
Symbol* Stack_findSymbol(Stack*stack,Token*name);

/*
add symbol to stack

makes a copy of the symbol
*/
void Stack_addSymol(Stack*stack,Symbol*symbol);

/*
find a type by name, returns nullptr if none is found

the returned pointer is valid until the module is destroyed (does not get invalidated from module changes)
*/
struct Type* Stack_findType(Stack*stack,Token*name);

/// @brief  type cannot be parsed separate from symbol, so we parse a symbol as combination of type and name parser
/// @param module module that provides the context for parsing
/// @param num_symbols number of symbols returned
/// @param symbols_out pointer to number of symbols (to be freed by the caller)
/// @param token_iter_in iterator over tokens to be consumed for symbol parsing, iterator is mutated only on success
enum SYMBOL_PARSE_RESULT SymbolDefinition_parse(Stack*Stack,int*num_symbol_defs,struct SymbolDefinition**symbols_out,struct TokenIter*token_iter_in,struct Symbol_parse_options*options);

enum STATEMENT_PARSE_RESULT{
	/// @brief statement was parsed successfully
	STATEMENT_PARSE_RESULT_PRESENT,
	/// @brief statement was not parsed successfully (e.g. syntax error)
	STATEMENT_PARSE_RESULT_INVALID,
};
enum STATEMENT_PARSE_RESULT Statement_parse(Stack*Stack,Statement*out,struct TokenIter*token_iter);
/* add already parsed statement to stack (assumes the statement is valid) */
void Stack_addStatement(Stack*stack,Statement*statement);

enum VALUE_PARSE_RESULT{
	/// @brief  value was parsed successfully
	VALUE_PRESENT,
	/// @brief  value was not parsed successfully (e.g. syntax error)
	VALUE_INVALID,
    VALUE_SYMBOL_UNKNOWN,
};
enum VALUE_PARSE_RESULT Value_parse(Stack*stack,Value*value,struct TokenIter*token_iter);
