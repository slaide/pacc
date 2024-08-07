#include <stdint.h>

#include<parser/parser.h>
#include<util/util.h>
#include<tokenizer.h>

enum VALUE_PARSE_RESULT Value_parse(Stack*stack,Value*value,struct TokenIter*token_iter_in){
	struct TokenIter token_iter_=*token_iter_in;
	struct TokenIter *token_iter=&token_iter_;

	*value=(Value){};

	Token token;
	TokenIter_lastToken(token_iter,&token);
	/// used by some cases
	Token nameToken=token;

	switch(token.tag){
		case TOKEN_TAG_LITERAL:
		{
			switch(token.literal.tag){
				case TOKEN_LITERAL_TAG_NUMERIC:{
					switch(token.literal.numeric.tag){
						case TOKEN_LITERAL_NUMERIC_TAG_CHAR:{
							Token literalValueToken=token;
							TokenIter_nextToken(token_iter,&token);

							value->kind=VALUE_KIND_STATIC_VALUE;
							value->static_value.value_repr=allocAndCopy(sizeof(Token),&literalValueToken);
							break;
						}
						case TOKEN_LITERAL_NUMERIC_TAG_INTEGER:
						{
							Token literalValueToken=token;
							TokenIter_nextToken(token_iter,&token);

							value->kind=VALUE_KIND_STATIC_VALUE;
							value->static_value.value_repr=allocAndCopy(sizeof(Token),&literalValueToken);
							break;
						}
						case TOKEN_LITERAL_NUMERIC_TAG_FLOAT:{
							Token literalValueToken=token;
							TokenIter_nextToken(token_iter,&token);

							// print message with token
							println("float literal %.*s at line %d col %d",token.len,token.p,token.line,token.col);

							value->kind=VALUE_KIND_STATIC_VALUE;
							value->static_value.value_repr=allocAndCopy(sizeof(Token),&literalValueToken);

							break;
						}
						default:fatal("bug");
					}
					break;
				}
				case TOKEN_LITERAL_TAG_STRING:{
					Token literalValueToken=token;
					TokenIter_nextToken(token_iter,&token);

					value->kind=VALUE_KIND_STATIC_VALUE;
					value->static_value.value_repr=allocAndCopy(sizeof(Token),&literalValueToken);
					break;
				}
				default:fatal("bug");
			}
			break;
		}
		case TOKEN_TAG_SYMBOL:
		{
			TokenIter_nextToken(token_iter,&token);

			value->kind=VALUE_KIND_SYMBOL_REFERENCE;
			Symbol*symbol=Stack_findSymbol(stack, &nameToken);
			if(symbol==nullptr){
				// check if nameToken refers to a type
				Type*type=Stack_findType(stack,&nameToken);
				if(type!=nullptr){
					value->kind=VALUE_KIND_TYPEREF;
					value->typeref.type=type;
					break;
				}
				println("symbol not found %.*s",nameToken.len,nameToken.p);
				goto VALUE_PARSE_SYMBOL_NOT_FOUND;
			}
			value->symbol=symbol;

			break;
		}

		case TOKEN_TAG_KEYWORD:{
			if(Token_equalString(&token,KEYWORD_ASTERISK)){
				TokenIter_nextToken(token_iter,&token);

				Value dereferencedValue={};
				enum VALUE_PARSE_RESULT res=Value_parse(stack,&dereferencedValue,token_iter);
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
			}else if(Token_equalString(&token,KEYWORD_AMPERSAND)){
				TokenIter_nextToken(token_iter,&token);

				Value addressedValue={};
				enum VALUE_PARSE_RESULT res=Value_parse(stack,&addressedValue,token_iter);
				if(res==VALUE_INVALID){
					fatal("invalid value after & at %s",Token_print(&token));
				}

				*value=(Value){
					.kind=VALUE_KIND_ADDRESS_OF,
					.addrOf={
						.addressedValue=allocAndCopy(sizeof(Value), &addressedValue)
					},
				};

				break;
			}else if(Token_equalString(&token,KEYWORD_PARENS_OPEN)){
				TokenIter_nextToken(token_iter,&token);

				// try parsing type first, assuming this is a type cast
				// if type parsing fails, attempt parsing value
				bool foundValue=false;
				do{
					int numCastTypeSymbols=0;
					struct SymbolDefinition*symbols=nullptr;
					enum SYMBOL_PARSE_RESULT res=SymbolDefinition_parse(stack,&numCastTypeSymbols,&symbols,token_iter,&(struct Symbol_parse_options){.forbid_multiple=true});

					switch(res){
						case SYMBOL_INVALID:
							// fallthrough outer switch to parse value 
							break;
						
						case SYMBOL_PRESENT:{
							if(numCastTypeSymbols==0){
								break;
							}
							if(numCastTypeSymbols>1){
								fatal("expected exactly one symbol but got %d at %s",numCastTypeSymbols,Token_print(&token));
							}

							Symbol castTypeSymbol=symbols[0].symbol;
							if(castTypeSymbol.name==nullptr){
								// print next token
								TokenIter_lastToken(token_iter, &token);
								// if symbol is present, assume this is a type cast and continue parsing value

								// make sure there is no symbol name though
								if(castTypeSymbol.name){
									// not a casting operation
									break;
								}

								// check for closing )
								TokenIter_lastToken(token_iter, &token);
								if(!Token_equalString(&token,KEYWORD_PARENS_CLOSE)){
									println("parens not closed, got instead %s",Token_print(&token));
									// not a casting operation
									break;
								}
								TokenIter_nextToken(token_iter, &token);

								Value castValue={};
								enum VALUE_PARSE_RESULT res=Value_parse(stack,&castValue,token_iter);
								if(res==VALUE_INVALID){
									fatal("invalid value after cast");
								}

								TokenIter_lastToken(token_iter, &token);
								*value=(Value){
									.kind=VALUE_KIND_CAST,
									.cast={
										.castTo=castTypeSymbol.type,
										.value=COPY_(&castValue),
									}
								};
								foundValue=true;
							}else{
								fatal("cannot cast to named symbol");
							}
							break;
						}
					}
				}while(0);
				if(foundValue){
					break;
				}

				Value innerValue={};
				enum VALUE_PARSE_RESULT res=Value_parse(stack,&innerValue,token_iter);
				if(res==VALUE_INVALID){
					fatal("invalid value after ( at %s",Token_print(&token));
				}
				TokenIter_lastToken(token_iter, &token);
				if(!Token_equalString(&token,KEYWORD_PARENS_CLOSE)){
					println("got value %s",Value_asString(&innerValue));
					fatal("expected ) after (, instead got %.*s",token.len,token.p);
				}
				TokenIter_nextToken(token_iter, &token);

				// the current value could now be
				// 1) operator precendence override, e.g. (a+b)*c
				// 2) value cast, e.g. (int)2.3f

				// check for casting operator first, then fall back to precedence override otherwise
				if(innerValue.kind==VALUE_KIND_TYPEREF){
					println("attempting cast");
					// this is a type cast
					Value castValue={};
					res=Value_parse(stack,&castValue,token_iter);
					if(res==VALUE_INVALID){
						fatal("invalid value after cast");
					}else{
						println("cast to %s",Type_asString(innerValue.typeref.type));
					}
					TokenIter_lastToken(token_iter, &token);

					*value=(Value){
						.kind=VALUE_KIND_CAST,
						.cast={
							.castTo=innerValue.typeref.type,
							.value=COPY_(&castValue),
						}
					};
					break;
				}

				*value=(Value){
					.kind=VALUE_KIND_PARENS_WRAPPED,
					.parens_wrapped={
						.innerValue=allocAndCopy(sizeof(Value),&innerValue),
					}
				};

				break;
			}else if(Token_equalString(&token,KEYWORD_CURLY_BRACES_OPEN)){
				// parse struct/array initializer
				TokenIter_nextToken(token_iter, &token);

				array fields;
				array_init(&fields,sizeof(struct FieldInitializer));
				while(!TokenIter_isEmpty(token_iter)){
					if (Token_equalString(&token, KEYWORD_CURLY_BRACES_CLOSE)){
						break;
					}

					struct FieldInitializer field={};
					array_init(&field.fieldNameSegments,sizeof(struct FieldInitializerSegment));

					// if next token is dot, parse field name followed by assignment symbol
					while(1){
						if(Token_equalString(&token,KEYWORD_DOT)){
							TokenIter_nextToken(token_iter,&token);
							
							struct FieldInitializerSegment newSegment={
								.kind=FIELD_INITIALIZER_SEGMENT_FIELD,
								.field=allocAndCopy(sizeof(Token),&token),
							};
							array_append(&field.fieldNameSegments,&newSegment);
							TokenIter_nextToken(token_iter,&token);
						}else if(Token_equalString(&token,KEYWORD_SQUARE_BRACKETS_OPEN)){
							TokenIter_nextToken(token_iter,&token);
							// check that token is integer literal
							if(!(token.tag==TOKEN_TAG_LITERAL && token.literal.tag==TOKEN_LITERAL_TAG_NUMERIC)){
								fatal("expected integer literal after [ in field name at line %d col %d",token.line,token.col);
							}
							struct FieldInitializerSegment newSegment={
								.kind=FIELD_INITIALIZER_SEGMENT_INDEX,
								.index=allocAndCopy(sizeof(Token),&token),
							};
							array_append(&field.fieldNameSegments,&newSegment);
							TokenIter_nextToken(token_iter,&token);
							if(!Token_equalString(&token,KEYWORD_SQUARE_BRACKETS_CLOSE)){
								fatal("expected ] after [ in field name at line %d col %d",token.line,token.col);
							}
							TokenIter_nextToken(token_iter,&token);
						}else{
							break;
						}
					}
					if(field.fieldNameSegments.len>0){
						if(!Token_equalString(&token,KEYWORD_EQUAL)){
							fatal("expected = after field name at line %d col %d but got %.*s",token.line,token.col,token.len,token.p);
						}
						TokenIter_nextToken(token_iter,&token);
					}

					Value fieldValue={};
					enum VALUE_PARSE_RESULT res=Value_parse(stack,&fieldValue,token_iter);
					TokenIter_lastToken(token_iter, &token);
					if(res==VALUE_INVALID){
						fatal("invalid value in struct initializer");
					}
					field.value=allocAndCopy(sizeof(Value),&fieldValue);

					array_append(&fields,&field);

					// check for comma
					if(Token_equalString(&token,KEYWORD_COMMA)){
						TokenIter_nextToken(token_iter,&token);
						continue;
					}

					break;
				}

				if (!Token_equalString(&token, KEYWORD_CURLY_BRACES_CLOSE)){
					fatal("expected %s after struct initializer at %s",KEYWORD_CURLY_BRACES_CLOSE,Token_print(&token));
				}
				TokenIter_nextToken(token_iter, &token);

				*value=(Value){
					.kind=VALUE_KIND_STRUCT_INITIALIZER,
					.struct_initializer={
						.structFields=fields,
					}
				};

				break;
			}else if(Token_equalString(&token,KEYWORD_BANG)){
				TokenIter_nextToken(token_iter,&token);

				Value innerValue={};
				enum VALUE_PARSE_RESULT res=Value_parse(stack,&innerValue,token_iter);
				if(res==VALUE_INVALID){
					fatal("invalid value after %s",KEYWORD_BANG);
				}

				*value=(Value){
					.kind=VALUE_KIND_OPERATOR,
					.op={
						.left=allocAndCopy(sizeof(Value),&innerValue),
						.op=VALUE_OPERATOR_LOGICAL_NOT,
					}
				};

				break;
			}else if(Token_equalString(&token,KEYWORD_PLUS)){
				TokenIter_nextToken(token_iter,&token);

				Value innerValue={};
				enum VALUE_PARSE_RESULT res=Value_parse(stack,&innerValue,token_iter);
				if(res==VALUE_INVALID){
					fatal("invalid value after %s",KEYWORD_PLUS);
				}

				*value=(Value){
					.kind=VALUE_KIND_OPERATOR,
					.op={
						.left=allocAndCopy(sizeof(Value),&innerValue),
						.op=VALUE_OPERATOR_UNARY_PLUS,
					}
				};

				break;
			}else if(Token_equalString(&token,KEYWORD_MINUS)){
				TokenIter_nextToken(token_iter,&token);

				Value innerValue={};
				enum VALUE_PARSE_RESULT res=Value_parse(stack,&innerValue,token_iter);
				if(res==VALUE_INVALID){
					fatal("invalid value after %s",KEYWORD_MINUS);
				}

				*value=(Value){
					.kind=VALUE_KIND_OPERATOR,
					.op={
						.left=allocAndCopy(sizeof(Value),&innerValue),
						.op=VALUE_OPERATOR_UNARY_MINUS,
					}
				};

				break;
			}else if(Token_equalString(&token,KEYWORD_TILDE)){
				TokenIter_nextToken(token_iter,&token);

				Value innerValue={};
				enum VALUE_PARSE_RESULT res=Value_parse(stack,&innerValue,token_iter);
				if(res==VALUE_INVALID){
					fatal("invalid value after %s",KEYWORD_TILDE);
				}

				*value=(Value){
					.kind=VALUE_KIND_OPERATOR,
					.op={
						.left=allocAndCopy(sizeof(Value),&innerValue),
						.op=VALUE_OPERATOR_BITWISE_NOT,
					}
				};

				break;
			}else if(Token_equalString(&token,"++")){
				TokenIter_nextToken(token_iter,&token);

				Value innerValue={};
				enum VALUE_PARSE_RESULT res=Value_parse(stack,&innerValue,token_iter);
				if(res==VALUE_INVALID){
					fatal("invalid value after ++");
				}

				*value=(Value){
					.kind=VALUE_KIND_OPERATOR,
					.op={
						.left=allocAndCopy(sizeof(Value),&innerValue),
						.op=VALUE_OPERATOR_PREFIX_INCREMENT,
					}
				};

				break;
			}else if(Token_equalString(&token,"--")){
				TokenIter_nextToken(token_iter,&token);

				Value innerValue={};
				enum VALUE_PARSE_RESULT res=Value_parse(stack,&innerValue,token_iter);
				if(res==VALUE_INVALID){
					fatal("invalid value after --");
				}

				*value=(Value){
					.kind=VALUE_KIND_OPERATOR,
					.op={
						.left=allocAndCopy(sizeof(Value),&innerValue),
						.op=VALUE_OPERATOR_PREFIX_DECREMENT,
					}
				};

				break;
			}else{
				// encountered unexpected keyword
				if(value->kind!=VALUE_KIND_UNKNOWN){
					// if value kind is already set, return valid value
					goto VALUE_PARSE_RET_SUCCESS;
				}

				// try parsing a type, i.e. a symbol without a name, as type reference value
				{
					int numTypeSymbols=0;
					struct SymbolDefinition*typeSymbols=nullptr;
					enum SYMBOL_PARSE_RESULT res=SymbolDefinition_parse(stack,&numTypeSymbols,&typeSymbols,token_iter,&(struct Symbol_parse_options){.forbid_multiple=true});
					TokenIter_lastToken(token_iter,&token);
					if(res!=SYMBOL_PRESENT || numTypeSymbols==0)goto VALUE_PARSE_RET_FAILURE;
					if(numTypeSymbols>1)fatal("expected exactly one symbol but got %d at %s",numTypeSymbols,Token_print(&token));
					Symbol typeSymbol=typeSymbols[0].symbol;
					
					// only if symbol has no name (i.e. just type) and is not a reference (i.e. not just a token that might be a value)
					if(typeSymbol.name==nullptr && typeSymbol.type->kind==TYPE_KIND_REFERENCE){
						*value=(Value){
							.kind=VALUE_KIND_TYPEREF,
							.typeref.type=typeSymbol.type,
						};
						// cannot operate on this type, so return instead of break
						goto VALUE_PARSE_RET_SUCCESS;
					}
					// on SYMBOL_PRESENT the iterator is in an invalid state (has iterated past the symbol, even though the symbol is discarded)
					// this function fails in this case though, so we discard the invalid iterator anyway
				}
				
				goto VALUE_PARSE_RET_FAILURE;
			}
		}

		default:
			goto VALUE_PARSE_RET_FAILURE;
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
		}else if(Token_equalString(&token,"+=")){
			op=VALUE_OPERATOR_ADD_ASSIGN;
		}else if(Token_equalString(&token,"-=")){
			op=VALUE_OPERATOR_SUB_ASSIGN;
		}else if(Token_equalString(&token,"*=")){
			op=VALUE_OPERATOR_MULT_ASSIGN;
		}else if(Token_equalString(&token,"/=")){
			op=VALUE_OPERATOR_DIV_ASSIGN;
		}else if(Token_equalString(&token,"%=")){
			op=VALUE_OPERATOR_MODULO_ASSIGN;
		}else if(Token_equalString(&token,"&=")){
			op=VALUE_OPERATOR_BITWISE_AND_ASSIGN;
		}else if(Token_equalString(&token,"|=")){
			op=VALUE_OPERATOR_BITWISE_OR_ASSIGN;
		}else if(Token_equalString(&token,"^=")){
			op=VALUE_OPERATOR_BITWISE_XOR_ASSIGN;

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
		}else if(Token_equalString(&token,"?")){
			op=VALUE_OPERATOR_CONDITIONAL;
		}

