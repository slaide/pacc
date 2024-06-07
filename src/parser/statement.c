#include<parser/parser.h>
#include<util/util.h>
#include<tokenizer.h>

char*Statement_asString(Statement*statement,int depth){
	char*ret=makeStringn(16000);
	switch(statement->tag){
		case STATEMENT_EMPTY:{
			stringAppend(ret,"%s",ind(depth*4));
			stringAppend(ret,"empty statement\n");
			break;
		}
		case STATEMENT_KIND_RETURN:{
			stringAppend(ret,"%s",ind(depth*4));
			if(statement->return_.retval==nullptr){
				stringAppend(ret,"return\n");
			}else{
				stringAppend(ret,"return %s\n",Value_asString(statement->return_.retval));
			}
			break;
		}
		case STATEMENT_FUNCTION_DEFINITION:{
			stringAppend(ret,"%s",ind(depth*4));
			stringAppend(ret,"defined function %.*s",statement->functionDef.symbol.name->len,statement->functionDef.symbol.name->p);
			stringAppend(ret," of type %s",Type_asString(statement->functionDef.symbol.type));
			for(int i=0;i<statement->functionDef.bodyStatements.len;i++){
				Statement*bodyStatement=array_get(&statement->functionDef.bodyStatements,i);
				stringAppend(ret,"\n-%s",Statement_asString(bodyStatement,depth+1));
			}
			break;
		}
		case STATEMENT_KIND_SYMBOL_DEFINITION:{
			stringAppend(ret,"%s",ind(depth*4));
			if(statement->symbolDef.symbol.name!=nullptr){
				stringAppend(ret,"%.*s of type %s",statement->symbolDef.symbol.name->len,statement->symbolDef.symbol.name->p,Type_asString(statement->symbolDef.symbol.type));
			}else{
				stringAppend(ret,"unnamed symbol of type %s",Type_asString(statement->symbolDef.symbol.type));
			}

			if(statement->symbolDef.init_value){
				stringAppend(ret," = %s",Value_asString(statement->symbolDef.init_value));
			}
			break;
		}
		case STATEMENT_KIND_FOR:{
			stringAppend(ret,"%s",ind(depth*4));
			stringAppend(ret,"for loop\n");

			stringAppend(ret,"%s",ind((depth+1)*4));
			if(statement->forLoop.init!=nullptr)
				stringAppend(ret,"init:\n%s",Statement_asString(statement->forLoop.init,depth+1));
			else
				stringAppend(ret,"init:\n%s","none");

			stringAppend(ret,"%s",ind((depth+1)*4));
			if(statement->forLoop.condition!=nullptr)
				stringAppend(ret,"condition:\n%s",Value_asString(statement->forLoop.condition));
			else
				stringAppend(ret,"condition:\n%s","none");

			stringAppend(ret,"%s",ind((depth+1)*4));
			if(statement->forLoop.step!=nullptr)
				stringAppend(ret,"step:\n%s",Value_asString(statement->forLoop.step));
			else
				stringAppend(ret,"step:\n%s","none");

			for(int i=0;i<statement->forLoop.body.len;i++){
				Statement*bodyStatement=array_get(&statement->forLoop.body,i);
				stringAppend(ret,"%s",Statement_asString(bodyStatement,depth+1));
			}

			break;
		}
		case STATEMENT_VALUE:{
			stringAppend(ret,"%sval %s\n",ind(depth*4),Value_asString(statement->value.value));
			break;
		}
		case STATEMENT_KIND_IF:{
			stringAppend(ret,"%s",ind(depth*4));
			stringAppend(ret,"if %s\n",Value_asString(statement->if_.condition));
			stringAppend(ret,"%s",Statement_asString(statement->if_.body,depth+1));

			if(statement->if_.elseBody){
				stringAppend(ret,"else\n");
				stringAppend(ret,"%s",Statement_asString(statement->if_.elseBody,depth+1));
			}
			break;
		}
		case STATEMENT_KIND_WHILE:{
			stringAppend(ret,"%s",ind(depth*4));
			if(statement->whileLoop.doWhile){
				stringAppend(ret,"loop: do while ( %s )",Value_asString(statement->whileLoop.condition));
			}else{
				stringAppend(ret,"loop: while ( %s )",Value_asString(statement->whileLoop.condition));
			}
			for(int i=0;i<statement->whileLoop.body.len;i++){
				Statement*bodyStatement=array_get(&statement->whileLoop.body,i);
				stringAppend(ret,"\n%s",ind(depth*4));
				stringAppend(ret,"%s",Statement_asString(bodyStatement,depth+1));
			}
			break;
		}
		case STATEMENT_SWITCH:{
			stringAppend(ret,"%s",ind(depth*4));
			stringAppend(ret,"switch %s",Value_asString(statement->switch_.condition));
			for(int i=0;i<statement->switch_.body.len;i++){
				Statement*bodyStatement=array_get(&statement->switch_.body,i);
				stringAppend(ret,"\n%s",ind(depth*4));
				stringAppend(ret,"%s",Statement_asString(bodyStatement,depth+1));
			}
			break;
		}
		case STATEMENT_DEFAULT:{
			stringAppend(ret,"%s",ind(depth*4));
			stringAppend(ret, "default\n");
			break;
		}
		case STATEMENT_BREAK:{
			stringAppend(ret,"%s",ind(depth*4));
			stringAppend(ret, "break\n");
			break;
		}
		case STATEMENT_CONTINUE:{
			stringAppend(ret,"%s",ind(depth*4));
			stringAppend(ret, "continue\n");
			break;
		}
		case STATEMENT_SWITCHCASE:{
			stringAppend(ret,"%s",ind(depth*4));
			stringAppend(ret,"case %s:\n",Value_asString(statement->switchCase.value));
			break;
		}
		case STATEMENT_BLOCK:{
			stringAppend(ret,"%s",ind(depth*4));
			stringAppend(ret,"block:\n");
			for(int i=0;i<statement->block.body.len;i++){
				Statement*bodyStatement=array_get(&statement->block.body,i);
				stringAppend(ret,"%s",Statement_asString(bodyStatement,depth+1));
			}
			break;
		}
		case STATEMENT_LABEL:{
			stringAppend(ret,"%s",ind(depth*4));
			stringAppend(ret,"label %.*s:\n",statement->labelDefinition.label->len,statement->labelDefinition.label->p);
			break;
		}
		case STATEMENT_GOTO:{
			stringAppend(ret,"%s",ind(depth*4));
			stringAppend(ret,"goto %s\n",Value_asString(statement->goto_.label));
			break;
		}
		case STATEMENT_TYPEDEF:{
			stringAppend(ret,"%s",ind(depth*4));
			if(statement->typedef_.symbol==nullptr){
				stringAppend(ret,"typedef");
			}else{
				if(statement->typedef_.symbol->name==nullptr){
					stringAppend(ret,"typedef %s",Type_asString(statement->typedef_.symbol->type));
				}else{
					stringAppend(ret,"typedef %.*s : %s",statement->typedef_.symbol->name->len,statement->typedef_.symbol->name->p,Type_asString(statement->typedef_.symbol->type));
				}
			}
			break;
		}
		case STATEMENT_FUNCTION_DECLARATION:{
			stringAppend(ret,"%s",ind(depth*4));
			if(statement->functionDecl.symbol.name)
				stringAppend(ret,"declared function %.*s",statement->functionDecl.symbol.name->len,statement->functionDecl.symbol.name->p);
			else
				stringAppend(ret,"declared function");
			stringAppend(ret," of type %s",Type_asString(statement->functionDecl.symbol.type));
			break;
		}
		default:
			fatal("unimplemented %s",Statementkind_asString(statement->tag));
	}
	return ret;
}

