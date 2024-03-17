#include<parser.h>
#include<tokenizer.h>

typedef struct Statement{
	enum{
		STATEMENT_PREP_DEFINE,
		STATEMENT_PREP_INCLUDE,
	}tag;
	union{

	};
}Statement;

void Module_parse(Module* module,Tokenizer*tokenizer){
	module->num_statements=0;

	struct TokenIter token_iter;
	TokenIter_init(&token_iter,tokenizer,(struct TokenIterConfig){.skip_comments=true,});

	Token token;
	while((TokenIter_nextToken(&token_iter,&token))){
		if(token.p==KEYWORD_HASH){
			TokenIter_nextToken(&token_iter,&token);
			println("got preprocessor directive %.*s",token.len,token.p);
			if(Token_symbolEqual(&token,"include")){}
		}
		//println("got token (%d) %.*s",token.len,token.len,token.p);
	}
}
