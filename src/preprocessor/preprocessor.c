#include <libgen.h>
#include <string.h>
#include <unistd.h> // access

#include<util/util.h>

#include<preprocessor/preprocessor.h>

static const char*const PLACEHOLDER_FILENAME="unknownfile";

#define PREPROCESSOR_RECURSIVE 0

#define DEBUG_PRINTS false

void Preprocessor_init(struct Preprocessor*preprocessor){
	*preprocessor=(struct Preprocessor){};

	array_init(&preprocessor->include_paths,sizeof(char*));
	array_init(&preprocessor->defines,sizeof(struct PreprocessorDefine));

	array_init(&preprocessor->already_included_files,sizeof(char*));	

	array_init(&preprocessor->stack,sizeof(struct PreprocessorIfStack));

	// read all tokens into memory
	array_init(&preprocessor->tokens_out,sizeof(Token));

	// include some standard defined macros
	array a__FILE__tokens={};
	array_init(&a__FILE__tokens,sizeof(Token));
	array_append(&a__FILE__tokens,&(Token){
		.tag=TOKEN_TAG_LITERAL,
		.p=PLACEHOLDER_FILENAME,
		.len=(int)strlen(PLACEHOLDER_FILENAME),
		.literal={
			.string={
				.len=(int)strlen(PLACEHOLDER_FILENAME),
				.str=allocAndCopy(strlen(PLACEHOLDER_FILENAME)+1,PLACEHOLDER_FILENAME),
			}
		}
	});
	array_append(&preprocessor->defines,&(struct PreprocessorDefine){
		.name={ .tag=TOKEN_TAG_SYMBOL, .p="__FILE__", .len=strlen("__FILE__"), },
		.tokens=a__FILE__tokens,
	});

	array a__LINE__tokens={};
	array_init(&a__LINE__tokens,sizeof(Token));
	array_append(&a__LINE__tokens,&(Token){
		.tag=TOKEN_TAG_LITERAL,
		.p="1",
		.len=strlen("1"),
	});
	array_append(&preprocessor->defines,&(struct PreprocessorDefine){
		.name={ .tag=TOKEN_TAG_SYMBOL, .p="__LINE__", .len=strlen("__LINE__"), },
		.tokens=a__LINE__tokens,
	});

	array a__STDC__tokens={};
	array_init(&a__STDC__tokens,sizeof(Token));
	array_append(&a__STDC__tokens,&(Token){
		.tag=TOKEN_TAG_SYMBOL,
		.p="1",
		.len=strlen("1"),
	});
	array_append(&preprocessor->defines,&(struct PreprocessorDefine){
		.name={ .tag=TOKEN_TAG_SYMBOL, .p="__STDC__", .len=strlen("__STDC__"), },
		.tokens=a__STDC__tokens,
	});

	/* from https://gcc.gnu.org/onlinedocs/cpp/Standard-Predefined-Macros.html
	C Standard’s version number, a long integer constant of the form yyyymmL where yyyy and mm are the year and month of the Standard version

	199409L signifies the 1989 C standard as amended in 1994
	199901L signifies the 1999 revision of the C standard
	201112L signifies the 2011 revision of the C standard
	201710L signifies the 2017 revision of the C standard
	202311L is used for the experimental -std=c23 [...] modes <- we are aiming for this one
	*/
	array a__STDC_VERSION__tokens={};
	array_init(&a__STDC_VERSION__tokens,sizeof(Token));
	array_append(&a__STDC_VERSION__tokens,&(Token){
		.tag=TOKEN_TAG_SYMBOL,
		.p="202311L",
		.len=strlen("202311L"),
	});
	array_append(&preprocessor->defines,&(struct PreprocessorDefine){
		.name={ .tag=TOKEN_TAG_SYMBOL, .p="__STDC_VERSION__", .len=strlen("__STDC_VERSION__"), },
		.tokens=a__STDC_VERSION__tokens,
	});

	array a__STDC_HOSTED__tokens={};
	array_init(&a__STDC_HOSTED__tokens,sizeof(Token));
	array_append(&a__STDC_HOSTED__tokens,&(Token){
		.tag=TOKEN_TAG_SYMBOL,
		.p="1",
		.len=strlen("1"),
	});
	array_append(&preprocessor->defines,&(struct PreprocessorDefine){
		.name={ .tag=TOKEN_TAG_SYMBOL, .p="__STDC_HOSTED__", .len=strlen("__STDC_HOSTED__"), },
		.tokens=a__STDC_HOSTED__tokens,
	});
}

int Preprocessor_evalExpression(struct Preprocessor *preprocessor,struct PreprocessorExpression*expr){
	if(expr->value_is_known){
		return expr->value;
	}

	switch(expr->tag){
		case PREPROCESSOR_EXPRESSION_TAG_DEFINED:
			expr->value=0;
			for(int i=0;i<preprocessor->defines.len;i++){
				Token* define_token=array_get(&preprocessor->defines,i);
				if(Token_equalString(define_token,expr->defined.name)){
					expr->value=1;
					break;
				}
			}
			break;
		case PREPROCESSOR_EXPRESSION_TAG_NOT:
			expr->value=!Preprocessor_evalExpression(preprocessor,expr->not.expr);
			break;
		case PREPROCESSOR_EXPRESSION_TAG_AND:
			expr->value=Preprocessor_evalExpression(preprocessor,expr->and.lhs) && Preprocessor_evalExpression(preprocessor,expr->and.rhs);
			break;
		case PREPROCESSOR_EXPRESSION_TAG_OR:
			expr->value=Preprocessor_evalExpression(preprocessor,expr->or.lhs) || Preprocessor_evalExpression(preprocessor,expr->or.rhs);
			break;
		case PREPROCESSOR_EXPRESSION_TAG_SUBTRACT:
			expr->value=Preprocessor_evalExpression(preprocessor,expr->subtract.lhs) - Preprocessor_evalExpression(preprocessor,expr->subtract.rhs);
			break;
		case PREPROCESSOR_EXPRESSION_TAG_ADD:
			expr->value=Preprocessor_evalExpression(preprocessor,expr->add.lhs) - Preprocessor_evalExpression(preprocessor,expr->add.rhs);
			break;
		case PREPROCESSOR_EXPRESSION_TAG_ELSE:
			expr->value=1;
			break;
		case PREPROCESSOR_EXPRESSION_TAG_EQUAL:
			expr->value=Preprocessor_evalExpression(preprocessor,expr->equal.lhs) == Preprocessor_evalExpression(preprocessor,expr->equal.rhs);
			break;
		case PREPROCESSOR_EXPRESSION_TAG_UNEQUAL:
			expr->value=Preprocessor_evalExpression(preprocessor,expr->equal.lhs) != Preprocessor_evalExpression(preprocessor,expr->equal.rhs);
			break;
		case PREPROCESSOR_EXPRESSION_TAG_GREATER_THAN:
			expr->value=Preprocessor_evalExpression(preprocessor,expr->greater_than.lhs) > Preprocessor_evalExpression(preprocessor,expr->greater_than.rhs);
			break;
		case PREPROCESSOR_EXPRESSION_TAG_GREATER_THAN_OR_EQUAL:
			expr->value=Preprocessor_evalExpression(preprocessor,expr->greater_than_or_equal.lhs) >= Preprocessor_evalExpression(preprocessor,expr->greater_than_or_equal.rhs);
			break;
		case PREPROCESSOR_EXPRESSION_TAG_LESSER_THAN:
			expr->value=Preprocessor_evalExpression(preprocessor,expr->lesser_than.lhs) < Preprocessor_evalExpression(preprocessor,expr->lesser_than.rhs);
			break;
		case PREPROCESSOR_EXPRESSION_TAG_LESSER_THAN_OR_EQUAL:
			expr->value=Preprocessor_evalExpression(preprocessor,expr->lesser_than_or_equal.lhs) <= Preprocessor_evalExpression(preprocessor,expr->lesser_than_or_equal.rhs);
			break;
		case PREPROCESSOR_EXPRESSION_TAG_LITERAL:
			break;
		case PREPROCESSOR_EXPRESSION_TAG_TERNARY:
			expr->value=
				Preprocessor_evalExpression(preprocessor,expr->ternary.condition)
				? Preprocessor_evalExpression(preprocessor,expr->ternary.then)
				: Preprocessor_evalExpression(preprocessor,expr->ternary.else_);
			break;
		default:
			fatal("unknown preprocessor expression tag %d",expr->tag);
	}
	expr->value_is_known=true;
	return expr->value;
}

