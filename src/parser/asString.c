#include<parser/parser.h>
#include <string.h>
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
		case STATEMENT_KIND_RETURN:
			return("STATEMENT_KIND_RETURN");
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
		case STATEMENT_KIND_WHILE:
			return("STATEMENT_KIND_WHILE");
		case STATEMENT_KIND_FOR:
			return("STATEMENT_KIND_FOR");
		case STATEMENT_KIND_SYMBOL_DEFINITION:
			return("STATEMENT_KIND_SYMBOL_DEFINITION");
		case STATEMENT_VALUE:
			return("STATEMENT_VALUE");
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

char* Type_asString(Type* type){
	Type* type_ref=type;
	bool printing_done=false;

	char*ret=malloc(1024);
	memset(ret,0,1024);
	char*ret_ptr=ret;
	while(!printing_done){
		if(type_ref->is_const){
			sprintf(ret_ptr,"const ");
			ret_ptr=ret+strlen(ret);
		}

		switch(type_ref->kind){
			case TYPE_KIND_REFERENCE:
				sprintf(ret_ptr,"%.*s",type_ref->reference.len,type_ref->reference.p);
				ret_ptr=ret+strlen(ret);

				printing_done=true;
				break;
			case TYPE_KIND_POINTER:
				sprintf(ret_ptr,"pointer to ");
				ret_ptr=ret+strlen(ret);

				if(type_ref->pointer.base==type_ref)
					fatal("pointer to self");

				type_ref=type_ref->pointer.base;
				break;
			case TYPE_KIND_ARRAY:
				sprintf(ret_ptr,"array");
				ret_ptr=ret+strlen(ret);

				type_ref=type_ref->array.base;
				break;
			case TYPE_KIND_FUNCTION:
				sprintf(ret_ptr,"function, return type is %s, with args (",Type_asString(type_ref->function.ret));
				ret_ptr=ret+strlen(ret);

				for(int i=0;i<type_ref->function.args.len;i++){
					Symbol* arg=array_get(&type_ref->function.args,i);
					sprintf(ret_ptr,"argument #%d called %.*s is of type %s,",i,arg->name->len,arg->name->p,Type_asString(arg->type));
					ret_ptr=ret+strlen(ret);
				}
				sprintf(ret_ptr,")");
				ret_ptr=ret+strlen(ret);
				printing_done=true;
				break;
			default:
				fatal("unknown type kind %d",type_ref->kind);
		}
	}

	return ret;
}
