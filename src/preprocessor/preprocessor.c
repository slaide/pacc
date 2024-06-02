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
	array_init(&preprocessor->temp_defines,sizeof(struct PreprocessorDefine));
	array_init(&preprocessor->already_included_files,sizeof(char*));	

	array_init(&preprocessor->stack,sizeof(struct PreprocessorIfStack));

	// read all tokens into memory
	array_init(&preprocessor->tokens_out,sizeof(Token));
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
				char*tok_filename=calloc(tok_filename_len+1,1);
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
	array*args=nullptr;
	bool done_parsing_args=true;
	while(TokenIter_nextToken(&preprocessor->token_iter,&token) && token.line==define_line_num){
		if(!done_parsing_args){
			if(Token_equalString(&token,")") && !done_parsing_args){
				done_parsing_args=true;
				continue;
			}else{
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
				array_append(args,&new_arg);
			}
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
		if(0){
			print("defined %.*s",define_name.len,define_name.p);
			if(define_value.len>0 || args!=nullptr){
				if(args!=nullptr){
					printf(" (");
					for(int i=0;i<args->len;i++){
						struct PreprocessorDefineFunctionlikeArg* arg=array_get(args,i);
						printf("%.*s",arg->name.name.len,arg->name.name.p);
						if(i<args->len-1)
							printf(",");
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
	sprintf(define_name,"%.*s",token.len,token.p);

	// iter past undef argument
	ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);

	// remove define from list of defines
	if(!preprocessor->doSkip){
		for(int i=0;i<preprocessor->defines.len;i++){
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

	// this is genuinely an error during compilation
	fatal("error: %s: ",Token_print(&token));
}
void Preprocessor_processWarning(struct Preprocessor*preprocessor){
	Token token={};
	int ntr=TokenIter_lastToken(&preprocessor->token_iter,&token);
	discard ntr;
	if(preprocessor->doSkip)
		return;

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

/*
expand tokens in tokens_in and append expanded tokens to tokens_out

tokens_out must already be initialized

preprocessor directives in tokens_in are not allowed
*/
void Preprocessor_expandMacros(struct Preprocessor*preprocessor,int num_tokens_in_arg,Token*tokens_in_arg,array*tokens_out_arg){
	bool expanded_any=true;

	// copy input arguments
	int num_tokens_in=num_tokens_in_arg;
	Token*tokens_in=malloc(sizeof(Token[num_tokens_in]));
	memcpy(tokens_in,tokens_in_arg,sizeof(Token[num_tokens_in]));

	if(0){
		for(int i=0;i<num_tokens_in_arg;i++){
			printf("%.*s ",(tokens_in+i)->len,(tokens_in+i)->p);
		}
		printf("\n");
	}

	// set up temporary array for output tokens, and only flush it to tokens_out if expanded_any is false
	array tokens_out_={};
	array*tokens_out=&tokens_out_;
	array_init(tokens_out,sizeof(Token));

	while(1){
		expanded_any=false;

		for(int i=0;i<num_tokens_in;i++){
			Token* token_in=tokens_in+i;
			Token first_token=*token_in;

			switch(token_in->tag){
				case TOKEN_TAG_SYMBOL:
					break;
				default:
					array_append(tokens_out,token_in);
					continue;
			}

			// check if token is a macro
			bool was_expanded=false;
			for(int j=0;j<(preprocessor->defines.len+preprocessor->temp_defines.len);j++){
				struct PreprocessorDefine* define=nullptr;
				if(j<preprocessor->defines.len)
					define=array_get(&preprocessor->defines,j);
				else
					define=array_get(&preprocessor->temp_defines,j-preprocessor->defines.len);
				
				if(Token_equalToken(&define->name,token_in)){
					/* num temp defines to pop off the stack afterwards */
					int num_temp_defines=0;

					// check for function-like macro
					if(define->args!=nullptr){
						i++; // skip over macro name

						// check for paranthesis
						if(i>=num_tokens_in) fatal("expected argument list after function-like macro %s",Token_print(token_in));
						token_in=tokens_in+i;
						if(!Token_equalString(token_in, "("))
							fatal("expected ( after function-like macro %.*s, but got instead %s",define->name.len,define->name.p,Token_print(token_in));
						i++; // skip over opening paranthesis

						// handle macro arguments
						for(int arg_index=0;arg_index<define->args->len;arg_index++){
							// handle arg as temporary define
							// 1) get arg name for name of define
							struct PreprocessorDefineFunctionlikeArg arg=*(struct PreprocessorDefineFunctionlikeArg*)array_get(define->args,arg_index);
							Token arg_name=arg.name.name;
							// 2) gather args, i.e. all tokens until next comma or closing paranthesis ( TODO handle nesting, e.g. (a,(b,c),d) as single argument )
							array arg_tokens={};
							array_init(&arg_tokens,sizeof(Token));
							/* list of chars to close nested statements, though only () qualify, others, e.g. curly braces, do not have to be closed */
							array nested_char_stack={};
							array_init(&nested_char_stack,sizeof(char));
							while(1){
								if(i>=num_tokens_in) fatal("expected argument list after function-like macro %s",Token_print(token_in));
								Token* define_token=tokens_in+i;

								// comma is allowed when nested
								if(
									Token_equalString(define_token,",")
									&& nested_char_stack.len==0
								){
									break;
								}
								if(Token_equalString(define_token,"(")){
									const char close_char=')';
									array_append(&nested_char_stack,&close_char);
								}
								if(Token_equalString(define_token,")")){
									if(nested_char_stack.len==0)
										break;

									char close_char=*(char*)array_get(&nested_char_stack,nested_char_stack.len-1);
									if(close_char!=')')
										fatal("unexpected closing paranthesis %s",Token_print(define_token));
									array_pop_back(&nested_char_stack);
								}
								i+=1;
								array_append(&arg_tokens,define_token);
							}
							// 3) create temporary define
							struct PreprocessorDefine arg_define={
								.name=arg_name,
								.tokens=arg_tokens,
								.args=nullptr
							};
							// 4) add define to preprocessor
							array_append(&preprocessor->temp_defines,&arg_define);
							num_temp_defines++;

							// 5) if next token is closing paranthesis, break
							if(Token_equalString(tokens_in+i,")")){
								break;
							}
							// 6) if next token is comma, continue
							if(Token_equalString(tokens_in+i,",")){
								i+=1;
								continue;
							}
							// 7) if next token is neither comma nor closing paranthesis, error
							fatal("expected , or ) after argument in function-like macro %s after %s",Token_print(tokens_in+i),Token_print(array_get(&arg_tokens,arg_tokens.len-1)));
						}

						// check for closing paranthesis
						if(i>=num_tokens_in) fatal("expected argument list after function-like macro %s",Token_print(token_in));
						token_in=tokens_in+i;
						if(!Token_equalString(token_in, ")"))
							fatal("expected ) after function-like macro %s",Token_print(token_in))
						// do not skip over closing paranthesis (with i++) here because the loop step will do that
					}

					// splice in define tokens
					array new_tokens_={};
					array_init(&new_tokens_,sizeof(Token));
					array*new_tokens=&new_tokens_;
					for(int d=0;d<define->tokens.len;d++){
						// check list of temporary defines for matches
						Token define_token=*(Token*)array_get(&define->tokens,d);
						// check for preprocessor operators
						// 1) concatenation operator
						if(new_tokens->len>=3
							&& /* n-1 token is hash */ Token_equalString(array_get(new_tokens,new_tokens->len-1),"#")
							&& /* n-2 token is hash */ Token_equalString(array_get(new_tokens,new_tokens->len-2),"#")
						){
							// get n-3 token
							Token last_token=*(Token*)array_get(new_tokens,new_tokens->len-3);

							// pop last two tokens (hashes)
							array_pop_back(new_tokens);
							array_pop_back(new_tokens);

							// pop n-3 token
							array_pop_back(new_tokens);

							Token left=last_token;
							Token right=define_token;

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

							// print debug info
							if(0){
								println("concatenating %.*s and %.*s",left.len,left.p,right.len,right.p);
								// go through all entries in defines and temp_defines, and print them
								for(int i=0;i<preprocessor->defines.len+preprocessor->temp_defines.len;i++){
									struct PreprocessorDefine* define=nullptr;
									if(i<preprocessor->defines.len)
										define=array_get(&preprocessor->defines,i);
									else
										define=array_get(&preprocessor->temp_defines,i-preprocessor->defines.len);
									printf("define %.*s: ",define->name.len,define->name.p);
									for(int j=0;j<define->tokens.len;j++){
										Token* define_token=array_get(&define->tokens,j);
										printf("%.*s ",define_token->len,define_token->p);
									}
									printf("\n");
								}
							}

							// append current token to last_token
							Token concatenated_token={
								.tag=TOKEN_TAG_SYMBOL,
								.line=left.line,
								.col=left.col,
								.len=left.len+right.len,
								.p=calloc(left.len+right.len,1)
							};
							sprintf(concatenated_token.p,"%.*s%.*s",left.len,left.p,right.len,right.p);
							array_append(new_tokens,&concatenated_token);
							println("concatenated to %.*s",concatenated_token.len,concatenated_token.p);

							continue;
						}
						// 2) stringification operator
						else if(new_tokens->len>=1
							&& /* n-1 token is hash */ Token_equalString(array_get(new_tokens,new_tokens->len-1),"#")
							&& ! /* n-2 token is hash */ Token_equalString(&define_token,"#")
						){
							// pop last token (hash)
							array_pop_back(new_tokens);

							// expand define_token
							array define_token_tokens={};
							array_init(&define_token_tokens,sizeof(Token));
							Preprocessor_expandMacros(preprocessor,1,&define_token,&define_token_tokens);
							if(define_token_tokens.len==0)
								fatal("expected at least one token after macro expansion");

							// wrap string literal in quotes
							int total_str_len=3;
							for(int i=0;i<define_token_tokens.len;i++){
								Token* define_token_token=array_get(&define_token_tokens,i);

								// insert space before symbols
								if(i>0 && define_token_token->tag==TOKEN_TAG_SYMBOL){
									total_str_len+=1;
								}

								total_str_len+=define_token_token->len;
							}
							char* arg_str=calloc(1,total_str_len+3);
							for(int i=0;i<define_token_tokens.len;i++){
								Token* define_token_token=array_get(&define_token_tokens,i);
								const char*padding_str="";
								if(i>0 && define_token_token->tag==TOKEN_TAG_SYMBOL){
									padding_str=" ";
								}
								snprintf(arg_str+strlen(arg_str),total_str_len-strlen(arg_str),"%s%.*s",padding_str,define_token_token->len,define_token_token->p);
								println("arg_str: %s",arg_str);
							}

							char* arg_str_out=calloc(1,total_str_len+3);
							snprintf(arg_str_out,total_str_len+3,"\"%s\"",arg_str);
							// replace last token in token_out with string literal
							Token string_literal_token={
								.tag=TOKEN_TAG_LITERAL_STRING,
								.line=define_token.line,
								.col=define_token.col,
								.len=total_str_len+2,
								.p=arg_str_out
							};
							array_append(new_tokens,&string_literal_token);

							continue;
						}

						define_token.line=first_token.line;
						// offset col slightly to make it more readable
						define_token.col=first_token.col+d*2;
						array_append(new_tokens,&define_token);
					}

					// expand new tokens
					Preprocessor_expandMacros(preprocessor,new_tokens->len,new_tokens->data,tokens_out);

					/* pop new temp defines off the stack again */
					for(;num_temp_defines>0;num_temp_defines--){
						array_pop_back(&preprocessor->temp_defines);
					}

					was_expanded=true;
					break;
				}
			}

			expanded_any=expanded_any||was_expanded;

			// if it's not a macro, append it to the output directly
			if(!was_expanded){
				array_append(tokens_out,token_in);
			}
		}

		if(expanded_any){
			// print debug info
			if(0){
				println("expanded from:");
				for(int i=0;i<num_tokens_in;i++){
					Token* token_in=tokens_in+i;
					printf("%.*s ",token_in->len,token_in->p);
				}
				printf("\n");
				println("to:");
				for(int i=0;i<tokens_out->len;i++){
					Token* token_out=array_get(tokens_out,i);
					printf("%.*s ",token_out->len,token_out->p);
				}
				printf("\n");
			}

			free(tokens_in);
			// copy tokens_out to tokens_in
			tokens_in=tokens_out->data;
			num_tokens_in=tokens_out->len;
			array_init(&tokens_out_,sizeof(Token));
			continue;
		}
		break;
	}

	// copy tokens_out to tokens_out_arg
	for(int i=0;i<tokens_out->len;i++){
		Token* token=array_get(tokens_out,i);
		array_append(tokens_out_arg,token);
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


#define PREPROCESSOR_EXPRESSION_PARSE_CHECK_PRECEDENCE(OPERATOR_PRECEDENCE) \
	if(precedence<=OPERATOR_PRECEDENCE) \
		goto PREPROCESSOR_EXPRESSION_PARSE_RET_CHECK;

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
	if(0){
		println("tokens:");
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
			case TOKEN_TAG_LITERAL_INTEGER:
				if(out->tag!=0)fatal("unexpected token %s",Token_print(nextToken));
				nextTokenIndex++;

				out->tag=PREPROCESSOR_EXPRESSION_TAG_LITERAL;
				out->value=atoi(nextToken->p);
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
									if(discardOpenParanthesis==0) break;
								}
							}
							// out is already literal 0
						}
						break;
					}else if(Token_equalString(nextToken,")")){
						PREPROCESSOR_EXPRESSION_PARSE_CHECK_PRECEDENCE(PREPROCESSOR_OPERATOR_PRECEDENCE_CALL)

						// if there are no open paranthesis, we may be inside a subexpression, hence return without consuming the token
						if(openParanthesis==0) goto PREPROCESSOR_EXPRESSION_PARSE_RET_SUCCESS;

						nextTokenIndex++;
						openParanthesis--;

						break;
					}else if(Token_equalString(nextToken,"||")){
						PREPROCESSOR_EXPRESSION_PARSE_CHECK_PRECEDENCE(PREPROCESSOR_OPERATOR_PRECEDENCE_OR)

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
							PREPROCESSOR_OPERATOR_PRECEDENCE_OR
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
						PREPROCESSOR_EXPRESSION_PARSE_CHECK_PRECEDENCE(PREPROCESSOR_OPERATOR_PRECEDENCE_AND)

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
							PREPROCESSOR_OPERATOR_PRECEDENCE_AND
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
						PREPROCESSOR_EXPRESSION_PARSE_CHECK_PRECEDENCE(PREPROCESSOR_OPERATOR_PRECEDENCE_GREATER_THAN)

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
							PREPROCESSOR_OPERATOR_PRECEDENCE_GREATER_THAN
						);

						*out=(struct PreprocessorExpression){
							.tag=PREPROCESSOR_EXPRESSION_TAG_GREATER_THAN,
							.greater_than={
								.lhs=allocAndCopy(sizeof(struct PreprocessorExpression),out),
								.rhs=allocAndCopy(sizeof(struct PreprocessorExpression),&sub_expr)
							}
						};
						break;
					}else if(Token_equalString(nextToken,">=")){
						PREPROCESSOR_EXPRESSION_PARSE_CHECK_PRECEDENCE(PREPROCESSOR_OPERATOR_PRECEDENCE_GREATER_THAN_OR_EQUAL)

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
							PREPROCESSOR_OPERATOR_PRECEDENCE_GREATER_THAN_OR_EQUAL
						);

						*out=(struct PreprocessorExpression){
							.tag=PREPROCESSOR_EXPRESSION_TAG_GREATER_THAN_OR_EQUAL,
							.greater_than_or_equal={
								.lhs=allocAndCopy(sizeof(struct PreprocessorExpression),out),
								.rhs=allocAndCopy(sizeof(struct PreprocessorExpression),&sub_expr)
							}
						};
						break;
					}else if(Token_equalString(nextToken,"<")){
						PREPROCESSOR_EXPRESSION_PARSE_CHECK_PRECEDENCE(PREPROCESSOR_OPERATOR_PRECEDENCE_LESSER_THAN)

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
							PREPROCESSOR_OPERATOR_PRECEDENCE_LESSER_THAN
						);

						*out=(struct PreprocessorExpression){
							.tag=PREPROCESSOR_EXPRESSION_TAG_LESSER_THAN,
							.lesser_than={
								.lhs=allocAndCopy(sizeof(struct PreprocessorExpression),out),
								.rhs=allocAndCopy(sizeof(struct PreprocessorExpression),&sub_expr)
							}
						};
						break;
					}else if(Token_equalString(nextToken,"<=")){
						PREPROCESSOR_EXPRESSION_PARSE_CHECK_PRECEDENCE(PREPROCESSOR_OPERATOR_PRECEDENCE_LESSER_THAN_OR_EQUAL)

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
							PREPROCESSOR_OPERATOR_PRECEDENCE_LESSER_THAN_OR_EQUAL
						);

						*out=(struct PreprocessorExpression){
							.tag=PREPROCESSOR_EXPRESSION_TAG_LESSER_THAN_OR_EQUAL,
							.lesser_than_or_equal={
								.lhs=allocAndCopy(sizeof(struct PreprocessorExpression),out),
								.rhs=allocAndCopy(sizeof(struct PreprocessorExpression),&sub_expr)
							}
						};
						break;
					}else if(Token_equalString(nextToken,"==")){
						PREPROCESSOR_EXPRESSION_PARSE_CHECK_PRECEDENCE(PREPROCESSOR_OPERATOR_PRECEDENCE_EQUAL)

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
							PREPROCESSOR_OPERATOR_PRECEDENCE_EQUAL
						);

						*out=(struct PreprocessorExpression){
							.tag=PREPROCESSOR_EXPRESSION_TAG_EQUAL,
							.equal={
								.lhs=allocAndCopy(sizeof(struct PreprocessorExpression),out),
								.rhs=allocAndCopy(sizeof(struct PreprocessorExpression),&sub_expr)
							}
						};
						break;
					}else if(Token_equalString(nextToken,"!=")){
						PREPROCESSOR_EXPRESSION_PARSE_CHECK_PRECEDENCE(PREPROCESSOR_OPERATOR_PRECEDENCE_UNEQUAL)

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
							PREPROCESSOR_OPERATOR_PRECEDENCE_UNEQUAL
						);

						*out=(struct PreprocessorExpression){
							.tag=PREPROCESSOR_EXPRESSION_TAG_UNEQUAL,
							.unequal={
								.lhs=allocAndCopy(sizeof(struct PreprocessorExpression),out),
								.rhs=allocAndCopy(sizeof(struct PreprocessorExpression),&sub_expr)
							}
						};
						break;
					}else if(Token_equalString(nextToken,"+")){
						PREPROCESSOR_EXPRESSION_PARSE_CHECK_PRECEDENCE(PREPROCESSOR_OPERATOR_PRECEDENCE_ADD)

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
							PREPROCESSOR_OPERATOR_PRECEDENCE_ADD
						);

						*out=(struct PreprocessorExpression){
							.tag=PREPROCESSOR_EXPRESSION_TAG_AND,
							.add={
								.lhs=allocAndCopy(sizeof(struct PreprocessorExpression),out),
								.rhs=allocAndCopy(sizeof(struct PreprocessorExpression),&sub_expr)
							}
						};
						break;
					}else if(Token_equalString(nextToken,"-")){
						PREPROCESSOR_EXPRESSION_PARSE_CHECK_PRECEDENCE(PREPROCESSOR_OPERATOR_PRECEDENCE_SUBTRACT)

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
							PREPROCESSOR_OPERATOR_PRECEDENCE_SUBTRACT
						);

						*out=(struct PreprocessorExpression){
							.tag=PREPROCESSOR_EXPRESSION_TAG_SUBTRACT,
							.subtract={
								.lhs=allocAndCopy(sizeof(struct PreprocessorExpression),out),
								.rhs=allocAndCopy(sizeof(struct PreprocessorExpression),&sub_expr)
							}
						};
						break;
					}else if(Token_equalString(nextToken,"!")){
						PREPROCESSOR_EXPRESSION_PARSE_CHECK_PRECEDENCE(PREPROCESSOR_OPERATOR_PRECEDENCE_NOT)

						if(out->tag!=0)fatal("unexpected token %s",Token_print(nextToken));
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
							PREPROCESSOR_OPERATOR_PRECEDENCE_NOT
						);

						*out=(struct PreprocessorExpression){
							.tag=PREPROCESSOR_EXPRESSION_TAG_NOT,
							.not={
								.expr=allocAndCopy(sizeof(struct PreprocessorExpression),&sub_expr)
							}
						};
						break;
					}else if(Token_equalString(nextToken,"?")){
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
					}else if(Token_equalString(nextToken,":")){
						if(precedence<=PREPROCESSOR_OPERATOR_PRECEDENCE_TERNARY)
							goto PREPROCESSOR_EXPRESSION_PARSE_RET_CHECK;

						fatal("unexpected token %s outside ternary expression",Token_print(nextToken));
					}else{
						fatal("unexpected keyword %s",Token_print(nextToken));
					}
				}
				break;
			default:
				fatal("unimplemented token: %s",Token_print(nextToken));
		}
	}
	if(openParanthesis>0)fatal("%d unmatched paranthesis at %s",openParanthesis,Token_print(array_get(&if_expr_tokens,0)));

