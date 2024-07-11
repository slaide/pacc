#include<parser/parser.h>
#include<util/util.h>
#include<tokenizer.h>

void Module_init(Module*module){
	Stack_init(&module->stack,nullptr);

	// add basic types

	{ // int
		Type*newtype=&Type_INT;
		array_append(&module->stack.types,&newtype);
	}
	{ // char
		Type*newtype=&Type_CHAR;
		array_append(&module->stack.types,&newtype);
	}
	{ // bool
		Type*newtype=&Type_BOOL;
		array_append(&module->stack.types,&newtype);
	}
	{ // void
		Type*newtype=&Type_VOID;
		array_append(&module->stack.types,&newtype);
	}
	{ // float
		Type*newtype=&Type_FLOAT;
		array_append(&module->stack.types,&newtype);
	}
	{ // double
		Type*newtype=&Type_DOUBLE;
		array_append(&module->stack.types,&newtype);
	}
	{ // nullptr_t
		Type*newtype=&Type_NULLPTR_T;
		array_append(&module->stack.types,&newtype);
	}
	{ // __builtin_va_list
		// the actual type is implementation defined
		// pointer-like though, it seems
		// gets aliased as va_list in stdarg.h
		Type*newtype=&Type_VA_LIST;
		array_append(&module->stack.types,&newtype);
	}
	{ // __type
		// for builtin functions taking a type as argument, e.g. sizeof
		Type*newtype=&Type_TYPE;
		array_append(&module->stack.types,&newtype);
	}
	{ // __any
		// to perform magic in builtin functions
		Type*newtype=&Type_ANY;
		array_append(&module->stack.types,&newtype);
	}
	

	// add builtin symbols
	// 1) sizeof function, which takes a TYPE_KIND_TYPE as argument and returns an int
	{
		Symbol symbol={
			.kind=SYMBOL_KIND_DECLARATION,
			.name=Token_fromString("sizeof"),
			.type=allocAndCopy(sizeof(Type),&(Type){
				.kind=TYPE_KIND_FUNCTION,
				.function={
					.args={},
					.ret=&Type_INT,
				},
			})
		};
		array_init(&symbol.type->function.args,sizeof(Symbol));
		array_append(&symbol.type->function.args,&(Symbol){
			.kind=SYMBOL_KIND_DECLARATION,
			.name=Token_fromString("type"),
			.type=&Type_ANY,
		});

		Stack_addSymol(&module->stack,&symbol);
	}
	// typeof function, which takes a TYPE_KIND_TYPE as argument and returns an int
	{
		Symbol symbol={
			.kind=SYMBOL_KIND_DECLARATION,
			.name=Token_fromString("typeof"),
			.type=allocAndCopy(sizeof(Type),&(Type){
				.kind=TYPE_KIND_FUNCTION,
				.function={
					.args={},
					.ret=&Type_TYPE,
				},
			})
		};
		array_init(&symbol.type->function.args,sizeof(Symbol));
		array_append(&symbol.type->function.args,&(Symbol){
			.kind=SYMBOL_KIND_DECLARATION,
			.name=Token_fromString("type"),
			.type=&Type_ANY,
		});

		Stack_addSymol(&module->stack,&symbol);
	}
	// nullptr, basic symbol of type nullptr_t
	{
		Symbol symbol={
			.kind=SYMBOL_KIND_DECLARATION,
			.name=Token_fromString("nullptr"),
			.type=&Type_NULLPTR_T,
		};
		Stack_addSymol(&module->stack,&symbol);
	}
	// true and false
	{
		Symbol symbol={
			.kind=SYMBOL_KIND_DECLARATION,
			.name=Token_fromString("true"),
			.type=&Type_BOOL,
		};
		Stack_addSymol(&module->stack,&symbol);

		symbol.name=Token_fromString("false");
		Stack_addSymol(&module->stack,&symbol);
	}
	// vararg related builtin magic functions
	{
		// __builtin_va_start should be a macro, but for now, i dont care
		// __builtin_va_start(va_list ap, <name of last arg in the function before varargs start, so e.g. int f(int b,...) -> va_start(my_va_list,b); even though b does not directly have something to do with the vararg list>)
		Symbol symbol={
			.kind=SYMBOL_KIND_DECLARATION,
			.name=Token_fromString("__builtin_va_start"),
			.type=allocAndCopy(sizeof(Type),&(Type){
				.kind=TYPE_KIND_FUNCTION,
				.function={
					.args={},
					.ret=&Type_VOID
				},
			}),
		};
		array_init(&symbol.type->function.args,sizeof(Symbol));
		array_append(&symbol.type->function.args,&(Symbol){
			.kind=SYMBOL_KIND_DECLARATION,
			.name=Token_fromString("ap"),
			.type=&Type_VA_LIST,
		});
		array_append(&symbol.type->function.args,&(Symbol){
			.kind=SYMBOL_KIND_VARARG,
			.type=&Type_VA_LIST,
		});
		Stack_addSymol(&module->stack,&symbol);

		// __builtin_va_end should be a macro, but for now, i dont care
		// __builtin_va_arg(va_list ap, <type of next argument>)
		symbol= (Symbol){
			.kind=SYMBOL_KIND_DECLARATION,
			.name=Token_fromString("__builtin_va_arg"),
			.type=allocAndCopy(sizeof(Type),&(Type){
				.kind=TYPE_KIND_FUNCTION,
				.function={
					.args={},
					.ret=&Type_ANY,
				},
			}),
		};
		array_init(&symbol.type->function.args,sizeof(Symbol));
		array_append(&symbol.type->function.args,&(Symbol){
			.kind=SYMBOL_KIND_DECLARATION,
			.name=Token_fromString("ap"),
			.type=&Type_VA_LIST,
		});
		array_append(&symbol.type->function.args,&(Symbol){
			.kind=SYMBOL_KIND_DECLARATION,
			.name=Token_fromString("type"),
			.type=&Type_TYPE,
		});
		Stack_addSymol(&module->stack,&symbol);

		// __builtin_va_end can be a macro or a function
		// __builtin_va_end(va_list ap)
		symbol= (Symbol){
			.kind=SYMBOL_KIND_DECLARATION,
			.name=Token_fromString("__builtin_va_end"),
			.type=allocAndCopy(sizeof(Type),&(Type){
				.kind=TYPE_KIND_FUNCTION,
				.function={
					.args={},
					.ret=&Type_VOID,
				},
			}),
		};
		array_init(&symbol.type->function.args,sizeof(Symbol));
		array_append(&symbol.type->function.args,&(Symbol){
			.kind=SYMBOL_KIND_DECLARATION,
			.name=Token_fromString("ap"),
			.type=&Type_VA_LIST,
		});
		Stack_addSymol(&module->stack,&symbol);

		// __builtin_va_copy can be a macro or a function
		// __builtin_va_copy(va_list dest, va_list src)
		symbol= (Symbol){
			.kind=SYMBOL_KIND_DECLARATION,
			.name=Token_fromString("__builtin_va_copy"),
			.type=allocAndCopy(sizeof(Type),&(Type){
				.kind=TYPE_KIND_FUNCTION,
				.function={
					.args={},
					.ret=nullptr,
				},
			}),
		};
		array_init(&symbol.type->function.args,sizeof(Symbol));
		array_append(&symbol.type->function.args,&(Symbol){
			.kind=SYMBOL_KIND_DECLARATION,
			.name=Token_fromString("dest"),
			.type=&Type_VA_LIST,
		});
		array_append(&symbol.type->function.args,&(Symbol){
			.kind=SYMBOL_KIND_DECLARATION,
			.name=Token_fromString("src"),
			.type=&Type_VA_LIST,
		});
		Stack_addSymol(&module->stack,&symbol);
	}
}
enum MODULE_PARSE_RESULT Module_parse(Module* module,struct TokenIter*token_iter_in){
	struct TokenIter token_iter=*token_iter_in;

	Stack_parse(&module->stack,&token_iter);

	*token_iter_in=token_iter;
	return MODULE_PRESENT;
}
bool Module_equal(Module*a,Module*b){
	if(a->stack.statements.len!=b->stack.statements.len){
		println("statement count mismatch %d %d",a->stack.statements.len,b->stack.statements.len);
		return false;
	}

	for(int i=0;i<a->stack.statements.len;i++){
		Statement*sa=array_get(&a->stack.statements,i);
		Statement*sb=array_get(&b->stack.statements,i);
		if(!Statement_equal(sa,sb)){
			println("statement %d mismatch",i);
			return false;
		}
	}
	return true;
}
