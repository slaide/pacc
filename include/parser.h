#pragma once

#include<util/util.h>
#include<util/array.h>
#include<tokenizer.h>

enum TYPEKIND{
	TYPE_KIND_UNKNOWN=0,

	TYPE_KIND_REFERENCE,
	TYPE_KIND_POINTER,
	TYPE_KIND_ARRAY,
	TYPE_KIND_FUNCTION,
	TYPE_KIND_STRUCT,
	TYPE_KIND_UNION,
	TYPE_KIND_ENUM,	
};
typedef struct Type Type;
struct Type{
	bool is_const;

	/**
	 * @brief name of the type, if any (e.g. for struct, union, enum, typedef, etc.)
	 * 
	 */
	Token* name;

	enum TYPEKIND kind;
	union{
		/**
		 * @brief reference some type by name
		 * 
		 */
		Token reference;
		/**
		 * @brief pointing to another type
		 * 
		 */
		struct{
			Type* base;
		}pointer;
		/**
		 * @brief array of some type
		 * 
		 * len is the number of elements in the array, or -1 if none is specified
		 */
		struct{
			Type* base;
			int len;
		}array;
		struct{
			/**
			 * @brief array of argument symbols
			 * 
			 */
			array args;
			Type* ret;
		}function;
	};
};
void Type_print(Type* type);


enum VALUE_KIND{
	VALUE_KIND_STATIC_VALUE,
	VALUE_KIND_OPERATOR,
	VALUE_KIND_SYMBOL_REFERENCE,
};
const char* ValueKind_asString(enum VALUE_KIND kind);

enum VALUE_OPERATOR{
	VALUE_OPERATOR_ADD,
	VALUE_OPERATOR_SUB,
	VALUE_OPERATOR_MULT,
	VALUE_OPERATOR_DIV,
	VALUE_OPERATOR_LESS_THAN,
	VALUE_OPERATOR_GREATER_THAN,
	VALUE_OPERATOR_POSTFIX_INCREMENT,
	VALUE_OPERATOR_POSTFIX_DECREMENT,
	
	VALUE_OPERATOR_CALL,
	VALUE_OPERATOR_DOT,
	VALUE_OPERATOR_ARROW,
};
typedef struct Value Value;
struct Value{
	enum VALUE_KIND kind;
	union{
		/// @brief VALUE_KIND_SYMBOL_REFERENCE reference to a symbol
		Token*symbol;
		/// @brief VALUE_KIND_STATIC_VALUE static value
		struct{
			Token*value_repr;
		}static_value;
		/// @brief VALUE_KIND_OPERATOR operator
		struct{
			Value*left;
			Value*right;
			enum VALUE_OPERATOR op;
		}op;
	};
	Type* type;
};

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
};
/// @brief  type cannot be parsed separate from symbol, so we parse a symbol as combination of type and name parser
/// @param symbol 
/// @param token_iter 
enum SYMBOL_PARSE_RESULT Symbol_parse(Symbol*symbol,struct TokenIter*token_iter);

enum STATEMENT_KIND{
	STATEMENT_UNKNOWN=0,

	STATEMENT_PREP_DEFINE,
	STATEMENT_PREP_INCLUDE,

	/// @brief function declaration, e.g. int foo(int a, int b);
	STATEMENT_FUNCTION_DECLARATION,
	/// @brief function definition, e.g. int foo(int a, int b){ return a+b; }
	STATEMENT_FUNCTION_DEFINITION,
	/// @brief return statement, e.g. return 0;
	STATEMENT_RETURN,
	STATEMENT_IF,
	STATEMENT_SWITCH,
	STATEMENT_CASE,
	STATEMENT_BREAK,
	STATEMENT_CONTINUE,
	STATEMENT_DEFAULT,
	STATEMENT_GOTO,
	STATEMENT_LABEL,
	STATEMENT_WHILE,
	STATEMENT_FOR,
};
const char* Statementkind_asString(enum STATEMENT_KIND kind);

typedef struct Statement Statement;
struct Statement{
	enum STATEMENT_KIND tag;
	union{
		/// @brief #define preprocessor directive
		/// also see STATEMENT_PREP_DEFINE_ARGUMENT
		struct{
			Token name;

			/// @brief array of argument tokens
			array args;
			
			/// @brief array of body tokens
			array body;
		}prep_define;

		/// @brief #include preprocessor directive
		/// also see STATEMENT_PREP_INCLUDE_ARGUMENT
		struct{
			Token path;
		}prep_include;

		/// @brief function declaration
		/// also see STATEMENT_FUNCTION_DEFINITION
		struct{
			Symbol symbol;
		}functionDecl;

		/// @brief function definition
		/// also see STATEMENT_FUNCTION_DECLARATION
		struct{
			Symbol symbol;
			array bodyStatements;
		}functionDef;


		/// @brief return statement
		struct{
			Value* retval;
		}return_;
	};
};

typedef struct Module Module;
void Module_parse(Module* module,Tokenizer*tokenizer);
struct Module{
	array statements;
};
