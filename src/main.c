#include<stdio.h>
#include<string.h>

#include <tokenizer.h>
#include <util/array.h>
#include <util/util.h>
#include <parser/parser.h>
#include <parser/module.h>
#include <util/ansi_esc_codes.h>

#include<preprocessor/preprocessor.h>

void Module_print(Module*module){
	printf("module:\n");
	for(int i=0;i<module->stack.statements.len;i++){
		Statement*statement=array_get(&module->stack.statements,i);
		printf("%s\n",Statement_asString(statement,0));
	}
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
		fatal("no input file given. aborting.");
	}

	char*input_filename=nullptr;
	bool run_preprocessor=false;
	bool run_parser=false;

	array defines={};
	array_init(&defines,sizeof(const char*));

	array include_paths={};
	array_init(&include_paths,sizeof(const char*));

	for(int i=1;i<argc;i++){
		if(
			strcmp(argv[i],"--preprocessor")==0
			|| strcmp(argv[i],"-p")==0
		){
			run_preprocessor=true;
			continue;
		}

		if(
			strcmp(argv[i],"--parse-ast")==0
			|| strcmp(argv[i],"-a")==0
		){
			run_parser=true;
			continue;
		}

		if(strncmp(argv[i],"-D",2)==0){
			char*define=calloc(1,strlen(argv[i])-2+1);
			strncpy(define,argv[i]+2,strlen(argv[i])-2);
			array_append(&defines,&define);
			println("cli define: %s",define);
			continue;
		}

		if(strncmp(argv[i],"-I",2)==0){
			char*include_path=calloc(1,strlen(argv[i])-2+1);
			strncpy(include_path,argv[i]+2,strlen(argv[i])-2);
			array_append(&include_paths,&include_path);
			println("cli include path: %s",include_path);
			continue;
		}

		if(input_filename==nullptr){
			input_filename=(char*)argv[i];
			continue;
		}

		fatal("unused input argument: %s",argv[i]);
	}

	// read file into memory
	File code_file={};
	File_read(input_filename,&code_file);

	// tokenize file (even preprocessor requires some tokenization, because of string literals)
	Tokenizer tokenizer={};
	Tokenizer_init(&tokenizer,&code_file);

	if(0){
		print("tokens from file %s:\n",input_filename);
		highlight_token_kind=TOKEN_TAG_SYMBOL;
		Tokenizer_print(&tokenizer);
	}

	// run preprocessor (phase 4)
	if(run_preprocessor){
		struct Preprocessor preprocessor={};
		Preprocessor_init(&preprocessor);

		// add defines from command line
		for(int i=0;i<defines.len;i++){
			const char*def_str=*(char**)array_get(&defines,i);
			struct PreprocessorDefine define={
				.name=(Token){
					.p=def_str,
					.len=(int)strlen(def_str),
					.tag=TOKEN_TAG_SYMBOL,
				},
				.tokens={},
				.args=nullptr,
			};
			array_init(&define.tokens,sizeof(Token));
			println("define: %.*s",define.name.len,define.name.p);
			array_append(&preprocessor.defines,&define);
		}

		// add include paths from command line
		for(int i=0;i<include_paths.len;i++){
			array_append(&preprocessor.include_paths,array_get(&include_paths,i));
		}

		struct TokenIter token_iter;
		TokenIter_init(&token_iter,&tokenizer,(struct TokenIterConfig){.skip_comments=true,});

		Preprocessor_consume(&preprocessor,&token_iter);
		
		Tokenizer preprocessed_tokenizer={
			.token_src=tokenizer.token_src,
			.tokens=preprocessor.tokens_out.data,
			.num_tokens=preprocessor.tokens_out.len,
		};

		if(0){
			println("tokens from file %s, after running preprocessor:",input_filename);

			highlight_token_kind=TOKEN_TAG_SYMBOL;
			Tokenizer_print(&preprocessed_tokenizer);
		}

		tokenizer=preprocessed_tokenizer;

		// print all defines executed during preprocessor run:
		if(0){
			for(int i=0;i<preprocessor.defines.len;i++){
				struct PreprocessorDefine*define=array_get(&preprocessor.defines,i);
				if(define->name.len==0)
					continue;
				printf("define (from %s ) %.*s ",define->name.filename,define->name.len,define->name.p);
				if(define->args!=nullptr){
					printf("( ");
					for(int j=0;j<define->args->len;j++){
						struct PreprocessorDefineFunctionlikeArg*arg=array_get(define->args,j);
						if(arg->tag==PREPROCESSOR_DEFINE_FUNCTIONLIKE_ARG_TYPE_NAME){
							printf("%.*s ,",arg->name.name.len,arg->name.name.p);
						}else if(arg->tag==PREPROCESSOR_DEFINE_FUNCTIONLIKE_ARG_TYPE_VARARGS){
							printf(" varargs");
						}
					}
					printf(" )");
				}
				printf(": ");
				for(int j=0;j<define->tokens.len;j++){
					Token*token=array_get(&define->tokens,j);
					printf("%.*s ",token->len,token->p);
				}
				printf("\n");
			}
		}
	}

	// join adjacent string literals (phase 5)
	{
		// set up new tokenizer
		// copy tokens from old tokenizer
		// join adjacent string literals by checking if new token is string literal, if so, check if previous token is string literal, if so, join them, otherwise just append

		array new_tokens={};
		array_init(&new_tokens,sizeof(Token));

		Token*prev_token=nullptr;
		Token*new_token=nullptr;
		for(int i=0;i<tokenizer.num_tokens;i++){
			prev_token=array_get(&new_tokens,new_tokens.len-1);
			new_token=&tokenizer.tokens[i];
			if(prev_token!=nullptr && prev_token->tag==TOKEN_TAG_LITERAL && prev_token->literal.tag==TOKEN_LITERAL_TAG_STRING && new_token->tag==TOKEN_TAG_LITERAL && new_token->literal.tag==TOKEN_LITERAL_TAG_STRING){
				char*new_p=calloc(1,prev_token->len+new_token->len+1);
				discard snprintf(new_p,prev_token->len+new_token->len-1,"%.*s%.*s",prev_token->len-1,prev_token->p,new_token->len-1,new_token->p+1);

				prev_token->p=new_p;
				prev_token->len=prev_token->len+new_token->len-2;
				prev_token->literal.string.len=prev_token->literal.string.len+new_token->literal.string.len-2;
				prev_token->literal.string.str=new_p;
				continue;
			}

			// append token to tokenizer
			array_append(&new_tokens,new_token);
		}	

		// overwrite old tokenizer with new tokenizer
		tokenizer=(struct Tokenizer){
			.token_src=tokenizer.token_src,
			.tokens=new_tokens.data,
			.num_tokens=new_tokens.len,
		};
	}

	if(run_parser){
		// parse tokens into AST
		struct TokenIter token_iter;
		TokenIter_init(&token_iter,&tokenizer,(struct TokenIterConfig){.skip_comments=true,});

		Module module={};
		Module_init(&module);
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
