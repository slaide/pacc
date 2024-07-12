#include<util/util.h>
#include<parser/stack.h>
#include<parser/statement.h>
#include<parser/symbol.h>
#include<tokenizer.h>

void Stack_init(Stack*stack,Stack*parent){
    Stack ret=(Stack){
        .symbols={}, // init below
        .parent=parent,
        .types={}, // init below
        .statements={}, // init below
    };
    array_init(&ret.symbols,sizeof(Symbol*));
    array_init(&ret.types,sizeof(Type*));
    array_init(&ret.statements,sizeof(Statement));

    *stack=ret;
}
void Stack_addSymol(Stack*stack,Symbol*symbol){
    if(symbol==nullptr)fatal("bug");
    Symbol*sym_copy=COPY_(symbol);
    array_append(&stack->symbols,&sym_copy);
}
Symbol* Stack_findSymbol(Stack*stack,Token*name){
    for(int i=0;i<stack->symbols.len;i++){
        Symbol*sym=*(Symbol**)array_get(&stack->symbols,i);
        if(sym->name==nullptr)continue;
        if(Token_equalToken(sym->name,name)){
            return sym;
        }
    }
    if(stack->parent){
        return Stack_findSymbol(stack->parent,name);
    }
    return nullptr;
}
struct Type* Stack_findType(Stack*stack,Token*name){
	for(int i=0;i<stack->types.len;i++){
		Type*type=*(Type**)array_get(&stack->types,i);

		if(Token_equalToken(name,type->name)){
			return type;
		}
	}
    if(stack->parent){
        return Stack_findType(stack->parent,name);
    }
	return nullptr;
}
// process type of symbol, which may be named and then be usable by reference from other symbols
void Stack_ingestSymbolType(Stack*stack,Type*type){
    if(type==nullptr)fatal("bug");
    switch(type->kind){
        case TYPE_KIND_REFERENCE:{
            Type*ref=type->reference.ref;
            return Stack_ingestSymbolType(stack,ref);
        }
        case TYPE_KIND_ARRAY:{
            Type*ref=type->array.base;
            return Stack_ingestSymbolType(stack,ref);
        }
        case TYPE_KIND_UNION:{
            // insert struct type if not already present
            Type*existing_type=Stack_findType(stack,type->name);
            if(existing_type==nullptr && type->union_.name!=nullptr){
                Type*t_copy=COPY_(type);
                t_copy->name=type->union_.name;
                array_append(&stack->types,&t_copy);
            }

            for(int i=0;i<type->union_.members.len;i++){
                Symbol*field=array_get(&type->union_.members,i);
                Stack_ingestSymbolType(stack,field->type);
            }
            break;
        }
        case TYPE_KIND_STRUCT:{
            // insert struct type if not already present
            Type*existing_type=Stack_findType(stack,type->name);
            if(existing_type==nullptr && type->struct_.name!=nullptr){
                Type*t_copy=COPY_(type);
                t_copy->name=type->struct_.name;
                array_append(&stack->types,&t_copy);
            }

            for(int i=0;i<type->struct_.members.len;i++){
                Symbol*field=array_get(&type->struct_.members,i);
                Stack_ingestSymbolType(stack,field->type);
            }
            break;
        }
        case TYPE_KIND_ENUM:{
            // insert struct type if not already present
            Type*existing_type=Stack_findType(stack,type->name);
            if(existing_type==nullptr && type->enum_.name!=nullptr){
                Type*t_copy=COPY_(type);
                t_copy->name=type->enum_.name;
                array_append(&stack->types,&t_copy);
            }
            
            for(int i=0;i<type->enum_.members.len;i++){
                struct EnumVariant*field=array_get(&type->enum_.members,i);
                // add symbol of type int to stack symbols
                Symbol newsym={
                    .name=COPY_(field->name),
                    .type=Stack_findType(stack,Token_fromString("int")),
                };
                if(newsym.type==nullptr)fatal("bug");
                Stack_addSymol(stack,&newsym);
            }
            break;
        }
        default:return;
    }
}
enum STACK_PARSE_RESULT Stack_parse(Stack*stack,struct TokenIter*token_iter_in){
    struct TokenIter token_iter=*token_iter_in;
    
	Token token;
	TokenIter_nextToken(&token_iter,&token);
	while(!TokenIter_isEmpty(&token_iter)){
		TokenIter_lastToken(&token_iter,&token);
		Statement statement={};
		enum STATEMENT_PARSE_RESULT res=Statement_parse(stack,&statement,&token_iter);
		switch(res){
			case STATEMENT_PARSE_RESULT_INVALID:
				fatal("invalid statement at line %d col %d",token.line,token.col);
				break;
			case STATEMENT_PARSE_RESULT_PRESENT:
                Stack_addStatement(stack, &statement);
				continue;
		}

		fatal("leftover tokens at end of file. next token is: line %d col %d %.*s",token.line,token.col,token.len,token.p);
	}

    *token_iter_in=token_iter;
    return STACK_PRESENT;
}

