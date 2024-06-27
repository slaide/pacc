#include<parser/parser.h>
#include<util/util.h>
#include<tokenizer.h>
#include<parser/statement.h>

char*Statement_asString(Statement*statement,int depth){
	char*ret=makeStringn(16000);
	switch(statement->tag){
		case STATEMENT_KIND_EMPTY:{
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
		case STATEMENT_KIND_FUNCTION_DEFINITION:{
			stringAppend(ret,"%s",ind(depth*4));
			stringAppend(ret,"defined function %.*s",statement->functionDef.symbol.name->len,statement->functionDef.symbol.name->p);
			stringAppend(ret," of type %s",Type_asString(statement->functionDef.symbol.type));
			for(int i=0;i<statement->functionDef.stack->statements.len;i++){
				Statement*bodyStatement=array_get(&statement->functionDef.stack->statements,i);
				stringAppend(ret,"\n-%s",Statement_asString(bodyStatement,depth+1));
			}
			break;
		}
		case STATEMENT_KIND_SYMBOL_DEFINITION:{
			stringAppend(ret,"%s",ind(depth*4));
			array *symbol_defs=&statement->symbolDef.symbols_defs;
			if(symbol_defs->len==0)fatal("empty symbol definition? bug!");
			stringAppend(ret,"symbols: ");
			for(int i=0;i<symbol_defs->len;i++){
				stringAppend(ret,"\n%s",ind((depth+1)*4));
				struct SymbolDefinition*def=array_get(symbol_defs,i);
				if(def->symbol.name!=nullptr){
					stringAppend(ret,"%.*s of type %s",def->symbol.name->len,def->symbol.name->p,Type_asString(def->symbol.type));
				}else{
					stringAppend(ret,"unnamed symbol of type %s",Type_asString(def->symbol.type));
				}

				if(def->initializer!=nullptr){
					stringAppend(ret," = %s",Value_asString(def->initializer));
				}
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

			for(int i=0;i<statement->forLoop.stack->statements.len;i++){
				Statement*bodyStatement=array_get(&statement->forLoop.stack->statements,i);
				stringAppend(ret,"%s",Statement_asString(bodyStatement,depth+1));
			}

			break;
		}
		case STATEMENT_KIND_VALUE:{
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
			stringAppend(ret,"%s",Statement_asString(statement->whileLoop.body,depth+1));
			break;
		}
		case STATEMENT_KIND_SWITCH:{
			stringAppend(ret,"%s",ind(depth*4));
			stringAppend(ret,"switch %s",Value_asString(statement->switch_.condition));
			for(int i=0;i<statement->switch_.body.len;i++){
				Statement*bodyStatement=array_get(&statement->switch_.body,i);
				stringAppend(ret,"\n%s",ind(depth*4));
				stringAppend(ret,"%s",Statement_asString(bodyStatement,depth+1));
			}
			break;
		}
		case STATEMENT_KIND_DEFAULT:{
			stringAppend(ret,"%s",ind(depth*4));
			stringAppend(ret, "default\n");
			break;
		}
		case STATEMENT_KIND_BREAK:{
			stringAppend(ret,"%s",ind(depth*4));
			stringAppend(ret, "break\n");
			break;
		}
		case STATEMENT_KIND_CONTINUE:{
			stringAppend(ret,"%s",ind(depth*4));
			stringAppend(ret, "continue\n");
			break;
		}
		case STATEMENT_KIND_SWITCHCASE:{
			stringAppend(ret,"%s",ind(depth*4));
			stringAppend(ret,"case %s:\n",Value_asString(statement->switchCase.value));
			break;
		}
		case STATEMENT_KIND_BLOCK:{
			stringAppend(ret,"%s",ind(depth*4));
			stringAppend(ret,"block:\n");
			for(int i=0;i<statement->block.stack->statements.len;i++){
				Statement*bodyStatement=array_get(&statement->block.stack->statements,i);
				stringAppend(ret,"%s",Statement_asString(bodyStatement,depth+1));
			}
			break;
		}
		case STATEMENT_KIND_LABEL:{
			stringAppend(ret,"%s",ind(depth*4));
			stringAppend(ret,"label %.*s:\n",statement->labelDefinition.label->len,statement->labelDefinition.label->p);
			break;
		}
		case STATEMENT_KIND_GOTO:{
			stringAppend(ret,"%s",ind(depth*4));
			switch(statement->goto_.variant){
				case VALUE_GOTO_LABEL_VARIANT_LABEL:
					stringAppend(ret,"goto %.*s\n",statement->goto_.labelName->len,statement->goto_.labelName->p);
					break;
				case VALUE_GOTO_LABEL_VARIANT_VALUE:
					stringAppend(ret,"goto %s\n",Value_asString(statement->goto_.label));
					break;
			}
			break;
		}
		case STATEMENT_KIND_TYPEDEF:{
			stringAppend(ret,"%s",ind(depth*4));
			for(int i=0;i<statement->typedef_.symbols.len;i++){
				Symbol*sym=array_get(&statement->typedef_.symbols,i);
				if(sym->name==nullptr){
					stringAppend(ret,"typedef %s\n",Type_asString(sym->type));
				}else{
					stringAppend(ret,"typedef %.*s : %s\n",sym->name->len,sym->name->p,Type_asString(sym->type));
				}
			}
			break;
		}
		default:
			fatal("unimplemented %s",Statementkind_asString(statement->tag));
	}
	return ret;
}

enum STATEMENT_PARSE_RESULT Statement_parse(Stack*stack,Statement*out,struct TokenIter*token_iter_in){
	struct TokenIter token_iter_copy=*token_iter_in;
	struct TokenIter*token_iter=&token_iter_copy;

	Token token;
	TokenIter_lastToken(token_iter,&token);

	// long list of stuff that can be a statement

	if(Token_equalString(&token,";")){
		TokenIter_nextToken(token_iter,&token);
		*out=(Statement){.tag=STATEMENT_KIND_EMPTY};
		goto STATEMENT_PARSE_RET_SUCCESS;
	}
	if(Token_equalString(&token, "{")){
		TokenIter_nextToken(token_iter,&token);

		Stack newStack={};
		Stack_init(&newStack,stack);

		bool stopParsingBody=false;
		while(!stopParsingBody){
			if(Token_equalString(&token,"}")){
				TokenIter_nextToken(token_iter,&token);
				break;
			}

			Statement bodyStatement={};
			enum STATEMENT_PARSE_RESULT res=Statement_parse(&newStack,&bodyStatement,token_iter);
			TokenIter_lastToken(token_iter,&token);
			switch(res){
				case STATEMENT_PARSE_RESULT_INVALID:
					fatal("invalid statement in block");
					stopParsingBody=true;
					break;
				case STATEMENT_PARSE_RESULT_PRESENT:
					Stack_addStatement(&newStack, &bodyStatement);
					break;
			}
		}

		*out=(Statement){
			.tag=STATEMENT_KIND_BLOCK,
			.block={
				.stack=allocAndCopy(sizeof(Stack),&newStack),
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

		*out=(Statement){.tag=STATEMENT_KIND_DEFAULT};
		goto STATEMENT_PARSE_RET_SUCCESS;
	}
	if(Token_equalString(&token,"typedef")){
		TokenIter_nextToken(token_iter,&token);

		int numTypedefSymbols=0;
		struct SymbolDefinition *typedefSymbols=nullptr;
		enum SYMBOL_PARSE_RESULT res=SymbolDefinition_parse(stack,&numTypedefSymbols,&typedefSymbols,token_iter,&(struct Symbol_parse_options){.forbid_multiple=false});
		// if valid symbol was parsed: check for semicolon
		// if not valid symbol was parsed: still check for semicolon because "typedef;" is valid
		// set .tag on out because failure here will be fatal anyway
		*out=(Statement){.tag=STATEMENT_KIND_TYPEDEF,};
		if(res==SYMBOL_PRESENT){
			TokenIter_lastToken(token_iter,&token);
			// it is legal to typedef nothing, or a type without a name, i.e. typedef; typedef int; typedef int a; or multiple at once, like typedef int A,*B; are all legal
			
			array_init(&out->typedef_.symbols,sizeof(Symbol));
			for(int i=0;i<numTypedefSymbols;i++){
				array_append(&out->typedef_.symbols,&typedefSymbols[i].symbol);
			}
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
		enum VALUE_PARSE_RESULT res=Value_parse(stack,&caseValue,token_iter);
		if(res==VALUE_INVALID){
			fatal("invalid case value at line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_lastToken(token_iter,&token);

		if(!Token_equalString(&token,":")){
			fatal("expected colon after case value: line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_nextToken(token_iter,&token);

		*out=(Statement){
			.tag=STATEMENT_KIND_SWITCHCASE,
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
		enum VALUE_PARSE_RESULT valres=Value_parse(stack,&condition,token_iter);
		if(valres==VALUE_INVALID){
			fatal("invalid condition in if statement at line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}

		TokenIter_lastToken(token_iter,&token);
		if(!Token_equalString(&token,")")){
			fatal("expected closing parenthesis after if condition: line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_nextToken(token_iter,&token);

		Statement ifBody={};
		enum STATEMENT_PARSE_RESULT res=Statement_parse(stack,&ifBody,token_iter);
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
			res=Statement_parse(stack,&elseBody,token_iter);
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
		enum VALUE_PARSE_RESULT valres=Value_parse(stack,&condition,token_iter);
		if(valres==VALUE_INVALID){
			fatal("invalid condition in while statement at line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_lastToken(token_iter,&token);

		if(!Token_equalString(&token,")")){
			fatal("expected closing parenthesis after while condition: line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_nextToken(token_iter,&token);

		Statement whileBody={};
		enum STATEMENT_PARSE_RESULT res=Statement_parse(stack,&whileBody,token_iter);
		TokenIter_lastToken(token_iter,&token);
		if(res==STATEMENT_PARSE_RESULT_INVALID){
			fatal("invalid statement in if body at line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}

		*out=(Statement){
			.tag=STATEMENT_KIND_WHILE,
			.whileLoop={
				.condition=allocAndCopy(sizeof(Value),&condition),
				.body=allocAndCopy(sizeof(Statement),&whileBody),
			}
		};

		goto STATEMENT_PARSE_RET_SUCCESS;
	}
	if(Token_equalString(&token,"do")){
		TokenIter_nextToken(token_iter,&token);
		
		Statement whileBody={};
		enum STATEMENT_PARSE_RESULT res=Statement_parse(stack,&whileBody,token_iter);
		TokenIter_lastToken(token_iter,&token);
		if(res==STATEMENT_PARSE_RESULT_INVALID){
			fatal("invalid statement in if body at line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}

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
		enum VALUE_PARSE_RESULT valres=Value_parse(stack,&condition,token_iter);
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
				.body=allocAndCopy(sizeof(Statement), &whileBody)
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

		Stack forStack={};
		Stack_init(&forStack,stack);

		Statement init_statement={};
		enum STATEMENT_PARSE_RESULT res=Statement_parse(&forStack,&init_statement,token_iter);
		if(res==STATEMENT_PARSE_RESULT_INVALID){
			out->forLoop.init=nullptr;
		}else{
			TokenIter_lastToken(token_iter,&token);
			out->forLoop.init=allocAndCopy(sizeof(Statement),&init_statement);
		}
		// TODO do this better
		Stack_addStatement(&forStack,&init_statement);

		Value condition={};
		enum VALUE_PARSE_RESULT valres=Value_parse(&forStack,&condition,token_iter);
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
		valres=Value_parse(&forStack,&post_expression,token_iter);
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

		Statement forBody={};
		res=Statement_parse(&forStack,&forBody,token_iter);
		TokenIter_lastToken(token_iter,&token);
		if(res==STATEMENT_PARSE_RESULT_INVALID){
			fatal("invalid statement in if body at line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}

		out->forLoop.stack=allocAndCopy(sizeof(Stack),&forStack);

		goto STATEMENT_PARSE_RET_SUCCESS;
	}

	if(Token_equalString(&token,"return")){
		TokenIter_nextToken(token_iter,&token);

		Value returnValue={};
		*out=(Statement){.tag=STATEMENT_KIND_RETURN};

		enum VALUE_PARSE_RESULT res=Value_parse(stack,&returnValue,token_iter);
		TokenIter_lastToken(token_iter,&token);
		switch(res){
			case VALUE_SYMBOL_UNKNOWN:
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
		*out=(Statement){.tag=STATEMENT_KIND_BREAK};
		if(!Token_equalString(&token,";")){
			fatal("expected semicolon after break statement: line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_nextToken(token_iter,&token);
		goto STATEMENT_PARSE_RET_SUCCESS;
	}
	if(Token_equalString(&token,"continue")){
		TokenIter_nextToken(token_iter,&token);
		*out=(Statement){.tag=STATEMENT_KIND_CONTINUE};
		if(!Token_equalString(&token,";")){
			fatal("expected semicolon after continue statement: line %d col %d %.*s",token.line,token.col,token.len,token.p);
		}
		TokenIter_nextToken(token_iter,&token);
		goto STATEMENT_PARSE_RET_SUCCESS;
	}
	if(Token_equalString(&token,"goto")){
		TokenIter_nextToken(token_iter,&token);
		Value label={};
		enum VALUE_PARSE_RESULT res=Value_parse(stack,&label,token_iter);
		switch(res){
			case VALUE_INVALID:
			case VALUE_SYMBOL_UNKNOWN:{
				// just check if next token is a valid identifier
				TokenIter_lastToken(token_iter,&token);
				if(!Token_isValidIdentifier(&token)){
					fatal("expected identifier after goto statement at %s",Token_print(&token));
				}

				Token gotoLabel=token;
				TokenIter_nextToken(token_iter,&token);	

				if(!Token_equalString(&token,KEYWORD_SEMICOLON)){
					fatal("expected semicolon after goto statement at %s",Token_print(&token));
				}
				TokenIter_nextToken(token_iter,&token);

				*out=(Statement){
					.tag=STATEMENT_KIND_GOTO,
					.goto_={
						.variant=VALUE_GOTO_LABEL_VARIANT_LABEL,
						.label=COPY_(&gotoLabel)
					}
				};
				break;
			}
			case VALUE_PRESENT:{
				TokenIter_lastToken(token_iter,&token);
				if(!Token_equalString(&token,";")){
					fatal("expected semicolon after goto statement: line %d col %d %.*s",token.line,token.col,token.len,token.p);
				}
				TokenIter_nextToken(token_iter,&token);

				*out=(Statement){
					.tag=STATEMENT_KIND_GOTO,
					.goto_={
						.variant=VALUE_GOTO_LABEL_VARIANT_VALUE,
						.label=COPY_(&label)
					}
				};
			}
		}

		goto STATEMENT_PARSE_RET_SUCCESS;
	}
	if(Token_equalString(&token,"switch")){
		TokenIter_nextToken(token_iter,&token);
		*out=(Statement){.tag=STATEMENT_KIND_SWITCH,.switch_={}};

		// check for (
		if(!Token_equalString(&token,"(")){
			fatal("expected opening parenthesis after switch");
		}
		TokenIter_nextToken(token_iter,&token);

		Value switchValue={};
		enum VALUE_PARSE_RESULT res=Value_parse(stack,&switchValue,token_iter);
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
			enum STATEMENT_PARSE_RESULT res=Statement_parse(stack,&bodyStatement,token_iter);
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
		struct SymbolDefinition *symbols=nullptr;
		enum SYMBOL_PARSE_RESULT symbolParseResult=SymbolDefinition_parse(stack,&numSymbols,&symbols,token_iter,&(struct Symbol_parse_options){.allow_initializers=true});
		if(symbolParseResult==SYMBOL_INVALID){
			break;
		}
		TokenIter_lastToken(token_iter,&token);

		if(numSymbols<1)fatal("declaration does not declare anything at %s",Token_print(&token));
		/* first symbol to check for function definition*/
		Symbol symbol=symbols[0].symbol;

		// parse function definition
		if(numSymbols==1 && symbol.type->kind==TYPE_KIND_FUNCTION && Token_equalString(&token,"{")){
			// TODO solve this better
			// add function symbol to stack before parsing the body
			Stack_addSymol(stack, &symbol);

			Statement statement={};

			TokenIter_nextToken(token_iter,&token);

			statement.tag=STATEMENT_KIND_FUNCTION_DEFINITION;
			statement.functionDef.symbol=symbol;

			Stack functionStack={};
			Stack_init(&functionStack,stack);

			// add function arguments as symbols to functionStack
			for(int i=0;i<symbol.type->function.args.len;i++){
				Symbol *argSymbol=array_get(&symbol.type->function.args,i);
				Stack_addSymol(&functionStack, argSymbol);
			}

			bool stopParsingFunctionBody=false;
			while(!stopParsingFunctionBody){
				if(Token_equalString(&token,"}")){
					TokenIter_nextToken(token_iter,&token);
					break;
				}

				Statement functionBodyStatement={};
				enum STATEMENT_PARSE_RESULT res=Statement_parse(&functionStack,&functionBodyStatement,token_iter);
				TokenIter_lastToken(token_iter,&token);

				switch(res){
					case STATEMENT_PARSE_RESULT_INVALID:
						stopParsingFunctionBody=true;
						break;
					case STATEMENT_PARSE_RESULT_PRESENT:
						Stack_addStatement(&functionStack, &functionBodyStatement);
						break;
				}
			}

			statement.functionDef.stack=allocAndCopy(sizeof(Stack),&functionStack);

			*out=statement;
			goto STATEMENT_PARSE_RET_SUCCESS;
		}

		// add symbol definitions
		Statement statement={
			.tag=STATEMENT_KIND_SYMBOL_DEFINITION,
			.symbolDef={
				.symbols_defs={},
			},
		};
		array_init(&statement.symbolDef.symbols_defs,sizeof(struct SymbolDefinition));
		for(int i=0;i<numSymbols;i++){
			array_append(&statement.symbolDef.symbols_defs,&symbols[i]);
		}

		if(!Token_equalString(&token,";")){
			fatal("expected semicolon after declaration but got instead: %s",Token_print(&token));
		}
		TokenIter_nextToken(token_iter,&token);

		*out=statement;
		goto STATEMENT_PARSE_RET_SUCCESS;
	}while(0);

	// parse a value
	do{
		Value value;
		enum VALUE_PARSE_RESULT res=Value_parse(stack,&value,token_iter);
		TokenIter_lastToken(token_iter,&token);
		if(res!=VALUE_PRESENT){
			// if next token is a valid identifier, might be goto label
			if(Token_isValidIdentifier(&token)){
				struct TokenIter beforeTokenCheck=*token_iter;
				TokenIter_nextToken(token_iter,&token);
				if(Token_equalString(&token,":")){
					*out=(Statement){
						.tag=STATEMENT_KIND_LABEL,
						.labelDefinition={
							.label=COPY_(&token)
						}
					};
					TokenIter_nextToken(token_iter,&token);
					goto STATEMENT_PARSE_RET_SUCCESS;
				}
				*token_iter=beforeTokenCheck;
			}
			break;
		}

		out->tag=STATEMENT_KIND_VALUE;
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

	*token_iter_in=*token_iter;
	return STATEMENT_PARSE_RESULT_PRESENT;
}
bool Statement_equal(Statement*a,Statement*b){
	if(a->tag!=b->tag){
		println("tag mismatch when comparing statements %d %d",a->tag,b->tag);
		return false;
	}

	switch(a->tag){
		case STATEMENT_KIND_FUNCTION_DEFINITION:{
			println("comparing function definitions");
			if(!Symbol_equal(&a->functionDef.symbol,&b->functionDef.symbol)){
				println("symbol mismatch");
				return false;
			}

			if(a->functionDef.stack->statements.len!=b->functionDef.stack->statements.len){
				println("body statement count mismatch %d %d",a->functionDef.stack->statements.len,b->functionDef.stack->statements.len);
				return false;
			}

			for(int i=0;i<a->functionDef.stack->statements.len;i++){
				Statement*sa=array_get(&a->functionDef.stack->statements,i);
				Statement*sb=array_get(&b->functionDef.stack->statements,i);
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
		case STATEMENT_KIND_UNKNOWN:
			return("STATEMENT_UNKNOWN");
		case STATEMENT_KIND_FUNCTION_DEFINITION:
			return("STATEMENT_FUNCTION_DEFINITION");
		case STATEMENT_KIND_RETURN:
			return("STATEMENT_KIND_RETURN");
		case STATEMENT_KIND_IF:
			return("STATEMENT_IF");
		case STATEMENT_KIND_SWITCH:
			return("STATEMENT_SWITCH");
		case STATEMENT_KIND_SWITCHCASE:
			return("STATEMENT_SWITCHCASE");
		case STATEMENT_KIND_BREAK:
			return("STATEMENT_BREAK");
		case STATEMENT_KIND_CONTINUE:
			return("STATEMENT_CONTINUE");
		case STATEMENT_KIND_DEFAULT:
			return("STATEMENT_DEFAULT");
		case STATEMENT_KIND_GOTO:
			return("STATEMENT_GOTO");
		case STATEMENT_KIND_LABEL:
			return("STATEMENT_LABEL");
		case STATEMENT_KIND_WHILE:
			return("STATEMENT_KIND_WHILE");
		case STATEMENT_KIND_FOR:
			return("STATEMENT_KIND_FOR");
		case STATEMENT_KIND_SYMBOL_DEFINITION:
			return("STATEMENT_KIND_SYMBOL_DEFINITION");
		case STATEMENT_KIND_VALUE:
			return("STATEMENT_VALUE");
		case STATEMENT_KIND_BLOCK:
			return("STATEMENT_BLOCK");
		case STATEMENT_KIND_EMPTY:
			return("STATEMENT_EMPTY");
		case STATEMENT_KIND_TYPEDEF:
			return("STATEMENT_TYPEDEF");

		default: fatal("unknown statement kind %d",kind);
	}
}
