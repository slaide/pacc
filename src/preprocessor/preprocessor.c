#include "tokenizer.h"
#include <unistd.h> // access

#include<util/util.h>

#include<preprocessor/preprocessor.h>

void Preprocessor_init(struct Preprocessor*preprocessor,struct Tokenizer *tokenizer){
	array_init(&preprocessor->include_paths,sizeof(char*));
	array_init(&preprocessor->defines,sizeof(Token));
	array_init(&preprocessor->already_included_files,sizeof(char*));

	preprocessor->tokenizer_in=tokenizer;
	TokenIter_init(&preprocessor->token_iter,preprocessor->tokenizer_in,(struct TokenIterConfig){.skip_comments=true,});

	array_init(&preprocessor->stack.items,sizeof(struct PreprocessorStackItem));

	// read all tokens into memory
	array_init(&preprocessor->tokens_out,sizeof(Token));
}

int Preprocessor_evalExpression(struct Preprocessor *preprocessor,struct PreprocessorExpression*expr){
	switch(expr->tag){
		case PREPROCESSOR_EXPRESSION_TAG_DEFINED:
			expr->value=0;
			for(int i=0;i<preprocessor->defines.len;i+=2){
				Token* define_token=array_get(&preprocessor->defines,i);
				if(Token_equalString(define_token,expr->defined.name)){
					expr->value=1;
					break;
				}
			}
			return expr->value;
		case PREPROCESSOR_EXPRESSION_TAG_NOT:
			expr->value=!Preprocessor_evalExpression(preprocessor,expr->not.expr);
			return expr->value;
		case PREPROCESSOR_EXPRESSION_TAG_AND:
			expr->value=Preprocessor_evalExpression(preprocessor,expr->and.lhs) && Preprocessor_evalExpression(preprocessor,expr->and.rhs);
			return expr->value;
		case PREPROCESSOR_EXPRESSION_TAG_OR:
			expr->value=Preprocessor_evalExpression(preprocessor,expr->or.lhs) || Preprocessor_evalExpression(preprocessor,expr->or.rhs);
			return expr->value;
		case PREPROCESSOR_EXPRESSION_TAG_ELSE:
			expr->value=1;
			return expr->value;
		case PREPROCESSOR_EXPRESSION_TAG_LITERAL:
			return expr->value;
		default:
			fatal("unknown preprocessor expression tag %d",expr->tag);
	}
	return 0;
}

