#include<parser/parser.h>
#include<util/util.h>
#include<tokenizer.h>

char*Statement_asString(Statement*statement){
	switch(statement->tag){
		case STATEMENT_EMPTY:{
			return "empty statement";
		}
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
			sprintf(ret+strlen(ret),"loop: for\n");
			if(statement->forLoop.init!=nullptr)
				sprintf(ret+strlen(ret),"  init: %s\n",Statement_asString(statement->forLoop.init));
			else
				sprintf(ret+strlen(ret),"  init: none\n");
			if(statement->forLoop.condition!=nullptr)
				sprintf(ret+strlen(ret),"  condition: %s\n",Value_asString(statement->forLoop.condition));
			else
				sprintf(ret+strlen(ret),"  condition: none\n");
			if(statement->forLoop.step!=nullptr)
				sprintf(ret+strlen(ret),"  step: %s\n",Value_asString(statement->forLoop.step));
			else
				sprintf(ret+strlen(ret),"  step: none\n");

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
		case STATEMENT_KIND_IF:{
			char*ret=calloc(1024,1);
			sprintf(ret,"if %s",Value_asString(statement->if_.condition));
			for(int i=0;i<statement->if_.body.len;i++){
				Statement*bodyStatement=array_get(&statement->if_.body,i);
				sprintf(ret+strlen(ret),"\n  body %d: %s",i,Statement_asString(bodyStatement));
			}
			if(statement->if_.elseBodyPresent){
				sprintf(ret+strlen(ret),"\nelse");
				for(int i=0;i<statement->if_.elseBody.len;i++){
					Statement*bodyStatement=array_get(&statement->if_.elseBody,i);
					sprintf(ret+strlen(ret),"\n  body %d: %s",i,Statement_asString(bodyStatement));
				}
			}
			return ret;
		}
		case STATEMENT_KIND_WHILE:{
			char*ret=calloc(1024,1);
			if(statement->whileLoop.doWhile){
				sprintf(ret,"loop: do while ( %s )",Value_asString(statement->whileLoop.condition));
			}else{
				sprintf(ret,"loop: while ( %s )",Value_asString(statement->whileLoop.condition));
			}
			for(int i=0;i<statement->whileLoop.body.len;i++){
				Statement*bodyStatement=array_get(&statement->whileLoop.body,i);
				sprintf(ret+strlen(ret),"\n  body %d: %s",i,Statement_asString(bodyStatement));
			}
			return ret;
		}
		default:
			fatal("unimplemented %s",Statementkind_asString(statement->tag));
	}
}

enum STATEMENT_PARSE_RESULT Statement_parse(Statement*out,struct TokenIter*token_iter_in){
	struct TokenIter token_iter_copy=*token_iter_in;
	struct TokenIter*token_iter=&token_iter_copy;

	Token token;
	TokenIter_lastToken(token_iter,&token);

	// long list of stuff that can be a statement

