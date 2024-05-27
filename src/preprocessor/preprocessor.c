#include <libgen.h>
#include <string.h>
#include <unistd.h> // access

#include<util/util.h>

#include<preprocessor/preprocessor.h>

void Preprocessor_init(struct Preprocessor*preprocessor){
	*preprocessor=(struct Preprocessor){};

	array_init(&preprocessor->include_paths,sizeof(char*));
	array_init(&preprocessor->defines,sizeof(struct PreprocessorDefine));
	array_init(&preprocessor->already_included_files,sizeof(char*));	

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
	int ntr=TokenIter_lastToken(&preprocessor->token_iter,&token);
	if(!ntr) fatal("");
	if(token.tag!=TOKEN_TAG_PREP_INCLUDE_ARGUMENT && token.tag!=TOKEN_TAG_LITERAL_STRING){
		fatal("expected include argument after #include directive but got instead %.*s",token.len,token.p);
	}

	bool local_include_path=token.p[0]=='"';

	char* include_path=calloc(token.len-1,1);
	sprintf(include_path,"%.*s",token.len-2,token.p+1);
	println("%s include path: %s",local_include_path?"local":"global",include_path);

	ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
	// we just consume the token, we don't use it in the rest of the function body
	discard ntr;

	// go through each entry in include paths and check if file exists there
	char* include_file_path=nullptr;
	for(int i=0-((int)local_include_path);i<preprocessor->include_paths.len;i++){
		char* include_dir=nullptr;
		if(i==-1){
			int tok_filename_len=strlen(preprocessor->token_iter.tokenizer->token_src);
			char*tok_filename=calloc(tok_filename_len,1);
			strncpy(tok_filename, preprocessor->token_iter.tokenizer->token_src, tok_filename_len);
			include_dir=dirname(tok_filename);
			println("testing local include dir: %s, from %s",include_dir,preprocessor->token_iter.tokenizer->token_src);
		}else{
			include_dir=*(char**)array_get(&preprocessor->include_paths,i);
		}

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
void Preprocessor_processDefine(struct Preprocessor*preprocessor){
	Token token;

	// read define argument
	int ntr=TokenIter_lastToken(&preprocessor->token_iter,&token);
	if(!ntr) fatal("");
	if(token.tag!=TOKEN_TAG_SYMBOL){
		fatal("expected symbol after #define directive but got instead %s",Token_print(&token));
	}

	Token define_name=token;

	// read define value
	int define_line_num=token.line;
	array define_value={};
	array_init(&define_value,sizeof(Token));
	while(TokenIter_nextToken(&preprocessor->token_iter,&token) && token.line==define_line_num){
		// append token to define value
		array_append(&define_value,&token);
	}

	array_append(
		&preprocessor->defines,
		&(struct PreprocessorDefine){
			.name=define_name,
			.tokens=define_value
		}
	);
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

			println("token %s",Token_print(&token));

			if(Token_equalString(&token, "if")){
				// get next token
				ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
				if(!ntr) fatal("");

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
					ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
					if(!ntr) fatal("");

					// if token is \, adjust line_num, fetch next token and continue
					if(Token_equalString(&token,"\\")){
						ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
						if(!ntr) fatal("");
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
				ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
				if(!ntr) fatal("");
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
						ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
						if(!ntr) fatal("");

						// if token is \, adjust line_num, fetch next token and continue
						if(Token_equalString(&token,"\\")){
							ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
							if(!ntr) fatal("");
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
				ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
				if(!ntr) fatal("");

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
			}else if(Token_equalString(&token, "ifndef")){
				// read define argument
				ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
				if(!ntr) fatal("");
				if(token.tag!=TOKEN_TAG_SYMBOL){
					fatal("expected symbol after #ifdef directive but got instead %s",Token_print(&token));
				}

				char* define_name=calloc(token.len+1,1);
				sprintf(define_name,"%.*s",token.len,token.p);

				ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
				if(!ntr) fatal("");

				// check if define is already defined
				struct PreprocessorExpression expr={
					.tag=PREPROCESSOR_EXPRESSION_TAG_NOT,
					.not={.expr=allocAndCopy(sizeof(struct PreprocessorExpression),&(struct PreprocessorExpression){
						.tag=PREPROCESSOR_EXPRESSION_TAG_DEFINED,
						.defined={.name=define_name}
					})}
				};
				int expr_value=Preprocessor_evalExpression(preprocessor,&expr);
				// push if statement on stack
				struct PreprocessorStackItem item={.tag=PREPROCESSOR_STACK_ITEM_TYPE_IF,.if_={.if_token=token,.expr=&expr}};
				array_append(&preprocessor->stack.items,&item);

				continue;
			}else if(Token_equalString(&token,"ifdef")){
				// read define argument
				ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
				if(!ntr) fatal("");
				if(token.tag!=TOKEN_TAG_SYMBOL){
					fatal("expected symbol after #ifdef directive but got instead %s",Token_print(&token));
				}

				char* define_name=calloc(token.len+1,1);
				sprintf(define_name,"%.*s",token.len,token.p);

				ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
				if(!ntr) fatal("");

				// check if define is already defined
				struct PreprocessorExpression expr={
					.tag=PREPROCESSOR_EXPRESSION_TAG_DEFINED,
					.defined={.name=define_name}
				};
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
			// free-standing preprocessor directives
			else{
				if(Token_equalString(&token,"include")){
					ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
					if(!ntr) fatal("no token after #include");

					Preprocessor_processInclude(preprocessor);
					ntr=TokenIter_lastToken(&preprocessor->token_iter,&token);

					continue;
				}else if(Token_equalString(&token,"define")){
					ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
					if(!ntr) fatal("no token after #define");

					Preprocessor_processDefine(preprocessor);
					ntr=TokenIter_lastToken(&preprocessor->token_iter,&token);

					continue;
				}else if(Token_equalString(&token,"undef")){
					ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
					if(!ntr) fatal("no token after #undef");
					if(token.tag!=TOKEN_TAG_SYMBOL){
						fatal("expected symbol after #undef directive but got instead %s",Token_print(&token));
					}

					char* define_name=calloc(token.len+1,1);
					sprintf(define_name,"%.*s",token.len,token.p);

					ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);

					// remove define from list of defines
					for(int i=0;i<preprocessor->defines.len;i+=2){
						Token* define_token=array_get(&preprocessor->defines,i);
						if(Token_equalString(define_token,define_name)){
							// TODO this needs a better solution, but the array api
							// currently does not support removing elements from
							// an arbitrary index
							define_token->len=0;
							break;
						}
					}

					continue;
				}else if(Token_equalString(&token, "pragma")){
					ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
					if(!ntr) fatal("no token after #pragma");
					if(Token_equalString(&token,"once")){
						// read include argument
						ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);

						// add file to list of already included files
						const char*include_path=preprocessor->token_iter.tokenizer->token_src;
						char* include_path_copy=allocAndCopy(strlen(include_path)+1,include_path);
						array_append(&preprocessor->already_included_files,&include_path_copy);

						continue;
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

		if(TokenIter_isEmpty(&preprocessor->token_iter))
			break;

		ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
		if(!ntr) fatal("");
	}

	// restore old token iter
	preprocessor->token_iter=old_token_iter;

	return;	
}