void Stack_validateValue(Stack*stack,Value*value){
    if(value->kind==VALUE_KIND_UNKNOWN) fatal("unreachable");
    Type*value_type=Value_getType(value);

    switch(value->kind){
        case VALUE_KIND_OPERATOR:{
            switch(value->op.op){
                case VALUE_OPERATOR_CALL:fatal("unreachable");
                default:;
            }

            break;
        }
        case VALUE_KIND_FUNCTION_CALL:{
            {
                Type*function_type=Value_getType(value->function_call.function);
                if(function_type->kind!=TYPE_KIND_FUNCTION){
                    fatal("cannot call non-function type %s",Type_asString(value->typeref.type));
                }
                array values=value->function_call.args;

                bool func_has_vararg=false;
                // check if last arg is vararg
                if(function_type->function.args.len>0){
                    Symbol*last_arg=array_get(&function_type->function.args,function_type->function.args.len-1);
                    func_has_vararg=last_arg->kind==SYMBOL_KIND_VARARG;
                }
                if(func_has_vararg){
                    if(function_type->function.args.len-1 > values.len){
                        fatal(
                            "expected at least %d arguments but got %d at %s",
                            function_type->function.args.len-1,
                            values.len,
                            "who know where.. (TODO)" // TODO(patrick)
                        );
                    }
                }else{
                    if(function_type->function.args.len!=values.len){
                        fatal(
                            "expected %d arguments but got %d at %s",
                            function_type->function.args.len,values.len,
                            "who know where.. (TODO)" // TODO(patrick)
                        );
                    }
                }

                // check argument types
                for(int i=0;i<values.len;i++){
                    Value*arg_value=array_get(&values,i);
                    // get func arg
                    Symbol*func_arg=nullptr;
                    if(i<function_type->function.args.len){
                        func_arg=array_get(&function_type->function.args,i);
                    }else{
                        if(!func_has_vararg){
                            // should be unreachable
                            fatal("unreachable");
                        }
                        func_arg=array_get(&function_type->function.args,function_type->function.args.len-1);
                    }

                    if(func_arg->type==nullptr)fatal("unreachable");
                    if(Value_getType(arg_value)==nullptr)fatal("unreachable %d",i);
                    bool can_assign=Type_convertibleTo(Value_getType(arg_value),func_arg->type);
                    if(!can_assign){
                        fatal(
                            "cannot assign %s to %s in function call at %s",
                            Type_asString(Value_getType(arg_value)),
                            Type_asString(func_arg->type),
                            "who know where.. (TODO)" // TODO(patrick)
                        );
                    }
                }
                break;
            }
        }
        default:;
    }
}

/* ingest statement and process to adjust stack state

e.g. add symbol definitions
*/
void Stack_ingestStatements(Stack*stack,Statement*statement){
    switch(statement->tag){
        case STATEMENT_KIND_TYPEDEF:{
            // append typedef to stack
            for(int i=0;i<statement->typedef_.symbols.len;i++){
                Symbol*sym=array_get(&statement->typedef_.symbols,i);
                if(sym==nullptr)fatal("bug");
                // this is allowed, but not useful
                if(sym->name==nullptr)continue;
                if(sym->type==nullptr)fatal("bug");

                Type t_copy={
                    .name=sym->name,
                    .kind=TYPE_KIND_REFERENCE,
                    .reference={
                        .ref=sym->type
                    }
                };
                Type*type=COPY_(&t_copy);
                array_append(&stack->types,&type);
            }
            break;
        }
        case STATEMENT_KIND_FUNCTION_DEFINITION:
            // statement->functionDef.stack.
            break;
        case STATEMENT_KIND_SYMBOL_DEFINITION:
            {
                for(int i=0;i<statement->symbolDef.symbols_defs.len;i++){
                    struct SymbolDefinition*sym_def=array_get(&statement->symbolDef.symbols_defs,i);
                    if(sym_def==nullptr)fatal("bug");

                    Stack_ingestSymbolType(stack,sym_def->symbol.type);

                    Symbol*newsym=COPY_(&sym_def->symbol);
                    Stack_addSymol(stack,&sym_def->symbol);
                }
            }
            break;
            
        case STATEMENT_KIND_VALUE:
            Stack_validateValue(stack,statement->value.value);
            break;

        case STATEMENT_KIND_RETURN:
            if(statement->return_.retval!=nullptr){
                Stack_validateValue(stack,statement->return_.retval);
            }
            break;

        case STATEMENT_KIND_EMPTY:
        case STATEMENT_KIND_BREAK:
        case STATEMENT_KIND_CONTINUE:
            break;

        case STATEMENT_KIND_WHILE:
            if(statement->whileLoop.condition==nullptr)fatal("");
            Stack_validateValue(stack,statement->whileLoop.condition);
            if(statement->whileLoop.body==nullptr)fatal("");
            Stack_ingestStatements(stack,statement->whileLoop.body);
            break;
        case STATEMENT_KIND_IF:
            if(statement->if_.condition==nullptr)fatal("");
            Stack_validateValue(stack,statement->if_.condition);
            if(statement->if_.body==nullptr)fatal("");
            Stack_ingestStatements(stack,statement->if_.body);
            if(statement->if_.elseBody!=nullptr){
                Stack_ingestStatements(stack,statement->if_.elseBody);
            }
            break;
        case STATEMENT_KIND_FOR:
        case STATEMENT_KIND_SWITCH:
        case STATEMENT_KIND_BLOCK:
        case STATEMENT_KIND_DEFAULT:
        case STATEMENT_KIND_GOTO:
        case STATEMENT_KIND_LABEL:
        case STATEMENT_KIND_SWITCHCASE:
            break;

        case STATEMENT_KIND_UNKNOWN:
            fatal("undefined statement");
            
        default:
            fatal("unhandled statement kind %d",statement->tag);
    }
}
void Stack_addStatement(Stack*stack,Statement*statement){
    Stack_ingestStatements(stack,statement);

    array_append(&stack->statements,COPY_(statement));
}

void Stack_addType(Stack*stack,Type*type){
    if(type->name==nullptr)fatal("bug");
    Type*t_copy=COPY_(type);
    array_append(&stack->types,&t_copy);
}