PREPROCESSOR_EXPRESSION_PARSE_RET_CHECK:
	if(out->tag==0)fatal("expected expression, got instead %s",Token_print(nextToken));

	// else, fallthrough

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
					.tag=TOKEN_TAG_LITERAL_INTEGER,
					.line=token.line,
					.col=token.col,
					.len=1,
					.p=defined?"1":"0"
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
	if(0){
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

				// push stack on stack list
				array_append(&preprocessor->stack,&new_if_stack);

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
				if(preprocessor->stack.len==0) fatal("else without if at %s",Token_print(&elseToken));
				struct PreprocessorIfStack* if_stack=array_get(&preprocessor->stack,preprocessor->stack.len-1);

				// append else to stack
				array_append(&if_stack->items,&item);

				// set doSkip to inherited doSkip
				preprocessor->doSkip=if_stack->inherited_doSkip || if_stack->anyPathEvaluatedToTrue;
				if_stack->anyPathEvaluatedToTrue=true; // superfluous, but for clarity

				continue;
			}else if(Token_equalString(&token,"endif")){
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
			else{
				if(Token_equalString(&token,"include")){
					ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
					if(!ntr) fatal("no token after #include %s",Token_print(&token));

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
				}else if(Token_equalString(&token,"warning")){
					ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
					if(!ntr) fatal("no token after #warning");

					Preprocessor_processWarning(preprocessor);
					ntr=TokenIter_lastToken(&preprocessor->token_iter,&token);

					continue;
				}
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

			if(TokenIter_isEmpty(&preprocessor->token_iter))
				break;

			ntr=TokenIter_nextToken(&preprocessor->token_iter,&token);
			if(!ntr) fatal("");

			if(Token_equalString(&token,"#"))
				break;
		}

		if(!preprocessor->doSkip){
			// expand the single token
			Preprocessor_expandMacros(preprocessor,new_tokens.len,new_tokens.data,&preprocessor->tokens_out);
		}

		if(TokenIter_isEmpty(&preprocessor->token_iter))
			break;
	}

	// restore old token iter
	preprocessor->token_iter=old_token_iter;

	return;	
}