void Preprocessor_processInclude(struct Preprocessor*preprocessor){
	Token token;

	// read include argument
	int ntr=TokenIter_lastToken(&preprocessor->token_iter,&token);
	if(!ntr) fatal("");
	if(token.tag!=TOKEN_TAG_PREP_INCLUDE_ARGUMENT && !(token.tag==TOKEN_TAG_LITERAL && token.literal.tag==TOKEN_LITERAL_TAG_STRING)){
		fatal("expected include argument after #include directive but got instead %.*s",token.len,token.p);
	}

	bool local_include_path=token.p[0]=='"';

	char* include_path=calloc(token.len-1,1);
	discard sprintf(include_path,"%.*s",token.len-2,token.p+1);

	ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
	// we just consume the token, we don't use it in the rest of the function body
	discard ntr;

	if(!preprocessor->doSkip){
		// go through each entry in include paths and check if file exists there
		char* include_file_path=nullptr;
		for(int i=0-((int)local_include_path);i<preprocessor->include_paths.len;i++){
			char* include_dir=nullptr;
			if(i==-1){
				unsigned tok_filename_len=strlen(preprocessor->token_iter.tokenizer->token_src);
				char*tok_filename=calloc(tok_filename_len+1,1);
				strncpy(tok_filename, preprocessor->token_iter.tokenizer->token_src, tok_filename_len);
				include_dir=dirname(tok_filename);
				println("testing local include dir: %s, from %s",include_dir,preprocessor->token_iter.tokenizer->token_src);
			}else{
				include_dir=*(char**)array_get(&preprocessor->include_paths,i);
			}

			static const int num_extra_chars=2; // for slash and terminating zero
			include_file_path=calloc(strlen(include_dir)+strlen(include_path)+num_extra_chars,1);
			discard sprintf(include_file_path,"%s/%s",include_dir,include_path);

			if(access(include_file_path,F_OK)!=-1){
				println("resolved include path %s to %s",include_path,include_file_path);
				break;
			}

			// if file does not exist, free memory and set pointer to null
			free(include_file_path);
			include_file_path=nullptr;
		}
		if(include_file_path==nullptr){
			fatal("could not find include file %s",include_path);
		}

		// check if file was already included
		bool file_already_included=false;
		for(int i=0;i<preprocessor->already_included_files.len;i++){
			char* already_included_file=*(char**)array_get(&preprocessor->already_included_files,i);
			if(strcmp(already_included_file,include_file_path)==0){
				println("file %s was already included",include_file_path);
				file_already_included=true;
				break;
			}
		}
		if(file_already_included){
			return;
		}

		// read include file
		File include_file;
		File_read(include_file_path,&include_file);
		// tokenize include file
		Tokenizer include_tokenizer;
		Tokenizer_init(&include_tokenizer,&include_file);
		struct TokenIter include_token_iter;
		TokenIter_init(&include_token_iter,&include_tokenizer,(struct TokenIterConfig){.skip_comments=true});

		Preprocessor_consume(preprocessor,&include_token_iter);
	}
	free(include_path);
}
void Preprocessor_processDefine(struct Preprocessor*preprocessor){
	Token token;

	// read define argument
	bool ntr=TokenIter_lastToken(&preprocessor->token_iter,&token);
	if(!ntr) fatal("");
	if(token.tag!=TOKEN_TAG_SYMBOL){
		fatal("expected symbol after #define directive but got instead %s",Token_print(&token));
	}

	Token define_name=token;

	// read define value
	int define_line_num=token.line;
	array define_value={};
	array_init(&define_value,sizeof(Token));
	array*args=nullptr;
	bool done_parsing_args=true;
	while(TokenIter_nextToken(&preprocessor->token_iter,&token) && token.line==define_line_num){
		if(!done_parsing_args){
			if(Token_equalString(&token,")") && !done_parsing_args){
				done_parsing_args=true;
				continue;
			}

			if(args==nullptr)
				fatal("unexpected define directive format");

			if(Token_equalString(&token,",")){
				if(args->len<1)
					fatal("unexpected , in define argument list before first argument");
				continue;
			}

			//append token to args
			struct PreprocessorDefineFunctionlikeArg new_arg={
				.tag=PREPROCESSOR_DEFINE_FUNCTIONLIKE_ARG_TYPE_NAME,
				.name={
					.name=token,
				},
			};
			
			if(Token_equalString(&token,"...")){
				// token indicates vararg
				new_arg=(struct PreprocessorDefineFunctionlikeArg){
					.tag=PREPROCESSOR_DEFINE_FUNCTIONLIKE_ARG_TYPE_VARARGS,
					.name={
						.name=*Token_fromString("__VA_ARGS__")
					}
				};
			}
			
			array_append(args,&new_arg);

			continue;
		}
		if(Token_equalString(&token,"(") && args==nullptr && token.col==define_name.len+define_name.col /* i.e. no whitespace between macro name and open paranthesis */){
			done_parsing_args=false;

			args=malloc(sizeof(array));
			array_init(args,sizeof(struct PreprocessorDefineFunctionlikeArg));
			continue;
		}
		if(Token_equalString(&token, "\\")){
			define_line_num++;
			continue;
		}
		// append token to define value
		array_append(&define_value,&token);
	}

	if(!preprocessor->doSkip){
		// debug print
		if(DEBUG_PRINTS){
			print("defined %.*s",define_name.len,define_name.p);
			if(define_value.len>0 || args!=nullptr){
				if(args!=nullptr){
					printf(" (");
					for(int i=0;i<args->len;i++){
						struct PreprocessorDefineFunctionlikeArg* arg=array_get(args,i);
						printf("%.*s",arg->name.name.len,arg->name.name.p);
						if(i<args->len-1){
							printf(",");
						}
					}
					printf(")");
				}
				printf(" as ");
				if(define_value.len>0){
					for(int i=0;i<define_value.len;i++){
						Token* token=array_get(&define_value,i);
						printf("%.*s",token->len,token->p);
					}
				}else{
					printf("<nothing>");
				}
			}
			printf("\n");
		}

		array_append(
			&preprocessor->defines,
			&(struct PreprocessorDefine){
				.name=define_name,
				.tokens=define_value,
				.args=args,
			}
		);
	}
}
void Preprocessor_processUndefine(struct Preprocessor*preprocessor){
	Token token={};
	int ntr=TokenIter_lastToken(&preprocessor->token_iter,&token);
	discard ntr;
	if(token.tag!=TOKEN_TAG_SYMBOL){
		fatal("expected symbol after #undef directive but got instead %s",Token_print(&token));
	}

	// copy undef name to string
	char* define_name=calloc(token.len+1,1);
	discard sprintf(define_name,"%.*s",token.len,token.p);

	// iter past undef argument
	ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);

	// remove define from list of defines
	if(!preprocessor->doSkip){
		for(int i=0;i<preprocessor->defines.len;i++){
			Token* define_token=array_get(&preprocessor->defines,i);
			if(Token_equalString(define_token,define_name)){
				// TODO(patrick): this needs a better solution, but the array api
				// currently does not support removing elements from
				// an arbitrary index
				define_token->len=0;
				break;
			}
		}
	}
	free(define_name);
}
void Preprocessor_processPragma(struct Preprocessor*preprocessor){
	Token token={};
	int ntr=TokenIter_lastToken(&preprocessor->token_iter,&token);
	discard ntr;
	if(Token_equalString(&token,"once")){
		// read include argument
		ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);

		if(preprocessor->doSkip){
			return;
		}

		// add file to list of already included files
		const char*include_path=preprocessor->token_iter.tokenizer->token_src;
		char* include_path_copy=allocAndCopy(strlen(include_path)+1,include_path);
		array_append(&preprocessor->already_included_files,&include_path_copy);

		return;
	}

	println("unknown pragma %s",Token_print(&token));
	int line_num=token.line;
	while(!TokenIter_isEmpty(&preprocessor->token_iter)){
		ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
		if(!ntr) fatal("");
		if(token.line!=line_num){
			break;
		}
	}
	fatal("unknown pragma");
}
void Preprocessor_processError(struct Preprocessor*preprocessor){
	Token token={};
	int ntr=TokenIter_lastToken(&preprocessor->token_iter,&token);
	discard ntr;
	if(preprocessor->doSkip){
		return;
	}

	// this is genuinely an error during compilation
	fatal("error: %s: ",Token_print(&token));
}
void Preprocessor_processWarning(struct Preprocessor*preprocessor){
	Token token={};
	int ntr=TokenIter_lastToken(&preprocessor->token_iter,&token);
	discard ntr;
	if(preprocessor->doSkip){
		return;
	}

	// just print this message during compilation
	print("warning: %s: ",Token_print(&token));
	// print all other tokens until end of line
	int line_num=token.line;
	while(!TokenIter_isEmpty(&preprocessor->token_iter)){
		ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
		if(!ntr) fatal("");
		if(Token_equalString(&token,"\\")){
			line_num++;
			continue;
		}
		if(token.line>line_num){
			break;
		}
		print("%s",Token_print(&token));
	}
}

