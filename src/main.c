#include <util/util.h>
#include <parser.h>

bool Type_equal(Type*a,Type*b);
bool Symbol_equal(Symbol*a,Symbol*b);
bool Statement_equal(Statement*a,Statement*b);
bool Module_equal(Module*a,Module*b);
bool Value_equal(Value*a,Value*b);


bool Value_equal(Value*a,Value*b){
	if(a->kind!=b->kind){
		println("kind mismatch when comparing values %s %s",ValueKind_asString(a->kind),ValueKind_asString(b->kind));
		return false;
	}

	switch(a->kind){
		case VALUE_KIND_STATIC_VALUE:{
			bool ret=Token_equalToken(a->static_value.value_repr,b->static_value.value_repr);
			if(!ret){
				println("value mismatch when comparing static values %.*s %.*s",a->static_value.value_repr->len,a->static_value.value_repr->p,b->static_value.value_repr->len,b->static_value.value_repr->p);
			}
			return ret;
		}
		default:
			fatal("unimplemented %d",a->kind);
	}
}
bool Type_equal(Type*a,Type*b){
	if(a==b){
		return true;
	}
	if(a==nullptr || b==nullptr){
		return false;
	}

	if(!Token_equalToken(a->name,b->name)){
		println("name mismatch when comparing types %.*s %.*s",a->name->len,a->name->p,b->name->len,b->name->p);
		return false;
	}

	if(a->kind!=b->kind){
		println("kind mismatch when comparing types %d %d",a->kind,b->kind);
		return false;
	}

	switch(a->kind){
		case TYPE_KIND_REFERENCE:
			return Token_equalToken(&a->reference,&b->reference);
		case TYPE_KIND_POINTER:
			return Type_equal(a->pointer.base,b->pointer.base);
		case TYPE_KIND_ARRAY:
			return Type_equal(a->array.base,b->array.base) && a->array.len==b->array.len;
		case TYPE_KIND_FUNCTION:
			if(!Type_equal(a->function.ret,b->function.ret)){
				println("return type mismatch");
				return false;
			}

			if(a->function.args.len!=b->function.args.len){
				println("argument count mismatch %d %d",a->function.args.len,b->function.args.len);
				return false;
			}

			for(int i=0;i<a->function.args.len;i++){
				Symbol*sa=array_get(&a->function.args,i);
				Symbol*sb=array_get(&b->function.args,i);
				if(!Symbol_equal(sa,sb)){
					println("argument %d mismatch",i);
					return false;
				}
			}
			return true;
		default:
			fatal("unimplemented");
	}
}

bool Symbol_equal(Symbol*a,Symbol*b){
	if(a->kind!=b->kind){
		println("kind mismatch when comparing symbols %d %d",a->kind,b->kind);
		return false;
	}

	if(!Token_equalToken(a->name,b->name)){
		println("name mismatch when comparing symbols %.*s %.*s",a->name->len,a->name->p,b->name->len,b->name->p);
		return false;
	}

	if(!Type_equal(a->type,b->type)){
		println("type mismatch when comparing symbols %.*s",a->name->len,a->name->p);
		return false;
	}

	return true;
}

bool Statement_equal(Statement*a,Statement*b){
	if(a->tag!=b->tag){
		println("tag mismatch when comparing statements %d %d",a->tag,b->tag);
		return false;
	}

	switch(a->tag){
		case STATEMENT_PREP_DEFINE:{
			return Token_equalToken(&a->prep_define.name,&b->prep_define.name);
				//&& Token_equalToken(&a->prep_define.value,&b->prep_define.value);
		}
		case STATEMENT_PREP_INCLUDE:{
			return Token_equalToken(&a->prep_include.path,&b->prep_include.path);
		}
		case STATEMENT_FUNCTION_DECLARATION:{
			println("comparing function declarations");
			return Symbol_equal(&a->functionDecl.symbol,&b->functionDecl.symbol);
		}
		case STATEMENT_FUNCTION_DEFINITION:{
			println("comparing function definitions");
			if(!Symbol_equal(&a->functionDef.symbol,&b->functionDef.symbol)){
				println("symbol mismatch");
				return false;
			}

			if(a->functionDef.bodyStatements.len!=b->functionDef.bodyStatements.len){
				println("body statement count mismatch %d %d",a->functionDef.bodyStatements.len,b->functionDef.bodyStatements.len);
				return false;
			}

			for(int i=0;i<a->functionDef.bodyStatements.len;i++){
				Statement*sa=array_get(&a->functionDef.bodyStatements,i);
				Statement*sb=array_get(&b->functionDef.bodyStatements,i);
				if(!Statement_equal(sa,sb)){
					println("statement %d mismatch",i);
					return false;
				}
			}
			return true;
		}

		case STATEMENT_RETURN:{
			println("comparing return statements");
			if(!Value_equal(a->return_.retval,b->return_.retval)){
				println("return value mismatch");
				return false;
			}
			return true;
		}
	
		default:
			fatal("unimplemented");
	}

	return true;
}
bool Module_equal(Module*a,Module*b){
	if(a->statements.len!=b->statements.len){
		println("statement count mismatch %d %d",a->statements.len,b->statements.len);
		return false;
	}

	for(int i=0;i<a->statements.len;i++){
		Statement*sa=array_get(&a->statements,i);
		Statement*sb=array_get(&b->statements,i);
		if(!Statement_equal(sa,sb)){
			println("statement %d mismatch",i);
			return false;
		}
	}
	return true;
}
void Module_print(Module*module){
	for(int i=0;i<module->statements.len;i++){
		Statement*statement=array_get(&module->statements,i);
		switch(statement->tag){
			case STATEMENT_PREP_DEFINE:
				println("#define <unimplemented>");
				break;
			case STATEMENT_PREP_INCLUDE:
				println("#include %.*s",statement->prep_include.path.len,statement->prep_include.path.p);
				break;
			case STATEMENT_FUNCTION_DECLARATION:
				println("declaration of function %.*s",statement->functionDecl.symbol.name->len,statement->functionDecl.symbol.name->p);
				println("return type");
				Type_print(statement->functionDecl.symbol.type);
				break;
			case STATEMENT_FUNCTION_DEFINITION:
				println("definition of function %.*s",statement->functionDef.symbol.name->len,statement->functionDef.symbol.name->p);
				println("return type");
				Type_print(statement->functionDef.symbol.type);
				break;
			default:
				fatal("unimplemented %d",statement->tag);
		}
	}
}

