#include<parser/parser.h>
#include<util/util.h>
#include<tokenizer.h>

void Module_init(Module*module){
	array_init(&module->statements,sizeof(Statement*));
	array_init(&module->types,sizeof(Type*));

	// add basic types
	static const char* basicTypes[]={
		"int",
		"char",
		"void",
		"float",
		"double",
		// the actual type is implementation defined
		// pointer-like though, it seems
		// gets aliased as va_list in stdarg.h
		"__builtin_va_list",
	};
	static const int num_types=sizeof(basicTypes)/sizeof(basicTypes[0]);

	for(int i=0;i<num_types;i++){
		Type*type=allocAndCopy(sizeof(Type),&(Type){
			.name=allocAndCopy(sizeof(Token),&(Token){.p=basicTypes[i],.len=strlen(basicTypes[i])}),
			.kind=TYPE_KIND_PRIMITIVE,
		});
		array_append(&module->types,&type);
	}
}
struct Type* Module_findType(Module*module,Token*name){
	for(int i=0;i<module->types.len;i++){
		Type*type=*(Type**)array_get(&module->types,i);
		if(Token_equalToken(name,type->name)){
			return type;
		}
	}
	return nullptr;
}
enum MODULE_PARSE_RESULT Module_parse(Module* module,struct TokenIter*token_iter_in){
	struct TokenIter token_iter=*token_iter_in;

	Token token;
	TokenIter_nextToken(&token_iter,&token);
	while(!TokenIter_isEmpty(&token_iter)){
		TokenIter_lastToken(&token_iter,&token);
		Statement statement={};
		enum STATEMENT_PARSE_RESULT res=Statement_parse(module,&statement,&token_iter);
		switch(res){
			case STATEMENT_PARSE_RESULT_INVALID:
				fatal("invalid statement at line %d col %d",token.line,token.col);
				break;
			case STATEMENT_PARSE_RESULT_PRESENT:
				{
					Statement*newStatement=allocAndCopy(sizeof(Statement),&statement);
					array_append(&module->statements,&newStatement);
				}
				continue;
		}

		fatal("leftover tokens at end of file. next token is: line %d col %d %.*s",token.line,token.col,token.len,token.p);
	}

	*token_iter_in=token_iter;
	return MODULE_PRESENT;
}
bool Module_equal(Module*a,Module*b){
	if(a->statements.len!=b->statements.len){
		println("statement count mismatch %d %d",a->statements.len,b->statements.len);
		return false;
	}

	for(int i=0;i<a->statements.len;i++){
		Statement*sa=array_get(&a->statements,i);
		Statement*sb=array_get(&b->statements,i);
		if(!Statement_equal(sa,sb)){
			println("statement %d mismatch",i);
			return false;
		}
	}
	return true;
}