enum PREPROCESSOR_MACRO_EXPANSION_ITEM_TAG{
	// this item is an unexpanded token (i.e. will be emitted like this)
	PREPROCESSOR_MACRO_EXPANSION_TOKEN_TAG_TOKEN,
	// this item contains another list of tokens to be expanded
	PREPROCESSOR_MACRO_EXPANSION_TOKEN_TAG_EXPANSION,
};
/*
to handle token expansion, we need to keep track of the source of a token

some rules/observations:
- expansion takes place in steps: all tokens are iterated over at once, and each one is replaced if a match is found
- then the new list of tokens is iterated over again, and so on, until no more replacements are made
- function-like macros can have their name replaced, e.g. F2 may be "called" when #define F2 F1 // #define F1(arg) arg <- this works
- function-like macro arguments cannot be expanded into an argument list, e.g. #define F1(a) (a) // #define F2 F1(1,2) <- this does not work
- recursive expansion is not allowed, which makes it possible to have a macro with the same name as a function (the macro will be replaced once, emitting the same name again, but because of forbidden recursion, the name will not be recognized as another macro and hence not be expanded again)
*/
struct PreprocessorExpandedToken{
	Token token;
	/* names of macros that generated this token, i.e. item type is struct PreprocessorDefine* */
	array generators;
};

/*
expand tokens in tokens_in and append expanded tokens to tokens_out

tokens_out must already be initialized

preprocessor directives in tokens_in are not allowed
*/
void Preprocessor_expandMacros(struct Preprocessor*preprocessor,int num_tokens_in_arg,Token*tokens_in_arg,array*tokens_out_arg){
	if(num_tokens_in_arg==0){
		return;
	}

	// copy input arguments into expansion struct (PreprocessorExpandedToken)
	array tokens_in_={};
	array*tokens_in=&tokens_in_;
	array_init(&tokens_in_,sizeof(struct PreprocessorExpandedToken));

	for(int i=0;i<num_tokens_in_arg;i++){
		/*tokens_in[i].token=tokens_in_arg[i];
		array_init(&tokens_in[i].generators,sizeof(struct PreprocessorDefine));*/
		struct PreprocessorExpandedToken token_in={
			.token=tokens_in_arg[i],
		};
		array_init(&token_in.generators,sizeof(struct PreprocessorDefine*));

		array_append(&tokens_in_,&token_in);
	}

	/* write output into this array, which may be overwritten at any expansion level */
	array tokens_out_={};
	array*tokens_out=&tokens_out_;

	/* indicate if any token got expanded in current expansion level */
	bool anyTokenGotExpanded=false;

	int debug_expansion_level=0;
	while(1){
		anyTokenGotExpanded=false;

		array_init(tokens_out,sizeof(struct PreprocessorExpandedToken));

		for(int i=0;i<tokens_in->len;i++){
			struct PreprocessorExpandedToken* token_in=array_get(tokens_in,i);
			/* token that is checked against macro names for expansion */
			struct PreprocessorExpandedToken* expanded_token=token_in;

			// only check symbols for expansion
			switch(token_in->token.tag){
				case TOKEN_TAG_SYMBOL:
					break;
				default:
					goto PREPROCESSOR_EXPANDMACROS_APPEND_TOKEN;
			}

			// check if token is a macro
			bool current_token_was_expanded=false;
			for(int j=0;(!current_token_was_expanded) && (j<preprocessor->defines.len);j++){
				struct PreprocessorDefine* define=array_get(&preprocessor->defines,j);
				
				if(Token_equalToken(&define->name,&token_in->token)){
					// check if this macro was already expanded
					bool already_expanded=false;
					for(int k=0;k<token_in->generators.len;k++){
						struct PreprocessorDefine* generator=*(struct PreprocessorDefine**)array_get(&token_in->generators,k);
						if(generator==define){
							already_expanded=true;
							break;
						}
					}
					if(already_expanded){
						continue;
					}

					current_token_was_expanded=true;

					// append matched macro to list of generators for expanded_token
					array_append(&expanded_token->generators,&define);

					// print info about which token got expanded
					if(DEBUG_PRINTS){
						printf("expanding %.*s from (%s) to ",define->name.len,define->name.p,Token_print(&define->name));
						for(int k=0;k<define->tokens.len;k++){
							Token* token=array_get(&define->tokens,k);
							printf("%.*s",token->len,token->p);
						}
						printf("\n");
					}

					// get input arguments, store them to allow expansion
					// item type is struct PreprocessorDefine, since arguments and defines work essentially the same, only that arguments are only valid for one expansion step
					array arguments={};
					array_init(&arguments,sizeof(struct PreprocessorDefine));

					// if the macro is function-like, parse and store arguments
					if(define->args!=nullptr){
						i++; // skip over macro name

						// check for paranthesis
						if(i>=tokens_in->len) fatal("expected argument list after function-like macro %s",Token_print(&token_in->token));
						token_in=array_get(tokens_in,i);

						if(!Token_equalString(&token_in->token, "("))
							fatal("expected ( after function-like macro %.*s, but got instead %s",define->name.len,define->name.p,Token_print(&token_in->token));
						i++; // skip over opening paranthesis

						// handle macro arguments
						while(1){
							array arg_tokens={};
							array_init(&arg_tokens,sizeof(Token));
							/* list of chars to close nested statements, though only () qualify, others, e.g. curly braces, do not have to be closed */
							array nested_char_stack={};
							array_init(&nested_char_stack,sizeof(char));
							while(1){
								if(i>=tokens_in->len) fatal("expected argument list after function-like macro %s",Token_print(&token_in->token));
								struct PreprocessorExpandedToken* define_token=array_get(tokens_in,i);

								// comma is allowed when nested
								if(
									Token_equalString(&define_token->token,",")
									&& nested_char_stack.len==0
								){
									break;
								}
								if(Token_equalString(&define_token->token,"(")){
									const char close_char=')';
									array_append(&nested_char_stack,&close_char);
								}
								if(Token_equalString(&define_token->token,")")){
									if(nested_char_stack.len==0){
										break;
									}

									char close_char=*(char*)array_get(&nested_char_stack,nested_char_stack.len-1);
									if(close_char!=')')
										fatal("unexpected closing paranthesis %s",Token_print(&define_token->token));
									array_pop_back(&nested_char_stack);
								}
								i+=1;
								array_append(&arg_tokens,define_token);
							}
							// 3) create temporary define
							struct PreprocessorDefine arg_define={
								.name={},
								.tokens=arg_tokens,
								.args=nullptr
							};
							// 4) add define to preprocessor
							array_append(&arguments,&arg_define);

							// 5) if next token is closing paranthesis, break
							const Token token_in_token=((struct PreprocessorExpandedToken*)array_get(tokens_in,i))->token;
							if(Token_equalString(&token_in_token,")")){
								break;
							}
							// 6) if next token is comma, continue
							if(Token_equalString(&token_in_token,",")){
								i+=1;
								continue;
							}
							// 7) if next token is neither comma nor closing paranthesis, error
							fatal("expected , or ) after argument in function-like macro %s after %s",Token_print(&token_in_token),Token_print(array_get(&arg_tokens,arg_tokens.len-1)));
						}

						bool macro_has_vararg_argument=false;
						if(define->args->len>0){
							// check last arg tag for vararg
							struct PreprocessorDefineFunctionlikeArg *arg=array_get(define->args,define->args->len-1);
							// tag can be:
							//PREPROCESSOR_DEFINE_FUNCTIONLIKE_ARG_TYPE_NAME,
							//PREPROCESSOR_DEFINE_FUNCTIONLIKE_ARG_TYPE_VARARGS
							if(arg->tag==PREPROCESSOR_DEFINE_FUNCTIONLIKE_ARG_TYPE_VARARGS){
								macro_has_vararg_argument=true;
							}
						}

						int min_number_of_args=define->args->len-(int)macro_has_vararg_argument;
						if(arguments.len<min_number_of_args){
							fatal("not enough arguments at %s",Token_print(&expanded_token->token));
						}
						if(arguments.len>min_number_of_args && !macro_has_vararg_argument){
							fatal("too many arguments at %s",Token_print(&expanded_token->token));
						}

						// combine trailing arguments into __VA_ARGS__ argument
						if(macro_has_vararg_argument){
							array args={};
							array_init(&args,sizeof(Token));

							// count number of items to pop from macro invocation argument list
							int num_args_to_pop=0;
							for(int i=min_number_of_args;i<arguments.len;i++){
								num_args_to_pop++;

								// vararg is expanded as argument list, i.e. commas between arguments need to be preserved
								if(i>min_number_of_args){ // on all iterations except the first one
									array_append(&args,Token_fromString(","));
								}

								// append tokens from arg
								struct PreprocessorDefine *arg=array_get(&arguments,i);
								for(int a=0;a<arg->tokens.len;a++){
									Token*tok=array_get(&arg->tokens,a);
									array_append(&args,tok);
								}
							}

							struct PreprocessorDefine vararg={
								.name=*Token_fromString("__VA_ARGS__"),
								.tokens=args,
							};

							// pop trailing args
							while(num_args_to_pop>0){
								num_args_to_pop-=1;
								array_pop_back(&arguments);
							}

							// append vararg
							array_append(&arguments,&vararg);
						}

						for(int arg_index=0;arg_index<define->args->len;arg_index++){
							// handle arg as temporary define
							// 1) get arg name for name of define
							struct PreprocessorDefineFunctionlikeArg *arg=array_get(define->args,arg_index);
							Token arg_name=arg->name.name;

							struct PreprocessorDefine*arg_val=array_get(&arguments,arg_index);
							arg_val->name=arg_name;
						}

						// check for closing paranthesis
						if(i>=tokens_in->len) fatal("expected argument list after function-like macro %s",Token_print(&token_in->token));
						
						token_in=array_get(tokens_in,i);
						if(!Token_equalString(&token_in->token, ")"))
							fatal("expected ) after function-like macro %s",Token_print(&token_in->token));
						// do not skip over closing paranthesis (with i++) here because the loop step will do that
					}

					// go through each token emitted by the macro, check if it matches the name of an argument, and replace it with the argument value if it does
					array new_tokens_={};
					array_init(&new_tokens_,sizeof(Token));
					array*new_tokens=&new_tokens_;
					for(int define_token_index=0;define_token_index<define->tokens.len;define_token_index++){
						// this is the next token emitted by the macro
						Token define_token=*(Token*)array_get(&define->tokens,define_token_index);

						// go through arguments and replace token if it is found there
						bool replaced=false;
						for(int arg_index=0;arg_index<arguments.len;arg_index++){
							struct PreprocessorDefine* arg_define=array_get(&arguments,arg_index);
							if(Token_equalToken(&define_token,&arg_define->name)){
								
								// check for preprocessor operators, part 1/2: stringification operator, which is only allowed on macro arguments
								if(new_tokens->len>=1
									&& /* n-1 token is hash */ Token_equalString(array_get(new_tokens,new_tokens->len-1),"#")
									&& !(new_tokens->len>=2 && /* n-2 token is not hash (i.e. this is not preceding a concatenation operator) */ Token_equalString(array_get(new_tokens,new_tokens->len-2),"#"))
								){
									// pop last token (hash)
									Token hashToken=*(Token*)array_get(new_tokens,new_tokens->len-1);
									array_pop_back(new_tokens);

									// concatenate all tokens in arg_define to a single string
									int total_str_len=0;
									for(int j=0;j<arg_define->tokens.len;j++){
										Token* arg_define_token=array_get(&arg_define->tokens,j);
										total_str_len+=arg_define_token->len;
									}
									char* arg_str=calloc(total_str_len+1,1);
									int str_offset=0;
									for(int j=0;j<arg_define->tokens.len;j++){
										Token* arg_define_token=array_get(&arg_define->tokens,j);
										strncpy(arg_str+str_offset,arg_define_token->p,arg_define_token->len);
										str_offset+=arg_define_token->len;
									}
									println("stringified %.*s to %s",arg_define->name.len,arg_define->name.p,arg_str);

									char* arg_str_out=calloc(1,total_str_len+3);
									discard snprintf(arg_str_out,total_str_len+3,"\"%s\"",arg_str);
									// replace last token in token_out with string literal
									Token string_literal_token={
										.tag=TOKEN_TAG_LITERAL,
										.line=hashToken.line,
										.col=hashToken.col,
										.len=total_str_len+2,
										.p=arg_str_out,
										.literal={
											.tag=TOKEN_LITERAL_TAG_STRING,
											.string={
												.len=total_str_len,
												.str=arg_str,
											}
										}
									};
									array_append(new_tokens,&string_literal_token);

									replaced=true;
									current_token_was_expanded=true;
									continue;
								}

								// expand argument by just copying all tokens in the argument to the output
								for(int j=0;j<arg_define->tokens.len;j++){
									Token* arg_define_token=array_get(&arg_define->tokens,j);
									array_append(new_tokens,arg_define_token);
								}
								
								replaced=true;
								break;
							}
						}
						if(replaced){
							continue;
						}

						// adjust source location of the output token
						//define_token.line=first_token.line;
						// offset col slightly to make it more readable
						//define_token.col=first_token.col+d*2;
						array_append(new_tokens,&define_token);
					}

					// append new tokens in new_tokens to tokens_out
					for(int j=0;j<new_tokens->len;j++){
						Token* new_token=array_get(new_tokens,j);

						// check for preprocessor operators, part 2/2: concatenation operator
						if(j>=2){
							struct PreprocessorExpandedToken *n_1_token=array_get(tokens_out,tokens_out->len-1);
							struct PreprocessorExpandedToken *n_2_token=array_get(tokens_out,tokens_out->len-2);

							if(
								/* n-1 token is hash */ Token_equalString(&n_1_token->token,"#")
								&& /* n-2 token is hash */ Token_equalString(&n_2_token->token,"#")
							){
								// get n-3 token
								Token last_token=*(Token*)array_get(tokens_out,tokens_out->len-3);

								// pop last two tokens (at n-1 and n-2, the hash characters)
								array_pop_back(tokens_out);
								array_pop_back(tokens_out);

								// pop n-3 token
								array_pop_back(tokens_out);

								Token left=last_token;
								Token right=*new_token;

								// expand macros on both, i.e. create new array to write expansion into, the expand, then read back from array
								array left_tokens={};
								array_init(&left_tokens,sizeof(Token));
								array right_tokens={};
								array_init(&right_tokens,sizeof(Token));
								Preprocessor_expandMacros(preprocessor,1,&left,&left_tokens);
								Preprocessor_expandMacros(preprocessor,1,&right,&right_tokens);
								if(left_tokens.len!=1 || right_tokens.len!=1)
									fatal("expected exactly one token after macro expansion");
								left=*(Token*)array_get(&left_tokens,0);
								right=*(Token*)array_get(&right_tokens,0);

								// append current token to last_token
								char* concatenated_token_p=calloc(left.len+right.len+1/*+1 for zero termination*/,1);
								Token concatenated_token={
									.tag=TOKEN_TAG_SYMBOL,
									.line=left.line,
									.col=left.col,
									.len=left.len+right.len,
									.p=concatenated_token_p
								};
								discard sprintf(concatenated_token_p,"%.*s%.*s",left.len,left.p,right.len,right.p);

								struct PreprocessorExpandedToken concatenated_token_expanded={
									.token=concatenated_token,
								};
								array_init(&concatenated_token_expanded.generators,sizeof(struct PreprocessorDefine*));

								array_append(tokens_out,&concatenated_token_expanded);
								println("concatenated to %.*s",concatenated_token.len,concatenated_token.p);

								continue;
							}
						}

						struct PreprocessorExpandedToken new_expand_token={
							.token=*new_token,
						};
						array_init(&new_expand_token.generators,sizeof(struct PreprocessorDefine*));
						// copy generators from expanded_token to new_expand_token
						for(int k=0;k<expanded_token->generators.len;k++){
							struct PreprocessorDefine* generator=*(struct PreprocessorDefine**)array_get(&expanded_token->generators,k);
							array_append(&new_expand_token.generators,&generator);
						}
						array_append(tokens_out,&new_expand_token);
					}
				}
			}


			anyTokenGotExpanded|=current_token_was_expanded;

			// if it's not a macro, append it to the output directly
			if(!current_token_was_expanded){
PREPROCESSOR_EXPANDMACROS_APPEND_TOKEN:
				array_append(tokens_out,token_in);
			}
		}

		if(!anyTokenGotExpanded){
			break;
		}

		// make output new input
		// TODO(patrick) free resources of tokens_in
		*tokens_in=*tokens_out;
		*tokens_out=(array){};
		debug_expansion_level++;
		if(debug_expansion_level>10){
			// print input and output tokens for comparison
			println("input tokens:");
			for(int i=0;i<tokens_in->len;i++){
				struct PreprocessorExpandedToken* token_in=array_get(tokens_in,i);
				println("%s",Token_print(&token_in->token));
			}
			println("output tokens:");
			for(int i=0;i<tokens_out->len;i++){
				struct PreprocessorExpandedToken* token_out=array_get(tokens_out,i);
				println("%s",Token_print(&token_out->token));
			}
			fatal("expansion level exceeded 10");
		}
	}

	// copy tokens_out to tokens_out_arg
	for(int i=0;i<tokens_out->len;i++){
		struct PreprocessorExpandedToken* expanded_token=array_get(tokens_out,i);

		array_append(tokens_out_arg,&expanded_token->token);
	}
}

