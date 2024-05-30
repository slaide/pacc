#include<util/array.h>
#include<tokenizer.h>

struct Preprocessor;
struct PreprocessorExpression;

enum PreprocessorExpressionTag{
	PREPROCESSOR_EXPRESSION_TAG_UNKNOWN=0,

	PREPROCESSOR_EXPRESSION_TAG_DEFINED,

	PREPROCESSOR_EXPRESSION_TAG_NOT,
	PREPROCESSOR_EXPRESSION_TAG_AND,
	PREPROCESSOR_EXPRESSION_TAG_OR,

	PREPROCESSOR_EXPRESSION_TAG_GREATER_THAN,
	PREPROCESSOR_EXPRESSION_TAG_LESSER_THAN,

	PREPROCESSOR_EXPRESSION_TAG_LITERAL,

	PREPROCESSOR_EXPRESSION_TAG_ELSE,
};
struct PreprocessorExpression{
	enum PreprocessorExpressionTag tag;
	/* (cached) value that this expression evaluates to */
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
			struct PreprocessorExpression*lhs;
			struct PreprocessorExpression*rhs;
		}greater_than;
		struct{
			struct PreprocessorExpression*lhs;
			struct PreprocessorExpression*rhs;
		}lesser_than;
		struct{
			int _reservedAndUnused;
		}else_;
		struct{
			Token token;
		}literal;
	};
};
/*
parse expression by consuming tokens (until end of line), then evaluating the expression

can use: <symbol>, <symbol>(args), defined, !, &&, ||, ==, !=, <=, >=, <, >, +, -, *, /, %, (, )

e.g.
1) #if FOO
2) #if defined(FOO)
3) #if !defined(FOO)
4) #if defined(FOO) && defined(BAR)
5) #if defined(FOO) || defined(BAR)
6) #if defined(FOO) && defined(BAR) || defined(BAZ)
7) #if defined(FOO) && ( defined(BAR) || defined(BAZ) )
*/
struct PreprocessorExpression*Preprocessor_parseExpression(struct Preprocessor*preprocessor);

/* stack of if[/elif[/else]] directives */
struct PreprocessorIfStack{
	/* if any parent statement has not evaluated to true, parse this stack, but do no evalute any clause and hence do not take any path */
	bool inherited_doSkip;
	/* any previous path in this stack has evaluated to true, used to track if a newly added may still be taken or not */
	bool anyPathEvaluatedToTrue;
	/* array of struct PreprocessorStackItem */
	array items;
};
struct PreprocessorIfStackItem{
	enum{
		PREPROCESSOR_STACK_ITEM_TYPE_UNKNOWN=0,

		PREPROCESSOR_STACK_ITEM_TYPE_IF,
		PREPROCESSOR_STACK_ITEM_TYPE_ELSE_IF,
		PREPROCESSOR_STACK_ITEM_TYPE_ELSE,
	}tag;
	union{
		struct{
			/* the if token at the start of the statement */
			Token if_token;
			struct PreprocessorExpression*expr;
		}if_;
		struct{
			/* the else token at the start of the statement */
			Token else_token;
			struct PreprocessorExpression*expr;
		}else_if;
		struct{
			/* the else token */
			Token else_token;
		}else_;
	};
};
/* get value of last item in the stack (fatal if called on empty stack) */
bool PreprocessorIfStack_getLastValue(struct PreprocessorIfStack*item);

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

	/* stack of struct PreprocessorIfStack (for [nested] if statements) */
	array stack;

	/* based on if directives, quickly indicate if non-conditional statements should be processed (include non-directive tokens) */
	bool doSkip;
};
/* initialize fields, must be called before any other function is called */
void Preprocessor_init(struct Preprocessor*preprocessor);

/* evaluate expression and return its value (see also docs for struct PreprocessorExpression) */
int Preprocessor_evalExpression(struct Preprocessor *preprocessor,struct PreprocessorExpression*expr);

/* process an include statement */
void Preprocessor_processInclude(struct Preprocessor*preprocessor);
/* process a define statements */
void Preprocessor_processDefine(struct Preprocessor*preprocessor);

/* run preprocessor on a stream of tokens */
void Preprocessor_consume(struct Preprocessor *preprocessor, struct TokenIter *token_iter);
