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
enum TYPE_PRIMITIVE_KIND{
	TYPE_PRIMITIVE_KIND_UNKNOWN=0,

	TYPE_PRIMITIVE_KIND_VOID,
	TYPE_PRIMITIVE_KIND_BOOL,

	TYPE_PRIMITIVE_KIND_I8,
	TYPE_PRIMITIVE_KIND_I16,
	TYPE_PRIMITIVE_KIND_I32,
	TYPE_PRIMITIVE_KIND_I64,

	TYPE_PRIMITIVE_KIND_U8,
	TYPE_PRIMITIVE_KIND_U16,
	TYPE_PRIMITIVE_KIND_U32,
	TYPE_PRIMITIVE_KIND_U64,

	TYPE_PRIMITIVE_KIND_F32,
	TYPE_PRIMITIVE_KIND_F64,

	// for __builtin_va_list
	TYPE_PRIMITIVE_KIND_VA_LIST,

	// for __type
	TYPE_PRIMITIVE_KIND_TYPE,
	// for __any
	TYPE_PRIMITIVE_KIND_ANY,
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
		enum TYPE_PRIMITIVE_KIND primitive;
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
/* returns true if a is convertible to b */
bool Type_convertibleTo(Type*a,Type*b);

static Type
	Type_I8={
		.kind=TYPE_KIND_PRIMITIVE,
		.name=&(Token){.tag=TOKEN_TAG_SYMBOL,.p="i8",.len=2},
		.primitive=TYPE_PRIMITIVE_KIND_I8
	},
	Type_I16={
		.kind=TYPE_KIND_PRIMITIVE,
		.name=&(Token){.tag=TOKEN_TAG_SYMBOL,.p="i16",.len=3},
		.primitive=TYPE_PRIMITIVE_KIND_I16
	},
	Type_I32={
		.kind=TYPE_KIND_PRIMITIVE,
		.name=&(Token){.tag=TOKEN_TAG_SYMBOL,.p="i32",.len=3},
		.primitive=TYPE_PRIMITIVE_KIND_I32
	},
	Type_I64={
		.kind=TYPE_KIND_PRIMITIVE,
		.name=&(Token){.tag=TOKEN_TAG_SYMBOL,.p="i64",.len=3},
		.primitive=TYPE_PRIMITIVE_KIND_I64
	},
	Type_U8={
		.kind=TYPE_KIND_PRIMITIVE,
		.name=&(Token){.tag=TOKEN_TAG_SYMBOL,.p="u8",.len=3},
		.primitive=TYPE_PRIMITIVE_KIND_U8
	},
	Type_U16={
		.kind=TYPE_KIND_PRIMITIVE,
		.name=&(Token){.tag=TOKEN_TAG_SYMBOL,.p="u16",.len=3},
		.primitive=TYPE_PRIMITIVE_KIND_U16
	},
	Type_U32={
		.kind=TYPE_KIND_PRIMITIVE,
		.name=&(Token){.tag=TOKEN_TAG_SYMBOL,.p="u32",.len=3},
		.primitive=TYPE_PRIMITIVE_KIND_U32
	},
	Type_U64={
		.kind=TYPE_KIND_PRIMITIVE,
		.name=&(Token){.tag=TOKEN_TAG_SYMBOL,.p="u64",.len=3},
		.primitive=TYPE_PRIMITIVE_KIND_U64
	},
	Type_F32={
		.kind=TYPE_KIND_PRIMITIVE,
		.name=&(Token){.tag=TOKEN_TAG_SYMBOL,.p="f32",.len=3},
		.primitive=TYPE_PRIMITIVE_KIND_F32
	},
	Type_F64={
		.kind=TYPE_KIND_PRIMITIVE,
		.name=&(Token){.tag=TOKEN_TAG_SYMBOL,.p="f64",.len=3},
		.primitive=TYPE_PRIMITIVE_KIND_F64
	},
	Type_BOOL={
		.kind=TYPE_KIND_PRIMITIVE,
		.name=&(Token){.tag=TOKEN_TAG_SYMBOL,.p="bool",.len=4},
		.primitive=TYPE_PRIMITIVE_KIND_BOOL
	},
	Type_VOID={
		.kind=TYPE_KIND_PRIMITIVE,
		.name=&(Token){.tag=TOKEN_TAG_SYMBOL,.p="void",.len=4},
		.primitive=TYPE_PRIMITIVE_KIND_VOID
	},
	
	Type_CHAR={
		.kind=TYPE_KIND_PRIMITIVE,
		.name=&(Token){.tag=TOKEN_TAG_SYMBOL,.p="char",.len=4},
		.primitive=TYPE_PRIMITIVE_KIND_U8
	},
	Type_WCHAR={
		.kind=TYPE_KIND_PRIMITIVE,
		.name=&(Token){.tag=TOKEN_TAG_SYMBOL,.p="wchar",.len=5},
		.primitive=TYPE_PRIMITIVE_KIND_U16
	},
	Type_INT={
		.kind=TYPE_KIND_PRIMITIVE,
		.name=&(Token){.tag=TOKEN_TAG_SYMBOL,.p="int",.len=3},
		.primitive=TYPE_PRIMITIVE_KIND_I32
	},
	Type_LONG={
		.kind=TYPE_KIND_PRIMITIVE,
		.name=&(Token){.tag=TOKEN_TAG_SYMBOL,.p="long",.len=4},
		.primitive=TYPE_PRIMITIVE_KIND_I32
	},
	Type_LONGLONG={
		.kind=TYPE_KIND_PRIMITIVE,
		.name=&(Token){.tag=TOKEN_TAG_SYMBOL,.p="longlong",.len=8},
		.primitive=TYPE_PRIMITIVE_KIND_I64
	},

	Type_FLOAT={
		.kind=TYPE_KIND_PRIMITIVE,
		.name=&(Token){.tag=TOKEN_TAG_SYMBOL,.p="float",.len=5},
		.primitive=TYPE_PRIMITIVE_KIND_F32
	},
	Type_DOUBLE={
		.kind=TYPE_KIND_PRIMITIVE,
		.name=&(Token){.tag=TOKEN_TAG_SYMBOL,.p="double",.len=6},
		.primitive=TYPE_PRIMITIVE_KIND_F64
	},
	
	Type_STRING={.kind=TYPE_KIND_POINTER,.pointer={.base=&Type_CHAR}},
	Type_VA_LIST={
		.kind=TYPE_KIND_PRIMITIVE,
		.name=&(Token){.tag=TOKEN_TAG_SYMBOL,.p="__builtin_va_list",.len=17},
		.primitive=TYPE_PRIMITIVE_KIND_VA_LIST
	},
	Type_NULLPTR_T={
		.kind=TYPE_KIND_POINTER,
		.name=&(Token){.tag=TOKEN_TAG_SYMBOL,.p="nullptr_t",.len=9},
		.pointer={
			.base=&Type_VOID
		}
	},
	
	Type_TYPE={
		.kind=TYPE_KIND_PRIMITIVE,
		.name=&(Token){.tag=TOKEN_TAG_SYMBOL,.p="__type",.len=6},
		.primitive=TYPE_PRIMITIVE_KIND_TYPE
	},
	Type_ANY={
		.kind=TYPE_KIND_PRIMITIVE,
		.name=&(Token){.tag=TOKEN_TAG_SYMBOL,.p="__any",.len=5},
		.primitive=TYPE_PRIMITIVE_KIND_ANY
	};
	