/* higher precedence means lower binding power */
enum PreprocessorOperatorPrecedence{
	PREPROCESSOR_OPERATOR_PRECEDENCE_UNKNOWN=0,

	PREPROCESSOR_OPERATOR_PRECEDENCE_CALL=1,
	PREPROCESSOR_OPERATOR_PRECEDENCE_NOT=2,

	PREPROCESSOR_OPERATOR_PRECEDENCE_ADD=4,
	PREPROCESSOR_OPERATOR_PRECEDENCE_SUBTRACT=4,

	PREPROCESSOR_OPERATOR_PRECEDENCE_LESSER_THAN=6,
	PREPROCESSOR_OPERATOR_PRECEDENCE_GREATER_THAN=6,
	PREPROCESSOR_OPERATOR_PRECEDENCE_LESSER_THAN_OR_EQUAL=6,
	PREPROCESSOR_OPERATOR_PRECEDENCE_GREATER_THAN_OR_EQUAL=6,

	PREPROCESSOR_OPERATOR_PRECEDENCE_EQUAL=7,
	PREPROCESSOR_OPERATOR_PRECEDENCE_UNEQUAL=7,

	PREPROCESSOR_OPERATOR_PRECEDENCE_AND=11,
	PREPROCESSOR_OPERATOR_PRECEDENCE_OR=12,
	PREPROCESSOR_OPERATOR_PRECEDENCE_TERNARY=13,

