#pragma once

#include<util/array.h>
#include<tokenizer.h>

typedef struct Statement Statement;

#include<parser/symbol.h>
#include<parser/value.h>
#include<parser/stack.h>

enum STATEMENT_KIND{
	STATEMENT_KIND_UNKNOWN=0,

	/// e.g. just a semicolon
	STATEMENT_KIND_EMPTY,

	/// @brief function definition, e.g. int foo(int a, int b){ return a+b; }
	STATEMENT_KIND_FUNCTION_DEFINITION,
	/// @brief return statement, e.g. return 0;
	STATEMENT_KIND_RETURN,
	/// @brief if statement with body
	STATEMENT_KIND_IF,
	/// @brief switch statement with body (cases are regular statements in this body)
	STATEMENT_KIND_SWITCH,
	/// @brief a case statement, e.g. case 0:
	STATEMENT_KIND_SWITCHCASE,

	/// @brief a break statement, e.g. break;
	STATEMENT_KIND_BREAK,
	/// @brief a continue statement, e.g. continue;
	STATEMENT_KIND_CONTINUE,
	/// @brief default label in switch case statement, e.g. default:
	STATEMENT_KIND_DEFAULT,
	/// @brief goto statement, e.g. goto label;
	STATEMENT_KIND_GOTO,
	STATEMENT_KIND_LABEL,
	STATEMENT_KIND_WHILE,
	STATEMENT_KIND_FOR,
	STATEMENT_KIND_TYPEDEF,

	STATEMENT_KIND_BLOCK,
	
	STATEMENT_KIND_VALUE,

	STATEMENT_KIND_SYMBOL_DEFINITION,
};
const char* Statementkind_asString(enum STATEMENT_KIND kind);

char*Statement_asString(Statement*statement,int indentDepth);

enum VALUE_GOTO_LABEL_VARIANT{
	VALUE_GOTO_LABEL_VARIANT_LABEL,
	VALUE_GOTO_LABEL_VARIANT_VALUE,
};

struct Statement{
	enum STATEMENT_KIND tag;
	union{
		/// @brief function definition
		/// also see STATEMENT_FUNCTION_DECLARATION
		struct{
			Symbol symbol;
			Stack*stack;
		}functionDef;

		struct{
			/* item type is struct SymbolDefinition */
			array symbols_defs;
		}symbolDef;

		/// @brief return statement
		struct{
			Value* retval;
		}return_;

		struct{
			Value* value;
		}value;

		struct{
			Stack*stack;
		}block;

		struct{
			Value* condition;
			Statement* body;
			Statement* elseBody;
		}if_;

		struct{
			Statement* init;
			Value* condition;
			Value* step;

			Stack*stack;
		}forLoop;

		struct{
			Value* condition;
			Statement* body;
			bool doWhile;
		}whileLoop;

		struct{
			Value* condition;
			/* item type is Statement */
			array body;
		}switch_;

		struct{
			Value* value;
		}switchCase;

		struct{
			enum VALUE_GOTO_LABEL_VARIANT variant;
			union{
				/// @brief value allows computed goto
				Value*label;
				Token*labelName;
			};
		}goto_;

		struct{
			Token* label;
		}labelDefinition;

		struct{
			/* item type is struct Symbol */
			array symbols;
		}typedef_;
	};
};
bool Statement_equal(Statement*a,Statement*b);
