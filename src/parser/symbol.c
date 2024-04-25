#include<parser/parser.h>
#include<util/util.h>
#include<tokenizer.h>

enum SYMBOL_PARSE_RESULT Symbol_parse(Symbol*symbol,struct TokenIter*token_iter_in){
	struct TokenIter this_iter=*token_iter_in;
	struct TokenIter *token_iter=&this_iter;

	*symbol=(Symbol){};

	Type type={};

	// currently, nothing else is possible (rest is unimplemented)
	//symbol->kind=SYMBOL_KIND_DECLARATION;

	Token token;
	TokenIter_lastToken(token_iter,&token);

	while(1){
		bool type_is_struct=false;
		bool type_is_enum=false;
		bool type_is_union=false;
		bool possible_type_definition=false;
		bool possibleUnnamedTypeDefinition=false;

		if(Token_equalString(&token,"const")){
			TokenIter_nextToken(token_iter,&token);
			type.is_const=true;
			continue;
		}
		if(Token_equalString(&token,"static")){
			TokenIter_nextToken(token_iter,&token);
			type.is_static=true;
			continue;
		}
		if(Token_equalString(&token,"thread_local")){
			TokenIter_nextToken(token_iter,&token);
			type.is_thread_local=true;
			continue;
		}

		Token nameToken=token;

		// check if type has prefix: struct,enum,union
		if(Token_equalString(&token,"struct"))
			type_is_struct=true;
		if(Token_equalString(&token,"enum"))
			type_is_enum=true;
		if(Token_equalString(&token,"union"))
			type_is_union=true;
		possible_type_definition=type_is_struct||type_is_enum||type_is_union;
		if(possible_type_definition){
			TokenIter_nextToken(token_iter,&token);

			if(Token_isValidIdentifier(&token)){
				nameToken=token;
				possibleUnnamedTypeDefinition=false;
				TokenIter_nextToken(token_iter,&token);
			}else{
				nameToken=(Token){};
				possibleUnnamedTypeDefinition=true;
			}

			if(Token_equalString(&token,"{")){
				TokenIter_nextToken(token_iter,&token);

				if(type_is_struct){
					array structMembers={};
					array_init(&structMembers,sizeof(Symbol));

					// parse struct
					while(!Token_equalString(&token,"}")){
						Symbol member={};
						enum SYMBOL_PARSE_RESULT symres=Symbol_parse(&member,token_iter);
						if(symres==SYMBOL_INVALID){
							fatal("invalid symbol in struct at line %d col %d %.*s",token.line,token.col,token.len,token.p);
						}
						array_append(&structMembers,&member);
						TokenIter_lastToken(token_iter,&token);
						
						// check for semicolon
						if(!Token_equalString(&token,";")){
							fatal("expected semicolon after member declaration");
						}
						TokenIter_nextToken(token_iter,&token);
					}
					TokenIter_nextToken(token_iter,&token);

					symbol->name=nullptr;
					type.kind=TYPE_KIND_STRUCT;
					type.struct_.name=possibleUnnamedTypeDefinition?nullptr:allocAndCopy(sizeof(Token),&nameToken);
					type.struct_.members=structMembers;

					goto SYMBOL_PARSE_RET_SUCCESS;
				}
				if(type_is_union){
					array unionMembers={};
					array_init(&unionMembers,sizeof(Symbol));

					// parse union
					while(!Token_equalString(&token,"}")){
						Symbol member={};
						enum SYMBOL_PARSE_RESULT symres=Symbol_parse(&member,token_iter);
						if(symres==SYMBOL_INVALID){
							fatal("invalid symbol in union");
						}
						array_append(&unionMembers,&member);
						TokenIter_lastToken(token_iter,&token);
						
						// check for semicolon
						if(!Token_equalString(&token,";")){
							fatal("expected semicolon after member declaration");
						}
						TokenIter_nextToken(token_iter,&token);
					}
					TokenIter_nextToken(token_iter,&token);

					symbol->name=nullptr;
					type.kind=TYPE_KIND_UNION;
					type.union_.name=possibleUnnamedTypeDefinition?nullptr:allocAndCopy(sizeof(Token),&nameToken);
					type.union_.members=unionMembers;

					goto SYMBOL_PARSE_RET_SUCCESS;
				}
				if(type_is_enum){
					array enumMembers={};
					array_init(&enumMembers,sizeof(Value));

					// parse enum
					while(!Token_equalString(&token,"}")){
						Value member={};
						enum VALUE_PARSE_RESULT valres=Value_parse(&member,token_iter);
						if(valres==VALUE_INVALID){
							fatal("invalid value in enum");
						}
						array_append(&enumMembers,&member);
						TokenIter_lastToken(token_iter,&token);
						
						// check for comma
						if(!Token_equalString(&token,",")){
							break;
						}
						TokenIter_nextToken(token_iter,&token);
					}
					TokenIter_nextToken(token_iter,&token);

					symbol->name=nullptr;
					type=(Type){
						.kind=TYPE_KIND_ENUM,
						.enum_={
							.name=possibleUnnamedTypeDefinition?nullptr:allocAndCopy(sizeof(Token),&nameToken),
							.members=enumMembers,
						},
					};

					goto SYMBOL_PARSE_RET_SUCCESS;
				}
				
				continue;
			}else{
				type.kind=TYPE_KIND_REFERENCE;

				// go to name token
				goto SYMBOL_PARSE_HANDLE_NAME_TOKEN;
			}
		}

SYMBOL_PARSE_HANDLE_NAME_TOKEN:

		if(Token_isValidIdentifier(&token)||possible_type_definition){
			bool usedNameToken=false;
			if(type.kind==TYPE_KIND_UNKNOWN){
				type.kind=TYPE_KIND_REFERENCE;
				type.reference.name=token;
				usedNameToken=true;
				TokenIter_nextToken(token_iter,&token);
			}else if(possible_type_definition){
				if(type_is_struct){
					type.struct_.name=allocAndCopy(sizeof(Token),&nameToken);
					type.reference.is_struct=true;
				}
				if(type_is_enum){
					type.enum_.name=allocAndCopy(sizeof(Token),&nameToken);
					type.reference.is_enum=true;
				}
				if(type_is_union){
					type.union_.name=allocAndCopy(sizeof(Token),&nameToken);
					type.reference.is_union=true;
				}
				usedNameToken=true;
			}

			if(usedNameToken){
				continue;
			}
		}
		// if struct/enum/union is not followed by { or a name, it is invalid, e.g. "struct;" is invalid
		if(possible_type_definition) goto SYMBOL_PARSE_RET_FAILURE;

		if(Token_equalString(&token,"*")){
			TokenIter_nextToken(token_iter,&token);

			type=(Type){
				.kind=TYPE_KIND_POINTER,
				.pointer={
					.base=allocAndCopy(sizeof(Type),&type),
				},
			};

			continue;
		}

		if(symbol->name==nullptr){
			// verify name of symbol is valid
			if(!Token_isValidIdentifier(&token)){
				goto SYMBOL_PARSE_RET_NONAME;
			}
			symbol->name=allocAndCopy(sizeof(Token),&token);
			TokenIter_nextToken(token_iter,&token);

			continue;
		}

		// check for function
		if(Token_equalString(&token,"(")){
			TokenIter_nextToken(token_iter,&token);
			
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

				array_append(&args,&argument);
			}
			type=(Type){
				.kind=TYPE_KIND_FUNCTION,
				.function={
					.ret=allocAndCopy(sizeof(Type), &type),
					.args=args,
				},
			};

			break;
		}

