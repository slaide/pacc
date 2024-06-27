#pragma once

#include<util/array.h>
#include<tokenizer.h>

typedef struct Type Type;

#include<parser/value.h>

enum TYPEKIND{
	TYPE_KIND_UNKNOWN=0,

	TYPE_KIND_REFERENCE,
	TYPE_KIND_POINTER,
	TYPE_KIND_ARRAY,
	TYPE_KIND_FUNCTION,

	TYPE_KIND_STRUCT,
	TYPE_KIND_UNION,
	TYPE_KIND_ENUM,

	TYPE_KIND_TYPE,

	TYPE_KIND_PRIMITIVE,
};
struct EnumVariant{
	// name of the variant, must be present
	Token*name;
	// value is optional
	Value*value;
};
char* TypeKind_asString(enum TYPEKIND kind);

struct Type{
	bool is_thread_local;
	bool is_static;
	bool is_const;
	bool is_extern;
	bool is_unsigned;
	bool is_signed;
	short size_mod;

	/// @brief name of the type, if any (e.g. for struct, union, enum, typedef, etc.)
	Token* name;

	enum TYPEKIND kind;
	union{
		/// @brief reference some type by name
		struct{
			Type*ref;
		}reference;
		/// @brief pointing to another type
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
			Value* len;
			bool is_static;
		}array;
		struct{
			/// @brief array of argument symbols
			array args;
			Type* ret;
		}function;
		struct{
			Token*name;
			/// @brief array of struct members, which are Symbols
			array members;
		}struct_;
		struct{
			Token*name;
			/// @brief array of struct members, which are Symbols
			array members;
		}union_;
		struct{
			Token*name;
			/*
			array of enum variants
			
			item type is struct EnumVariant
			*/
			array members;
		}enum_;
		struct{
			Type*type;
		}type;
	};
};
char* Type_asString(Type* type);

void Type_init(Type**type);
bool Type_equal(Type*a,Type*b);