		// catch invalid operators
		if(op==VALUE_OPERATOR_UNKNOWN && value->kind!=VALUE_KIND_UNKNOWN){
			goto VALUE_PARSE_RET_SUCCESS;
		}

		TokenIter_nextToken(token_iter,&token);

		bool requiresSecondOperand=false;
		switch(op){
			case VALUE_OPERATOR_ADD:
			case VALUE_OPERATOR_SUB:
			case VALUE_OPERATOR_MULT:
			case VALUE_OPERATOR_DIV:
			case VALUE_OPERATOR_MODULO:

			case VALUE_OPERATOR_ASSIGNMENT:
			case VALUE_OPERATOR_ADD_ASSIGN:
			case VALUE_OPERATOR_SUB_ASSIGN:
			case VALUE_OPERATOR_MULT_ASSIGN:
			case VALUE_OPERATOR_DIV_ASSIGN:
			case VALUE_OPERATOR_MODULO_ASSIGN:
			case VALUE_OPERATOR_BITWISE_AND_ASSIGN:
			case VALUE_OPERATOR_BITWISE_OR_ASSIGN:
			case VALUE_OPERATOR_BITWISE_XOR_ASSIGN:

			case VALUE_OPERATOR_LESS_THAN:
			case VALUE_OPERATOR_GREATER_THAN:

			case VALUE_OPERATOR_LESS_THAN_OR_EQUAL:
			case VALUE_OPERATOR_GREATER_THAN_OR_EQUAL:

			case VALUE_OPERATOR_INDEX:

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

			case VALUE_OPERATOR_CONDITIONAL:{
				Value trueValue={};
				enum VALUE_PARSE_RESULT resTrue=Value_parse(stack,&trueValue,token_iter);
				if(resTrue==VALUE_INVALID){
					fatal("invalid onTrue value in conditional");
				}
				TokenIter_lastToken(token_iter,&token);
				if(!Token_equalString(&token,":")){
					fatal("expected : after onTrue value in conditional");
				}
				TokenIter_nextToken(token_iter,&token);

				Value falseValue={};
				enum VALUE_PARSE_RESULT resFalse=Value_parse(stack,&falseValue,token_iter);
				if(resFalse==VALUE_INVALID){
					fatal("invalid onFalse value in conditional");
				}

				if(value->kind==VALUE_KIND_UNKNOWN){
					fatal("conditional operator must be the first operator in an expression");
				}

				*value=(Value){
					.kind=VALUE_KIND_CONDITIONAL,
					.conditional={
						.condition=allocAndCopy(sizeof(Value),value),
						.onTrue=allocAndCopy(sizeof(Value),&trueValue),
						.onFalse=allocAndCopy(sizeof(Value),&falseValue),
					}
				};

				continue;
			}
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
				// parse arguments
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
					enum VALUE_PARSE_RESULT res=Value_parse(stack,&arg,token_iter);
					TokenIter_lastToken(token_iter,&token);
					switch(res){
						case VALUE_SYMBOL_UNKNOWN:
						case VALUE_INVALID:
							{
								// try parsing a symbol instead
								int num_symbols=0;
								struct SymbolDefinition*symbols=nullptr;
								enum SYMBOL_PARSE_RESULT res=SymbolDefinition_parse(stack,&num_symbols,&symbols,token_iter,&(struct Symbol_parse_options){.forbid_multiple=true});
								TokenIter_lastToken(token_iter,&token);
								switch(res){
									case SYMBOL_INVALID:
										fatal("invalid value in function call");
									case SYMBOL_PRESENT:
										if(num_symbols!=1){
											fatal("expected exactly one symbol but got %d at %s",num_symbols,Token_print(&token));
										}
										if(symbols[0].symbol.name!=nullptr){
											println("symbol name %.*s",symbols[0].symbol.name->len,symbols[0].symbol.name->p);
											fatal("expected value in function call, got symbol instead, at %s",Token_print(&token));
										}
										arg.kind=VALUE_KIND_TYPEREF;
										arg.typeref.type=symbols[0].symbol.type;
										if(arg.typeref.type==nullptr)fatal("unreachable");
										array_append(&values,&arg);
										break;
									default:fatal("unreachable");
								}
							}
							break;
						case VALUE_PRESENT:
							array_append(&values,&arg);
							break;
					}
				}

