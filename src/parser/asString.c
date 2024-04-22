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
