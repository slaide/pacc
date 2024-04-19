#include "parser/value.h"
#include<parser/parser.h>
#include<util/util.h>
#include<tokenizer.h>


enum VALUE_PARSE_RESULT Value_parse(Value*value,struct TokenIter*token_iter){
	Token token;
	TokenIter_lastToken(token_iter,&token);
	/// used by some cases
	Token nameToken=token;

	switch(token.tag){
		case TOKEN_TAG_LITERAL_CHAR:{
			Token literalValueToken=token;
			TokenIter_nextToken(token_iter,&token);

			value->kind=VALUE_KIND_STATIC_VALUE;
			value->static_value.value_repr=allocAndCopy(sizeof(Token),&literalValueToken);
			break;
		}
		case TOKEN_TAG_LITERAL_INTEGER:
		{
			Token literalValueToken=token;
			TokenIter_nextToken(token_iter,&token);

			value->kind=VALUE_KIND_STATIC_VALUE;
			value->static_value.value_repr=allocAndCopy(sizeof(Token),&literalValueToken);
			break;
		}
		case TOKEN_TAG_LITERAL_STRING:
		{
			Token literalValueToken=token;
			TokenIter_nextToken(token_iter,&token);

			value->kind=VALUE_KIND_STATIC_VALUE;
			value->static_value.value_repr=allocAndCopy(sizeof(Token),&literalValueToken);
			break;
		}
		case TOKEN_TAG_SYMBOL:
		{
			TokenIter_nextToken(token_iter,&token);

			value->kind=VALUE_KIND_SYMBOL_REFERENCE;
			value->symbol=allocAndCopy(sizeof(Token),&nameToken);

			break;
		}
		case TOKEN_TAG_LITERAL_FLOAT:{
			Token literalValueToken=token;
			TokenIter_nextToken(token_iter,&token);

			// print message with token
			println("float literal %.*s at line %d col %d",token.len,token.p,token.line,token.col);

			value->kind=VALUE_KIND_STATIC_VALUE;
			value->static_value.value_repr=allocAndCopy(sizeof(Token),&literalValueToken);

			break;
		}

		case TOKEN_TAG_KEYWORD:{
			//Token_map(&token);

			if(token.p==KEYWORD_ASTERISK){
				fatal("unimplemented");
			}else if(token.p==KEYWORD_AMPERSAND){
				TokenIter_nextToken(token_iter,&token);

				Value addressedValue={};
				enum VALUE_PARSE_RESULT res=Value_parse(&addressedValue,token_iter);
				if(res==VALUE_INVALID){
					fatal("invalid value after &");
				}

				*value=(Value){
					.kind=VALUE_KIND_ADDRESS_OF,
					.addrOf.addressedValue=allocAndCopy(sizeof(Value), &addressedValue),
				};

				break;
			}else if(token.p==KEYWORD_PARENS_OPEN){
				TokenIter_nextToken(token_iter,&token);

				Value innerValue={};
				enum VALUE_PARSE_RESULT res=Value_parse(&innerValue,token_iter);
				if(res==VALUE_INVALID){
					fatal("invalid value after (");
				}
				TokenIter_lastToken(token_iter, &token);
				if(token.p!=KEYWORD_PARENS_CLOSE){
					fatal("expected ) after (");
				}
				TokenIter_nextToken(token_iter, &token);

				// the current value could now be
				// 1) operator precendence override, e.g. (a+b)*c
				// 2) value cast, e.g. (int)2.3f

				*value=(Value){
					.kind=VALUE_KIND_PARENS_WRAPPED,
					.parens_wrapped={
						.innerValue=allocAndCopy(sizeof(Value),&innerValue),
					}
				};

				break;
			}else if(token.p==KEYWORD_CURLY_BRACES_OPEN){
				// parse struct initializer
				TokenIter_nextToken(token_iter, &token);

				array structFields;
				array_init(&structFields,sizeof(struct StructFieldInitializer));
				while(!TokenIter_isEmpty(token_iter)){
					if (Token_equalString(&token, "}")){
						break;
					}

					struct StructFieldInitializer field={};

					// if next token is dot, parse field name followed by assignment symbol
					if(Token_equalString(&token,".")){
						TokenIter_nextToken(token_iter,&token);
						field.name=allocAndCopy(sizeof(Token),&token);
						TokenIter_nextToken(token_iter,&token);
						if(!Token_equalString(&token,"=")){
							fatal("expected = after field name");
						}
						TokenIter_nextToken(token_iter,&token);
					}

					Value fieldValue={};
					enum VALUE_PARSE_RESULT res=Value_parse(&fieldValue,token_iter);
					TokenIter_lastToken(token_iter, &token);
					if(res==VALUE_INVALID){
						fatal("invalid value in struct initializer");
					}
					field.value=allocAndCopy(sizeof(Value),&fieldValue);

					array_append(&structFields,&field);

					// check for comma
					if(Token_equalString(&token,",")){
						TokenIter_nextToken(token_iter,&token);
						continue;
					}

					break;
				}

				if (!Token_equalString(&token, "}")){
					fatal("expected } after struct initializer");	
				}
				TokenIter_nextToken(token_iter, &token);

				*value=(Value){
					.kind=VALUE_KIND_STRUCT_INITIALIZER,
					.struct_initializer={
						.structFields=structFields,
					}
				};

				break;
			}else{
				// print token and location
				println("unexpected keyword %.*s at line %d col %d",token.len,token.p,token.line,token.col);
				// and token after that
				TokenIter_nextToken(token_iter,&token);
				println("next token is: line %d col %d %.*s",token.line,token.col,token.len,token.p);
				fatal("unimplemented");
			}
		}

		default:
			return VALUE_INVALID;
	}

