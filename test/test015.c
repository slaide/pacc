#include <util/util.h>

bool test1(){
	Type* intType=allocAndCopy(sizeof(Type),&(Type){
		.kind=TYPE_KIND_REFERENCE,
		.reference=(Token){.len=3,.p="int",},
	});

	struct TokenIter token_iter;
	TokenIter_init(&token_iter,&tokenizer,(struct TokenIterConfig){.skip_comments.dothat=true,});

	return true;
}
// TESTGOAL casting expression on struct initializer