				Value*function=allocAndCopy(sizeof(Value),value);

				value->kind=VALUE_KIND_FUNCTION_CALL;
				value->function_call.function=function;
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

					enum VALUE_PARSE_RESULT res=Value_parse(stack,ret.op.right,token_iter);
					switch(res){
						case VALUE_SYMBOL_UNKNOWN:
						case VALUE_INVALID:
							// print next token
							println("next token is %s",Token_print(&token));
							fatal("invalid value after operator");
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

VALUE_PARSE_SYMBOL_NOT_FOUND:
	return VALUE_SYMBOL_UNKNOWN;

VALUE_PARSE_RET_FAILURE:
	return VALUE_INVALID;

}

Type*Value_getType(Value*value){
	if(value==nullptr)fatal("unreachable");
	switch(value->kind){
		case VALUE_KIND_SYMBOL_REFERENCE:{
			return value->symbol->type;
		}
		case VALUE_KIND_STATIC_VALUE:{
			switch(value->static_value.value_repr->tag){
				case TOKEN_TAG_LITERAL:{
					switch(value->static_value.value_repr->literal.tag){
						case TOKEN_LITERAL_TAG_NUMERIC:{
							switch(value->static_value.value_repr->literal.numeric.tag){
								case TOKEN_LITERAL_NUMERIC_TAG_CHAR:{
									return &Type_CHAR;
								}
								case TOKEN_LITERAL_NUMERIC_TAG_INTEGER:{
									return &Type_INT;
								}
								case TOKEN_LITERAL_NUMERIC_TAG_FLOAT:{
									return &Type_F32;
								}
								default:fatal("unreachable");
							}
						}
						case TOKEN_LITERAL_TAG_STRING:{
							return &Type_STRING;
						}
						default:fatal("unreachable");
					}
				}
				default:fatal("unreachable");
			}
		}
		case VALUE_KIND_FUNCTION_CALL:{
			// calling a function, which is a value containing a symbol, which has a type, this type is the function type, returning some value
			// -> calling the function returns a value of the return type of the function
			return value->function_call.function->symbol->type->function.ret;
		}
		case VALUE_KIND_TYPEREF:{
			return &Type_TYPE;
		}
		case VALUE_KIND_ADDRESS_OF:{
			Type pointerToInnerType={
				.kind=TYPE_KIND_POINTER,
				.pointer={
					.base=Value_getType(value->addrOf.addressedValue),
				}
			};
			return COPY_(&pointerToInnerType);
		}
		case VALUE_KIND_CAST:{
			return value->cast.castTo;
		}
		case VALUE_KIND_PARENS_WRAPPED:{
			return Value_getType(value->parens_wrapped.innerValue);
		}
		case VALUE_KIND_OPERATOR:{
			switch(value->op.op){
				case VALUE_OPERATOR_ADD:{
					// can be int-like, float-like, pointer or array
					Type*left_type=Value_getType(value->op.left);
					while(left_type->kind==TYPE_KIND_REFERENCE){
						left_type=left_type->reference.ref;
					}

					switch(left_type->kind){
						case TYPE_KIND_PRIMITIVE:{
							switch(left_type->primitive){
								case TYPE_PRIMITIVE_KIND_I32:
									return &Type_I32;
								case TYPE_PRIMITIVE_KIND_F32:
									return &Type_F32;
								case TYPE_PRIMITIVE_KIND_F64:
									return &Type_F64;
								default:
									fatal("unreachable");
							}
						}
						case TYPE_KIND_POINTER:
						case TYPE_KIND_ARRAY:
							return left_type;
						default:
							fatal("unreachable %s",Type_asString(left_type));
					}
					break;
				}
				case VALUE_OPERATOR_SUB:
					fatal("unimplemented");
				case VALUE_OPERATOR_MULT:
					fatal("unimplemented");
				case VALUE_OPERATOR_DIV:
					fatal("unimplemented");
				case VALUE_OPERATOR_MODULO:
					fatal("unimplemented");
				
				case VALUE_OPERATOR_ADD_ASSIGN:{
					Type*left_type=Value_getType(value->op.left);
					while(left_type->kind==TYPE_KIND_REFERENCE){
						left_type=left_type->reference.ref;
					}

					switch(left_type->kind){
						case TYPE_KIND_PRIMITIVE:{
							switch(left_type->primitive){
								case TYPE_PRIMITIVE_KIND_I32:
									return &Type_I32;
								case TYPE_PRIMITIVE_KIND_F32:
									return &Type_F32;
								case TYPE_PRIMITIVE_KIND_F64:
									return &Type_F64;
								default:
									fatal("unreachable");
							}
						}
						case TYPE_KIND_POINTER:
						case TYPE_KIND_ARRAY:
							return left_type;
						default:
							fatal("unreachable %s",Type_asString(left_type));
					}
				}
				case VALUE_OPERATOR_SUB_ASSIGN:
					fatal("unimplemented");
				case VALUE_OPERATOR_MULT_ASSIGN:
					fatal("unimplemented");
				case VALUE_OPERATOR_DIV_ASSIGN:
					fatal("unimplemented");
				case VALUE_OPERATOR_MODULO_ASSIGN:
					fatal("unimplemented");
				case VALUE_OPERATOR_BITWISE_AND_ASSIGN:
					fatal("unimplemented");
				case VALUE_OPERATOR_BITWISE_OR_ASSIGN:
					fatal("unimplemented");
				case VALUE_OPERATOR_BITWISE_XOR_ASSIGN:
					fatal("unimplemented");
				case VALUE_OPERATOR_INDEX:
					switch(Value_getType(value->op.left)->kind){
						case TYPE_KIND_ARRAY:
							return Value_getType(value->op.left)->array.base;
						case TYPE_KIND_POINTER:
							return Value_getType(value->op.left)->pointer.base;
						default:
							fatal("unreachable");
					}
				case VALUE_OPERATOR_BITWISE_AND:
					fatal("unimplemented");
				case VALUE_OPERATOR_BITWISE_OR:
					fatal("unimplemented");

				case VALUE_OPERATOR_LOGICAL_AND:
				case VALUE_OPERATOR_LOGICAL_OR:
				case VALUE_OPERATOR_LESS_THAN:
				case VALUE_OPERATOR_GREATER_THAN:
				case VALUE_OPERATOR_LESS_THAN_OR_EQUAL:
				case VALUE_OPERATOR_GREATER_THAN_OR_EQUAL:
				case VALUE_OPERATOR_NOT_EQUAL:
				case VALUE_OPERATOR_EQUAL:
					return &Type_BOOL;
					
				case VALUE_OPERATOR_DOT:
					fatal("unimplemented");
				case VALUE_OPERATOR_ARROW:
					fatal("unimplemented");
				case VALUE_OPERATOR_CALL:
					fatal("unimplemented");
				case VALUE_OPERATOR_CONDITIONAL:
					fatal("unimplemented");
				case VALUE_OPERATOR_LEFT_SHIFT_ASSIGN:
					fatal("unimplemented");
				case VALUE_OPERATOR_RIGHT_SHIFT_ASSIGN:
					fatal("unimplemented");
				case VALUE_OPERATOR_DEREFERENCE:{
					Type*left_type=Value_getType(value->op.left);
					if(left_type->kind!=TYPE_KIND_POINTER){
						fatal("cannot dereference non-pointer type %s",Type_asString(left_type));
					}
					return left_type->pointer.base;
				}

				case VALUE_OPERATOR_ASSIGNMENT:

				case VALUE_OPERATOR_POSTFIX_INCREMENT:
				case VALUE_OPERATOR_POSTFIX_DECREMENT:
				case VALUE_OPERATOR_PREFIX_INCREMENT:
				case VALUE_OPERATOR_PREFIX_DECREMENT:
				case VALUE_OPERATOR_UNARY_PLUS:
				case VALUE_OPERATOR_UNARY_MINUS:
				case VALUE_OPERATOR_LOGICAL_NOT:
				case VALUE_OPERATOR_BITWISE_NOT:
					return Value_getType(value->op.left);

				case VALUE_OPERATOR_UNKNOWN:
					fatal("unreachable");
			}
		}
		case VALUE_KIND_CONDITIONAL:{
			if(!Type_convertibleTo(Value_getType(value->conditional.onTrue),Value_getType(value->conditional.onFalse))){
				fatal("cannot convert %s to %s in conditional",Type_asString(Value_getType(value->conditional.onTrue)),Type_asString(Value_getType(value->conditional.onFalse)));
			}
			return Value_getType(value->conditional.onTrue);
		}
		case VALUE_KIND_ARROW:{
			// get type of dereferenced value
			Type*left_type=Value_getType(value->arrow.left);
			if(left_type->kind!=TYPE_KIND_POINTER){
				fatal("cannot dereference non-pointer type %s",Type_asString(left_type));
			}
			left_type=left_type->pointer.base;
			bool type_contains_field=Type_containsField(left_type,value->arrow.right);
			if(!type_contains_field){
				fatal("type %s does not contain field %.*s",Type_asString(left_type),value->arrow.right->len,value->arrow.right->p);
			}
			break;
		}
		case VALUE_KIND_DOT:{
			Type*left_type=Value_getType(value->dot.left);
			bool type_contains_field=Type_containsField(left_type,value->arrow.right);
			if(!type_contains_field){
				fatal("type %s does not contain field %.*s",Type_asString(left_type),value->arrow.right->len,value->arrow.right->p);
			}
			break;
		}
		default:
			fatal("unimplemented %s",ValueKind_asString(value->kind));
	}
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
					stringAppend(ret,"(%s) ADD (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_SUB:{
					stringAppend(ret,"(%s) SUB (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_MULT:{
					stringAppend(ret,"(%s) MULT (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_DIV:{
					stringAppend(ret,"(%s) DIV (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_MODULO:{
					stringAppend(ret,"(%s) MODULO (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_ASSIGNMENT:{
					stringAppend(ret,"(%s) ASSIGN (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_LESS_THAN:{
					stringAppend(ret,"(%s) LESS_THAN (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_LESS_THAN_OR_EQUAL:{
					stringAppend(ret,"(%s) LESS_THAN_OR_EQUAL (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_GREATER_THAN:{
					stringAppend(ret,"(%s) GREATER_THAN (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_GREATER_THAN_OR_EQUAL:{
					stringAppend(ret,"(%s) GREATER_THAN_OR_EQUAL (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_POSTFIX_INCREMENT:{
					stringAppend(ret,"POSTFIX_INCREMENT ( %s )",Value_asString(value->op.left));
					break;
				}
				case VALUE_OPERATOR_POSTFIX_DECREMENT:{
					stringAppend(ret,"POSTFIX_DECREMENT ( %s )",Value_asString(value->op.left));
					break;
				}
				case VALUE_OPERATOR_DOT:{
					stringAppend(ret,"(%s) DOT (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_INDEX:{
					stringAppend(ret,"(%s) INDEX (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_LOGICAL_AND:{
					stringAppend(ret,"(%s) LOGICAL_AND (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_LOGICAL_OR:{
					stringAppend(ret,"(%s) LOGICAL_OR (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_BITWISE_AND:{
					stringAppend(ret,"(%s) BITWISE_AND (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_BITWISE_OR:{
					stringAppend(ret,"(%s) BITWISE_OR (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_EQUAL:{
					stringAppend(ret,"(%s) EQUAL (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_NOT_EQUAL:{
					stringAppend(ret,"(%s) NOT_EQUAL (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_LOGICAL_NOT:{
					stringAppend(ret,"LOGICAL_NOT ( %s )",Value_asString(value->op.left));
					break;
				}
				case VALUE_OPERATOR_BITWISE_NOT:{
					stringAppend(ret,"BITWISE_NOT ( %s )",Value_asString(value->op.left));
					break;
				}
				case VALUE_OPERATOR_DEREFERENCE:{
					stringAppend(ret,"DEREFERENCE ( %s )",Value_asString(value->op.left));
					break;
				}
				case VALUE_OPERATOR_ADD_ASSIGN:{
					stringAppend(ret,"(%s) ADD_ASSIGN (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_SUB_ASSIGN:{
					stringAppend(ret,"(%s) SUB_ASSIGN (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_MULT_ASSIGN:{
					stringAppend(ret,"(%s) MULT_ASSIGN (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_DIV_ASSIGN:{
					stringAppend(ret,"(%s) DIV_ASSIGN (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_MODULO_ASSIGN:{
					stringAppend(ret,"(%s) MODULO_ASSIGN (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_BITWISE_AND_ASSIGN:{
					stringAppend(ret," (%s) BITWISE_AND_ASSIGN (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_BITWISE_OR_ASSIGN:{
					stringAppend(ret,"(%s) BITWISE_OR_ASSIGN (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_BITWISE_XOR_ASSIGN:{
					stringAppend(ret,"(%s) BITWISE_XOR_ASSIGN (%s)",Value_asString(value->op.left),Value_asString(value->op.right));
					break;
				}
				case VALUE_OPERATOR_UNARY_MINUS:{
					stringAppend(ret,"UNARY_MINUS ( %s )",Value_asString(value->op.left));
					break;
				}
				case VALUE_OPERATOR_UNARY_PLUS:{
					stringAppend(ret,"UNARY_PLUS ( %s )",Value_asString(value->op.left));
					break;
				}
				case VALUE_OPERATOR_PREFIX_INCREMENT:{
					stringAppend(ret,"PREFIX_INCREMENT ( %s )",Value_asString(value->op.left));
					break;
				}
				case VALUE_OPERATOR_PREFIX_DECREMENT:{
					stringAppend(ret,"PREFIX_DECREMENT ( %s )",Value_asString(value->op.left));
					break;
				}

				default:
					fatal("unimplemented %d",value->op.op);
			}
			break;
		}
		case VALUE_KIND_SYMBOL_REFERENCE:{
			if(value->symbol==nullptr)fatal("");
			if(value->symbol->name==nullptr)fatal("");
			if((uintptr_t)value->symbol->name%8!=0){
				stringAppend(ret,"symbol <unaligned> : %s",Type_asString(value->symbol->type));
			}else{
				stringAppend(ret,"symbol %.*s : %s",value->symbol->name->len,value->symbol->name->p,Type_asString(value->symbol->type));
			}
			break;
		}
		case VALUE_KIND_FUNCTION_CALL:{
			stringAppend(ret,"calling function %s with %d arguments (",Value_asString(value->function_call.function),value->function_call.args.len);
			for(int i=0;i<value->function_call.args.len;i++){
				Value*arg=array_get(&value->function_call.args,i);
				char*arg_str=Value_asString(arg);
				stringAppend(ret,"arg %d: %s,",i,arg_str);
			}
			stringAppend(ret,")");
			break;
		}
		case VALUE_KIND_DOT:{
			stringAppend(ret,"(%s) DOT (%.*s)",Value_asString(value->dot.left),value->dot.right->len,value->dot.right->p);
			break;
		}
		case VALUE_KIND_ARROW:{
			stringAppend(ret,"(%s) ARROW (%.*s)",Value_asString(value->arrow.left),value->arrow.right->len,value->arrow.right->p);
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
			stringAppend(ret,"( %s ) AS ( %s )",Value_asString(value->cast.value),Type_asString(value->cast.castTo));
			break;
		}
		case VALUE_KIND_STRUCT_INITIALIZER:{
			stringAppend(ret,"init struct with fields: ");
			for(int i=0;i<value->struct_initializer.structFields.len;i++){
				struct FieldInitializer*field=array_get(&value->struct_initializer.structFields,i);
				stringAppend(ret,"field ");

				for(int j=0;j<field->fieldNameSegments.len;j++){
					struct FieldInitializerSegment*segment=array_get(&field->fieldNameSegments,j);
					if(segment->kind==FIELD_INITIALIZER_SEGMENT_INDEX)
						stringAppend(ret,"[ %.*s ]",segment->index->len,segment->index->p);
					else if(segment->kind==FIELD_INITIALIZER_SEGMENT_FIELD)
						stringAppend(ret,".%.*s",segment->field->len,segment->field->p);
				}
				stringAppend(ret," = %s , ",Value_asString(field->value));
			}
			break;
		}
		case VALUE_KIND_TYPEREF:{
			stringAppend(ret,"type %s",Type_asString(value->typeref.type));
			break;
		}
		case VALUE_KIND_CONDITIONAL:{
			stringAppend(ret,"CONDITIONAL ( %s ) ? ( %s ) : ( %s )",Value_asString(value->conditional.condition),Value_asString(value->conditional.onTrue),Value_asString(value->conditional.onFalse));
			break;
		}
		default:
			fatal("unimplemented %s",ValueKind_asString(value->kind));
	}
	return ret;
}

const char* ValueKind_asString(enum VALUE_KIND kind){
	switch(kind){
		case VALUE_KIND_STATIC_VALUE:
			return "VALUE_KIND_STATIC_VALUE";

		case VALUE_KIND_OPERATOR:
			return "VALUE_KIND_OPERATOR";
			
		case VALUE_KIND_SYMBOL_REFERENCE:
			return "VALUE_KIND_SYMBOL_REFERENCE";
		case VALUE_KIND_FUNCTION_CALL:
			return "VALUE_KIND_FUNCTION_CALL";
		case VALUE_KIND_ARROW:
			return "VALUE_KIND_ARROW";
		case VALUE_KIND_DOT:
			return "VALUE_KIND_DOT";
		case VALUE_KIND_ADDRESS_OF:
			return "VALUE_KIND_ADDRESS_OF";
		case VALUE_KIND_STRUCT_INITIALIZER:
			return "VALUE_KIND_STRUCT_INITIALIZER";
		case VALUE_KIND_CAST:
			return "VALUE_KIND_CAST";

		case VALUE_KIND_CONDITIONAL:
			return "VALUE_KIND_CONDITIONAL";

		case VALUE_KIND_TYPEREF:
			return "VALUE_KIND_TYPEREF";

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