void Preprocessor_processInclude(struct Preprocessor*preprocessor){
	Token token;

	// read include argument
	TokenIter_nextToken(&preprocessor->token_iter,&token);
	if(token.tag!=TOKEN_TAG_PREP_INCLUDE_ARGUMENT && token.tag!=TOKEN_TAG_LITERAL_STRING){
		fatal("expected include argument after #include directive but got instead %.*s",token.len,token.p);
	}

	char* include_path=calloc(token.len-1,1);
	sprintf(include_path,"%.*s",token.len-2,token.p+1);
	println("include path: %s",include_path);

	// go through each entry in include paths and check if file exists there
	char* include_file_path=nullptr;
	for(int i=0;i<preprocessor->include_paths.len;i++){
		char* include_dir=*(char**)array_get(&preprocessor->include_paths,i);

		include_file_path=calloc(strlen(include_dir)+strlen(include_path)+20,1);
		sprintf(include_file_path,"%s/%s",include_dir,include_path);

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
		TokenIter_nextToken(&preprocessor->token_iter,&token);
		return;
	}

	// read include file
	File include_file;
	File_read(include_file_path,&include_file);
	// tokenize include file
	Tokenizer include_tokenizer;
	Tokenizer_init(&include_tokenizer,&include_file);
	// preprocess include file
	Preprocessor_run(preprocessor);
	
	// append tokens to current token list
	for(int i=0;i<include_tokenizer.num_tokens;i++){
		//array_append(&tokens,&include_tokenizer.tokens[i]);
	}

	TokenIter_nextToken(&preprocessor->token_iter,&token);
}
void Preprocessor_processDefine(struct Preprocessor*preprocessor){
	Token token;

	// read define argument
	TokenIter_nextToken(&preprocessor->token_iter,&token);
	println("next token %s",Token_print(&token));
	if(token.tag!=TOKEN_TAG_SYMBOL){
		fatal("expected symbol after #define directive but got instead %s",Token_print(&token));
	}

	char* define_name=calloc(token.len+1,1);
	sprintf(define_name,"%.*s",token.len,token.p);

	//array_append(&preprocessor->defines,&(Token){.len=strlen(define_name),.p=define_name});

	// read define value
	int define_line_num=token.line;
	TokenIter_nextToken(&preprocessor->token_iter,&token);
	if(token.line==define_line_num){
		char* define_value=calloc(token.len+1,1);
		sprintf(define_value,"%.*s",token.len,token.p);

		println("define %s as %s",define_name,define_value);

		TokenIter_nextToken(&preprocessor->token_iter,&token);
	}
}
void PreprocessorExpression_parse(
	array if_expr_tokens,
	struct PreprocessorExpression *out,
	int precedence
){
	for(int nextTokenIndex=0;nextTokenIndex<if_expr_tokens.len;nextTokenIndex++){
		Token* nextToken=array_get(&if_expr_tokens,nextTokenIndex);
		switch(nextToken->tag){
			case TOKEN_TAG_LITERAL_INTEGER:
				out->tag=PREPROCESSOR_EXPRESSION_TAG_LITERAL;
				out->value=atoi(nextToken->p);
				break;
			default:
				fatal("unimplemented token tag %d",nextToken->tag);
		}
	}
	return;
}
void Preprocessor_run(struct Preprocessor *preprocessor){
	Token token;
	println("running preprocessor");
	TokenIter_nextToken(&preprocessor->token_iter,&token);
	while(!TokenIter_isEmpty(&preprocessor->token_iter)){
		// check for preprocessor directives
		if(token.len==1 && token.p[0]=='#'){
			TokenIter_nextToken(&preprocessor->token_iter,&token);

			println("token %s",Token_print(&token));

			if(Token_equalString(&token,"include")){
				Preprocessor_processInclude(preprocessor);
				TokenIter_lastToken(&preprocessor->token_iter,&token);
				continue;
			}else if(Token_equalString(&token,"define")){
				Preprocessor_processDefine(preprocessor);
				TokenIter_lastToken(&preprocessor->token_iter,&token);
				continue;
			}else if(Token_equalString(&token, "pragma")){
				TokenIter_nextToken(&preprocessor->token_iter,&token);
				if(Token_equalString(&token,"once")){
					// read include argument
					TokenIter_nextToken(&preprocessor->token_iter,&token);

					// add file to list of already included files
					char* include_path_copy=allocAndCopy(strlen(preprocessor->tokenizer_in->tokens[0].filename)+1,preprocessor->tokenizer_in->tokens[0].filename);
					array_append(&preprocessor->already_included_files,&include_path_copy);

					continue;
				}
				println("unknown pragma %s",Token_print(&token));
				int line_num=token.line;
				while(!TokenIter_isEmpty(&preprocessor->token_iter)){
					TokenIter_nextToken(&preprocessor->token_iter,&token);
					if(token.line!=line_num){
						break;
					}
				}
				fatal("unknown pragma");
			}else if(Token_equalString(&token, "ifndef")){
				// read define argument
				TokenIter_nextToken(&preprocessor->token_iter,&token);
				if(token.tag!=TOKEN_TAG_SYMBOL){
					fatal("expected symbol after #ifndef directive but got instead %s",Token_print(&token));
				}

				char* define_name=calloc(token.len+1,1);
				sprintf(define_name,"%.*s",token.len,token.p);

				// check if define is already defined
				bool define_already_defined=false;
				for(int i=0;i<preprocessor->defines.len;i+=2){
					Token* define_token=array_get(&preprocessor->defines,i);
					if(Token_equalString(define_token,define_name)){
						define_already_defined=true;
						break;
					}
				}

				// if not defined, skip to #endif
				if(define_already_defined){
					// skip over tokens until #endif or #else or #elif
					bool stop=false;
					while(!TokenIter_isEmpty(&preprocessor->token_iter) && !stop){
						TokenIter_nextToken(&preprocessor->token_iter,&token);
						if(token.len==1 && token.p[0]=='#'){
							TokenIter_nextToken(&preprocessor->token_iter,&token);
							if(Token_equalString(&token,"endif")){
								stop=true;
								break;
							}else if(Token_equalString(&token,"else") || Token_equalString(&token,"elif")){
								// skip over tokens until #endif
								while(!TokenIter_isEmpty(&preprocessor->token_iter) && !stop){
									TokenIter_nextToken(&preprocessor->token_iter,&token);
									if(token.len==1 && token.p[0]=='#'){
										TokenIter_nextToken(&preprocessor->token_iter,&token);
										if(Token_equalString(&token,"endif")){
											stop=true;
											break;
										}
									}
								}
								stop=true;
								break;
							}
						}
					}
				}

				continue;
			}else if(Token_equalString(&token, "if")){
				// get next token
				TokenIter_nextToken(&preprocessor->token_iter,&token);

				// parse preprocessor expression
				// can use: <symbol>, <symbol>(args), defined, !, &&, ||, ==, !=, <=, >=, <, >, +, -, *, /, %, (, )
				array if_expr_tokens;
				array_init(&if_expr_tokens,sizeof(Token));
				// read tokens until newline
				int line_num=token.line;
				while(!TokenIter_isEmpty(&preprocessor->token_iter)){
					if(token.line!=line_num){
						break;
					}
					array_append(&if_expr_tokens,&token);
					TokenIter_nextToken(&preprocessor->token_iter,&token);

					// if token is \, adjust line_num, fetch next token and continue
					if(Token_equalString(&token,"\\")){
						TokenIter_nextToken(&preprocessor->token_iter,&token);
						line_num=token.line;
					}
				}

				// push token on stack
				struct PreprocessorExpression if_expr={};
				PreprocessorExpression_parse(if_expr_tokens,&if_expr,0);
				Preprocessor_evalExpression(preprocessor,&if_expr);

				struct PreprocessorStackItem item={
					.tag=PREPROCESSOR_STACK_ITEM_TYPE_IF,
					.if_={
						.if_token=token,
						.expr=allocAndCopy(sizeof(struct PreprocessorExpression),&if_expr)
					}
				};
				array_append(&preprocessor->stack.items,&item);

				continue;
			}else if(Token_equalString(&token,"elif")){
				TokenIter_nextToken(&preprocessor->token_iter,&token);
				// check last stack item, which is either if or elif
				// check its value
				// 1) if if statement, and that value is true, set flag that that had triggered
				// 2) if elif statement and either flag is set, or value is true, set flag here that had triggered
				// 3) if neither, evaluate expression

				bool anyPreviousIfEvaluatedToTrue=false;
				if(preprocessor->stack.items.len>0){
					struct PreprocessorStackItem* top_item=array_get(&preprocessor->stack.items,preprocessor->stack.items.len-1);
					switch(top_item->tag){
						case PREPROCESSOR_STACK_ITEM_TYPE_IF:{
							if(top_item->if_.expr->value){
								anyPreviousIfEvaluatedToTrue=true;
							}
							break;
						}
						case PREPROCESSOR_STACK_ITEM_TYPE_ELSE_IF:{
							if(
								top_item->else_if.anyPreviousIfEvaluatedToTrue
								|| top_item->else_if.expr->value
							){
								anyPreviousIfEvaluatedToTrue=true;
							}
							break;
						}
						default: break;
					}

					// read preprocessor expression tokens
					array if_expr_tokens;
					array_init(&if_expr_tokens,sizeof(Token));
					// read tokens until newline
					int line_num=token.line;
					while(!TokenIter_isEmpty(&preprocessor->token_iter)){
						if(token.line!=line_num){
							break;
						}
						array_append(&if_expr_tokens,&token);
						TokenIter_nextToken(&preprocessor->token_iter,&token);

						// if token is \, adjust line_num, fetch next token and continue
						if(Token_equalString(&token,"\\")){
							TokenIter_nextToken(&preprocessor->token_iter,&token);
							line_num=token.line;
						}
					}
					// parse expression from tokens
					struct PreprocessorExpression if_expr={};
					PreprocessorExpression_parse(if_expr_tokens,&if_expr,0);
					// eval only if anyPreviousIfEvaluatedToTrue is false
					if(!anyPreviousIfEvaluatedToTrue){
						Preprocessor_evalExpression(preprocessor,&if_expr);
					}

					// write back to stack
					struct PreprocessorStackItem item={
						.tag=PREPROCESSOR_STACK_ITEM_TYPE_ELSE_IF,
						.else_if={
							.else_token=token,
							.expr=allocAndCopy(sizeof(struct PreprocessorExpression),&if_expr),
							.anyPreviousIfEvaluatedToTrue=anyPreviousIfEvaluatedToTrue
						}
					};
					array_append(&preprocessor->stack.items,&item);
				}
				continue;
			}else if(Token_equalString(&token,"endif")){
				TokenIter_nextToken(&preprocessor->token_iter,&token);

				// pop stack
				int num_items_to_pop=0;
				for(int i=preprocessor->stack.items.len-1;i>=0;i--){
					struct PreprocessorStackItem* item=array_get(&preprocessor->stack.items,i);
					num_items_to_pop++;
					if(item->tag==PREPROCESSOR_STACK_ITEM_TYPE_IF){
						break;
					}
				}
				for(int i=0;i<num_items_to_pop;i++)
					array_pop_back(&preprocessor->stack.items);
				println("popped %d items from stack, now got %d left",num_items_to_pop,preprocessor->stack.items.len);
				continue;
			}else if(Token_equalString(&token,"ifdef")){
				// read define argument
				TokenIter_nextToken(&preprocessor->token_iter,&token);
				if(token.tag!=TOKEN_TAG_SYMBOL){
					fatal("expected symbol after #ifdef directive but got instead %s",Token_print(&token));
				}

				char* define_name=calloc(token.len+1,1);
				sprintf(define_name,"%.*s",token.len,token.p);

				// check if define is already defined
				struct PreprocessorExpression expr={.tag=PREPROCESSOR_EXPRESSION_TAG_DEFINED,.defined={.name=define_name}};
				int expr_value=Preprocessor_evalExpression(preprocessor,&expr);
				// push if statement on stack
				struct PreprocessorStackItem item={.tag=PREPROCESSOR_STACK_ITEM_TYPE_IF,.if_={.if_token=token,.expr=&expr}};
				array_append(&preprocessor->stack.items,&item);

				continue;
			}else if(Token_equalString(&token,"else")){
				TokenIter_nextToken(&preprocessor->token_iter,&token);

				// push else statement on stack
				struct PreprocessorStackItem item={
					.tag=PREPROCESSOR_STACK_ITEM_TYPE_ELSE,
					.else_={
						._reservedAndUnused=0
					}
				};
				
				array_append(&preprocessor->stack.items,&item);
				continue;
			} 

			fatal("unknown preprocessor directive %s",Token_print(&token));
		}

		bool append_token=true;
		if(preprocessor->stack.items.len>0){
			struct PreprocessorStackItem* top_item=array_get(&preprocessor->stack.items,preprocessor->stack.items.len-1);

			switch(top_item->tag){
				case PREPROCESSOR_STACK_ITEM_TYPE_IF:
					if(!top_item->if_.expr){
						fatal("");
					}
					if(!top_item->if_.expr->value){
						append_token=false;
					}
					break;
				case PREPROCESSOR_STACK_ITEM_TYPE_ELSE_IF:
					println("found elseif")
					if(!top_item->else_if.expr){
						fatal("");
					}
					if(!(top_item->else_if.anyPreviousIfEvaluatedToTrue || top_item->else_if.expr->value)){
						append_token=false;
					}
					break;
				case PREPROCESSOR_STACK_ITEM_TYPE_ELSE:
					// check previous stack item, which is expected to be an if
					// then invert the value of the if expression
					if(preprocessor->stack.items.len<2){
						fatal("else directive without matching if");
					}
					struct PreprocessorStackItem* prev_item=array_get(&preprocessor->stack.items,preprocessor->stack.items.len-2);
					switch(prev_item->tag){
						case PREPROCESSOR_STACK_ITEM_TYPE_IF:
							if(!prev_item->if_.expr){
								fatal("");
							}
							if(prev_item->if_.expr->value){
								append_token=false;
							}
							break;
						case PREPROCESSOR_STACK_ITEM_TYPE_ELSE_IF:
							if(!prev_item->else_if.expr){
								fatal("");
							}
							if(prev_item->else_if.anyPreviousIfEvaluatedToTrue || prev_item->else_if.expr->value){
								append_token=false;
							}
							break;
						default:
							fatal("else directive without matching if");
					}
					break;
				default:
					fatal("unimplemented preprocessor stack item tag %d",top_item->tag);
			}
		}

		if(append_token)
			array_append(&preprocessor->tokens_out,&token);

		TokenIter_nextToken(&preprocessor->token_iter,&token);
	}

	// print preprocessed tokens
	
}