	PREPROCESSOR_OPERATOR_PRECEDENCE_MAX=0xFF,
};

const char*PreprocessorExpression_print(struct PreprocessorExpression*expr){
	if(expr==nullptr)fatal("expr is null");

	char*ret=makeString();
	switch(expr->tag){
		case PREPROCESSOR_EXPRESSION_TAG_LITERAL:
			stringAppend(ret,"%d",expr->value);
			break;
		case PREPROCESSOR_EXPRESSION_TAG_NOT:
			stringAppend(ret,"not( %s )",PreprocessorExpression_print(expr->not.expr));
			break;
		case PREPROCESSOR_EXPRESSION_TAG_OR:
			stringAppend(ret,"( %s ) or ( %s )",PreprocessorExpression_print(expr->or.lhs),PreprocessorExpression_print(expr->or.rhs));
			break;
		case PREPROCESSOR_EXPRESSION_TAG_AND:
			stringAppend(ret,"( %s ) and ( %s )",PreprocessorExpression_print(expr->and.lhs),PreprocessorExpression_print(expr->and.rhs));
			break;
		case PREPROCESSOR_EXPRESSION_TAG_UNKNOWN:
			stringAppend(ret,"unknown");
			break;

		case PREPROCESSOR_EXPRESSION_TAG_EQUAL:
			stringAppend(ret,"( %s ) == ( %s )",PreprocessorExpression_print(expr->equal.lhs),PreprocessorExpression_print(expr->equal.rhs));
			break;
		case PREPROCESSOR_EXPRESSION_TAG_UNEQUAL:
			stringAppend(ret,"( %s ) != ( %s )",PreprocessorExpression_print(expr->equal.lhs),PreprocessorExpression_print(expr->equal.rhs));
			break;
		case PREPROCESSOR_EXPRESSION_TAG_GREATER_THAN:
			stringAppend(ret,"( %s ) > ( %s )",PreprocessorExpression_print(expr->greater_than.lhs),PreprocessorExpression_print(expr->greater_than.rhs));
			break;
		case PREPROCESSOR_EXPRESSION_TAG_GREATER_THAN_OR_EQUAL:
			stringAppend(ret,"( %s ) >= ( %s )",PreprocessorExpression_print(expr->greater_than_or_equal.lhs),PreprocessorExpression_print(expr->greater_than_or_equal.rhs));
			break;
		case PREPROCESSOR_EXPRESSION_TAG_LESSER_THAN:
			stringAppend(ret,"( %s ) < ( %s )",PreprocessorExpression_print(expr->lesser_than.lhs),PreprocessorExpression_print(expr->lesser_than.rhs));
			break;
		case PREPROCESSOR_EXPRESSION_TAG_LESSER_THAN_OR_EQUAL:
			stringAppend(ret,"( %s ) <= ( %s )",PreprocessorExpression_print(expr->lesser_than_or_equal.lhs),PreprocessorExpression_print(expr->lesser_than_or_equal.rhs));
			break;

		case PREPROCESSOR_EXPRESSION_TAG_SUBTRACT:
			stringAppend(ret,"( %s ) - ( %s )",PreprocessorExpression_print(expr->subtract.lhs),PreprocessorExpression_print(expr->subtract.rhs));
			break;
		case PREPROCESSOR_EXPRESSION_TAG_ADD:
			stringAppend(ret,"( %s ) + ( %s )",PreprocessorExpression_print(expr->add.lhs),PreprocessorExpression_print(expr->add.rhs));
			break;

		case PREPROCESSOR_EXPRESSION_TAG_DEFINED:
		case PREPROCESSOR_EXPRESSION_TAG_TERNARY:
		case PREPROCESSOR_EXPRESSION_TAG_ELSE:
			fatal("unimplemented tag %d",expr->tag);
	}

	return ret;
}

// for internal use only
int PreprocessorExpression_parse(
	struct Preprocessor*preprocessor,
	array if_expr_tokens,
	struct PreprocessorExpression *out,
	int precedence
);

/*
for internal use only, from inside PreprocessorExpression_parse (arguments carry internal state)

*/
void PreprocessorExpression_parse_processOperator(
	struct Preprocessor*preprocessor,

	int num_operands,

	int *nextTokenIndex,
	struct PreprocessorExpression*out,
	array if_expr_tokens,
	int precedence,
	enum PreprocessorExpressionTag operator_expression_tag,
	Token*nextToken
){
	if(num_operands>1 && out->tag==0)fatal("n>1 operand operation was not preceded by another operand, instead got token %s",Token_print(nextToken));
	if(num_operands==1 && out->tag!=0)fatal("unexpected token %s, %s",Token_print(nextToken),PreprocessorExpression_print(out));
	(*nextTokenIndex)++;

	switch(num_operands){
		case 1: case 2: case 3:break;
		default:fatal("invalid number of operands %d",num_operands);
	}

	// parse new expression
	struct PreprocessorExpression sub_expr[2]={};

	int num_operands_yet_to_parse=0;
	switch(num_operands){
		case 1:
			// for unary operations, the operatand follows the operator
			num_operands_yet_to_parse=1;
			break;
		case 2:
			// for binary and tertiary operations, the second (and third) operands follow the operator
			num_operands_yet_to_parse=1;
			break;
		case 3:
			num_operands_yet_to_parse=2;
			break;
		default:fatal("unreachable");
	}
	for(int i=0;i<num_operands_yet_to_parse;i++){
		(*nextTokenIndex)+=PreprocessorExpression_parse(
			preprocessor,
			(array){
				.data=((Token*)if_expr_tokens.data)+nextTokenIndex[0],
				.len=if_expr_tokens.len-nextTokenIndex[0],
				.elem_size=if_expr_tokens.elem_size,
				// omit cap
			},
			&sub_expr[i],
			precedence
		);
	}

	switch(num_operands){
		case 1:
			*out=(struct PreprocessorExpression){
				.tag=operator_expression_tag,
				.unary_operand={
					.expr=allocAndCopy(sizeof(struct PreprocessorExpression),&sub_expr[0]),
				}
			};
			return;
		case 2:
			if(sub_expr[0].tag==0)fatal("");
			*out=(struct PreprocessorExpression){
				.tag=operator_expression_tag,
				.binary_operands={
					.lhs=allocAndCopy(sizeof(struct PreprocessorExpression),out),
					.rhs=allocAndCopy(sizeof(struct PreprocessorExpression),&sub_expr[0])
				}
			};
			return;
		case 3:fatal("unimplemented");
		default:fatal("unreachable");
	}
	fatal("unreachable");
}

#define PREPROCESSOR_EXPRESSION_PARSE_CHECK_PRECEDENCE(OPERATOR_PRECEDENCE) \
	if(precedence<=(OPERATOR_PRECEDENCE)) \
		goto PREPROCESSOR_EXPRESSION_PARSE_RET_SUCCESS;

