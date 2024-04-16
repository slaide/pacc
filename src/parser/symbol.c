#include<parser/parser.h>
#include<util/util.h>
#include<tokenizer.h>

enum SYMBOL_PARSE_RESULT Symbol_parse(Symbol*symbol,struct TokenIter*token_iter_in){
	struct TokenIter this_iter=*token_iter_in;
	struct TokenIter *token_iter=&this_iter;

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
				return SYMBOL_INVALID;
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
			fatal("arrays not yet implemented");
		}

		break;
	}

	*token_iter_in=this_iter;

	return SYMBOL_PRESENT;
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
