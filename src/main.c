#include <tokenizer.h>
#include <util/array.h>
#include <util/util.h>
#include <parser/parser.h>
#include <util/ansi_esc_codes.h>

#include<preprocessor/preprocessor.h>

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

	// read file into memory
	File code_file={};
	File_read(argv[1],&code_file);

	// tokenize file (even preprocessor requires some tokenization, because of string literals)
	Tokenizer tokenizer={};
	Tokenizer_init(&tokenizer,&code_file);

	for(int i=0;i<tokenizer.num_tokens;i++){
		Token token=tokenizer.tokens[i];
		println("token %d: %s",i,Token_print(&token));
	}

	print("tokens from file %s:\n",argv[1]);
	highlight_token_kind=TOKEN_TAG_SYMBOL;
	Tokenizer_print(&tokenizer);

	char*include_paths[]={
		".",
		"./include",
		"/usr/lib/llvm-16/lib/clang/16/include",
		"/usr/local/include",
		"/usr/include/aarch64-linux-gnu",
		"/usr/include",
	};

	// run preprocessor
	struct Preprocessor preprocessor={};
	Preprocessor_init(&preprocessor,&tokenizer);

	for(int i=0;i<sizeof(include_paths)/sizeof(char*);i++){
		array_append(&preprocessor.include_paths,&include_paths[i]);
	}

	println("running preprocessor");
	Preprocessor_run(&preprocessor);
	println("running preprocessor done");

	print("tokens from file %s:\n",argv[1]);
	highlight_token_kind=TOKEN_TAG_SYMBOL;
	Tokenizer preprocessed_tokenizer={
		.token_src=tokenizer.token_src,
		.tokens=preprocessor.tokens_out.data,
		.num_tokens=preprocessor.tokens_out.len,
	};
	Tokenizer_print(&preprocessed_tokenizer);

	if(0){
		// parse tokens into AST
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
	}

	
	return 0;
}
