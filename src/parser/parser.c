#include<parser/parser.h>
#include<util/util.h>
#include<tokenizer.h>
#include<ctype.h> // isalpha, isalnum


bool Token_isValidIdentifier(Token*token){
	if(token->tag!=TOKEN_TAG_SYMBOL)
		return false;

	if(token->len==0)
		return false;

	if(!isalpha(token->p[0])&&token->p[0]!='_')
		return false;

	for(int i=1;i<token->len;i++){
		if(!isalnum(token->p[i])&&token->p[i]!='_')
			return false;
	}

	return true;
}

void Module_parse(Module* module,Tokenizer*tokenizer){
	array_init(&module->statements,sizeof(Statement));

	struct TokenIter token_iter;
	TokenIter_init(&token_iter,tokenizer,(struct TokenIterConfig){.skip_comments=true,});

	Token token;
	TokenIter_nextToken(&token_iter,&token);
	while(!TokenIter_isEmpty(&token_iter)){
		if(Token_equalString(&token,KEYWORD_HASH)){
			// keep track of current line number because preprocessor directives are terminated by newline (unless escaped with '\')
			int token_iter_linenum=token.line;

			// get directive name
			TokenIter_nextToken(&token_iter,&token);
			Token directiveName=token;

			// get args
			array prep_body_tokens;
			array_init(&prep_body_tokens,sizeof(Token));

			// get directive body
			while(TokenIter_nextToken(&token_iter,&token)){
				if(Token_equalString(&token,"\\")){
					token_iter_linenum=token.line+1;
					continue;
				}
				if(token.line>token_iter_linenum){
					break;
				}

				array_append(&prep_body_tokens,&token);
			}

			if(Token_equalString(&directiveName,KEYWORD_DEFINE)){
				if(prep_body_tokens.len<1){
					fatal("define directive must have syntax\n#define NAME [ ( ARGS ) ] ) [ BODY ]");
				}

				Token name=*(Token*)array_get(&prep_body_tokens,0);

				// adjust for name
				array_pop_front(&prep_body_tokens);

				// check for function style define
				array prep_args_tokens;
				array_init(&prep_args_tokens,sizeof(Token));
				if(Token_equalString(prep_body_tokens.data,"(")){
					// adjust for opening parenthesis
					array_pop_front(&prep_body_tokens);

					while(!Token_equalString(prep_body_tokens.data,")")){
						array_append(&prep_args_tokens,array_get(&prep_body_tokens,0));

						// decrease number of body tokens
						array_pop_front(&prep_body_tokens);
					}
					// adjust for closing parenthesis
					array_pop_front(&prep_body_tokens);
				}
				// debug print define name, args and body
				println("define name %.*s",name.len,name.p);
				for(int i=0;i<prep_args_tokens.len;i++){
					Token* token=array_get(&prep_args_tokens,i);
					println("define arg %.*s",token->len,token->p);
				}
				for(int i=0;i<prep_body_tokens.len;i++){
					Token*t=array_get(&prep_body_tokens,i);
					println("define body %.*s",t->len,t->p);
				}


				Statement prep_statement={
					.tag=STATEMENT_PREP_DEFINE,
					.prep_define={
						.name=name,
						.args=prep_args_tokens,
						.body=prep_body_tokens,
					}
				};
				array_append(&module->statements,&prep_statement);
			}else if(Token_equalString(&directiveName,KEYWORD_INCLUDE)){
				if(prep_body_tokens.len!=1){
					// print next token
					println("next token is: line %d col %d %.*s",token.line,token.col,token.len,token.p);
					fatal("include directive must have syntax\n#include <path/to/file>\nor\n#include \"path/to/file>\"");
				}

				Token* incl_token=array_get(&prep_body_tokens,0);
				println("including file %.*s",incl_token->len,incl_token->p);
				Statement prep_statement={
					.tag=STATEMENT_PREP_INCLUDE,
					.prep_include={
						.path=token,
					}
				};
				array_append(&module->statements,&prep_statement);
			}else{
				fatal("unknown preprocessor directive %.*s",directiveName.len,directiveName.p);
			}
		}else{
			Statement statement={};
			enum STATEMENT_PARSE_RESULT res=Statement_parse(&statement,&token_iter);
			switch(res){
				case STATEMENT_INVALID:
					fatal("invalid statement at line %d col %d",token.line,token.col);
					break;
				case STATEMENT_PRESENT:
					println("found statement at line %d col %d",token.line,token.col);
					array_append(&module->statements,&statement);
					continue;
			}

			fatal("leftover tokens at end of file. next token is: line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
	}
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