/*
parse expression by consuming tokens (until end of line), then evaluating the expression

returns number of tokens consumed
*/
int PreprocessorExpression_parse(
	struct Preprocessor*preprocessor,
	array if_expr_tokens,
	struct PreprocessorExpression *out,
	int precedence
){
	//print all tokens
	if(DEBUG_PRINTS){
		println("PreprocessorExpression_parse tokens:");
		for(int i=0;i<if_expr_tokens.len;i++){
			Token* token=array_get(&if_expr_tokens,i);
			println("%s",Token_print(token));
		}
	}
	// init out
	*out=(struct PreprocessorExpression){};
	/* track number of currently open paranthesis */
	int openParanthesis=0;

	int nextTokenIndex=0;
	Token* nextToken=nullptr;
	for(;nextTokenIndex<if_expr_tokens.len;){
		nextToken=array_get(&if_expr_tokens,nextTokenIndex);

		switch(nextToken->tag){
			case TOKEN_TAG_LITERAL:
				if(out->tag!=0){
					fatal("unexpected token %s, %d",Token_print(nextToken),out->tag);
				}
				nextTokenIndex++;

				if(nextToken->literal.tag!=TOKEN_LITERAL_TAG_NUMERIC)fatal("unexpected literal %s",Token_print(nextToken));

				out->tag=PREPROCESSOR_EXPRESSION_TAG_LITERAL;
				int64_t value=0;
				TokenLiteral_getNumericValue(nextToken,nullptr,&value,nullptr);
				out->value=(int)value;
				break;
			case TOKEN_TAG_SYMBOL:
				{
					// if this symbol is defined, replace it with the defined value
					// otherwise, replace it with 0 (per spec)
					if(out->tag!=0)fatal("unexpected token %s",Token_print(nextToken));
					nextTokenIndex++;

					// check if symbol is defined
					int defined=0;
					for(int i=0;i<preprocessor->defines.len;i++){
						Token* define_token=array_get(&preprocessor->defines,i);
						if(Token_equalToken(define_token,nextToken)){
							defined=1;
							// assume new token is a literal
							out->tag=PREPROCESSOR_EXPRESSION_TAG_LITERAL;
							// get value from define_token->p, which is a string of len define_token->len
							char* value=calloc(define_token->len+1,1);
							discard sprintf(value,"%.*s",define_token->len,define_token->p);

							static const int BASE_10=10;
							out->value=(int)strtol(value,nullptr,BASE_10);
							free(value);
							break;
						}
					}
					if(!defined){
						out->tag=PREPROCESSOR_EXPRESSION_TAG_LITERAL;
						out->value=0;
					}
					break;
				}
				break;
			case TOKEN_TAG_KEYWORD:
				{
					if(Token_equalString(nextToken,"(")){
						PREPROCESSOR_EXPRESSION_PARSE_CHECK_PRECEDENCE(PREPROCESSOR_OPERATOR_PRECEDENCE_CALL)

						bool skip_expression=false;
						if(out->tag!=0){
							if(out->tag==PREPROCESSOR_EXPRESSION_TAG_LITERAL && out->value==0){
								// if the last token was a 0, we can skip the paranthesis
								// and return a 0
								skip_expression=true;
							}else{
								fatal("unexpected token %s",Token_print(nextToken));
							}
						}
						nextTokenIndex++;

						if(!skip_expression){
							openParanthesis++;

							// parse new expression
							struct PreprocessorExpression sub_expr={};
							nextTokenIndex+=PreprocessorExpression_parse(
								preprocessor,
								(array){
									.data=((Token*)if_expr_tokens.data)+nextTokenIndex,
									.len=if_expr_tokens.len-nextTokenIndex,
									.elem_size=if_expr_tokens.elem_size,
									// omit cap
								},
								&sub_expr,
								PREPROCESSOR_OPERATOR_PRECEDENCE_MAX // no precedence, just parse until closing paranthesis
							);
							*out=sub_expr;
						}else{
							int discardOpenParanthesis=1;
							// skip over tokens, while:
							// 1) counting the number of opened paranthesis
							// 2) decreasing that count on close paranthesis
							// 3) stopping when the count reaches 0
							while(nextTokenIndex<if_expr_tokens.len){
								nextToken=array_get(&if_expr_tokens,nextTokenIndex);
								nextTokenIndex++;
								if(Token_equalString(nextToken,"(")){
									discardOpenParanthesis++;
								}else if(Token_equalString(nextToken,")")){
									discardOpenParanthesis--;
									if(discardOpenParanthesis==0){
										break;
									}
								}
							}
							// out is already literal 0
						}
						break;
					}
					if(Token_equalString(nextToken,")")){
						PREPROCESSOR_EXPRESSION_PARSE_CHECK_PRECEDENCE(PREPROCESSOR_OPERATOR_PRECEDENCE_CALL)

						// if there are no open paranthesis, we may be inside a subexpression, hence return without consuming the token
						if(openParanthesis==0){
							goto PREPROCESSOR_EXPRESSION_PARSE_RET_SUCCESS;
						}

						nextTokenIndex++;
						openParanthesis--;

						break;
					}
					if(Token_equalString(nextToken,"||")){
						PREPROCESSOR_EXPRESSION_PARSE_CHECK_PRECEDENCE(PREPROCESSOR_OPERATOR_PRECEDENCE_OR)

						PreprocessorExpression_parse_processOperator(
							preprocessor,
							2,
							&nextTokenIndex,
							out,
							if_expr_tokens,
							PREPROCESSOR_OPERATOR_PRECEDENCE_OR,
							PREPROCESSOR_EXPRESSION_TAG_OR,
							nextToken
						);
						break;
					}
					if(Token_equalString(nextToken,"&&")){
						PREPROCESSOR_EXPRESSION_PARSE_CHECK_PRECEDENCE(PREPROCESSOR_OPERATOR_PRECEDENCE_AND)

						PreprocessorExpression_parse_processOperator(
							preprocessor,
							2,
							&nextTokenIndex,
							out,
							if_expr_tokens,
							PREPROCESSOR_OPERATOR_PRECEDENCE_AND,
							PREPROCESSOR_EXPRESSION_TAG_AND,
							nextToken
						);
						break;
					}
					if(Token_equalString(nextToken,">")){
						PREPROCESSOR_EXPRESSION_PARSE_CHECK_PRECEDENCE(PREPROCESSOR_OPERATOR_PRECEDENCE_GREATER_THAN)

						PreprocessorExpression_parse_processOperator(
							preprocessor,
							2,
							&nextTokenIndex,
							out,
							if_expr_tokens,
							PREPROCESSOR_OPERATOR_PRECEDENCE_GREATER_THAN,
							PREPROCESSOR_EXPRESSION_TAG_GREATER_THAN,
							nextToken
						);
						break;
					}
					if(Token_equalString(nextToken,">=")){
						PREPROCESSOR_EXPRESSION_PARSE_CHECK_PRECEDENCE(PREPROCESSOR_OPERATOR_PRECEDENCE_GREATER_THAN_OR_EQUAL)

						PreprocessorExpression_parse_processOperator(
							preprocessor,
							2,
							&nextTokenIndex,
							out,
							if_expr_tokens,
							PREPROCESSOR_OPERATOR_PRECEDENCE_GREATER_THAN_OR_EQUAL,
							PREPROCESSOR_EXPRESSION_TAG_GREATER_THAN_OR_EQUAL,
							nextToken
						);
						break;
					}
					if(Token_equalString(nextToken,"<")){
						PREPROCESSOR_EXPRESSION_PARSE_CHECK_PRECEDENCE(PREPROCESSOR_OPERATOR_PRECEDENCE_LESSER_THAN)

						PreprocessorExpression_parse_processOperator(
							preprocessor,
							2,
							&nextTokenIndex,
							out,
							if_expr_tokens,
							PREPROCESSOR_OPERATOR_PRECEDENCE_LESSER_THAN,
							PREPROCESSOR_EXPRESSION_TAG_LESSER_THAN,
							nextToken
						);
						break;
					}
					if(Token_equalString(nextToken,"<=")){
						PREPROCESSOR_EXPRESSION_PARSE_CHECK_PRECEDENCE(PREPROCESSOR_OPERATOR_PRECEDENCE_LESSER_THAN_OR_EQUAL)

						PreprocessorExpression_parse_processOperator(
							preprocessor,
							2,
							&nextTokenIndex,
							out,
							if_expr_tokens,
							PREPROCESSOR_OPERATOR_PRECEDENCE_LESSER_THAN_OR_EQUAL,
							PREPROCESSOR_EXPRESSION_TAG_LESSER_THAN_OR_EQUAL,
							nextToken
						);
						break;
					}
					if(Token_equalString(nextToken,"==")){
						PREPROCESSOR_EXPRESSION_PARSE_CHECK_PRECEDENCE(PREPROCESSOR_OPERATOR_PRECEDENCE_EQUAL)

						PreprocessorExpression_parse_processOperator(
							preprocessor,
							2,
							&nextTokenIndex,
							out,
							if_expr_tokens,
							PREPROCESSOR_OPERATOR_PRECEDENCE_EQUAL,
							PREPROCESSOR_EXPRESSION_TAG_EQUAL,
							nextToken
						);
						break;
					}
					if(Token_equalString(nextToken,"!=")){
						PREPROCESSOR_EXPRESSION_PARSE_CHECK_PRECEDENCE(PREPROCESSOR_OPERATOR_PRECEDENCE_UNEQUAL)

						PreprocessorExpression_parse_processOperator(
							preprocessor,
							2,
							&nextTokenIndex,
							out,
							if_expr_tokens,
							PREPROCESSOR_OPERATOR_PRECEDENCE_UNEQUAL,
							PREPROCESSOR_EXPRESSION_TAG_UNEQUAL,
							nextToken
						);
						break;
					}
					if(Token_equalString(nextToken,"+")){
						PREPROCESSOR_EXPRESSION_PARSE_CHECK_PRECEDENCE(PREPROCESSOR_OPERATOR_PRECEDENCE_ADD)

						PreprocessorExpression_parse_processOperator(
							preprocessor,
							2,
							&nextTokenIndex,
							out,
							if_expr_tokens,
							PREPROCESSOR_OPERATOR_PRECEDENCE_ADD,
							PREPROCESSOR_EXPRESSION_TAG_ADD,
							nextToken
						);
						break;
					}
					if(Token_equalString(nextToken,"-")){
						PREPROCESSOR_EXPRESSION_PARSE_CHECK_PRECEDENCE(PREPROCESSOR_OPERATOR_PRECEDENCE_SUBTRACT)

						PreprocessorExpression_parse_processOperator(
							preprocessor,
							2,
							&nextTokenIndex,
							out,
							if_expr_tokens,
							PREPROCESSOR_OPERATOR_PRECEDENCE_SUBTRACT,
							PREPROCESSOR_EXPRESSION_TAG_SUBTRACT,
							nextToken
						);
						break;
					}
					if(Token_equalString(nextToken,"!")){
						//PREPROCESSOR_EXPRESSION_PARSE_CHECK_PRECEDENCE(PREPROCESSOR_OPERATOR_PRECEDENCE_NOT)

						PreprocessorExpression_parse_processOperator(
							preprocessor,
							1,
							&nextTokenIndex,
							out,
							if_expr_tokens,
							PREPROCESSOR_OPERATOR_PRECEDENCE_NOT,
							PREPROCESSOR_EXPRESSION_TAG_NOT,
							nextToken
						);
						break;
					}
					if(Token_equalString(nextToken,"?")){
						if(out->tag==0)fatal("unexpected token %s",Token_print(nextToken));
						nextTokenIndex++;

						// parse new expression
						struct PreprocessorExpression sub_expr={};
						nextTokenIndex+=PreprocessorExpression_parse(
							preprocessor,
							(array){
								.data=((Token*)if_expr_tokens.data)+nextTokenIndex,
								.len=if_expr_tokens.len-nextTokenIndex,
								.elem_size=if_expr_tokens.elem_size,
								// omit cap
							},
							&sub_expr,
							PREPROCESSOR_OPERATOR_PRECEDENCE_TERNARY
						);

						if(nextTokenIndex>=if_expr_tokens.len)fatal("expected : after ?");

						nextToken=array_get(&if_expr_tokens,nextTokenIndex);
						if(!Token_equalString(nextToken,":"))fatal("expected : after ?");

						nextTokenIndex++;

						// parse new expression
						struct PreprocessorExpression sub_expr2={};
						nextTokenIndex+=PreprocessorExpression_parse(
							preprocessor,
							(array){
								.data=((Token*)if_expr_tokens.data)+nextTokenIndex,
								.len=if_expr_tokens.len-nextTokenIndex,
								.elem_size=if_expr_tokens.elem_size,
								// omit cap
							},
							&sub_expr2,
							PREPROCESSOR_OPERATOR_PRECEDENCE_TERNARY
						);

						*out=(struct PreprocessorExpression){
							.tag=PREPROCESSOR_EXPRESSION_TAG_TERNARY,
							.ternary={
								.condition=allocAndCopy(sizeof(struct PreprocessorExpression),out),
								.then=allocAndCopy(sizeof(struct PreprocessorExpression),&sub_expr),
								.else_=allocAndCopy(sizeof(struct PreprocessorExpression),&sub_expr2)
							}
						};
						break;
					}

					if(Token_equalString(nextToken,":")){
						PREPROCESSOR_EXPRESSION_PARSE_CHECK_PRECEDENCE(PREPROCESSOR_OPERATOR_PRECEDENCE_TERNARY)

						fatal("unexpected token %s",Token_print(nextToken));
					}

					fatal("unexpected keyword %s",Token_print(nextToken));
				}
				break;
			default:
				fatal("unimplemented token: %s",Token_print(nextToken));
		}
	}
	if(openParanthesis>0)fatal("%d unmatched paranthesis at %s",openParanthesis,Token_print(array_get(&if_expr_tokens,0)));

PREPROCESSOR_EXPRESSION_PARSE_RET_SUCCESS:
	if(out->tag==0){
PREPROCESSOR_EXPRESSION_PARSE_RET_FAILURE:
		fatal("expected expression, got instead %s",Token_print(nextToken));
	}
	return nextTokenIndex;
}