bool test1(){
	Type* intType=allocAndCopy(sizeof(Type),&(Type){
		.kind=TYPE_KIND_REFERENCE,
		.reference=(Token){.len=3,.p="int",},
	});

	File testFile;
	File_fromString("testfile.c","int main(){}",&testFile);

	Tokenizer tokenizer={};
	Tokenizer_init(&tokenizer,&testFile);

	Module module={};
	Module_parse(&module,&tokenizer);

	Module expected_module={};
	array_init(&expected_module.statements,sizeof(Statement));

	Statement statements[]={(Statement){
		.tag=STATEMENT_FUNCTION_DEFINITION,
		.functionDef={
			.symbol={
				.name=&(Token){.len=4,.p="main",},
				.kind=SYMBOL_KIND_DECLARATION,
				.type=allocAndCopy(sizeof(Type),&(Type){
					.kind=TYPE_KIND_FUNCTION,
					.function={
						.ret=intType,
					},
				}),
			},
			.bodyStatements={},
		},
	}};
		array_init(&statements[0].functionDef.symbol.type->function.args,sizeof(Statement));
		array_init(&statements[0].functionDef.bodyStatements,sizeof(Statement));

	array_append(&expected_module.statements,&statements[0]);

	Module_print(&expected_module);
	return Module_equal(&module,&expected_module);
}

bool test2(){
	Type* intType=allocAndCopy(sizeof(Type),&(Type){
		.kind=TYPE_KIND_REFERENCE,
		.reference=(Token){.len=3,.p="int",},
	});

	File testFile;
	File_fromString("testfile.c",
	"int main(){"
	"	return 2;"
	"}"
	,&testFile);

	Tokenizer tokenizer={};
	Tokenizer_init(&tokenizer,&testFile);

	Module module={};
	Module_parse(&module,&tokenizer);

	Module expected_module={};
	array_init(&expected_module.statements,sizeof(Statement));

	Statement statements[]={(Statement){
		.tag=STATEMENT_FUNCTION_DEFINITION,
		.functionDef={
			.symbol={
				.name=&(Token){.len=4,.p="main",},
				.kind=SYMBOL_KIND_DECLARATION,
				.type=allocAndCopy(sizeof(Type),&(Type){
					.kind=TYPE_KIND_FUNCTION,
					.function={
						.ret=intType,
					},
				}),
			},
			.bodyStatements={},
		},
	}};
		array_init(&statements[0].functionDef.symbol.type->function.args,sizeof(Statement));
		array_init(&statements[0].functionDef.bodyStatements,sizeof(Statement));

		array_append(&statements[0].functionDef.bodyStatements,&(Statement){
			.tag=STATEMENT_RETURN,
			.return_={
				.retval=&(Value){
					.kind=VALUE_KIND_STATIC_VALUE,
					.static_value.value_repr=&(Token){.len=1,.p="2"},
				},
			},
		});

	array_append(&expected_module.statements,&statements[0]);

	Module_print(&expected_module);
	return Module_equal(&module,&expected_module);
}

#define TEXT_COLOR_BLACK "\033[1;30m"
#define TEXT_COLOR_RED "\033[1;31m"
#define TEXT_COLOR_GREEN "\033[1;32m"
#define TEXT_COLOR_YELLOW "\033[1;33m"
#define TEXT_COLOR_BLUE "\033[1;34m"
#define TEXT_COLOR_MAGENTA "\033[1;35m"
#define TEXT_COLOR_CYAN "\033[1;36m"
#define TEXT_COLOR_WHITE "\033[1;37m"
#define TEXT_COLOR_RESET "\033[1;0m"

void print_test_result(const char*testname,bool passed){
	if(passed){
		println(TEXT_COLOR_GREEN "%s passed" TEXT_COLOR_RESET,testname);
	}else{
		println(TEXT_COLOR_RED "%s failed" TEXT_COLOR_RESET,testname);
	}

}

int main(int argc, const char**argv){
	{
		print_test_result("test1",test1());
		print_test_result("test2",test2());
	}

	if(argc<2)
		fatal("no input file given. aborting.");

	File code_file={};
	File_read(argv[1],&code_file);

	Tokenizer tokenizer={};
	Tokenizer_init(&tokenizer,&code_file);

	Module module={};
	Module_parse(&module,&tokenizer);
	
	return 0;
}
