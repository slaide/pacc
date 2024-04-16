#include "parser/statement.h"
#include<parser/parser.h>
#include<util/util.h>
#include<tokenizer.h>

char*Statement_asString(Statement*statement){
	switch(statement->tag){
		case STATEMENT_KIND_RETURN:{
			char*ret=calloc(1024,1);
			sprintf(ret,"return %s",Value_asString(statement->return_.retval));
			return ret;
		}
		case STATEMENT_FUNCTION_DEFINITION:{
			char*ret=calloc(1024,1);
			sprintf(ret,"defined function %.*s",statement->functionDef.symbol.name->len,statement->functionDef.symbol.name->p);
			sprintf(ret+strlen(ret)," of type %s",Type_asString(statement->functionDef.symbol.type));
			for(int i=0;i<statement->functionDef.bodyStatements.len;i++){
				Statement*bodyStatement=array_get(&statement->functionDef.bodyStatements,i);
				sprintf(ret,"%s\n  body %d: %s",ret,i,Statement_asString(bodyStatement));
			}
			return ret;
		}
		case STATEMENT_KIND_SYMBOL_DEFINITION:{
			char*ret=calloc(1024,1);
			sprintf(ret,"%.*s:%s",statement->symbolDef.symbol.name->len,statement->symbolDef.symbol.name->p,Type_asString(statement->symbolDef.symbol.type));
			if(statement->symbolDef.init_value){
				sprintf(ret+strlen(ret)," = %s",Value_asString(statement->symbolDef.init_value));
			}
			return ret;
		}
		case STATEMENT_KIND_FOR:{
			char *ret=calloc(1024,1);
			sprintf(ret+strlen(ret),"for loop:\n");
			sprintf(ret+strlen(ret),"  init: %s\n",Statement_asString(statement->forLoop.init));
			sprintf(ret+strlen(ret),"  condition: %s\n",Value_asString(statement->forLoop.condition));
			sprintf(ret+strlen(ret),"  step: %s\n",Value_asString(statement->forLoop.step));

			for(int i=0;i<statement->forLoop.body.len;i++){
				Statement*bodyStatement=array_get(&statement->forLoop.body,i);
				sprintf(ret+strlen(ret),"  body %d: %s\n",i,Statement_asString(bodyStatement));
			}

			return ret;
		}
		case STATEMENT_VALUE:{
			return Value_asString(statement->value.value);
		}
		case STATEMENT_PREP_INCLUDE:{
			char*ret=calloc(1024,1);
			sprintf(ret,"#include %.*s",statement->prep_include.path.len,statement->prep_include.path.p);
			return ret;
		}
		case STATEMENT_PREP_DEFINE:{
			char*ret=calloc(1024,1);
			sprintf(ret,"#define %.*s",statement->prep_define.name.len,statement->prep_define.name.p);

			if(statement->prep_define.args.len>0){
				sprintf(ret+strlen(ret)," args (");
				for(int i=0;i<statement->prep_define.args.len;i++){
					Token*arg=array_get(&statement->prep_define.args,i);
					sprintf(ret+strlen(ret)," %.*s ",arg->len,arg->p);
				}
				sprintf(ret+strlen(ret),")");
			}

			if(statement->prep_define.body.len>0){
				sprintf(ret+strlen(ret)," body {");
				for(int i=0;i<statement->prep_define.body.len;i++){
					Token*body_token=array_get(&statement->prep_define.body,i);
					sprintf(ret+strlen(ret)," %.*s ",body_token->len,body_token->p);
				}
				sprintf(ret+strlen(ret),"}");
			}
			return ret;
		}
		default:
			fatal("unimplemented %s",Statementkind_asString(statement->tag));
	}
}

enum STATEMENT_PARSE_RESULT Statement_parse(Statement*out,struct TokenIter*token_iter){
	Token token;
	TokenIter_lastToken(token_iter,&token);

	// long list of stuff that can be a statement

