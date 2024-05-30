#include "tokenizer.h"
#include "util/array.h"
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

	array_init(&preprocessor->stack,sizeof(struct PreprocessorIfStack));

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
		case PREPROCESSOR_EXPRESSION_TAG_GREATER_THAN:
			expr->value=Preprocessor_evalExpression(preprocessor,expr->greater_than.lhs) > Preprocessor_evalExpression(preprocessor,expr->greater_than.rhs);
			return expr->value;
		case PREPROCESSOR_EXPRESSION_TAG_LESSER_THAN:
			expr->value=Preprocessor_evalExpression(preprocessor,expr->lesser_than.lhs) < Preprocessor_evalExpression(preprocessor,expr->lesser_than.rhs);
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

	if(!preprocessor->doSkip){
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
	free(include_path);
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

	if(!preprocessor->doSkip){
		array_append(
			&preprocessor->defines,
			&(struct PreprocessorDefine){
				.name=define_name,
				.tokens=define_value
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
	sprintf(define_name,"%.*s",token.len,token.p);

	// iter past undef argument
	ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);

	// remove define from list of defines
	if(!preprocessor->doSkip){
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

		if(preprocessor->doSkip)
			return;

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
	if(preprocessor->doSkip)
		return;

	println("%s: ",Token_print(&token));
	while(!TokenIter_isEmpty(&preprocessor->token_iter)){
		ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
		if(!ntr) fatal("");
		println("%.*s",token.len,token.p);
	}
	fatal("");
}

/* returns number of tokens consumed */
int PreprocessorExpression_parse(
	struct Preprocessor*preprocessor,
	array if_expr_tokens,
	struct PreprocessorExpression *out,
	int precedence
){
	// init out
	*out=(struct PreprocessorExpression){};
	/* track number of currently open paranthesis */
	int openParanthesis=0;

	int nextTokenIndex=0;
	for(;nextTokenIndex<if_expr_tokens.len;){
		Token* nextToken=array_get(&if_expr_tokens,nextTokenIndex);

		switch(nextToken->tag){
			case TOKEN_TAG_LITERAL_INTEGER:
				if(out->tag!=0)fatal("unexpected token %s",Token_print(nextToken));
				nextTokenIndex++;

				out->tag=PREPROCESSOR_EXPRESSION_TAG_LITERAL;
				out->value=atoi(nextToken->p);
				break;
			case TOKEN_TAG_SYMBOL:
				{
					if(Token_equalString(nextToken,"defined")){
						if(out->tag!=0)fatal("unexpected token %s",Token_print(nextToken));
						nextTokenIndex++;

						// read next token
						if(nextTokenIndex>=if_expr_tokens.len) fatal("expected symbol after defined keyword");
						Token* nextToken=array_get(&if_expr_tokens,nextTokenIndex);

						if(nextToken->tag!=TOKEN_TAG_SYMBOL) fatal("expected symbol after defined keyword");
						out->tag=PREPROCESSOR_EXPRESSION_TAG_DEFINED;
						out->defined.name=calloc(nextToken->len+1,1);
						sprintf(out->defined.name,"%.*s",nextToken->len,nextToken->p);

						nextTokenIndex++;

						break;
					}else{
						// if this symbol is defined, replace it with the defined value
						// otherwise, replace it with 0 (per spec)
						if(out->tag!=0)fatal("unexpected token %s",Token_print(nextToken));
						nextTokenIndex++;

						// check if symbol is defined
						int defined=0;
						for(int i=0;i<preprocessor->defines.len;i+=2){
							Token* define_token=array_get(&preprocessor->defines,i);
							if(Token_equalToken(define_token,nextToken)){
								defined=1;
								// assume new token is a literal
								out->tag=PREPROCESSOR_EXPRESSION_TAG_LITERAL;
								// get value from define_token->p, which is a string of len define_token->len
								char* value=calloc(define_token->len+1,1);
								sprintf(value,"%.*s",define_token->len,define_token->p);
								out->value=atoi(value);
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
				}
				break;
			case TOKEN_TAG_KEYWORD:
				{
					if(Token_equalString(nextToken,"(")){
						if(out->tag!=0)fatal("unexpected token %s",Token_print(nextToken));
						nextTokenIndex++;

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
							0
						);
						*out=sub_expr;
						break;
					}else if(Token_equalString(nextToken,")")){
						if(out->tag==0)fatal("unexpected token %s",Token_print(nextToken));

						// if there are no open paranthesis, we may be inside a subexpression, hence return without consuming the token
						if(openParanthesis==0) goto PREPROCESSOR_EXPRESSION_PARSE_RET_SUCCESS;

						nextTokenIndex++;
						openParanthesis--;

						break;
					}else if(Token_equalString(nextToken,"||")){
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
							0
						);

						*out=(struct PreprocessorExpression){
							.tag=PREPROCESSOR_EXPRESSION_TAG_OR,
							.or={
								.lhs=allocAndCopy(sizeof(struct PreprocessorExpression),out),
								.rhs=allocAndCopy(sizeof(struct PreprocessorExpression),&sub_expr)
							}
						};
						break;
					}else if(Token_equalString(nextToken,"&&")){
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
							0
						);

						*out=(struct PreprocessorExpression){
							.tag=PREPROCESSOR_EXPRESSION_TAG_AND,
							.and={
								.lhs=allocAndCopy(sizeof(struct PreprocessorExpression),out),
								.rhs=allocAndCopy(sizeof(struct PreprocessorExpression),&sub_expr)
							}
						};
						break;
					}else if(Token_equalString(nextToken,">")){
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
							0
						);

						*out=(struct PreprocessorExpression){
							.tag=PREPROCESSOR_EXPRESSION_TAG_GREATER_THAN,
							.greater_than={
								.lhs=allocAndCopy(sizeof(struct PreprocessorExpression),out),
								.rhs=allocAndCopy(sizeof(struct PreprocessorExpression),&sub_expr)
							}
						};
						break;
					}else if(Token_equalString(nextToken,"<")){
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
							0
						);

						*out=(struct PreprocessorExpression){
							.tag=PREPROCESSOR_EXPRESSION_TAG_LESSER_THAN,
							.lesser_than={
								.lhs=allocAndCopy(sizeof(struct PreprocessorExpression),out),
								.rhs=allocAndCopy(sizeof(struct PreprocessorExpression),&sub_expr)
							}
						};
						break;
					}else{
						fatal("unexpected keyword %s",Token_print(nextToken));
					}
				}
				break;
			default:
				fatal("unimplemented token: %s",Token_print(nextToken));
		}
	}
	if(openParanthesis>0)fatal("%d unmatched paranthesis",openParanthesis);

PREPROCESSOR_EXPRESSION_PARSE_RET_SUCCESS:
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
struct PreprocessorExpression* Preprocessor_parseExpression(struct Preprocessor*preprocessor){
	Token token={};
	int ntr=TokenIter_lastToken(&preprocessor->token_iter,&token);

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
	PreprocessorExpression_parse(preprocessor,if_expr_tokens,&if_expr,0);
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
				new_if_stack.anyPathEvaluatedToTrue|=if_expr->value;

				array_append(&preprocessor->stack,&new_if_stack);

				continue;
			}else if(Token_equalString(&token, "ifndef")){
				Token ifToken=token;
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
				struct PreprocessorExpression if_expr={
					.tag=PREPROCESSOR_EXPRESSION_TAG_NOT,
					.not={.expr=allocAndCopy(sizeof(struct PreprocessorExpression),&(struct PreprocessorExpression){
						.tag=PREPROCESSOR_EXPRESSION_TAG_DEFINED,
						.defined={.name=define_name}
					})}
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
						.expr=&if_expr
					}
				};
				array_append(&new_if_stack.items,&item);

				// update stack evaluation state
				preprocessor->doSkip=new_if_stack.inherited_doSkip || new_if_stack.anyPathEvaluatedToTrue || !if_expr.value;
				new_if_stack.anyPathEvaluatedToTrue|=if_expr.value;

				// push stack on stack list
				array_append(&preprocessor->stack,&new_if_stack);

				continue;
			}else if(Token_equalString(&token,"ifdef")){
				Token ifToken=token;
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
				new_if_stack.anyPathEvaluatedToTrue|=if_expr.value;

				// push if statement on stack
				struct PreprocessorIfStackItem item={
					.tag=PREPROCESSOR_STACK_ITEM_TYPE_IF,
					.if_={
						.if_token=ifToken,
						.expr=&if_expr
					}
				};
				array_append(&new_if_stack.items,&item);

				continue;
			}else if(Token_equalString(&token,"elif")){
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
				if_stack->anyPathEvaluatedToTrue|=if_expr->value;

				continue;
			}else if(Token_equalString(&token,"else")){
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
				if(preprocessor->stack.len==0) fatal("else without if");
				struct PreprocessorIfStack* if_stack=array_get(&preprocessor->stack,preprocessor->stack.len-1);

				// append else to stack
				array_append(&if_stack->items,&item);

				// set doSkip to inherited doSkip
				preprocessor->doSkip=if_stack->inherited_doSkip || if_stack->anyPathEvaluatedToTrue;
				if_stack->anyPathEvaluatedToTrue=true; // superfluous, but for clarity

				continue;
			}else if(Token_equalString(&token,"endif")){
				ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
				if(!ntr) fatal("");

				// pop stack
				array_pop_back(&preprocessor->stack);

				// set doSkip to inherited doSkip
				if(preprocessor->stack.len==0){
					preprocessor->doSkip=false;
				}else{
					struct PreprocessorIfStack* if_stack=array_get(&preprocessor->stack,preprocessor->stack.len-1);
					preprocessor->doSkip=if_stack->inherited_doSkip || PreprocessorIfStack_getLastValue(if_stack);
				}
				
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

					Preprocessor_processUndefine(preprocessor);
					ntr=TokenIter_lastToken(&preprocessor->token_iter,&token);

					continue;
				}else if(Token_equalString(&token, "pragma")){
					ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
					if(!ntr) fatal("no token after #pragma");

					Preprocessor_processPragma(preprocessor);
					ntr=TokenIter_lastToken(&preprocessor->token_iter,&token);

					continue;
				}else if(Token_equalString(&token,"error")){
					ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
					if(!ntr) fatal("no token after #error");

					Preprocessor_processError(preprocessor);
					ntr=TokenIter_lastToken(&preprocessor->token_iter,&token);

					continue;
				}
			}

			fatal("unknown preprocessor directive %s",Token_print(&token));
		}

		if(!preprocessor->doSkip)
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
