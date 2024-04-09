#include<parser/parser.h>
#include<util/util.h>
#include<tokenizer.h>

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
		println("got for loop");

		TokenIter_nextToken(token_iter,&token);
		if(!Token_equalString(&token,"(")){
			fatal("expected opening parenthesis after for");
		}
		TokenIter_nextToken(token_iter,&token);

		Statement init_statement={};
		enum STATEMENT_PARSE_RESULT res=Statement_parse(&init_statement,token_iter);
		if(res==STATEMENT_INVALID) fatal("invalid init statement in for loop");
		TokenIter_lastToken(token_iter,&token);
		println("next token after init statement is: line %d col %d %.*s",token.line,token.col,token.len,token.p);

		Value condition={};
		enum VALUE_PARSE_RESULT valres=Value_parse(&condition,token_iter);
		if(valres==VALUE_INVALID) fatal("invalid condition in for loop");
		TokenIter_lastToken(token_iter,&token);
		// check for semicolon
		if(!Token_equalString(&token,";")){
			fatal("expected semicolon after for condition statement: line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_nextToken(token_iter,&token);

		println("next token after for conidition statement is: line %d col %d %.*s",token.line,token.col,token.len,token.p);
		Value post_expression={};
		valres=Value_parse(&post_expression,token_iter);
		if(valres==VALUE_INVALID) fatal("invalid post expression in for loop");
		TokenIter_lastToken(token_iter,&token);

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
				println("parsing for body statement with next token %.*s",token.len,token.p);
				Statement forBodyStatement={};
				enum STATEMENT_PARSE_RESULT res=Statement_parse(&forBodyStatement,token_iter);
				TokenIter_lastToken(token_iter,&token);
				switch(res){
					case STATEMENT_INVALID:
						println("invalid statement in for body");
						stopParsingForBody=true;
						break;
					case STATEMENT_PRESENT:
						println("got statement in for body");
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
					println("got statement in for body");
					array_append(&body_tokens,&forBodyStatement);
					break;
			}
		}

		println("got %d statements in for body",body_tokens.len);

		return STATEMENT_PRESENT;
	}

	if(Token_equalString(&token,"return")){
		TokenIter_nextToken(token_iter,&token);

		Value returnValue={};
		enum VALUE_PARSE_RESULT res=Value_parse(&returnValue,token_iter);
		TokenIter_lastToken(token_iter,&token);
		switch(res){
			case VALUE_INVALID:
				println("return statement without return value");
				break;
			case VALUE_PRESENT:
				println("return statement with return value");
				break;
		}
		if(Token_equalString(&token,";")){
			TokenIter_nextToken(token_iter,&token);

			out->tag=STATEMENT_RETURN;
			out->return_.retval=allocAndCopy(sizeof(Value),&returnValue);

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

		println("symbol name %.*s",symbol.name->len,symbol.name->p);
		Type_print(symbol.type);

		switch(symbol.type->kind){
			case TYPE_KIND_FUNCTION:
				{
					Statement statement={};
					if(Token_equalString(&token,";")){
						TokenIter_nextToken(token_iter,&token);

						statement.tag=STATEMENT_FUNCTION_DECLARATION;
						statement.functionDecl.symbol=symbol;

						println("got function declaration");
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

							println("parsing function body statement");
							Statement functionBodyStatement={};
							enum STATEMENT_PARSE_RESULT res=Statement_parse(&functionBodyStatement,token_iter);
							TokenIter_lastToken(token_iter,&token);

							switch(res){
								case STATEMENT_INVALID:
									println("invalid statement in function body");
									stopParsingFunctionBody=true;
									break;
								case STATEMENT_PRESENT:
									println("got statement in function body");
									array_append(&statement.functionDef.bodyStatements,&functionBodyStatement);
									break;
							}
						}

						println("got %d statements in function body",statement.functionDef.bodyStatements.len);
					}else {
						fatal("expected symbol after function declaration: line %d col %d %.*s",token.line,token.col,token.len,token.p);
					}

					*out=statement;
					return STATEMENT_PRESENT;
				}
				break;
			default:
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
							println("got value after assignment operator");
							break;
					}
				}
				if(!Token_equalString(&token,";")){
					fatal("expected semicolon after statement: line %d col %d %.*s",token.line,token.col,token.len,token.p);
				}
				TokenIter_nextToken(token_iter,&token);
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

		case STATEMENT_RETURN:{
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
