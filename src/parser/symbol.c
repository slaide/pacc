#include "parser/symbol.h"
#include<parser/parser.h>
#include<util/util.h>
#include<tokenizer.h>

enum SYMBOL_PARSE_RESULT Symbol_parse(Symbol*symbol,struct TokenIter*token_iter_in){
	struct TokenIter this_iter=*token_iter_in;
	struct TokenIter *token_iter=&this_iter;

	*symbol=(Symbol){};

	Type type={.kind=TYPE_KIND_UNKNOWN};

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

					type.kind=TYPE_KIND_STRUCT;
					type.struct_.name=possibleUnnamedTypeDefinition?nullptr:allocAndCopy(sizeof(Token),&nameToken);
					type.struct_.members=structMembers;
				}
				else if(type_is_union){
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

					type.kind=TYPE_KIND_UNION;
					type.union_.name=possibleUnnamedTypeDefinition?nullptr:allocAndCopy(sizeof(Token),&nameToken);
					type.union_.members=unionMembers;
				}
				else if(type_is_enum){
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

					type.kind=TYPE_KIND_ENUM;
					type.enum_.name=possibleUnnamedTypeDefinition?nullptr:allocAndCopy(sizeof(Token),&nameToken);
					type.enum_.members=enumMembers;
				}
				else fatal("");
				
				continue;
			}else{
				type.kind=TYPE_KIND_REFERENCE;

				// go to name token
				goto SYMBOL_PARSE_HANDLE_NAME_TOKEN;
			}
		}

SYMBOL_PARSE_HANDLE_NAME_TOKEN:

		if(Token_isValidIdentifier(&token)||possible_type_definition){
			if(type.kind==TYPE_KIND_UNKNOWN){
				type.kind=TYPE_KIND_REFERENCE;
				type.reference.name=token;
				TokenIter_nextToken(token_iter,&token);
			}else if(possible_type_definition){
				type.reference.name=nameToken;
				type.reference.is_struct=true;
			}else if(symbol->name==nullptr){
				symbol->name=allocAndCopy(sizeof(Token),&token);
				TokenIter_nextToken(token_iter,&token);
			}else{
				goto SYMBOL_PARSE_RET_FAILURE;
			}

			continue;
		}
		// if struct/enum/union is not followed by { or a name, it is invalid, e.g. "struct;" is invalid
		if(possible_type_definition) goto SYMBOL_PARSE_RET_FAILURE;

		// check for pointer
		if(type.kind!=TYPE_KIND_UNKNOWN && Token_equalString(&token,"*")){
			TokenIter_nextToken(token_iter,&token);

			type=(Type){
				.kind=TYPE_KIND_POINTER,
				.pointer={
					.base=allocAndCopy(sizeof(Type),&type),
				},
			};

			continue;
		}

		// function declaration: int a(); <- easy to parse
		// function pointer: int (*b)(); <- tricky to parse
		// check for function
		if(symbol->name!=nullptr && Token_equalString(&token,"(")){
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
				enum SYMBOL_PARSE_RESULT symres=Symbol_parse(&argument,token_iter);
				while(symres==SYMBOL_INVALID){
					// check for varargs
					if(Token_equalString(&token,"...")){
						argument.type=allocAndCopy(sizeof(Type),&(Type){
							.kind=TYPE_KIND_REFERENCE,
							.reference={.name=(Token){.len=3,.p="...",}},
						});
						TokenIter_nextToken(token_iter,&token);

						break;
					}
					goto SYMBOL_PARSE_RET_FAILURE;
				}
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

		if(type.kind!=TYPE_KIND_UNKNOWN && Token_equalString(&token,"[")){
			TokenIter_nextToken(token_iter,&token);

			// test for static
			bool typeLenIsStatic=false;
			if(Token_equalString(&token,"static")){
				TokenIter_nextToken(token_iter,&token);
				typeLenIsStatic=true;
			}

			Value array_len={};
			enum VALUE_PARSE_RESULT res=Value_parse(&array_len,token_iter);
			// if there is a value: dynamic array
			// otherwise, value with some size (may be static or runtime determined)
			Value*arrayLen=nullptr;
			if(res==VALUE_PRESENT){
				arrayLen=allocAndCopy(sizeof(Value),&array_len);
				TokenIter_lastToken(token_iter,&token);
			}else{
				if(typeLenIsStatic){
					goto SYMBOL_PARSE_RET_FAILURE;
				}
			}

			type=(Type){
				.kind=TYPE_KIND_ARRAY,
				.array={
					.base=allocAndCopy(sizeof(Type),&type),
					.len=arrayLen,
					.is_static=typeLenIsStatic,
				},
			};

			// check for trailing ]
			if(!Token_equalString(&token,"]")){
				goto SYMBOL_PARSE_RET_FAILURE;
			}
			TokenIter_nextToken(token_iter,&token);

			continue;
		}

		break;
	}

	if(type.kind==TYPE_KIND_UNKNOWN){
		goto SYMBOL_PARSE_RET_FAILURE;
	}

	if(symbol->name==nullptr){
		goto SYMBOL_PARSE_RET_NONAME;
	}else{
		goto SYMBOL_PARSE_RET_SUCCESS;
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
