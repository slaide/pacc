#pragma once

#include<util/array.h>
#include<tokenizer.h>

#include<parser/type.h>

enum VALUE_KIND{
	VALUE_KIND_STATIC_VALUE,
	VALUE_KIND_OPERATOR,
	VALUE_KIND_SYMBOL_REFERENCE,
	VALUE_KIND_FUNCTION_CALL,
	VALUE_KIND_ARROW,
	VALUE_KIND_DOT,
	VALUE_KIND_ADDRESS_OF,
	VALUE_KIND_STRUCT_INITIALIZER,
	VALUE_KIND_PARENS_WRAPPED,
	VALUE_KIND_CAST,
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
	VALUE_OPERATOR_INDEX,
	VALUE_OPERATOR_DOT,
	VALUE_OPERATOR_ARROW,
};
typedef struct Value Value;

char*Value_asString(Value*value);

struct StructFieldInitializer{
	Token*name;
	Value*value;
};
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

		/// @brief VALUE_KIND_FUNCTION_CALL function call
		struct{
			Token*name;
			array args;
		}function_call;

		struct{
			Value*left;
			Token*right;
		}dot;

		struct{
			Value*left;
			Token*right;
		}arrow;

		struct{
			Value*addressedValue;
		}addrOf;

		struct{
			/// array of struct StructInitializerField
			array structFields;
		}struct_initializer;

		struct{
			Value*innerValue;
		}parens_wrapped;

		/// hacky implementation of a cast operation
		struct{
			/// tricky.. castTo should be a type, but we dont know that it is a type while parsing
			/// and a type may look like a value expression when used for casting..
			Value*castTo;
			Value*value;
		}cast;
	};
	Type* type;
};

enum VALUE_PARSE_RESULT{
	/// @brief  value was parsed successfully
	VALUE_PRESENT,
	/// @brief  value was not parsed successfully (e.g. syntax error)
	VALUE_INVALID,
};
enum VALUE_PARSE_RESULT Value_parse(Value*value,struct TokenIter*token_iter);
bool Value_equal(Value*a,Value*b);