		if(Token_equalString(&token,"[")){
			TokenIter_nextToken(token_iter,&token);

			Value array_len={};
			enum VALUE_PARSE_RESULT res=Value_parse(&array_len,token_iter);
			// if there is a value: dynamic array
			// otherwise, value with some size (may be static or runtime determined)
			if(res==VALUE_PRESENT){
				type=(Type){
					.kind=TYPE_KIND_ARRAY,
					.array={
						.base=allocAndCopy(sizeof(Type),&type),
						.len=allocAndCopy(sizeof(Value), &array_len),
					},
				};
			}else{
				type=(Type){
					.kind=TYPE_KIND_ARRAY,
					.array={
						.base=allocAndCopy(sizeof(Type),&type),
					},
				};
			}
			// check for trailing ]
			TokenIter_lastToken(token_iter,&token);
			if(!Token_equalString(&token,"]")){
				goto SYMBOL_PARSE_RET_FAILURE;
			}
			TokenIter_nextToken(token_iter,&token);

			continue;
		}

		break;
	}

SYMBOL_PARSE_RET_SUCCESS:

	symbol->type=allocAndCopy(sizeof(Type), &type);

	*token_iter_in=this_iter;
	return SYMBOL_PRESENT;

SYMBOL_PARSE_RET_NONAME:

	symbol->type=allocAndCopy(sizeof(Type), &type);

	*token_iter_in=this_iter;
	return SYMBOL_WITHOUT_NAME;

SYMBOL_PARSE_RET_FAILURE:
	return SYMBOL_INVALID;
}

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

bool Symbol_equal(Symbol*a,Symbol*b){
	if(a->kind!=b->kind){
		println("kind mismatch when comparing symbols %d %d",a->kind,b->kind);
		return false;
	}

	if(!Token_equalToken(a->name,b->name)){
		println("name mismatch when comparing symbols %.*s %.*s",a->name->len,a->name->p,b->name->len,b->name->p);
		return false;
	}

	if(!Type_equal(a->type,b->type)){
		println("type mismatch when comparing symbols %.*s",a->name->len,a->name->p);
		return false;
	}

	return true;
}