	if(Token_equalString(&token,";")){
		TokenIter_nextToken(token_iter,&token);
		*out=(Statement){.tag=STATEMENT_EMPTY};
		goto STATEMENT_PARSE_RET_SUCCESS;
	}
	if(Token_equalString(&token,"if")){
		TokenIter_nextToken(token_iter,&token);
		// check for (
		if(!Token_equalString(&token,"(")){
			fatal("expected opening parenthesis after if");
		}
		TokenIter_nextToken(token_iter,&token);

		Value condition={};
		enum VALUE_PARSE_RESULT valres=Value_parse(&condition,token_iter);
		if(valres==VALUE_INVALID){
			fatal("invalid condition in if statement at line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}

		TokenIter_lastToken(token_iter,&token);
		if(!Token_equalString(&token,")")){
			fatal("expected closing parenthesis after if condition: line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_nextToken(token_iter,&token);

		array ifBodyTokens;
		array_init(&ifBodyTokens,sizeof(Statement));

		bool singleStatementBody=!Token_equalString(&token,"{");
		if(!singleStatementBody){
			TokenIter_nextToken(token_iter,&token);
		}

		do{
			Statement ifBodyStatement={};
			enum STATEMENT_PARSE_RESULT res=Statement_parse(&ifBodyStatement,token_iter);
			TokenIter_lastToken(token_iter,&token);
			switch(res){
				case STATEMENT_INVALID:
					fatal("invalid statement in if body at line %d col %d %.*s",token.line,token.col,token.len,token.p);
					break;
				default:
					array_append(&ifBodyTokens,&ifBodyStatement);
					break;
			}
		}while(!singleStatementBody && !Token_equalString(&token,"}"));
		if(!singleStatementBody)
			TokenIter_nextToken(token_iter,&token);

		bool elseBodyPresent=false;
		array elseBody={};
		if(Token_equalString(&token, "else")){
			TokenIter_nextToken(token_iter,&token);

			array elseBodyTokens;
			array_init(&elseBodyTokens,sizeof(Statement));

			bool singleStatementElseBody=!Token_equalString(&token,"{");
			if(!singleStatementElseBody){
				TokenIter_nextToken(token_iter,&token);
			}

			do{
				Statement elseBodyStatement={};
				enum STATEMENT_PARSE_RESULT res=Statement_parse(&elseBodyStatement,token_iter);
				TokenIter_lastToken(token_iter,&token);
				switch(res){
					case STATEMENT_INVALID:
						fatal("invalid statement in else body");
						break;
					default:
						array_append(&elseBodyTokens,&elseBodyStatement);
						break;
				}
			}while(!singleStatementElseBody && !Token_equalString(&token,"}"));
			if(!singleStatementElseBody)
				TokenIter_nextToken(token_iter,&token);

			elseBodyPresent=true;
			elseBody=elseBodyTokens;
		}

		*out=(Statement){
			.tag=STATEMENT_KIND_IF,
			.if_={
				.condition=allocAndCopy(sizeof(Value),&condition),
				.body=ifBodyTokens,
				.elseBodyPresent=elseBodyPresent,
				.elseBody=elseBody,
			}
		};

		goto STATEMENT_PARSE_RET_SUCCESS;
	}
	if(Token_equalString(&token,"while")){
		TokenIter_nextToken(token_iter,&token);
		// check for (
		if(!Token_equalString(&token,"(")){
			fatal("expected opening parenthesis after while");
		}
		TokenIter_nextToken(token_iter,&token);

		Value condition={};
		enum VALUE_PARSE_RESULT valres=Value_parse(&condition,token_iter);
		if(valres==VALUE_INVALID){
			fatal("invalid condition in while statement at line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_lastToken(token_iter,&token);

		if(!Token_equalString(&token,")")){
			fatal("expected closing parenthesis after while condition: line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_nextToken(token_iter,&token);

		bool singleStatementBody=!Token_equalString(&token,"{");
		if(!singleStatementBody){
			TokenIter_nextToken(token_iter,&token);
		}

		array body_tokens;
		array_init(&body_tokens,sizeof(Statement));

		bool endBody=false;
		do{
			Statement whileBodyStatement={};
			enum STATEMENT_PARSE_RESULT res=Statement_parse(&whileBodyStatement,token_iter);
			TokenIter_lastToken(token_iter,&token);
			switch(res){
				case STATEMENT_INVALID:
					// e.g. empty body
					endBody=true;
					break;
				default:
					println("found statement %s",Statement_asString(&whileBodyStatement));
					array_append(&body_tokens,&whileBodyStatement);
					break;
			}
		}while(!singleStatementBody && !endBody &&!Token_equalString(&token,"}"));
		if(!singleStatementBody)
			TokenIter_nextToken(token_iter,&token);

		*out=(Statement){
			.tag=STATEMENT_KIND_WHILE,
			.whileLoop={
				.condition=allocAndCopy(sizeof(Value),&condition),
				.body=body_tokens,
			}
		};

		goto STATEMENT_PARSE_RET_SUCCESS;
	}
	if(Token_equalString(&token,"do")){
		TokenIter_nextToken(token_iter,&token);
		
		bool singleStatementBody=!Token_equalString(&token,"{");
		if(!singleStatementBody){
			TokenIter_nextToken(token_iter,&token);
		}

		array body_tokens;
		array_init(&body_tokens,sizeof(Statement));

		do{
			Statement doWhileBodyStatement={};
			enum STATEMENT_PARSE_RESULT res=Statement_parse(&doWhileBodyStatement,token_iter);
			TokenIter_lastToken(token_iter,&token);
			switch(res){
				case STATEMENT_INVALID:
					fatal("invalid statement in do while body");
					break;
				default:
					array_append(&body_tokens,&doWhileBodyStatement);
					break;
			}
		}while(!singleStatementBody && !Token_equalString(&token,"}"));
		if(!singleStatementBody)
			TokenIter_nextToken(token_iter,&token);

		if(!Token_equalString(&token,"while")){
			fatal("expected while after do while body");
		}
		TokenIter_nextToken(token_iter,&token);

		// check for (
		if(!Token_equalString(&token,"(")){
			fatal("expected opening parenthesis after do while");
		}
		TokenIter_nextToken(token_iter,&token);

		Value condition={};
		enum VALUE_PARSE_RESULT valres=Value_parse(&condition,token_iter);
		if(valres==VALUE_INVALID){
			fatal("invalid condition in do while statement at line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_lastToken(token_iter,&token);

		// check for )
		if(!Token_equalString(&token,")")){
			fatal("expected closing parenthesis after do while condition: line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_nextToken(token_iter,&token);

		// check for trailing semicolon
		if(!Token_equalString(&token,";")){
			fatal("expected semicolon after do while statement: line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_nextToken(token_iter,&token);

		*out=(Statement){
			.tag=STATEMENT_KIND_WHILE,
			.whileLoop={
				.doWhile=true,
				.condition=allocAndCopy(sizeof(Value),&condition),
				.body=body_tokens,
			}
		};

		goto STATEMENT_PARSE_RET_SUCCESS;
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
		if(res==STATEMENT_INVALID){
			out->forLoop.init=nullptr;
		}else{
			TokenIter_lastToken(token_iter,&token);
			out->forLoop.init=allocAndCopy(sizeof(Statement),&init_statement);
		}

		Value condition={};
		enum VALUE_PARSE_RESULT valres=Value_parse(&condition,token_iter);
		if(valres==VALUE_INVALID){
			out->forLoop.condition=nullptr;
		}else{
			TokenIter_lastToken(token_iter,&token);
			out->forLoop.condition=allocAndCopy(sizeof(Value),&condition);
		}

		// check for semicolon
		if(!Token_equalString(&token,";")){
			fatal("expected semicolon after for condition statement: line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_nextToken(token_iter,&token);

		Value post_expression={};
		valres=Value_parse(&post_expression,token_iter);
		if(valres==VALUE_INVALID){
			out->forLoop.step=nullptr;
		}else{
			TokenIter_lastToken(token_iter,&token);

			out->forLoop.step=allocAndCopy(sizeof(Value),&post_expression);
		}

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

		goto STATEMENT_PARSE_RET_SUCCESS;
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

			goto STATEMENT_PARSE_RET_SUCCESS;
		}
		fatal("missing semicolon after return statement at line %d col %d %.*s",token.line,token.col,token.len,token.p);
	}
	if(Token_equalString(&token,"break")){
		TokenIter_nextToken(token_iter,&token);
		*out=(Statement){.tag=STATEMENT_BREAK};
		if(!Token_equalString(&token,";")){
			fatal("expected semicolon after break statement: line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_nextToken(token_iter,&token);
		goto STATEMENT_PARSE_RET_SUCCESS;
	}
	if(Token_equalString(&token,"continue")){
		TokenIter_nextToken(token_iter,&token);
		*out=(Statement){.tag=STATEMENT_CONTINUE};
		if(!Token_equalString(&token,";")){
			fatal("expected semicolon after continue statement: line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_nextToken(token_iter,&token);
		goto STATEMENT_PARSE_RET_SUCCESS;
	}
	if(Token_equalString(&token,"switch")){
		fatal("switch not yet implemented");
	}

	// parse symbol definition
	do{
		Symbol symbol={};
		struct TokenIter preSymbolParseIter=*token_iter;
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
					goto STATEMENT_PARSE_RET_SUCCESS;
				}
				break;
			default:{
				// check for value [expression]
				if(symbolParseResult==SYMBOL_WITHOUT_NAME){
					Value value={};
					struct TokenIter valueParseIter=preSymbolParseIter;
					enum VALUE_PARSE_RESULT res=Value_parse(&value,&valueParseIter);
					TokenIter_lastToken(token_iter,&token);

					bool foundValue=false;
					switch(res){
						case VALUE_INVALID:
							fatal("invalid value in statement");
							break;
						case VALUE_PRESENT:
							*token_iter=valueParseIter;
							foundValue=true;
							break;
					}

					if(foundValue){
						*out=(Statement){.tag=STATEMENT_VALUE,.value.value=allocAndCopy(sizeof(Value), &value)};

						// check for termination with semicolon
						TokenIter_lastToken(token_iter,&token);
						if(!Token_equalString(&token,";")){
							fatal("expected semicolon after statement: line %d col %d %.*s",token.line,token.col,token.len,token.p);
						}
						TokenIter_nextToken(token_iter,&token);
						goto STATEMENT_PARSE_RET_SUCCESS;
					}
				}

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
				goto STATEMENT_PARSE_RET_SUCCESS;
			}
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

		goto STATEMENT_PARSE_RET_SUCCESS;
	}while(0);

	return STATEMENT_INVALID;

STATEMENT_PARSE_RET_SUCCESS:
	*token_iter_in=*token_iter;
	return STATEMENT_PRESENT;
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

const char* Statementkind_asString(enum STATEMENT_KIND kind){
	switch(kind){
		case STATEMENT_UNKNOWN:
			return("STATEMENT_UNKNOWN");
		case STATEMENT_PREP_DEFINE:
			return("STATEMENT_PREP_DEFINE");
		case STATEMENT_PREP_INCLUDE:
			return("STATEMENT_PREP_INCLUDE");
		case STATEMENT_FUNCTION_DECLARATION:
			return("STATEMENT_FUNCTION_DECLARATION");
		case STATEMENT_FUNCTION_DEFINITION:
			return("STATEMENT_FUNCTION_DEFINITION");
		case STATEMENT_KIND_RETURN:
			return("STATEMENT_KIND_RETURN");
		case STATEMENT_KIND_IF:
			return("STATEMENT_IF");
		case STATEMENT_SWITCH:
			return("STATEMENT_SWITCH");
		case STATEMENT_CASE:
			return("STATEMENT_CASE");
		case STATEMENT_BREAK:
			return("STATEMENT_BREAK");
		case STATEMENT_CONTINUE:
			return("STATEMENT_CONTINUE");
		case STATEMENT_DEFAULT:
			return("STATEMENT_DEFAULT");
		case STATEMENT_GOTO:
			return("STATEMENT_GOTO");
		case STATEMENT_LABEL:
			return("STATEMENT_LABEL");
		case STATEMENT_KIND_WHILE:
			return("STATEMENT_KIND_WHILE");
		case STATEMENT_KIND_FOR:
			return("STATEMENT_KIND_FOR");
		case STATEMENT_KIND_SYMBOL_DEFINITION:
			return("STATEMENT_KIND_SYMBOL_DEFINITION");
		case STATEMENT_VALUE:
			return("STATEMENT_VALUE");
		default:
			fatal("unknown symbol kind %d",kind);
	}
}