	while(1){
		// check for operators
		TokenIter_lastToken(token_iter,&token);
		enum VALUE_OPERATOR op;
		bool requiresSecondOperand=false;
		const char*opTerminator=nullptr;
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
			requiresSecondOperand=false;
			opTerminator=")";
		}else if(Token_equalString(&token,"[")){
			op=VALUE_OPERATOR_INDEX;
			requiresSecondOperand=true;
			opTerminator="]";
		}else{
			// if current value is parensWrapped, interpret as casting operation, i.e. parse new value
			// and continue this parsing loop with the new casted value
			if(value->kind==VALUE_KIND_PARENS_WRAPPED){
				Value castedValue={};
				enum VALUE_PARSE_RESULT res=Value_parse(&castedValue,token_iter);
				TokenIter_lastToken(token_iter,&token);
				if(res==VALUE_INVALID){
					fatal("invalid value after (");
				}
				*value=(Value){
					.kind=VALUE_KIND_CAST,
					.cast={
						.castTo=allocAndCopy(sizeof(Value),value),
						.value=allocAndCopy(sizeof(Value),&castedValue),
					}
				};
				continue;
			}
			return VALUE_PRESENT;
		}
		TokenIter_nextToken(token_iter,&token);

		switch(op){
			case VALUE_OPERATOR_DOT:{
				// get member name via next token
				Token memberToken=token;
				TokenIter_nextToken(token_iter,&token);

				*value=(Value){
					.kind=VALUE_KIND_DOT,
					.dot={
						.left=allocAndCopy(sizeof(Value),value),
						.right=allocAndCopy(sizeof(Token),&memberToken),
					}
				};
				continue;
			}
			case VALUE_OPERATOR_ARROW:{
				// get member name via next token
				Token memberToken=token;
				TokenIter_nextToken(token_iter,&token);

				*value=(Value){
					.kind=VALUE_KIND_ARROW,
					.arrow={
						.left=allocAndCopy(sizeof(Value),value),
						.right=allocAndCopy(sizeof(Token),&memberToken),
					}
				};
				continue;
			}
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
							fatal("invalid value in function call, got instead %.*s",token.len,token.p);
							break;
						case VALUE_PRESENT:
							array_append(&values,&arg);
							break;
					}
				}

				value->kind=VALUE_KIND_FUNCTION_CALL;
				value->function_call.name=allocAndCopy(sizeof(Token),&nameToken);
				value->function_call.args=values;			

				continue;
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
					switch(res){
						case VALUE_INVALID:
							// print next token
							println("next token is: line %d col %d %.*s",token.line,token.col,token.len,token.p);
							fatal("invalid value after operator at line %d col %d",token.line,token.col);
							break;
						case VALUE_PRESENT:
							TokenIter_lastToken(token_iter,&token);
							break;
					}
				}

				if(opTerminator){
					if(!Token_equalString(&token,opTerminator)){
						fatal("expected %s after operator",opTerminator);
					}
					TokenIter_nextToken(token_iter,&token);
				}

				*value=ret;
				continue;
			}
		}
		fatal("");
	}
	return VALUE_PRESENT;
}
char*Value_asString(Value*value){
	switch(value->kind){
		case VALUE_KIND_STATIC_VALUE:{
			char*ret=calloc(1024,1);
			sprintf(ret,"%.*s",value->static_value.value_repr->len,value->static_value.value_repr->p);
			return ret;
		}
		case VALUE_KIND_OPERATOR:{
			switch(value->op.op){
				case VALUE_OPERATOR_ADD:{
					char *ret=calloc(1024,1);
					sprintf(ret,"left (%s) ADD right (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					return ret;
				}
				case VALUE_OPERATOR_SUB:{
					char *ret=calloc(1024,1);
					sprintf(ret,"left (%s) SUB right (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					return ret;
				}
				case VALUE_OPERATOR_MULT:{
					char *ret=calloc(1024,1);
					sprintf(ret,"left (%s) MULT right (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					return ret;
				}
				case VALUE_OPERATOR_DIV:{
					char *ret=calloc(1024,1);
					sprintf(ret,"left (%s) DIV right (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					return ret;
				}
				case VALUE_OPERATOR_LESS_THAN:{
					char *ret=calloc(1024,1);
					sprintf(ret,"left (%s) LESS_THAN right (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					return ret;
				}
				case VALUE_OPERATOR_GREATER_THAN:{
					char *ret=calloc(1024,1);
					sprintf(ret,"left (%s) GREATER_THAN right (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					return ret;
				}
				case VALUE_OPERATOR_POSTFIX_INCREMENT:{
					char *ret=calloc(1024,1);
					sprintf(ret,"POSTFIX_INCREMENT %s",Value_asString(value->op.left));
					return ret;
				}
				case VALUE_OPERATOR_POSTFIX_DECREMENT:{
					char *ret=calloc(1024,1);
					sprintf(ret,"POSTFIX_DECREMENT %s",Value_asString(value->op.left));
					return ret;
				}
				case VALUE_OPERATOR_DOT:
					fatal("unimplemented %d",value->op.op);
				case VALUE_OPERATOR_INDEX:{
					char *ret=calloc(1024,1);
					sprintf(ret,"left (%s) INDEX right (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					return ret;
				}
				default:
					fatal("unimplemented %d",value->op.op);
			}
			return "VALUE_KIND_OPERATOR unimplemented";
		}
		case VALUE_KIND_SYMBOL_REFERENCE:{
			char*ret=calloc(1024,1);
			sprintf(ret,"%.*s",value->symbol->len,value->symbol->p);
			return ret;
		}
		case VALUE_KIND_FUNCTION_CALL:{
			char*ret=calloc(1024,1);
			sprintf(ret,"calling function %.*s with %d arguments",value->function_call.name->len,value->function_call.name->p,value->function_call.args.len);
			for(int i=0;i<value->function_call.args.len;i++){
				Value*arg=array_get(&value->function_call.args,i);
				char*arg_str=Value_asString(arg);
				sprintf(ret+strlen(ret),"\narg %d: %s",i,arg_str);
			}
			return ret;
		}
		case VALUE_KIND_DOT:{
			char*ret=calloc(1024,1);
			sprintf(ret,"left (%s) DOT right (%.*s)",Value_asString(value->dot.left),value->dot.right->len,value->dot.right->p);
			return ret;
		}
		case VALUE_KIND_ARROW:{
			char*ret=calloc(1024,1);
			sprintf(ret,"left (%s) ARROW right (%.*s)",Value_asString(value->arrow.left),value->arrow.right->len,value->arrow.right->p);
			return ret;
		}
		case VALUE_KIND_ADDRESS_OF:{
			char*ret=calloc(1024,1);
			sprintf(ret,"ADDR_OF ( %s )",Value_asString(value->addrOf.addressedValue));
			return ret;
		}
		case VALUE_KIND_PARENS_WRAPPED:{
			char*ret=calloc(1024,1);
			sprintf(ret,"_( %s )_",Value_asString(value->parens_wrapped.innerValue));
			return ret;
		}
		case VALUE_KIND_CAST:{
			char*ret=calloc(1024,1);
			sprintf(ret,"CAST ( %s ) TO ( %s )",Value_asString(value->cast.value),Value_asString(value->cast.castTo));
			return ret;
		}
		case VALUE_KIND_STRUCT_INITIALIZER:{
			char*ret=calloc(1024,1);
			sprintf(ret,"init struct with fields:");
			for(int i=0;i<value->struct_initializer.structFields.len;i++){
				struct StructFieldInitializer*field=array_get(&value->struct_initializer.structFields,i);
				char*field_str=Value_asString(field->value);
				if(field->name){
					sprintf(ret+strlen(ret),"\nfield %d (%.*s): %s",i,field->name->len,field->name->p,field_str);
				}else{
					sprintf(ret+strlen(ret),"\nfield %d : %s",i,field_str);
				}
			}
			return ret;
		}
		default:
			fatal("unimplemented %d",value->kind);
	}
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
