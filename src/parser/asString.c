#include<parser/parser.h>
#include<util/util.h>

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
const char* Statementkind_asString(enum STATEMENT_KIND kind){
	switch(kind){
		case STATEMENT_UNKNOWN:
			return("STATEMENT_UNKNOWN");
		case STATEMENT_PREP_DEFINE:
			return("STATEMENT_PREP_DEFINE");
		case STATEMENT_PREP_INCLUDE:
			return("STATEMENT_PREP_INCLUDE");
		case STATEMENT_FUNCTION_DECLARATION:
			return("STATEMENT_FUNCTION_DECLARATION");
		case STATEMENT_FUNCTION_DEFINITION:
			return("STATEMENT_FUNCTION_DEFINITION");
		case STATEMENT_RETURN:
			return("STATEMENT_RETURN");
		case STATEMENT_IF:
			return("STATEMENT_IF");
		case STATEMENT_SWITCH:
			return("STATEMENT_SWITCH");
		case STATEMENT_CASE:
			return("STATEMENT_CASE");
		case STATEMENT_BREAK:
			return("STATEMENT_BREAK");
		case STATEMENT_CONTINUE:
			return("STATEMENT_CONTINUE");
		case STATEMENT_DEFAULT:
			return("STATEMENT_DEFAULT");
		case STATEMENT_GOTO:
			return("STATEMENT_GOTO");
		case STATEMENT_LABEL:
			return("STATEMENT_LABEL");
		case STATEMENT_WHILE:
			return("STATEMENT_WHILE");
		case STATEMENT_FOR:
			return("STATEMENT_FOR");
		default:
			fatal("unknown symbol kind %d",kind);
	}
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

void Type_print(Type* type){
	Type* type_ref=type;
	bool printing_done=false;
	while(!printing_done){
		if(type_ref->is_const)
			println("const ");

		switch(type_ref->kind){
			case TYPE_KIND_REFERENCE:
				println("referencing type %.*s",type_ref->reference.len,type_ref->reference.p);
				printing_done=true;
				break;
			case TYPE_KIND_POINTER:
				println("pointer to ");
				if(type_ref->pointer.base==type_ref)
					fatal("pointer to self");
				type_ref=type_ref->pointer.base;
				break;
			case TYPE_KIND_ARRAY:
				println("array");
				type_ref=type_ref->array.base;
				break;
			case TYPE_KIND_FUNCTION:
				println("function");
				println("return type is");
				Type_print(type_ref->function.ret);

				for(int i=0;i<type_ref->function.args.len;i++){
					Symbol* arg=array_get(&type_ref->function.args,i);
					println("argument #%d called %.*s is of type",i,arg->name->len,arg->name->p);
					Type_print(arg->type);
				}
				printing_done=true;
				break;
			default:
				fatal("unknown type kind %d",type_ref->kind);
		}
	}
}