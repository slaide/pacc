#include<parser.h>
#include<util/util.h>
#include<tokenizer.h>
#include<ctype.h> // isalpha, isalnum


const char* Symbolkind_asString(enum SYMBOLKIND kind){
	switch(kind){
		case SYMBOL_KIND_UNKNOWN:
			return("SYMBOL_KIND_UNKNOWN");
		case SYMBOL_KIND_DECLARATION:
			return("SYMBOL_KIND_DECLARATION");
			break;
		case SYMBOL_KIND_REFERENCE:
			return("SYMBOL_KIND_REFERENCE");
		default:
			fatal("unknown symbol kind %d",kind);
	}
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
		case STATEMENT_RETURN:
			return("STATEMENT_RETURN");
		case STATEMENT_IF:
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
		case STATEMENT_WHILE:
			return("STATEMENT_WHILE");
		case STATEMENT_FOR:
			return("STATEMENT_FOR");
		default:
			fatal("unknown symbol kind %d",kind);
	}

}
const char* ValueKind_asString(enum VALUE_KIND kind){
	switch(kind){
		case VALUE_KIND_STATIC_VALUE:
			return("VALUE_KIND_STATIC_VALUE");
		case VALUE_KIND_OPERATOR:
			return("VALUE_KIND_STATIC_VALUE");
		case VALUE_KIND_SYMBOL_REFERENCE:
			return("VALUE_KIND_STATIC_VALUE");
		default:
			fatal("unknown value kind %d",kind);
	}

}

void Type_init(Type**type){
	*type=allocAndCopy(sizeof(Type),&(Type){});
}

void Type_print(Type* type){
	Type* type_ref=type;
	bool printing_done=false;
	while(!printing_done){
		if(type_ref->is_const)
			println("const ");

		switch(type_ref->kind){
			case TYPE_KIND_REFERENCE:
				println("referencing type %.*s",type_ref->reference.len,type_ref->reference.p);
				printing_done=true;
				break;
			case TYPE_KIND_POINTER:
				println("pointer to ");
				if(type_ref->pointer.base==type_ref)
					fatal("pointer to self");
				type_ref=type_ref->pointer.base;
				break;
			case TYPE_KIND_ARRAY:
				println("array");
				type_ref=type_ref->array.base;
				break;
			case TYPE_KIND_FUNCTION:
				println("function");
				println("return type is");
				Type_print(type_ref->function.ret);

				for(int i=0;i<type_ref->function.args.len;i++){
					Symbol* arg=array_get(&type_ref->function.args,i);
					println("argument #%d called %.*s is of type",i,arg->name->len,arg->name->p);
					Type_print(arg->type);
				}
				printing_done=true;
				break;
			default:
				fatal("unknown type kind %d",type_ref->kind);
		}
	}
}

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

enum SYMBOL_PARSE_RESULT Symbol_parse(Symbol*symbol,struct TokenIter*token_iter){
	*symbol=(Symbol){};
	Type_init(&symbol->type);

	// currently, nothing else is possible (rest is unimplemented)
	symbol->kind=SYMBOL_KIND_DECLARATION;

	Token token;
	TokenIter_lastToken(token_iter,&token);

	if(Token_equalString(&token,"const")){
		symbol->type->is_const=true;
		TokenIter_nextToken(token_iter,&token);
	}

	// verify name of type is valid
	if(!Token_isValidIdentifier(&token))
		fatal("expected valid identifier at line %d col %d but got instead %.*s",token.line,token.col,token.len,token.p);

	symbol->type->kind=TYPE_KIND_REFERENCE;
	symbol->type->reference=token;
	TokenIter_nextToken(token_iter,&token);

	while(1){
		if(Token_equalString(&token,"*")){
			TokenIter_nextToken(token_iter,&token);

			Type*base_type=symbol->type;

			Type_init(&symbol->type);
			symbol->type->kind=TYPE_KIND_POINTER;
			symbol->type->pointer.base=base_type;

			continue;
		}

		if(symbol->name==nullptr){
			// verify name of symbol is valid
			if(!Token_isValidIdentifier(&token)){
				fatal("expected valid identifier at line %d col %d but got instead %.*s",token.line,token.col,token.len,token.p);
			}
			symbol->name=allocAndCopy(sizeof(Token),&token);
			TokenIter_nextToken(token_iter,&token);
		}

		// check for function
		if(Token_equalString(&token,"(")){
			println("function %.*s",symbol->name->len,symbol->name->p);

			Type* ret=symbol->type;
			Type_init(&symbol->type);

			symbol->type->kind=TYPE_KIND_FUNCTION;
			symbol->type->function.ret=ret;
			TokenIter_nextToken(token_iter,&token);
			if(Token_equalString(&token,")")){
				TokenIter_nextToken(token_iter,&token);
				break;
			}
			
			// parse function arguments
			array args;
			array_init(&args,sizeof(Symbol));
			while(1){
				if(Token_equalString(&token,")")){
					TokenIter_nextToken(token_iter,&token);
					break;
				}
				if(Token_equalString(&token,",")){
					TokenIter_nextToken(token_iter,&token);
					continue;
				}

				Symbol argument={};
				Symbol_parse(&argument,token_iter);
				TokenIter_lastToken(token_iter,&token);
				println("argument name %.*s",argument.name->len,argument.name->p);
				Type_print(argument.type);

				array_append(&args,&argument);
			}
			symbol->type->function.args=args;

			break;
		}

		if(Token_equalString(&token,"[")){
			fatal("arrays not yet implemented");
		}

		break;
	}

	return SYMBOL_PRESENT;
}

