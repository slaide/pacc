#include<parser/parser.h>
#include<util/util.h>
#include<tokenizer.h>

enum MODULE_PARSE_RESULT Module_parse(Module* module,struct TokenIter*token_iter_in){
	array_init(&module->statements,sizeof(Statement));

	struct TokenIter token_iter=*token_iter_in;

	Token token;
	TokenIter_nextToken(&token_iter,&token);
	while(!TokenIter_isEmpty(&token_iter)){
			TokenIter_lastToken(&token_iter,&token);
			Statement statement={};
			enum STATEMENT_PARSE_RESULT res=Statement_parse(&statement,&token_iter);
			switch(res){
				case STATEMENT_PARSE_RESULT_INVALID:
					fatal("invalid statement at line %d col %d",token.line,token.col);
					break;
				case STATEMENT_PARSE_RESULT_PRESENT:
					array_append(&module->statements,&statement);
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
