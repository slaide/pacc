#include "parser/symbol.h"
#include "parser/module.h"
#include "parser/type.h"
#include "util/array.h"
#include<parser/parser.h>
#include<util/util.h>
#include<tokenizer.h>

enum SYMBOL_PARSE_RESULT SymbolDefinition_parse(Module*module,int*num_symbol_defs,struct SymbolDefinition**symbol_defs_out,struct TokenIter*token_iter_in,struct Symbol_parse_options*options){
	struct TokenIter this_iter=*token_iter_in;
	struct TokenIter *token_iter=&this_iter;

	/* temporary store for return value, i.e. item type is struct SymbolDefinition */
	array symbol_defs={};
	array_init(&symbol_defs,sizeof(struct SymbolDefinition));

	// in preparation for multi-statements, a statement may start with a type, and then be succeeded 
	// by definitions of multiple symbols, each one with a type derived from the base type
	// e.g. int a, *b, c[2], d(); , where int would be the base type
	Type base_type={.kind=TYPE_KIND_UNKNOWN};
	Type*current_type=&base_type;

	struct SymbolDefinition _symbol_def={},*symbol_def=&_symbol_def;

	Token token;
	TokenIter_lastToken(token_iter,&token);

	while(1){
		while(1){
			bool type_is_struct=false;
			bool type_is_enum=false;
			bool type_is_union=false;
			/* type is not just referenced by name, but "enum|struct|union name" */
			bool explicit_type_name=false;
			/* type is unnamed, e.g. struct{}a; -> a is symbol name, type is unnamed struct */
			bool unnamedTypeDefinition=false;
			/* type name can be implicit numeric, e.g. short, long, unsigned, signed (which can apply to int, float, double)*/
			bool type_name_can_be_implicit_numeric=false;

			/* check for modifiers */
			if(Token_equalString(&token,"const")){
				TokenIter_nextToken(token_iter,&token);
				base_type.is_const=true;
				continue;
			}
			if(Token_equalString(&token,"static")){
				TokenIter_nextToken(token_iter,&token);
				base_type.is_static=true;
				continue;
			}
			if(Token_equalString(&token,"extern")){
				TokenIter_nextToken(token_iter,&token);
				base_type.is_extern=true;
				continue;
			}
			if(Token_equalString(&token,"thread_local")){
				TokenIter_nextToken(token_iter,&token);
				base_type.is_thread_local=true;
				continue;
			}
			if(Token_equalString(&token,"unsigned")){
				if(base_type.is_signed)fatal("unsigned and signed cannot be combined at %s",Token_print(&token));
				TokenIter_nextToken(token_iter,&token);
				base_type.is_unsigned=true;
				continue;
			}
			if(Token_equalString(&token,"signed")){
				if(base_type.is_unsigned)fatal("unsigned and signed cannot be combined at %s",Token_print(&token));
				TokenIter_nextToken(token_iter,&token);
				base_type.is_signed=true;
				continue;
			}
			if(Token_equalString(&token,"short")){
				if(base_type.size_mod>0)fatal("short and long cannot be combined at %s",Token_print(&token));
				if(base_type.size_mod==-2)fatal("short short short is not supported at %s",Token_print(&token));
				TokenIter_nextToken(token_iter,&token);
				base_type.size_mod-=1;
				continue;
			}
			if(Token_equalString(&token,"long")){
				if(base_type.size_mod<0)fatal("short and long cannot be combined at %s",Token_print(&token));
				if(base_type.size_mod==2)fatal("long long long is not supported at %s",Token_print(&token));
				TokenIter_nextToken(token_iter,&token);
				base_type.size_mod+=1;
				continue;
			}
			if(Token_equalString(&token,"_Noreturn")){
				TokenIter_nextToken(token_iter,&token);
				// TODO store this property
				continue;
			}

			// next token is expected to reference a name, can be either symbol name or type name, e.g. int a; -> name may be int, or a
			Token nameToken=token;

			// check if type has prefix: struct,enum,union
			if(Token_equalString(&token,"struct"))
				type_is_struct=true;
			if(Token_equalString(&token,"enum"))
				type_is_enum=true;
			if(Token_equalString(&token,"union"))
				type_is_union=true;
			explicit_type_name=type_is_struct||type_is_enum||type_is_union;
			if(explicit_type_name){
				TokenIter_nextToken(token_iter,&token);

				if(Token_isValidIdentifier(&token)){
					nameToken=token;
					unnamedTypeDefinition=false;
					TokenIter_nextToken(token_iter,&token);
				}else{
					nameToken=(Token){};
					unnamedTypeDefinition=true;
				}

				if(Token_equalString(&token,"{")){
					TokenIter_nextToken(token_iter,&token);

					if(type_is_struct){
						array structMembers={};
						array_init(&structMembers,sizeof(Symbol));

						// parse struct
						while(!Token_equalString(&token,"}")){
							int num_members=0;
							struct SymbolDefinition *members=nullptr;
							enum SYMBOL_PARSE_RESULT symres=SymbolDefinition_parse(module,&num_members,&members,token_iter,&(struct Symbol_parse_options){});
							if(symres==SYMBOL_INVALID){
								fatal("invalid symbol in struct at %s",Token_print(&token));
							}
							for(int i=0;i<num_members;i++){
								array_append(&structMembers,&members[i].symbol);
							}
							TokenIter_lastToken(token_iter,&token);
							
							// check for semicolon
							if(!Token_equalString(&token,";")){
								fatal("expected semicolon after member declaration at %s",Token_print(&token));
							}
							TokenIter_nextToken(token_iter,&token);
						}
						TokenIter_nextToken(token_iter,&token);

						base_type.kind=TYPE_KIND_STRUCT;
						base_type.struct_.name=unnamedTypeDefinition?nullptr:allocAndCopy(sizeof(Token),&nameToken);
						base_type.struct_.members=structMembers;
					}else if(type_is_union){
						array unionMembers={};
						array_init(&unionMembers,sizeof(Symbol));

						// parse union
						while(!Token_equalString(&token,"}")){
							int num_members=0;
							struct SymbolDefinition *members=nullptr;
							enum SYMBOL_PARSE_RESULT symres=SymbolDefinition_parse(module,&num_members,&members,token_iter,&(struct Symbol_parse_options){});
							if(symres==SYMBOL_INVALID){
								fatal("invalid symbol in struct at %s",Token_print(&token));
							}
							for(int i=0;i<num_members;i++){
								array_append(&unionMembers,&members[i].symbol);
							}
							TokenIter_lastToken(token_iter,&token);
							
							// check for semicolon
							if(!Token_equalString(&token,";")){
								fatal("expected semicolon after member declaration");
							}
							TokenIter_nextToken(token_iter,&token);
						}
						TokenIter_nextToken(token_iter,&token);

						base_type.kind=TYPE_KIND_UNION;
						base_type.union_.name=unnamedTypeDefinition?nullptr:allocAndCopy(sizeof(Token),&nameToken);
						base_type.union_.members=unionMembers;
					}else if(type_is_enum){
						array enumMembers={};
						array_init(&enumMembers,sizeof(Value));

						// parse enum
						while(!Token_equalString(&token,"}")){
							Value member={};
							enum VALUE_PARSE_RESULT valres=Value_parse(module,&member,token_iter);
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

						base_type.kind=TYPE_KIND_ENUM;
						base_type.enum_.name=unnamedTypeDefinition?nullptr:allocAndCopy(sizeof(Token),&nameToken);
						base_type.enum_.members=enumMembers;
					}
					else fatal("");
					
					continue;
				}

				if(unnamedTypeDefinition)fatal("unnamed type declaration is not allowed at %s",Token_print(&token));

				if(type_is_struct){
					base_type.kind=TYPE_KIND_STRUCT;
					base_type.struct_.name=allocAndCopy(sizeof(Token),&nameToken);
				}else if(type_is_enum){
					base_type.kind=TYPE_KIND_ENUM;
					base_type.enum_.name=allocAndCopy(sizeof(Token),&nameToken);
				}else if(type_is_union){
					base_type.kind=TYPE_KIND_UNION;
					base_type.union_.name=allocAndCopy(sizeof(Token),&nameToken);
				}else fatal("bug");
				
				continue;
			}

			type_name_can_be_implicit_numeric=(base_type.is_unsigned || base_type.is_signed || (base_type.size_mod!=0)) && base_type.kind==TYPE_KIND_UNKNOWN;

			// we are holding a name. it can either be a type name or a symbol name
			// if we do not have a type yet, i.e. base_type.kind==TYPE_KIND_UNKNOWN, we assume it is a type name
			// but we check if it is a known type to confirm this
			if(base_type.kind==TYPE_KIND_UNKNOWN){
				Type*type=Module_findType(module,&nameToken);
				if(type_name_can_be_implicit_numeric){
					// check if typename is float, double or int, which are the only types to which this can apply
					if(type!=nullptr){
						bool name_is_int=Token_equalString(type->name,"int");
						bool name_is_char=Token_equalString(type->name,"char");
						bool name_is_float=Token_equalString(type->name,"float");
						bool name_is_double=Token_equalString(type->name,"double");
						if(name_is_int || name_is_char || name_is_float || name_is_double){
							base_type.kind=TYPE_KIND_REFERENCE;
							base_type.reference.ref=type;
							TokenIter_nextToken(token_iter,&token);
							continue;
						}
					}

					// if neither of these names is present, it is implicitely int
					type=Module_findType(module,&(Token){.len=3,.p="int"});
					if(type==nullptr)fatal("bug");
					base_type.kind=TYPE_KIND_REFERENCE;
					base_type.reference.ref=type;
					if(base_type.reference.ref==nullptr)fatal("bug");
					continue;
				}
				if(type!=nullptr){
					// referencing the found type, which we need to copy
					base_type.kind=TYPE_KIND_REFERENCE;
					base_type.reference.ref=type;
					TokenIter_nextToken(token_iter,&token);
					continue;
				}

				// print all types in module
				/*for(int i=0;i<module->types.len;i++){
					Type*type=*(Type**)array_get(&module->types,i);
					println("module type %d is %s",i,Type_asString(type));
				}*/
				
				if(symbol_defs.len==0)
					goto SYMBOL_PARSE_RET_FAILURE;
				goto SYMBOL_PARSE_RET_SUCCESS;
			}

			if(Token_isValidIdentifier(&token) /* [implicit] && base_type.kind!=TYPE_KIND_UNKNOWN */){
				if(symbol_def->symbol.name==nullptr){
					symbol_def->symbol.name=allocAndCopy(sizeof(Token),&token);
					TokenIter_nextToken(token_iter,&token);
				}else{
					goto SYMBOL_PARSE_RET_FAILURE;
				}

				continue;
			}

			// check for pointer
			if(base_type.kind!=TYPE_KIND_UNKNOWN && Token_equalString(&token,"*")){
				TokenIter_nextToken(token_iter,&token);

				current_type=allocAndCopy(sizeof(Type),&(Type){
					.kind=TYPE_KIND_POINTER,
					.pointer={
						.base=allocAndCopy(sizeof(Type),current_type),
					},
				});

				continue;
			}

			// check for function (also, function pointer)
			if(base_type.kind!=TYPE_KIND_UNKNOWN && Token_equalString(&token,"(")){
				TokenIter_nextToken(token_iter,&token);

				// next token could now be 1) * -> function pointer, 2) [potentially empty] arg list
				// 1)
				bool is_function_pointer=Token_equalString(&token,"*");
				if(is_function_pointer){
					TokenIter_nextToken(token_iter,&token);

					if(Token_isValidIdentifier(&token)){
						if(symbol_def->symbol.name==nullptr){
							symbol_def->symbol.name=allocAndCopy(sizeof(Token),&token);
							TokenIter_nextToken(token_iter,&token);
						}else{
							goto SYMBOL_PARSE_RET_FAILURE;
						}
					}

					// check for trailing )
					if(!Token_equalString(&token,")")){
						goto SYMBOL_PARSE_RET_FAILURE;
					}
					TokenIter_nextToken(token_iter,&token);

					// check for argument list after function pointer
					if(!Token_equalString(&token,"(")){
						goto SYMBOL_PARSE_RET_FAILURE;
					}
					TokenIter_nextToken(token_iter,&token);
				}
				
				// 2) parse function arguments
				array args;
				array_init(&args,sizeof(Symbol));
				bool arg_list_should_end=false;
				while(1){
					if(Token_equalString(&token,")")){
						TokenIter_nextToken(token_iter,&token);
						break;
					}
					if(arg_list_should_end)fatal("expected end of argument list at %s",Token_print(&token));
					if(Token_equalString(&token,",")){
						TokenIter_nextToken(token_iter,&token);
						continue;
					}

					int num_arguments=0;
					struct SymbolDefinition *argument=nullptr;
					enum SYMBOL_PARSE_RESULT symres=SymbolDefinition_parse(module,&num_arguments,&argument,token_iter,&(struct Symbol_parse_options){.forbid_multiple=true});
					TokenIter_lastToken(token_iter,&token);
					if(symres==SYMBOL_INVALID){
						// check for varargs
						if(Token_equalString(&token,"...")){
							TokenIter_nextToken(token_iter,&token);
							Symbol vararg_argument=(Symbol){
								.kind=SYMBOL_KIND_VARARG,
								.type=Module_findType(module, &(Token){.len=strlen("__builtin_va_list"),.p="__builtin_va_list"}),
							};
							array_append(&args,&vararg_argument);

							continue;
						}

						fatal("invalid symbol in function argument list at %s",Token_print(&token));
					}

					for(int i=0;i<num_arguments;i++){
						array_append(&args,&argument[i].symbol);
					}
				}

				current_type=allocAndCopy(sizeof(Type),&(Type){
					.kind=TYPE_KIND_FUNCTION,
					.function={
						.ret=allocAndCopy(sizeof(Type), current_type),
						.args=args,
					},
				});

				if(is_function_pointer){
					current_type=allocAndCopy(sizeof(Type),&(Type){
						.kind=TYPE_KIND_POINTER,
						.pointer={
							.base=allocAndCopy(sizeof(Type),current_type),
						},
					});
				}

				break;
			}

			// check for array
			if(base_type.kind!=TYPE_KIND_UNKNOWN && Token_equalString(&token,"[")){
				TokenIter_nextToken(token_iter,&token);

				// test for static
				bool typeLenIsStatic=false;
				if(Token_equalString(&token,"static")){
					TokenIter_nextToken(token_iter,&token);
					typeLenIsStatic=true;
				}

				Value array_len={};
				enum VALUE_PARSE_RESULT res=Value_parse(module,&array_len,token_iter);
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

				current_type=allocAndCopy(sizeof(Type),&(Type){
					.kind=TYPE_KIND_ARRAY,
					.array={
						.base=allocAndCopy(sizeof(Type),current_type),
						.len=arrayLen,
						.is_static=typeLenIsStatic,
					},
				});

				// check for trailing ]
				if(!Token_equalString(&token,"]")){
					goto SYMBOL_PARSE_RET_FAILURE;
				}
				TokenIter_nextToken(token_iter,&token);

				continue;
			}

			// check for initialization
			if(Token_equalString(&token,"=")){
				if(options->allow_initializers==false){
					fatal("initializers are not allowed at %s",Token_print(&token));
				}

				TokenIter_nextToken(token_iter,&token);
				Value value={};
				enum VALUE_PARSE_RESULT res=Value_parse(module,&value,token_iter);
				TokenIter_lastToken(token_iter,&token);
				switch(res){
					case VALUE_INVALID:
						fatal("invalid value after assignment operator at %s",Token_print(&token));
						break;
					case VALUE_PRESENT:
						symbol_def->initializer=allocAndCopy(sizeof(Value),&value);
						break;
				}
			}

			break;
		}

		if(base_type.kind==TYPE_KIND_UNKNOWN){
			goto SYMBOL_PARSE_RET_FAILURE;
		}

		// append symbol to output
		symbol_def->symbol.type=allocAndCopy(sizeof(Type), current_type);

		array_append(&symbol_defs,symbol_def);

		// if next token is a comma, fatal
		if((!options->forbid_multiple) &&  Token_equalString(&token,",")){
			TokenIter_nextToken(token_iter,&token);
			// zero out symbol again
			*symbol_def=(struct SymbolDefinition){};
			continue;
		}
		break; // TODO: remove this break when multi-symbol declarations are implemented
	}

SYMBOL_PARSE_RET_SUCCESS:
	*num_symbol_defs=symbol_defs.len;
	*symbol_defs_out=symbol_defs.data;

	*token_iter_in=this_iter;
	return SYMBOL_PRESENT;

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