enum VALUE_PARSE_RESULT{
	/// @brief  value was parsed successfully
	VALUE_PRESENT,
	/// @brief  value was not parsed successfully (e.g. syntax error)
	VALUE_INVALID,
};
enum VALUE_PARSE_RESULT Value_parse(Value*value,struct TokenIter*token_iter){
	Token token;
	TokenIter_lastToken(token_iter,&token);

	switch(token.tag){
		case TOKEN_TAG_LITERAL_CHAR:{
			println("got char value %.*s",token.len,token.p);
			TokenIter_nextToken(token_iter,&token);

			value->kind=VALUE_KIND_STATIC_VALUE;
			value->static_value.value_repr=allocAndCopy(sizeof(Token),&token);
			break;
		}
		case TOKEN_TAG_LITERAL_INTEGER:
		{
			println("got integer value %.*s",token.len,token.p);
			Token literalValueToken=token;
			TokenIter_nextToken(token_iter,&token);

			value->kind=VALUE_KIND_STATIC_VALUE;
			value->static_value.value_repr=allocAndCopy(sizeof(Token),&literalValueToken);
			break;
		}
		case TOKEN_TAG_LITERAL_STRING:
		{
			println("got string value %.*s",token.len,token.p);
			TokenIter_nextToken(token_iter,&token);

			value->kind=VALUE_KIND_STATIC_VALUE;
			value->static_value.value_repr=allocAndCopy(sizeof(Token),&token);
			break;
		}
		case TOKEN_TAG_SYMBOL:
		{
			println("got symbol value %.*s",token.len,token.p);
			TokenIter_nextToken(token_iter,&token);

			value->kind=VALUE_KIND_SYMBOL_REFERENCE;

			break;
		}

		default:
			return VALUE_INVALID;
	}

	// check for operators
	TokenIter_lastToken(token_iter,&token);
	enum VALUE_OPERATOR op;
	bool requiresSecondOperand=false;
	if(Token_equalString(&token,"+")){
		op=VALUE_OPERATOR_ADD;
		requiresSecondOperand=true;
	}else if(Token_equalString(&token,"-")){
		op=VALUE_OPERATOR_SUB;
		requiresSecondOperand=true;
	}else if(Token_equalString(&token,"*")){
		op=VALUE_OPERATOR_MULT;
		requiresSecondOperand=true;
	}else if(Token_equalString(&token,"/")){
		op=VALUE_OPERATOR_DIV;
		requiresSecondOperand=true;
	}else if(Token_equalString(&token,"<")){
		op=VALUE_OPERATOR_LESS_THAN;
		requiresSecondOperand=true;
	}else if(Token_equalString(&token,">")){
		op=VALUE_OPERATOR_GREATER_THAN;
		requiresSecondOperand=true;
	}else if(Token_equalString(&token,"++")){
		op=VALUE_OPERATOR_POSTFIX_INCREMENT;
		requiresSecondOperand=false;
	}else if(Token_equalString(&token,"--")){
		op=VALUE_OPERATOR_POSTFIX_DECREMENT;
		requiresSecondOperand=false;
	}else if(Token_equalString(&token,".")){
		op=VALUE_OPERATOR_DOT;
	}else if(Token_equalString(&token,"->")){
		op=VALUE_OPERATOR_ARROW;
	}else if(Token_equalString(&token,"(")){
		op=VALUE_OPERATOR_CALL;
	}
	else{
		return VALUE_PRESENT;
	}
	TokenIter_nextToken(token_iter,&token);

	switch(op){
		case VALUE_OPERATOR_DOT:
			fatal("unimplemented");
		case VALUE_OPERATOR_ARROW:
			fatal("unimplemented");
		case VALUE_OPERATOR_CALL:{
			array values;
			array_init(&values,sizeof(Value));
			while(1){
				if(Token_equalString(&token,")")){
					TokenIter_nextToken(token_iter,&token);
					break;
				}
				if(Token_equalString(&token,",")){
					TokenIter_nextToken(token_iter,&token);
					continue;
				}

				Value arg={};
				enum VALUE_PARSE_RESULT res=Value_parse(&arg,token_iter);
				TokenIter_lastToken(token_iter,&token);
				switch(res){
					case VALUE_INVALID:
						fatal("invalid value in function call");
						break;
					case VALUE_PRESENT:
						println("got value in function call");
						array_append(&values,&arg);
						break;
				}
			}
			println("found function call with %d arguments",values.len);
			break;
		}
		default:{
			Value ret={
				.kind=VALUE_KIND_OPERATOR,
				.op={
					.left=allocAndCopy(sizeof(Value),value),
					.op=op,
				}
			};

			if(requiresSecondOperand){
				ret.op.right=malloc(sizeof(Value));

				enum VALUE_PARSE_RESULT res=Value_parse(ret.op.right,token_iter);
				TokenIter_lastToken(token_iter,&token);
				switch(res){
					case VALUE_INVALID:
						fatal("invalid value after operator");
						break;
					case VALUE_PRESENT:
						println("got value after operator");
						break;
				}
			}

			*value=ret;
			return VALUE_PRESENT;
		}
	}
	fatal("");
}

enum STATEMENT_PARSE_RESULT{
	/// @brief statement was parsed successfully
	STATEMENT_PRESENT,
	/// @brief statement was not parsed successfully (e.g. syntax error)
	STATEMENT_INVALID,
};
enum STATEMENT_PARSE_RESULT Statement_parse(Statement*out,struct TokenIter*token_iter);
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
	{
		Symbol symbol={};
		enum SYMBOL_PARSE_RESULT symbolParseResult=Symbol_parse(&symbol,token_iter);
		if(symbolParseResult==STATEMENT_INVALID){
			fatal("invalid symbol in statement. TODO parse value instead");
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
	}

	return STATEMENT_INVALID;
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