	if(Token_equalString(&token,"if")){
		fatal("if not yet implemented");
	}
	if(Token_equalString(&token,"while")){
		fatal("while not yet implemented");
	}
	if(Token_equalString(&token,"for")){
		TokenIter_nextToken(token_iter,&token);
		if(!Token_equalString(&token,"(")){
			fatal("expected opening parenthesis after for");
		}
		TokenIter_nextToken(token_iter,&token);

		*out=(Statement){.tag=STATEMENT_KIND_FOR,.forLoop={}};

		Statement init_statement={};
		enum STATEMENT_PARSE_RESULT res=Statement_parse(&init_statement,token_iter);
		if(res==STATEMENT_INVALID) fatal("invalid init statement in for loop");
		TokenIter_lastToken(token_iter,&token);
		out->forLoop.init=allocAndCopy(sizeof(Statement),&init_statement);

		Value condition={};
		enum VALUE_PARSE_RESULT valres=Value_parse(&condition,token_iter);
		if(valres==VALUE_INVALID) fatal("invalid condition in for loop");
		TokenIter_lastToken(token_iter,&token);

		out->forLoop.condition=allocAndCopy(sizeof(Value),&condition);

		// check for semicolon
		if(!Token_equalString(&token,";")){
			fatal("expected semicolon after for condition statement: line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_nextToken(token_iter,&token);

		Value post_expression={};
		valres=Value_parse(&post_expression,token_iter);
		if(valres==VALUE_INVALID) fatal("invalid post expression in for loop");
		TokenIter_lastToken(token_iter,&token);

		out->forLoop.step=allocAndCopy(sizeof(Value),&post_expression);

		// check for closing paranthesis
		if(!Token_equalString(&token,")")){
			fatal("expected closing parenthesis after for post expression statement: line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_nextToken(token_iter,&token);

		array body_tokens;
		array_init(&body_tokens,sizeof(Statement));

		// if next token is {, parse statement block
		if(Token_equalString(&token,"{")){
			TokenIter_nextToken(token_iter,&token);

			bool stopParsingForBody=false;
			while(!stopParsingForBody){
				if(Token_equalString(&token,"}")){
					TokenIter_nextToken(token_iter,&token);
					break;
				}
				Statement forBodyStatement={};
				enum STATEMENT_PARSE_RESULT res=Statement_parse(&forBodyStatement,token_iter);
				TokenIter_lastToken(token_iter,&token);
				switch(res){
					case STATEMENT_INVALID:
						fatal("invalid statement in for body");
						stopParsingForBody=true;
						break;
					case STATEMENT_PRESENT:
						array_append(&body_tokens,&forBodyStatement);
						break;
				}
			}
		// otherwise parse single statement
		}else{
			Statement forBodyStatement={};
			enum STATEMENT_PARSE_RESULT res=Statement_parse(&forBodyStatement,token_iter);
			TokenIter_lastToken(token_iter,&token);
			switch(res){
				case STATEMENT_INVALID:
					fatal("invalid statement in for body");
					break;
				case STATEMENT_PRESENT:
					array_append(&body_tokens,&forBodyStatement);
					break;
			}
		}

		out->forLoop.body=body_tokens;

		return STATEMENT_PRESENT;
	}

	if(Token_equalString(&token,"return")){
		TokenIter_nextToken(token_iter,&token);

		Value returnValue={};
		*out=(Statement){.tag=STATEMENT_KIND_RETURN,.return_={}};

		enum VALUE_PARSE_RESULT res=Value_parse(&returnValue,token_iter);
		TokenIter_lastToken(token_iter,&token);
		switch(res){
			case VALUE_INVALID:
				break;
			case VALUE_PRESENT:
				out->return_.retval=allocAndCopy(sizeof(Value),&returnValue);
				break;
		}
		if(Token_equalString(&token,";")){
			TokenIter_nextToken(token_iter,&token);

			return STATEMENT_PRESENT;
		}
		fatal("missing semicolon after return statement at line %d col %d %.*s",token.line,token.col,token.len,token.p);
	}
	if(Token_equalString(&token,"break")){
		fatal("break not yet implemented");
	}
	if(Token_equalString(&token,"continue")){
		fatal("continue not yet implemented");
	}
	if(Token_equalString(&token,"switch")){
		fatal("switch not yet implemented");
	}

	// check for value [expression]
	if(0){
		Value value={};
		enum VALUE_PARSE_RESULT res=Value_parse(&value,token_iter);
		TokenIter_lastToken(token_iter,&token);
		switch(res){
			case VALUE_INVALID:
				fatal("invalid value in statement");
				break;
			case VALUE_PRESENT:
				println("got value in statement");
				break;
		}
	}

	// parse symbol definition
	do{
		Symbol symbol={};
		enum SYMBOL_PARSE_RESULT symbolParseResult=Symbol_parse(&symbol,token_iter);
		if(symbolParseResult==STATEMENT_INVALID){
			break;
		}
		TokenIter_lastToken(token_iter,&token);

		Statement statement={};

		switch(symbol.type->kind){
			case TYPE_KIND_FUNCTION:
				{
					if(Token_equalString(&token,";")){
						TokenIter_nextToken(token_iter,&token);

						statement.tag=STATEMENT_FUNCTION_DECLARATION;
						statement.functionDecl.symbol=symbol;
					}else if(Token_equalString(&token,"{")){
						TokenIter_nextToken(token_iter,&token);

						statement.tag=STATEMENT_FUNCTION_DEFINITION;
						statement.functionDef.symbol=symbol;

						array_init(&statement.functionDef.bodyStatements,sizeof(Statement));
						bool stopParsingFunctionBody=false;
						while(!stopParsingFunctionBody){
							if(Token_equalString(&token,"}")){
								TokenIter_nextToken(token_iter,&token);
								break;
							}

							Statement functionBodyStatement={};
							enum STATEMENT_PARSE_RESULT res=Statement_parse(&functionBodyStatement,token_iter);
							TokenIter_lastToken(token_iter,&token);

							switch(res){
								case STATEMENT_INVALID:
									stopParsingFunctionBody=true;
									break;
								case STATEMENT_PRESENT:
									array_append(&statement.functionDef.bodyStatements,&functionBodyStatement);
									break;
							}
						}
					}else {
						fatal("expected symbol after function declaration: line %d col %d %.*s",token.line,token.col,token.len,token.p);
					}

					*out=statement;
					return STATEMENT_PRESENT;
				}
				break;
			default:
				statement.tag=STATEMENT_KIND_SYMBOL_DEFINITION;
				statement.symbolDef.symbol=symbol;
				statement.symbolDef.init_value=nullptr;

				// next token should be assignment operator or semicolon
				if(Token_equalString(&token,"=")){
					TokenIter_nextToken(token_iter,&token);
					Value value={};
					enum VALUE_PARSE_RESULT res=Value_parse(&value,token_iter);
					TokenIter_lastToken(token_iter,&token);
					switch(res){
						case VALUE_INVALID:
							fatal("invalid value after assignment operator");
							break;
						case VALUE_PRESENT:
							statement.symbolDef.init_value=allocAndCopy(sizeof(Value),&value);
							break;
					}
				}
				if(!Token_equalString(&token,";")){
					fatal("expected semicolon after statement: line %d col %d %.*s",token.line,token.col,token.len,token.p);
				}
				TokenIter_nextToken(token_iter,&token);

				*out=statement;
				return STATEMENT_PRESENT;
		}
	}while(0);

	do{
		Value value;
		enum VALUE_PARSE_RESULT res=Value_parse(&value,token_iter);
		TokenIter_lastToken(token_iter,&token);
		if(res==VALUE_INVALID){
			break;
		}

		out->tag=STATEMENT_VALUE;
		out->value.value=allocAndCopy(sizeof(Value),&value);

		// check for terminating ;
		if(!Token_equalString(&token,";")){
			fatal("expected semicolon after statement: line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_nextToken(token_iter,&token);

		return STATEMENT_PRESENT;
	}while(0);

	return STATEMENT_INVALID;
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

		case STATEMENT_KIND_RETURN:{
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
