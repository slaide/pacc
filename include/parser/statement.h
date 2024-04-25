#pragma once

#include<util/array.h>
#include<tokenizer.h>

#include<parser/symbol.h>
#include<parser/value.h>

enum STATEMENT_KIND{
	STATEMENT_UNKNOWN=0,

	/// e.g. just a semicolon
	STATEMENT_EMPTY,

	STATEMENT_PREP_DEFINE,
	STATEMENT_PREP_INCLUDE,

	/// @brief function declaration, e.g. int foo(int a, int b);
	STATEMENT_FUNCTION_DECLARATION,
	/// @brief function definition, e.g. int foo(int a, int b){ return a+b; }
	STATEMENT_FUNCTION_DEFINITION,
	/// @brief return statement, e.g. return 0;
	STATEMENT_KIND_RETURN,
	/// @brief if statement with body
	STATEMENT_KIND_IF,
	/// @brief switch statement with body (cases are regular statements in this body)
	STATEMENT_SWITCH,
	/// @brief a case statement, e.g. case 0:
	STATEMENT_SWITCHCASE,

	/// @brief a break statement, e.g. break;
	STATEMENT_BREAK,
	/// @brief a continue statement, e.g. continue;
	STATEMENT_CONTINUE,
	/// @brief default label in switch case statement, e.g. default:
	STATEMENT_DEFAULT,
	/// @brief goto statement, e.g. goto label;
	STATEMENT_GOTO,
	STATEMENT_LABEL,
	STATEMENT_KIND_WHILE,
	STATEMENT_KIND_FOR,
	STATEMENT_TYPEDEF,

	STATEMENT_BLOCK,
	
	STATEMENT_VALUE,

	STATEMENT_KIND_SYMBOL_DEFINITION,
};
const char* Statementkind_asString(enum STATEMENT_KIND kind);

typedef struct Statement Statement;

char*Statement_asString(Statement*statement,int indentDepth);

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

		struct{
			Symbol symbol;
			Value*init_value;
		}symbolDef;

		/// @brief return statement
		struct{
			Value* retval;
		}return_;

		struct{
			Value* value;
		}value;

		struct{
			Statement* init;
			Value* condition;
			Value* step;

			array body;
		}forLoop;

		struct{
			array body;
		}block;

		struct{
			Value* condition;
			Statement* body;
			Statement* elseBody;
		}if_;

		struct{
			Value* condition;
			array body;
			bool doWhile;
		}whileLoop;

		struct{
			Value* condition;
			array body;
		}switch_;

		struct{
			Value* value;
		}switchCase;

		struct{
			/// @brief label, which is value instead of token since C23 computed goto
			Value*label;
		}goto_;

		struct{
			Token* label;
		}labelDefinition;

		struct{
			Symbol*symbol;
		}typedef_;
	};
};

enum STATEMENT_PARSE_RESULT{
	/// @brief statement was parsed successfully
	STATEMENT_PARSE_RESULT_PRESENT,
	/// @brief statement was not parsed successfully (e.g. syntax error)
	STATEMENT_PARSE_RESULT_INVALID,
};
enum STATEMENT_PARSE_RESULT Statement_parse(Statement*out,struct TokenIter*token_iter);
bool Statement_equal(Statement*a,Statement*b);
