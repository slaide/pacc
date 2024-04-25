#include <util/util.h>
#include <parser/parser.h>
#include<util/ansi_esc_codes.h>


void Module_print(Module*module){
	printf("module:\n");
	for(int i=0;i<module->statements.len;i++){
		Statement*statement=array_get(&module->statements,i);
		printf("%s\n",Statement_asString(statement,0));
	}
}

bool test1(){
	Type* intType=allocAndCopy(sizeof(Type),&(Type){
		.kind=TYPE_KIND_REFERENCE,
		.reference={.name=(Token){.len=3,.p="int",}},
	});

	File testFile;
	File_fromString("testfile.c","int main(){}",&testFile);

	Tokenizer tokenizer={};
	Tokenizer_init(&tokenizer,&testFile);

	struct TokenIter token_iter;
	TokenIter_init(&token_iter,&tokenizer,(struct TokenIterConfig){.skip_comments=true,});

	Module module={};
	Module_parse(&module,&token_iter);

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
		.reference={.name=(Token){.len=3,.p="int",}},
	});

	File testFile;
	File_fromString("testfile.c",
	"int main(){"
	"	return 2;"
	"}"
	,&testFile);

	Tokenizer tokenizer={};
	Tokenizer_init(&tokenizer,&testFile);

	struct TokenIter token_iter;
	TokenIter_init(&token_iter,&tokenizer,(struct TokenIterConfig){.skip_comments=true,});

	Module module={};
	Module_parse(&module,&token_iter);

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
			.tag=STATEMENT_KIND_RETURN,
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

void print_test_result(const char*testname,bool passed){
	if(passed){
		println(TEXT_COLOR_GREEN "%s passed" TEXT_COLOR_RESET,testname);
	}else{
		println(TEXT_COLOR_RED "%s failed" TEXT_COLOR_RESET,testname);
	}
}

int main(int argc, const char**argv){
	if(argc<2){
		print_test_result("test1",test1());
		print_test_result("test2",test2());

		fatal("no input file given. aborting.");
	}

	File code_file={};
	File_read(argv[1],&code_file);

	Tokenizer tokenizer={};
	Tokenizer_init(&tokenizer,&code_file);

	struct TokenIter token_iter;
	TokenIter_init(&token_iter,&tokenizer,(struct TokenIterConfig){.skip_comments=true,});

	Module module={};
	Module_parse(&module,&token_iter);

	Module_print(&module);

	if(!TokenIter_isEmpty(&token_iter)){
		Token next_token;
		TokenIter_lastToken(&token_iter,&next_token);
		fatal("unexpected tokens at end of file at line %d col %d: %.*s",next_token.line,next_token.col,next_token.len,next_token.p);
	}
	
	return 0;
}