bool PreprocessorIfStack_getLastValue(struct PreprocessorIfStack*item){
	if(item->items.len==0) fatal("stack is empty");
	struct PreprocessorIfStackItem*last_item=array_get(&item->items,item->items.len-1);
	switch(last_item->tag){
		case PREPROCESSOR_STACK_ITEM_TYPE_IF:
			return last_item->if_.expr->value;
		case PREPROCESSOR_STACK_ITEM_TYPE_ELSE_IF:
			return last_item->else_if.expr->value;
		case PREPROCESSOR_STACK_ITEM_TYPE_ELSE:
			return true;
		default:
			fatal("unknown preprocessor stack item tag %d",last_item->tag);
	}
}
/* parse preprocessor expression starting from current token */
struct PreprocessorExpression* Preprocessor_parseExpression(struct Preprocessor*preprocessor){
	Token token={};
	int ntr=TokenIter_lastToken(&preprocessor->token_iter,&token);

	array if_expr_tokens={};
	array_init(&if_expr_tokens,sizeof(Token));
	// read tokens until newline
	int line_num=token.line;
	while(!TokenIter_isEmpty(&preprocessor->token_iter)){
		// check for line continuation
		if(Token_equalString(&token,"\\")){
			ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
			if(!ntr) fatal("");
			line_num=token.line;
			continue;
		}

		if(token.line>line_num){
			break;
		}

		if(Token_equalString(&token,"defined")){
			ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
			if(!ntr) fatal("expected symbol after defined keyword");

			// next argument can be freestanding, or surrounded by paranthesis
			bool paranthesis=false;
			if(Token_equalString(&token,"(")){
				paranthesis=true;
				ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
				if(!ntr) fatal("expected symbol after defined keyword");
			}
			if(token.tag!=TOKEN_TAG_SYMBOL){							
				fatal("expected symbol after defined keyword, got instead %s",Token_print(&token));
			}

			// check if symbol is defined
			int defined=0;
			for(int i=0;i<preprocessor->defines.len;i++){
				Token* define_token=array_get(&preprocessor->defines,i);
				if(Token_equalToken(define_token,&token)){
					defined=1;
					break;
				}
			}

			// append 1 or 0 to if_expr_tokens
			array_append(&if_expr_tokens,(Token[]){
				{
					.tag=TOKEN_TAG_LITERAL,
					.filename=token.filename,
					.line=token.line,
					.col=token.col,
					.len=1,
					.p=defined?"1":"0",
					.literal={
						.tag=TOKEN_LITERAL_TAG_NUMERIC,
						.numeric={
							.tag=TOKEN_LITERAL_NUMERIC_TAG_INTEGER,
							.value.int_=defined?1:0
						}
					}
				}
			});

			ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
			if(!ntr) fatal("expected symbol after defined keyword");

			if(paranthesis){
				// check for closing paranthesis
				if(!Token_equalString(&token,")")) fatal("expected closing paranthesis after defined keyword");

				ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
				if(!ntr) fatal("expected symbol after defined keyword");
			}

			continue;
		}

		array_append(&if_expr_tokens,&token);
		ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
		if(!ntr) fatal("");
	}

	// expand macros in expression
	array expanded_expr_tokens={};
	array_init(&expanded_expr_tokens,sizeof(Token));
	Preprocessor_expandMacros(preprocessor,if_expr_tokens.len,if_expr_tokens.data,&expanded_expr_tokens);

	// parse expression from tokens
	struct PreprocessorExpression if_expr={};
	// print expanded_expr_tokens
	if(DEBUG_PRINTS){
		println("expanded expression:");
		for(int i=0;i<expanded_expr_tokens.len;i++){
			Token* token=array_get(&expanded_expr_tokens,i);
			printf("%s\n",Token_print(token));
		}
	}
	PreprocessorExpression_parse(preprocessor,expanded_expr_tokens,&if_expr,PREPROCESSOR_OPERATOR_PRECEDENCE_MAX);
	Preprocessor_evalExpression(preprocessor,&if_expr);

	return allocAndCopy(sizeof(struct PreprocessorExpression),&if_expr);
}

