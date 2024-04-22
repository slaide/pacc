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
