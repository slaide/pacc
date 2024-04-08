#include<parser/parser.h>
#include<util/util.h>
#include<tokenizer.h>


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

bool Value_equal(Value*a,Value*b){
	if(a->kind!=b->kind){
		println("kind mismatch when comparing values %s %s",ValueKind_asString(a->kind),ValueKind_asString(b->kind));
		return false;
	}

	switch(a->kind){
		case VALUE_KIND_STATIC_VALUE:{
			bool ret=Token_equalToken(a->static_value.value_repr,b->static_value.value_repr);
			if(!ret){
				println("value mismatch when comparing static values %.*s %.*s",a->static_value.value_repr->len,a->static_value.value_repr->p,b->static_value.value_repr->len,b->static_value.value_repr->p);
			}
			return ret;
		}
		default:
			fatal("unimplemented %d",a->kind);
	}
}
