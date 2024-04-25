#include<parser/parser.h>
#include<util/util.h>
#include<tokenizer.h>

enum VALUE_PARSE_RESULT Value_parse(Value*value,struct TokenIter*token_iter_in){
	struct TokenIter token_iter_=*token_iter_in;
	struct TokenIter *token_iter=&token_iter_;

	*value=(Value){};

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
			if(token.p==KEYWORD_ASTERISK){
				TokenIter_nextToken(token_iter,&token);

				Value dereferencedValue={};
				enum VALUE_PARSE_RESULT res=Value_parse(&dereferencedValue,token_iter);
				if(res==VALUE_INVALID){
					fatal("invalid value after *");
				}
				*value=(Value){
					.kind=VALUE_KIND_OPERATOR,
					.op={
						.left=allocAndCopy(sizeof(Value),&dereferencedValue),
						.op=VALUE_OPERATOR_DEREFERENCE,
					}
				};
				
				break;
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

				// try parsing type first, assuming this is a type cast
				// if type parsing fails, attempt parsing value
				bool foundValue=false;
				do{
					struct TokenIter castTestIter=*token_iter;
					Symbol castTypeSymbol={};
					enum SYMBOL_PARSE_RESULT res=Symbol_parse(&castTypeSymbol,&castTestIter);

					switch(res){
						case SYMBOL_PRESENT:
							fatal("cannot cast to named symbol");
						case SYMBOL_INVALID:
							// fallthrough outer switch to parse value 
							break;
						case SYMBOL_WITHOUT_NAME:{
							// print next token
							TokenIter_lastToken(&castTestIter, &token);
							// if symbol is present, assume this is a type cast and continue parsing value

							// make sure there is no symbol name though
							if(castTypeSymbol.name){
								// not a casting operation
								break;
							}

							// check for closing )
							TokenIter_lastToken(&castTestIter, &token);
							if(token.p!=KEYWORD_PARENS_CLOSE){
								// not a casting operation
								break;
							}
							TokenIter_nextToken(&castTestIter, &token);

							Value castValue={};
							enum VALUE_PARSE_RESULT res=Value_parse(&castValue,&castTestIter);
							if(res==VALUE_INVALID){
								fatal("invalid value after cast");
							}

							*token_iter=castTestIter;
							TokenIter_lastToken(token_iter, &token);
							*value=(Value){
								.kind=VALUE_KIND_CAST,
								.cast={
									.castTo=castTypeSymbol.type,
									.value=allocAndCopy(sizeof(Value),&castValue),
								}
							};
							foundValue=true;
							break;
						}
					}
				}while(0);
				if(foundValue){
					break;
				}

				Value innerValue={};
				enum VALUE_PARSE_RESULT res=Value_parse(&innerValue,token_iter);
				if(res==VALUE_INVALID){
					fatal("invalid value after (");
				}
				TokenIter_lastToken(token_iter, &token);
				if(token.p!=KEYWORD_PARENS_CLOSE){
					println("got value %s",Value_asString(&innerValue));
					fatal("expected ) after (, instead got %.*s",token.len,token.p);
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
					array_init(&field.fieldNameSegments,sizeof(Token));

					// if next token is dot, parse field name followed by assignment symbol
					while(Token_equalString(&token,".")){
						TokenIter_nextToken(token_iter,&token);
						array_append(&field.fieldNameSegments,&token);
						TokenIter_nextToken(token_iter,&token);
					}
					if(field.fieldNameSegments.len>0){
						if(!Token_equalString(&token,"=")){
							fatal("expected = after field name at line %d col %d but got %.*s",token.line,token.col,token.len,token.p);
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
					fatal("expected } after struct initializer at line %d col %d",token.line,token.col);
				}
				TokenIter_nextToken(token_iter, &token);

				*value=(Value){
					.kind=VALUE_KIND_STRUCT_INITIALIZER,
					.struct_initializer={
						.structFields=structFields,
					}
				};

				break;
			}else if(token.p==KEYWORD_BANG){
				TokenIter_nextToken(token_iter,&token);

				Value innerValue={};
				enum VALUE_PARSE_RESULT res=Value_parse(&innerValue,token_iter);
				if(res==VALUE_INVALID){
					fatal("invalid value after !");
				}

				*value=(Value){
					.kind=VALUE_KIND_OPERATOR,
					.op={
						.left=allocAndCopy(sizeof(Value),&innerValue),
						.op=VALUE_OPERATOR_LOGICAL_NOT,
					}
				};

				break;
			}else if(token.p==KEYWORD_TILDE){
				TokenIter_nextToken(token_iter,&token);

				Value innerValue={};
				enum VALUE_PARSE_RESULT res=Value_parse(&innerValue,token_iter);
				if(res==VALUE_INVALID){
					fatal("invalid value after ~");
				}

				*value=(Value){
					.kind=VALUE_KIND_OPERATOR,
					.op={
						.left=allocAndCopy(sizeof(Value),&innerValue),
						.op=VALUE_OPERATOR_BITWISE_NOT,
					}
				};

				break;
			}else{
				// encountered unexpected keyword
				if(value->kind!=VALUE_KIND_UNKNOWN){
					// if value kind is already set, return valid value
					return VALUE_PRESENT;
				}
				return VALUE_INVALID;
			}
		}

		default:
			return VALUE_INVALID;
	}

