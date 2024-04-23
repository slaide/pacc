#include "parser/symbol.h"
#include<parser/parser.h>
#include<util/util.h>
#include<tokenizer.h>

enum SYMBOL_PARSE_RESULT Symbol_parse(Symbol*symbol,struct TokenIter*token_iter_in){
	struct TokenIter this_iter=*token_iter_in;
	struct TokenIter *token_iter=&this_iter;

	*symbol=(Symbol){};

	// currently, nothing else is possible (rest is unimplemented)
	//symbol->kind=SYMBOL_KIND_DECLARATION;

	Token token;
	TokenIter_lastToken(token_iter,&token);

	// check if type has prefix: const
	bool type_is_const=Token_equalString(&token,"const");
	if(type_is_const){
		TokenIter_nextToken(token_iter,&token);
	}

	// check if type has prefix: struct,enum,union
	bool type_is_struct=Token_equalString(&token,"struct");
	bool type_is_enum=Token_equalString(&token,"enum");
	bool type_is_union=Token_equalString(&token,"union");
	bool possible_type_definition=type_is_struct||type_is_enum||type_is_union;
	Token nameToken=token;
	if(possible_type_definition){
		TokenIter_nextToken(token_iter,&token);
		nameToken=token;
	}

	// verify name of type is valid
	bool tokenIsValidIdentifier=Token_isValidIdentifier(&token);
	if(!tokenIsValidIdentifier && !possible_type_definition){
		goto SYMBOL_PARSE_RET_FAILURE;
	}
	bool possibleUnnamedTypeDefinition=!tokenIsValidIdentifier&&possible_type_definition;

	Type_init(&symbol->type);

	if(tokenIsValidIdentifier){
		*symbol->type=(Type){
			.kind=TYPE_KIND_REFERENCE,
			.is_const=type_is_const,
			.reference={
				.name=nameToken,
				.is_struct=type_is_struct,
				.is_enum=type_is_enum,
				.is_union=type_is_union,
			}
		};

		// skip over name token
		TokenIter_nextToken(token_iter,&token);
	}

	while(1){
		if(possible_type_definition && Token_equalString(&token,"{")){
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
				*(symbol->type)=(Type){
					.kind=TYPE_KIND_STRUCT,
					.struct_={
						.name=nullptr,
						.members=structMembers,
					},
				};
				if(tokenIsValidIdentifier)
					symbol->type->struct_.name=allocAndCopy(sizeof(Token),&nameToken);
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
				*(symbol->type)=(Type){
					.kind=TYPE_KIND_UNION,
					.union_={
						.name=nullptr,
						.members=unionMembers,
					},
				};
				if(tokenIsValidIdentifier)
					symbol->type->union_.name=allocAndCopy(sizeof(Token),&nameToken);
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
				*(symbol->type)=(Type){
					.kind=TYPE_KIND_ENUM,
					.enum_={
						.name=nullptr,
						.members=enumMembers,
					},
				};
				if(tokenIsValidIdentifier)
					symbol->type->struct_.name=allocAndCopy(sizeof(Token),&nameToken);
				goto SYMBOL_PARSE_RET_SUCCESS;
			}
			fatal("unreachable");
		}
		if(possibleUnnamedTypeDefinition)
			goto SYMBOL_PARSE_RET_FAILURE;

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
				goto SYMBOL_PARSE_RET_NONAME;
			}
			symbol->name=allocAndCopy(sizeof(Token),&token);
			TokenIter_nextToken(token_iter,&token);
		}

		// check for function
		if(Token_equalString(&token,"(")){
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

				array_append(&args,&argument);
			}
			symbol->type->function.args=args;

			break;
		}

		if(Token_equalString(&token,"[")){
			TokenIter_nextToken(token_iter,&token);

			Value array_len={};
			enum VALUE_PARSE_RESULT res=Value_parse(&array_len,token_iter);
			// if there is a value: dynamic array
			// otherwise, value with some size (may be static or runtime determined)
			if(res==VALUE_PRESENT){
				symbol->type=allocAndCopy(sizeof(Type),&(Type){
					.kind=TYPE_KIND_ARRAY,
					.array={
						.base=symbol->type,
						.len=allocAndCopy(sizeof(Value), &array_len),
					},
				});
			}else{
				symbol->type=allocAndCopy(sizeof(Type),&(Type){
					.kind=TYPE_KIND_ARRAY,
					.array={
						.base=symbol->type,
					},
				});
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
	*token_iter_in=this_iter;
	return SYMBOL_PRESENT;

SYMBOL_PARSE_RET_NONAME:
	*token_iter_in=this_iter;
	return SYMBOL_WITHOUT_NAME;

SYMBOL_PARSE_RET_FAILURE:
	return SYMBOL_INVALID;
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
