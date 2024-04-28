#pragma once

#include<util/array.h>
#include<tokenizer.h>

typedef struct Value Value;

#include<parser/type.h>

enum VALUE_KIND{
	VALUE_KIND_UNKNOWN,

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

	VALUE_KIND_CONDITIONAL,

	VALUE_KIND_TYPEREF,
};
const char* ValueKind_asString(enum VALUE_KIND kind);

enum VALUE_OPERATOR{
	VALUE_OPERATOR_UNKNOWN,
	
	VALUE_OPERATOR_ADD,
	VALUE_OPERATOR_SUB,
	VALUE_OPERATOR_MULT,
	VALUE_OPERATOR_DIV,
	VALUE_OPERATOR_MODULO,
	VALUE_OPERATOR_LESS_THAN,
	VALUE_OPERATOR_GREATER_THAN,
	VALUE_OPERATOR_LESS_THAN_OR_EQUAL,
	VALUE_OPERATOR_GREATER_THAN_OR_EQUAL,

	VALUE_OPERATOR_ASSIGNMENT,
	VALUE_OPERATOR_ADD_ASSIGN,
	VALUE_OPERATOR_SUB_ASSIGN,
	VALUE_OPERATOR_MULT_ASSIGN,
	VALUE_OPERATOR_DIV_ASSIGN,
	VALUE_OPERATOR_MODULO_ASSIGN,
	VALUE_OPERATOR_BITWISE_AND_ASSIGN,
	VALUE_OPERATOR_BITWISE_OR_ASSIGN,
	VALUE_OPERATOR_BITWISE_XOR_ASSIGN,
	VALUE_OPERATOR_LEFT_SHIFT_ASSIGN,
	VALUE_OPERATOR_RIGHT_SHIFT_ASSIGN,

	VALUE_OPERATOR_POSTFIX_INCREMENT,
	VALUE_OPERATOR_POSTFIX_DECREMENT,

	VALUE_OPERATOR_LOGICAL_AND,
	VALUE_OPERATOR_LOGICAL_OR,

	VALUE_OPERATOR_BITWISE_AND,
	VALUE_OPERATOR_BITWISE_OR,

	VALUE_OPERATOR_EQUAL,
	VALUE_OPERATOR_NOT_EQUAL,

	VALUE_OPERATOR_LOGICAL_NOT,
	VALUE_OPERATOR_BITWISE_NOT,
	
	VALUE_OPERATOR_CALL,
	VALUE_OPERATOR_INDEX,
	VALUE_OPERATOR_DOT,
	VALUE_OPERATOR_ARROW,
	VALUE_OPERATOR_DEREFERENCE,

	/// the only ternary operator (3 operands)
	VALUE_OPERATOR_CONDITIONAL,
};

char*Value_asString(Value*value);

enum FieldInitializerSegmentKind{
	FIELD_INITIALIZER_SEGMENT_UNKNOWN,
	FIELD_INITIALIZER_SEGMENT_FIELD,
	FIELD_INITIALIZER_SEGMENT_INDEX,
};
struct FieldInitializerSegment{
	enum FieldInitializerSegmentKind kind;
	union{
		Token*field;
		Token*index;
	};
};
struct FieldInitializer{
	/// @brief field name segments, of type FieldInitializerSegment, used for array and struct initializers
	///
	/// may be nested
	///
	/// e.g.
	/// 1) {field1value}
	/// 2) {.field=field1value}
	/// 3) (.field.subfield.innervalue=field1value}
	/// 4) [1]=field1value
	array fieldNameSegments;
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

		/// @brief VALUE_KIND_DOT dot operator
		struct{
			Value*left;
			Token*right;
		}dot;

		/// @brief VALUE_KIND_ARROW arrow operator
		struct{
			Value*left;
			Token*right;
		}arrow;

		/// @brief VALUE_KIND_ADDRESS_OF address of operator
		struct{
			Value*addressedValue;
		}addrOf;

		/// @brief VALUE_KIND_STRUCT_INITIALIZER struct initializer
		struct{
			/// array of struct StructFieldInitializer
			array structFields;
		}struct_initializer;

		/// @brief VALUE_KIND_PARENS_WRAPPED parens wrapped value
		struct{
			Value*innerValue;
		}parens_wrapped;

		/// @brief VALUE_KIND_CAST cast operation
		struct{
			/// tricky.. castTo should be a type, but we dont know that it is a type while parsing
			/// and a type may look like a value expression when used for casting..
			Type*castTo;
			Value*value;
		}cast;

		/// @brief VALUE_KIND_TYPEREF type reference
		/// enables using a type as a value, e.g. for function-like macros, like sizeof
		struct{
			Type*type;
		}typeref;

		/// @brief VALUE_KIND_CONDITIONAL conditional operator
		/// e.g. a?b:c
		struct{
			Value*condition;
			Value*onTrue;
			Value*onFalse;
		}conditional;
	};
};

enum VALUE_PARSE_RESULT{
	/// @brief  value was parsed successfully
	VALUE_PRESENT,
	/// @brief  value was not parsed successfully (e.g. syntax error)
	VALUE_INVALID,
};
enum VALUE_PARSE_RESULT Value_parse(Value*value,struct TokenIter*token_iter);
bool Value_equal(Value*a,Value*b);