	while(1){
		// check for operators
		TokenIter_lastToken(token_iter,&token);

		enum VALUE_OPERATOR op=VALUE_OPERATOR_UNKNOWN;
		const char*opTerminator=nullptr;
		if(Token_equalString(&token,"+")){
			op=VALUE_OPERATOR_ADD;
		}else if(Token_equalString(&token,"-")){
			op=VALUE_OPERATOR_SUB;
		}else if(Token_equalString(&token,"*")){
			op=VALUE_OPERATOR_MULT;
		}else if(Token_equalString(&token,"/")){
			op=VALUE_OPERATOR_DIV;
		}else if(Token_equalString(&token,"%")){
			op=VALUE_OPERATOR_MODULO;
		}else if(Token_equalString(&token,"=")){
			op=VALUE_OPERATOR_ASSIGNMENT;
		}else if(Token_equalString(&token,"<")){
			op=VALUE_OPERATOR_LESS_THAN;
		}else if(Token_equalString(&token,"<=")){
			op=VALUE_OPERATOR_LESS_THAN_OR_EQUAL;
		}else if(Token_equalString(&token,">=")){
			op=VALUE_OPERATOR_GREATER_THAN_OR_EQUAL;
		}else if(Token_equalString(&token,">")){
			op=VALUE_OPERATOR_GREATER_THAN;
		}else if(Token_equalString(&token,"&&")){
			op=VALUE_OPERATOR_LOGICAL_AND;
		}else if(Token_equalString(&token,"||")){
			op=VALUE_OPERATOR_LOGICAL_OR;
		}else if(Token_equalString(&token,"&")){
			op=VALUE_OPERATOR_BITWISE_AND;
		}else if(Token_equalString(&token,"|")){
			op=VALUE_OPERATOR_BITWISE_OR;
		}else if(Token_equalString(&token,"==")){
			op=VALUE_OPERATOR_EQUAL;
		}else if(Token_equalString(&token,"!=")){
			op=VALUE_OPERATOR_NOT_EQUAL;
		}else if(Token_equalString(&token,"++")){
			op=VALUE_OPERATOR_POSTFIX_INCREMENT;
		}else if(Token_equalString(&token,"--")){
			op=VALUE_OPERATOR_POSTFIX_DECREMENT;
		}else if(Token_equalString(&token,".")){
			op=VALUE_OPERATOR_DOT;
		}else if(Token_equalString(&token,"->")){
			op=VALUE_OPERATOR_ARROW;
		}else if(Token_equalString(&token,"(")){
			op=VALUE_OPERATOR_CALL;
			opTerminator=")";
		}else if(Token_equalString(&token,"[")){
			op=VALUE_OPERATOR_INDEX;
			opTerminator="]";
		}

		if(op==VALUE_OPERATOR_UNKNOWN && value->kind!=VALUE_KIND_UNKNOWN){
			bool gotOperator=false;
			// check if next token is numeric, where an operator may have been interpreted as a unary operator or a sign
			if(token.tag==TOKEN_TAG_LITERAL_INTEGER){
				if(token.num_info.hasLeadingSign){
					switch(token.p[0]){
						case '+':
							op=VALUE_OPERATOR_ADD;
							break;
						case '-':
							op=VALUE_OPERATOR_SUB;
							break;
						default:
							fatal("invalid sign %c",token.p[0]);
					}
					token.num_info.hasLeadingSign=false;
					token.p++;
					token.len--;
					gotOperator=true;
				}
			}

			if(!gotOperator)
				goto VALUE_PARSE_RET_SUCCESS;

			// do not retrieve new token since current token contained sign for operator plus a value as second operand
		}else{
			TokenIter_nextToken(token_iter,&token);
		}

		bool requiresSecondOperand=false;
		switch(op){
			case VALUE_OPERATOR_ADD:
			case VALUE_OPERATOR_SUB:
			case VALUE_OPERATOR_MULT:
			case VALUE_OPERATOR_DIV:
			case VALUE_OPERATOR_MODULO:
			case VALUE_OPERATOR_ASSIGNMENT:
			case VALUE_OPERATOR_LESS_THAN:
			case VALUE_OPERATOR_GREATER_THAN:
			case VALUE_OPERATOR_INDEX:
			case VALUE_OPERATOR_LESS_THAN_OR_EQUAL:
			case VALUE_OPERATOR_GREATER_THAN_OR_EQUAL:

			case VALUE_OPERATOR_LOGICAL_AND:
			case VALUE_OPERATOR_LOGICAL_OR:
			
			case VALUE_OPERATOR_BITWISE_AND:
			case VALUE_OPERATOR_BITWISE_OR:

			case VALUE_OPERATOR_NOT_EQUAL:
			case VALUE_OPERATOR_EQUAL:
				requiresSecondOperand=true;
				break;
			default:;
		}

		switch(op){
			case VALUE_OPERATOR_UNKNOWN:
				fatal("error lead to unknown operator, with next token %.*s",token.len,token.p);
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

VALUE_PARSE_RET_SUCCESS:
	*token_iter_in=*token_iter;
	return VALUE_PRESENT;
}
char*Value_asString(Value*value){
	char*ret=makeString();
	switch(value->kind){
		case VALUE_KIND_STATIC_VALUE:{
			stringAppend(ret,"%.*s",value->static_value.value_repr->len,value->static_value.value_repr->p);
			break;
		}
		case VALUE_KIND_OPERATOR:{
			switch(value->op.op){
				case VALUE_OPERATOR_ADD:{
					stringAppend(ret,"left (%s) ADD right (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_SUB:{
					stringAppend(ret,"left (%s) SUB right (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_MULT:{
					stringAppend(ret,"left (%s) MULT right (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_DIV:{
					stringAppend(ret,"left (%s) DIV right (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_MODULO:{
					stringAppend(ret,"left (%s) MODULO right (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_ASSIGNMENT:{
					stringAppend(ret,"left (%s) ASSIGN right (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_LESS_THAN:{
					stringAppend(ret,"left (%s) LESS_THAN right (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_LESS_THAN_OR_EQUAL:{
					stringAppend(ret,"left (%s) LESS_THAN_OR_EQUAL right (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_GREATER_THAN:{
					stringAppend(ret,"left (%s) GREATER_THAN right (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_GREATER_THAN_OR_EQUAL:{
					stringAppend(ret,"left (%s) GREATER_THAN_OR_EQUAL right (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_POSTFIX_INCREMENT:{
					stringAppend(ret,"POSTFIX_INCREMENT %s",Value_asString(value->op.left));
					break;
				}
				case VALUE_OPERATOR_POSTFIX_DECREMENT:{
					stringAppend(ret,"POSTFIX_DECREMENT %s",Value_asString(value->op.left));
					break;
				}
				case VALUE_OPERATOR_DOT:{
					stringAppend(ret,"left (%s) DOT right (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_INDEX:{
					stringAppend(ret,"left (%s) INDEX right (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_LOGICAL_AND:{
					stringAppend(ret,"left (%s) LOGICAL_AND right (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_LOGICAL_OR:{
					stringAppend(ret,"left (%s) LOGICAL_OR right (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_BITWISE_AND:{
					stringAppend(ret,"left (%s) BITWISE_AND right (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_BITWISE_OR:{
					stringAppend(ret,"left (%s) BITWISE_OR right (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_EQUAL:{
					stringAppend(ret,"left (%s) EQUAL right (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_NOT_EQUAL:{
					stringAppend(ret,"left (%s) NOT_EQUAL right (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_LOGICAL_NOT:{
					stringAppend(ret,"LOGICAL_NOT %s",Value_asString(value->op.left));
					break;
				}
				case VALUE_OPERATOR_BITWISE_NOT:{
					stringAppend(ret,"BITWISE_NOT %s",Value_asString(value->op.left));
					break;
				}
				case VALUE_OPERATOR_DEREFERENCE:{
					stringAppend(ret,"DEREFERENCE %s",Value_asString(value->op.left));
					break;
				}

				default:
					fatal("unimplemented %d",value->op.op);
			}
			break;
		}
		case VALUE_KIND_SYMBOL_REFERENCE:{
			stringAppend(ret,"%.*s",value->symbol->len,value->symbol->p);
			break;
		}
		case VALUE_KIND_FUNCTION_CALL:{
			stringAppend(ret,"calling function %.*s with %d arguments (",value->function_call.name->len,value->function_call.name->p,value->function_call.args.len);
			for(int i=0;i<value->function_call.args.len;i++){
				Value*arg=array_get(&value->function_call.args,i);
				char*arg_str=Value_asString(arg);
				stringAppend(ret,"arg %d: %s,",i,arg_str);
			}
			stringAppend(ret,")");
			break;
		}
		case VALUE_KIND_DOT:{
			stringAppend(ret,"left (%s) DOT right (%.*s)",Value_asString(value->dot.left),value->dot.right->len,value->dot.right->p);
			break;
		}
		case VALUE_KIND_ARROW:{
			stringAppend(ret,"left (%s) ARROW right (%.*s)",Value_asString(value->arrow.left),value->arrow.right->len,value->arrow.right->p);
			break;
		}
		case VALUE_KIND_ADDRESS_OF:{
			stringAppend(ret,"ADDR_OF ( %s )",Value_asString(value->addrOf.addressedValue));
			break;
		}
		case VALUE_KIND_PARENS_WRAPPED:{
			stringAppend(ret,"_( %s )_",Value_asString(value->parens_wrapped.innerValue));
			break;
		}
		case VALUE_KIND_CAST:{
			stringAppend(ret,"CAST ( %s ) TO ( %s )",Value_asString(value->cast.value),Type_asString(value->cast.castTo));
			break;
		}
		case VALUE_KIND_STRUCT_INITIALIZER:{
			stringAppend(ret,"init struct with fields: ");
			for(int i=0;i<value->struct_initializer.structFields.len;i++){
				struct StructFieldInitializer*field=array_get(&value->struct_initializer.structFields,i);
				stringAppend(ret,"field %d ",i);
				for(int j=0;j<field->fieldNameSegments.len;j++){
					Token*segment=array_get(&field->fieldNameSegments,j);
					stringAppend(ret,".%.*s",segment->len,segment->p);
				}
				stringAppend(ret," = %s , ",Value_asString(field->value));
			}
			break;
		}
		default:
			fatal("unimplemented %d",value->kind);
	}
	return ret;
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