void Preprocessor_consume(struct Preprocessor *preprocessor, struct TokenIter *token_iter){
	struct TokenIter old_token_iter=preprocessor->token_iter;
	preprocessor->token_iter=*token_iter;
	
	println("running preprocessor on file %s",preprocessor->token_iter.tokenizer->token_src);

	/* last attempt to fetch a token was successfull? */
	int ntr=1;
	Token token;

	ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
	if(!ntr) fatal("");
	while(ntr){
		// check for preprocessor directives
		if(token.len==1 && token.p[0]=='#'){
			ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
			if(!ntr) fatal("");

			if(Token_equalString(&token, "if")){
				Token ifToken=token;
				// get next token
				ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
				if(!ntr) fatal("");

				// parse expression
				struct PreprocessorExpression* if_expr=Preprocessor_parseExpression(preprocessor);
				ntr=TokenIter_lastToken(&preprocessor->token_iter,&token);

				struct PreprocessorIfStack new_if_stack=(struct PreprocessorIfStack){
					.anyPathEvaluatedToTrue=false,
					.inherited_doSkip=preprocessor->doSkip,
					.items={}
				};
				array_init(&new_if_stack.items, sizeof(struct PreprocessorIfStackItem));

				struct PreprocessorIfStackItem item={
					.tag=PREPROCESSOR_STACK_ITEM_TYPE_IF,
					.if_={
						.if_token=ifToken,
						.expr=allocAndCopy(sizeof(struct PreprocessorExpression),if_expr)
					}
				};
				array_append(&new_if_stack.items,&item);

				preprocessor->doSkip=new_if_stack.inherited_doSkip || new_if_stack.anyPathEvaluatedToTrue || !if_expr->value;
				new_if_stack.anyPathEvaluatedToTrue|=(bool)if_expr->value;

				array_append(&preprocessor->stack,&new_if_stack);

				continue;
			}
			if(Token_equalString(&token, "ifndef")){
				Token ifToken=token;
				// read define argument
				ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
				if(!ntr) fatal("");
				if(token.tag!=TOKEN_TAG_SYMBOL){
					fatal("expected symbol after #ifdef directive but got instead %s",Token_print(&token));
				}

				char* define_name=calloc(token.len+1,1);
				discard sprintf(define_name,"%.*s",token.len,token.p);

				ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
				if(!ntr) fatal("");

				// check if define is already defined
				struct PreprocessorExpression if_expr={
					.tag=PREPROCESSOR_EXPRESSION_TAG_NOT,
					.not={
						.expr=allocAndCopy(sizeof(struct PreprocessorExpression),&(struct PreprocessorExpression){
							.tag=PREPROCESSOR_EXPRESSION_TAG_DEFINED,
							.defined={
								.name=define_name
							}
						}
					)}
				};
				Preprocessor_evalExpression(preprocessor,&if_expr);

				// create new stack
				struct PreprocessorIfStack new_if_stack=(struct PreprocessorIfStack){
					.anyPathEvaluatedToTrue=false,
					.inherited_doSkip=preprocessor->doSkip,
					.items={}
				};
				array_init(&new_if_stack.items, sizeof(struct PreprocessorIfStackItem));

				// push if statement on stack
				struct PreprocessorIfStackItem item={
					.tag=PREPROCESSOR_STACK_ITEM_TYPE_IF,
					.if_={
						.if_token=ifToken,
						.expr=allocAndCopy(sizeof(if_expr),&if_expr)
					}
				};
				array_append(&new_if_stack.items,&item);

				// update stack evaluation state
				preprocessor->doSkip=new_if_stack.inherited_doSkip || new_if_stack.anyPathEvaluatedToTrue || !if_expr.value;
				new_if_stack.anyPathEvaluatedToTrue|=(bool)if_expr.value;

				// push stack on stack list
				array_append(&preprocessor->stack,&new_if_stack);

				continue;
			}
			if(Token_equalString(&token,"ifdef")){
				Token ifToken=token;
				// read define argument
				ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
				if(!ntr) fatal("");
				if(token.tag!=TOKEN_TAG_SYMBOL){
					fatal("expected symbol after #ifdef directive but got instead %s",Token_print(&token));
				}

				char* define_name=calloc(token.len+1,1);
				discard sprintf(define_name,"%.*s",token.len,token.p);

				ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
				if(!ntr) fatal("");

				// check if define is already defined
				struct PreprocessorExpression if_expr={
					.tag=PREPROCESSOR_EXPRESSION_TAG_DEFINED,
					.defined={.name=define_name}
				};
				Preprocessor_evalExpression(preprocessor,&if_expr);

				// create new stack
				struct PreprocessorIfStack new_if_stack=(struct PreprocessorIfStack){
					.anyPathEvaluatedToTrue=false,
					.inherited_doSkip=preprocessor->doSkip,
					.items={}
				};
				array_init(&new_if_stack.items, sizeof(struct PreprocessorIfStackItem));

				// update stack evaluation state
				preprocessor->doSkip=new_if_stack.inherited_doSkip || new_if_stack.anyPathEvaluatedToTrue || !if_expr.value;
				new_if_stack.anyPathEvaluatedToTrue|=(bool)if_expr.value;

				// push if statement on stack
				struct PreprocessorIfStackItem item={
					.tag=PREPROCESSOR_STACK_ITEM_TYPE_IF,
					.if_={
						.if_token=ifToken,
						.expr=allocAndCopy(sizeof(if_expr),&if_expr)
					}
				};
				array_append(&new_if_stack.items,&item);

				// push stack on stack list
				array_append(&preprocessor->stack,&new_if_stack);

				continue;
			}
			if(Token_equalString(&token,"elif")){
				Token elifToken=token;
				ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
				if(!ntr) fatal("");

				// parse expression from tokens
				struct PreprocessorExpression *if_expr=Preprocessor_parseExpression(preprocessor);
				ntr=TokenIter_lastToken(&preprocessor->token_iter,&token);

				// get reference to last ifstack
				if(preprocessor->stack.len==0) fatal("elif without if");
				struct PreprocessorIfStack* if_stack=array_get(&preprocessor->stack,preprocessor->stack.len-1);

				// write back to stack
				struct PreprocessorIfStackItem item={
					.tag=PREPROCESSOR_STACK_ITEM_TYPE_ELSE_IF,
					.else_if={
						.else_token=elifToken,
						.expr=allocAndCopy(sizeof(struct PreprocessorExpression),if_expr),
					}
				};
				array_append(&if_stack->items,&item);

				preprocessor->doSkip=if_stack->inherited_doSkip || if_stack->anyPathEvaluatedToTrue || !if_expr->value;
				if_stack->anyPathEvaluatedToTrue|=(bool)if_expr->value;

				continue;
			}
			if(Token_equalString(&token,"else")){
				Token elseToken=token;
				TokenIter_nextToken(&preprocessor->token_iter,&token);

				// push else statement on stack
				struct PreprocessorIfStackItem item={
					.tag=PREPROCESSOR_STACK_ITEM_TYPE_ELSE,
					.else_={
						.else_token=elseToken
					}
				};
				
				// get reference to last ifstack
				if(preprocessor->stack.len==0) fatal("else without if at %s",Token_print(&elseToken));
				struct PreprocessorIfStack* if_stack=array_get(&preprocessor->stack,preprocessor->stack.len-1);

				// append else to stack
				array_append(&if_stack->items,&item);

				// set doSkip to inherited doSkip
				preprocessor->doSkip=if_stack->inherited_doSkip || if_stack->anyPathEvaluatedToTrue;
				if_stack->anyPathEvaluatedToTrue=true; // superfluous, but for clarity

				continue;
			}
			if(Token_equalString(&token,"endif")){
				ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);

				if(preprocessor->stack.len==0) fatal("endif without if");
				// pop stack
				array_pop_back(&preprocessor->stack);

				// set doSkip to inherited doSkip
				if(preprocessor->stack.len==0){
					preprocessor->doSkip=false;
				}else{
					struct PreprocessorIfStack* if_stack=array_get(&preprocessor->stack,preprocessor->stack.len-1);
					preprocessor->doSkip=if_stack->inherited_doSkip || !PreprocessorIfStack_getLastValue(if_stack);
				}
				
				continue;
			}

			// free-standing preprocessor directives
			if(Token_equalString(&token,"include")){
				ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
				if(!ntr) fatal("no token after #include %s",Token_print(&token));

				Preprocessor_processInclude(preprocessor);
				ntr=TokenIter_lastToken(&preprocessor->token_iter,&token);

				continue;
			}
			if(Token_equalString(&token,"define")){
				ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
				if(!ntr) fatal("no token after #define");

				Preprocessor_processDefine(preprocessor);
				ntr=TokenIter_lastToken(&preprocessor->token_iter,&token);

				continue;
			}
			if(Token_equalString(&token,"undef")){
				ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
				if(!ntr) fatal("no token after #undef");

				Preprocessor_processUndefine(preprocessor);
				ntr=TokenIter_lastToken(&preprocessor->token_iter,&token);

				continue;
			}
			if(Token_equalString(&token, "pragma")){
				ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
				if(!ntr) fatal("no token after #pragma");

				Preprocessor_processPragma(preprocessor);
				ntr=TokenIter_lastToken(&preprocessor->token_iter,&token);

				continue;
			}
			if(Token_equalString(&token,"error")){
				ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
				if(!ntr) fatal("no token after #error");

				Preprocessor_processError(preprocessor);
				ntr=TokenIter_lastToken(&preprocessor->token_iter,&token);

				continue;
			}
			if(Token_equalString(&token,"warning")){
				ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
				if(!ntr) fatal("no token after #warning");

				Preprocessor_processWarning(preprocessor);
				ntr=TokenIter_lastToken(&preprocessor->token_iter,&token);

				continue;
			}

			fatal("unknown preprocessor directive %s",Token_print(&token));
		}

		// get view of all tokens until next preprocessor directive
		array new_tokens={};
		array_init(&new_tokens,sizeof(Token));
		while(1){
			if(!preprocessor->doSkip){
				array_append(&new_tokens,&token);
			}

			if(TokenIter_isEmpty(&preprocessor->token_iter)){
				break;
			}

			ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
			if(!ntr){
				break;
			}

			if(Token_equalString(&token,"#")){
				break;
			}
		}

		if(!preprocessor->doSkip){
			// expand the single token
			Preprocessor_expandMacros(preprocessor,new_tokens.len,new_tokens.data,&preprocessor->tokens_out);
		}

		if(TokenIter_isEmpty(&preprocessor->token_iter)){
			break;
		}
	}

	// restore old token iter
	preprocessor->token_iter=old_token_iter;
}
