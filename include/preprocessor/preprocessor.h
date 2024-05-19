#include<util/array.h>
#include<tokenizer.h>

struct Preprocessor;
struct PreprocessorExpression;

enum PreprocessorExpressionTag{
	PREPROCESSOR_EXPRESSION_TAG_DEFINED,

	PREPROCESSOR_EXPRESSION_TAG_NOT,
	PREPROCESSOR_EXPRESSION_TAG_AND,
	PREPROCESSOR_EXPRESSION_TAG_OR,
	PREPROCESSOR_EXPRESSION_TAG_LITERAL,

	PREPROCESSOR_EXPRESSION_TAG_ELSE,
};
struct PreprocessorExpression{
	enum PreprocessorExpressionTag tag;
	/* value that this expression evaluates to */
	int value;
	union{
		struct{
			char* name;
		}defined;
		struct{
			struct PreprocessorExpression*expr;
		}not;
		struct{
			struct PreprocessorExpression*lhs;
			struct PreprocessorExpression*rhs;
		}and;
		struct{
			struct PreprocessorExpression*lhs;
			struct PreprocessorExpression*rhs;
		}or;
		struct{
			int _reservedAndUnused;
		}else_;
		struct{
			Token token;
		}literal;
	};
};

struct PreprocessorStack{
	array items;
};
struct PreprocessorStackItem{
	enum{
		PREPROCESSOR_STACK_ITEM_TYPE_IF,
		PREPROCESSOR_STACK_ITEM_TYPE_ELSE,
		PREPROCESSOR_STACK_ITEM_TYPE_ELSE_IF,
	}tag;
	union{
		struct{
			/* the if token at the start of the statement */
			Token if_token;
			struct PreprocessorExpression*expr;
		}if_;
		struct{
			Token else_token;
			int _reservedAndUnused;
		}else_;
		struct{
			Token else_token;
			struct PreprocessorExpression*expr;
			bool anyPreviousIfEvaluatedToTrue;
		}else_if;
	};
};
struct PreprocessorDefine{
	Token name;
	array tokens;
};
struct Preprocessor{
	/* include paths, type char* */
	array include_paths;
	
	/* definitions, element type is struct PreprocessorDefine */
	array defines;
	/* protect against double include with pragma once, the results of which are saved here, i.e. element is char* */
	array already_included_files;

	/* iterator over tokenizer_in */
	struct TokenIter token_iter;

	/* eventual output*/
	array tokens_out;

	/* stack of preprocessor directives, e.g. for nested ifs */
	struct PreprocessorStack stack;
};
void Preprocessor_init(struct Preprocessor*preprocessor);

int Preprocessor_evalExpression(struct Preprocessor *preprocessor,struct PreprocessorExpression*expr);

/* process an include statement */
void Preprocessor_processInclude(struct Preprocessor*preprocessor);
/* process a define statements */
void Preprocessor_processDefine(struct Preprocessor*preprocessor);
/* run preprocessor on a stream of tokens */
void Preprocessor_consume(struct Preprocessor *preprocessor, struct TokenIter *token_iter);