enum STATEMENT_PARSE_RESULT Statement_parse(Module*module,Statement*out,struct TokenIter*token_iter_in){
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
	if(Token_equalString(&token, "{")){
		TokenIter_nextToken(token_iter,&token);

		array body;
		array_init(&body,sizeof(Statement));

		bool stopParsingBody=false;
		while(!stopParsingBody){
			if(Token_equalString(&token,"}")){
				TokenIter_nextToken(token_iter,&token);
				break;
			}

			Statement bodyStatement={};
			enum STATEMENT_PARSE_RESULT res=Statement_parse(module,&bodyStatement,token_iter);
			TokenIter_lastToken(token_iter,&token);
			switch(res){
				case STATEMENT_PARSE_RESULT_INVALID:
					fatal("invalid statement in block");
					stopParsingBody=true;
					break;
				case STATEMENT_PARSE_RESULT_PRESENT:
					array_append(&body,&bodyStatement);
					break;
			}
		}

		*out=(Statement){
			.tag=STATEMENT_BLOCK,
			.block={
				.body=body,
			}
		};

		goto STATEMENT_PARSE_RET_SUCCESS;
	}
	if(Token_equalString(&token,"default")){
		TokenIter_nextToken(token_iter,&token);
		if(!Token_equalString(&token,":")){
			fatal("expected colon after default: line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_nextToken(token_iter,&token);

		*out=(Statement){.tag=STATEMENT_DEFAULT};
		goto STATEMENT_PARSE_RET_SUCCESS;
	}
	if(Token_equalString(&token,"typedef")){
		TokenIter_nextToken(token_iter,&token);

		int numTypedefSymbols=0;
		Symbol *typedefSymbols=nullptr;
		enum SYMBOL_PARSE_RESULT res=Symbol_parse(module,&numTypedefSymbols,&typedefSymbols,token_iter);
		TokenIter_lastToken(token_iter,&token);
		if(numTypedefSymbols!=1)fatal("expected exactly one symbol in typedef statement but got %d at %s",numTypedefSymbols,Token_print(&token));
		Symbol typedefSymbol=*typedefSymbols;

		// it is legal to typedef nothing, or a type without a name, i.e. typedef; typedef int; typedef int a; are all legal
		
		*out=(Statement){.tag=STATEMENT_TYPEDEF,};
		if(res!=SYMBOL_INVALID){
			out->typedef_.symbol=allocAndCopy(sizeof(Symbol),&typedefSymbol);
		}

		if(!Token_equalString(&token,";")){
			fatal("expected semicolon after typedef but got instead %s",Token_print(&token));
		}
		TokenIter_nextToken(token_iter,&token);

		goto STATEMENT_PARSE_RET_SUCCESS;
	}
	if(Token_equalString(&token,"case")){
		TokenIter_nextToken(token_iter,&token);

		Value caseValue={};
		enum VALUE_PARSE_RESULT res=Value_parse(module,&caseValue,token_iter);
		if(res==VALUE_INVALID){
			fatal("invalid case value at line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_lastToken(token_iter,&token);

		if(!Token_equalString(&token,":")){
			fatal("expected colon after case value: line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_nextToken(token_iter,&token);

		*out=(Statement){
			.tag=STATEMENT_SWITCHCASE,
			.switchCase={
				.value=allocAndCopy(sizeof(Value),&caseValue),
			}
		};

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
		enum VALUE_PARSE_RESULT valres=Value_parse(module,&condition,token_iter);
		if(valres==VALUE_INVALID){
			fatal("invalid condition in if statement at line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}

		TokenIter_lastToken(token_iter,&token);
		if(!Token_equalString(&token,")")){
			fatal("expected closing parenthesis after if condition: line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_nextToken(token_iter,&token);

		Statement ifBody={};
		enum STATEMENT_PARSE_RESULT res=Statement_parse(module,&ifBody,token_iter);
		TokenIter_lastToken(token_iter,&token);
		if(res==STATEMENT_PARSE_RESULT_INVALID){
			fatal("invalid statement in if body at line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		
		*out=(Statement){
			.tag=STATEMENT_KIND_IF,
			.if_={
				.condition=allocAndCopy(sizeof(Value),&condition),
				.body=allocAndCopy(sizeof(Statement),&ifBody),
				.elseBody=nullptr,
			}
		};

		if(Token_equalString(&token,"else")){
			TokenIter_nextToken(token_iter,&token);

			Statement elseBody={};
			res=Statement_parse(module,&elseBody,token_iter);
			TokenIter_lastToken(token_iter,&token);

			if(res==STATEMENT_PARSE_RESULT_PRESENT){
				out->if_.elseBody=allocAndCopy(sizeof(Statement),&elseBody);
			}
		}

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
		enum VALUE_PARSE_RESULT valres=Value_parse(module,&condition,token_iter);
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
			enum STATEMENT_PARSE_RESULT res=Statement_parse(module,&whileBodyStatement,token_iter);
			TokenIter_lastToken(token_iter,&token);
			switch(res){
				case STATEMENT_PARSE_RESULT_INVALID:
					// e.g. empty body
					endBody=true;
					break;
				default:
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
			enum STATEMENT_PARSE_RESULT res=Statement_parse(module,&doWhileBodyStatement,token_iter);
			TokenIter_lastToken(token_iter,&token);
			switch(res){
				case STATEMENT_PARSE_RESULT_INVALID:
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
		enum VALUE_PARSE_RESULT valres=Value_parse(module,&condition,token_iter);
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
		enum STATEMENT_PARSE_RESULT res=Statement_parse(module,&init_statement,token_iter);
		if(res==STATEMENT_PARSE_RESULT_INVALID){
			out->forLoop.init=nullptr;
		}else{
			TokenIter_lastToken(token_iter,&token);
			out->forLoop.init=allocAndCopy(sizeof(Statement),&init_statement);
		}

		Value condition={};
		enum VALUE_PARSE_RESULT valres=Value_parse(module,&condition,token_iter);
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
		valres=Value_parse(module,&post_expression,token_iter);
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
				enum STATEMENT_PARSE_RESULT res=Statement_parse(module,&forBodyStatement,token_iter);
				TokenIter_lastToken(token_iter,&token);
				switch(res){
					case STATEMENT_PARSE_RESULT_INVALID:
						fatal("invalid statement in for body");
						stopParsingForBody=true;
						break;
					case STATEMENT_PARSE_RESULT_PRESENT:
						array_append(&body_tokens,&forBodyStatement);
						break;
				}
			}
		// otherwise parse single statement
		}else{
			Statement forBodyStatement={};
			enum STATEMENT_PARSE_RESULT res=Statement_parse(module,&forBodyStatement,token_iter);
			TokenIter_lastToken(token_iter,&token);
			switch(res){
				case STATEMENT_PARSE_RESULT_INVALID:
					fatal("invalid statement in for body");
					break;
				case STATEMENT_PARSE_RESULT_PRESENT:
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
		*out=(Statement){.tag=STATEMENT_KIND_RETURN};

		enum VALUE_PARSE_RESULT res=Value_parse(module,&returnValue,token_iter);
		TokenIter_lastToken(token_iter,&token);
		switch(res){
			case VALUE_INVALID:
				break;
			case VALUE_PRESENT:
				out->return_.retval=allocAndCopy(sizeof(Value),&returnValue);
				break;
		}
		if(!Token_equalString(&token,";")){
			println("value missing? %d",res);
			fatal("missing semicolon after return statement at line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_nextToken(token_iter,&token);

		goto STATEMENT_PARSE_RET_SUCCESS;
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
	if(Token_equalString(&token,"goto")){
		TokenIter_nextToken(token_iter,&token);
		Value label={};
		enum VALUE_PARSE_RESULT res=Value_parse(module,&label,token_iter);
		if(res==VALUE_INVALID){
			fatal("invalid label in goto statement at line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_lastToken(token_iter,&token);
		if(!Token_equalString(&token,";")){
			fatal("expected semicolon after goto statement: line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_nextToken(token_iter,&token);

		*out=(Statement){
			.tag=STATEMENT_GOTO,
			.goto_={
				.label=allocAndCopy(sizeof(Value),&label)
			}
		};
		goto STATEMENT_PARSE_RET_SUCCESS;
	}
	if(Token_equalString(&token,"switch")){
		TokenIter_nextToken(token_iter,&token);
		*out=(Statement){.tag=STATEMENT_SWITCH,.switch_={}};

		// check for (
		if(!Token_equalString(&token,"(")){
			fatal("expected opening parenthesis after switch");
		}
		TokenIter_nextToken(token_iter,&token);

		Value switchValue={};
		enum VALUE_PARSE_RESULT res=Value_parse(module,&switchValue,token_iter);
		if(res==VALUE_INVALID){
			fatal("invalid switch value at line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_lastToken(token_iter,&token);

		if(!Token_equalString(&token,")")){
			fatal("expected closing parenthesis after switch value: line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_nextToken(token_iter,&token);

		if(!Token_equalString(&token,"{")){
			fatal("expected opening curly brace after switch value: line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_nextToken(token_iter,&token);

		array body;
		array_init(&body,sizeof(Statement));

		bool stopParsingSwitchBody=false;
		while(!stopParsingSwitchBody){
			if(Token_equalString(&token,"}")){
				TokenIter_nextToken(token_iter,&token);
				break;
			}

			Statement bodyStatement={};
			enum STATEMENT_PARSE_RESULT res=Statement_parse(module,&bodyStatement,token_iter);
			TokenIter_lastToken(token_iter,&token);
			switch(res){
				case STATEMENT_PARSE_RESULT_INVALID:
					fatal("invalid statement in switch body");
					stopParsingSwitchBody=true;
					break;
				case STATEMENT_PARSE_RESULT_PRESENT:
					array_append(&body,&bodyStatement);
					break;
			}
		}

		out->switch_.condition=allocAndCopy(sizeof(Value),&switchValue);
		out->switch_.body=body;

		goto STATEMENT_PARSE_RET_SUCCESS;
	}

	// parse symbol definition
	do{
		int numSymbols=0;
		Symbol *symbols=nullptr;
		struct TokenIter preSymbolParseIter=*token_iter;
		enum SYMBOL_PARSE_RESULT symbolParseResult=Symbol_parse(module,&numSymbols,&symbols,token_iter);
		if(symbolParseResult==SYMBOL_INVALID){
			break;
		}
		TokenIter_lastToken(token_iter,&token);

		if(numSymbols!=1)fatal("expected exactly one symbol in statement but got %d at %s",numSymbols,Token_print(&token));
		Symbol symbol=*symbols;

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
							enum STATEMENT_PARSE_RESULT res=Statement_parse(module,&functionBodyStatement,token_iter);
							TokenIter_lastToken(token_iter,&token);

							switch(res){
								case STATEMENT_PARSE_RESULT_INVALID:
									stopParsingFunctionBody=true;
									break;
								case STATEMENT_PARSE_RESULT_PRESENT:
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
				if(numSymbols==1 && symbol.name==nullptr){
					Value value={};
					// discard iterator result from symbol parse attempt
					struct TokenIter valueParseIter=preSymbolParseIter;
					enum VALUE_PARSE_RESULT res=Value_parse(module,&value,&valueParseIter);

					bool foundValue=false;
					switch(res){
						case VALUE_INVALID:
							break;
						case VALUE_PRESENT:
							*token_iter=valueParseIter;
							foundValue=true;
							break;
					}
					// note the iterator writeback after value parsing within the switch statement above!
					TokenIter_lastToken(token_iter,&token);

					if(foundValue){
						// check for label definition
						if(Token_equalString(&token,":") && value.kind==VALUE_KIND_SYMBOL_REFERENCE){
							TokenIter_nextToken(token_iter,&token);
							*out=(Statement){
								.tag=STATEMENT_LABEL,
								.labelDefinition={
									.label=value.symbol,
								}
							};
							goto STATEMENT_PARSE_RET_SUCCESS;
						}

						// check for termination with semicolon
						if(!Token_equalString(&token,";")){
							fatal("expected semicolon but got instead: %s",Token_print(&token));
						}
						TokenIter_nextToken(token_iter,&token);
						
						*out=(Statement){
							.tag=STATEMENT_VALUE,
							.value={
								.value=allocAndCopy(sizeof(Value), &value)
							},
						};
						goto STATEMENT_PARSE_RET_SUCCESS;
					}else{
						// we got a symbol declaration without a name
						// i.e. still valid statement, of kind STATEMENT_KIND_SYMBOL_DEFINITION
						// next token should be semicolon
						if(!Token_equalString(&token,";")){
							fatal("expected semicolon but got instead: %s",Token_print(&token));
						}
						TokenIter_nextToken(token_iter,&token);
						*out=(Statement){
							.tag=STATEMENT_KIND_SYMBOL_DEFINITION,
							.symbolDef={
								.symbol=symbol,
								.init_value=nullptr,
							}
						};
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
					enum VALUE_PARSE_RESULT res=Value_parse(module,&value,token_iter);
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
					fatal("expected semicolon after statement but got instead: %s",Token_print(&token));
				}
				TokenIter_nextToken(token_iter,&token);

				*out=statement;
				goto STATEMENT_PARSE_RET_SUCCESS;
			}
		}
	}while(0);

	// parse a value
	do{
		Value value;
		enum VALUE_PARSE_RESULT res=Value_parse(module,&value,token_iter);
		TokenIter_lastToken(token_iter,&token);
		if(res==VALUE_INVALID){
			break;
		}

		out->tag=STATEMENT_VALUE;
		out->value.value=allocAndCopy(sizeof(Value),&value);

		// check for terminating ;
		if(!Token_equalString(&token,";")){
			fatal("expected semicolon after statement at %s",Token_print(&token));
		}
		TokenIter_nextToken(token_iter,&token);

		goto STATEMENT_PARSE_RET_SUCCESS;
	}while(0);

	return STATEMENT_PARSE_RESULT_INVALID;

STATEMENT_PARSE_RET_SUCCESS:
	switch(out->tag){
		case STATEMENT_TYPEDEF:{
			// append typedef to module
			if(out->typedef_.symbol==nullptr)fatal("bug");
			if(out->typedef_.symbol->type==nullptr)fatal("bug");
			Type*t_copy=allocAndCopy(sizeof(Type),&(Type){
				.name=out->typedef_.symbol->name,
				.kind=TYPE_KIND_REFERENCE,
				.reference={.ref=out->typedef_.symbol->type}
			});
			array_append(&module->types,&t_copy);
			Token alias_name=*out->typedef_.symbol->name;
			println("just copied type %s as %.*s",Type_asString(out->typedef_.symbol->type),alias_name.len,alias_name.p);
			break;
		}
		default:;
	}

	*token_iter_in=*token_iter;
	return STATEMENT_PARSE_RESULT_PRESENT;
}
bool Statement_equal(Statement*a,Statement*b){
	if(a->tag!=b->tag){
		println("tag mismatch when comparing statements %d %d",a->tag,b->tag);
		return false;
	}

	switch(a->tag){
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
		case STATEMENT_SWITCHCASE:
			return("STATEMENT_SWITCHCASE");
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
		case STATEMENT_BLOCK:
			return("STATEMENT_BLOCK");
		case STATEMENT_EMPTY:
			return("STATEMENT_EMPTY");
		case STATEMENT_TYPEDEF:
			return("STATEMENT_TYPEDEF");

		default: fatal("unknown statement kind %d",kind);
	}
}
