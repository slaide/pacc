#include<parser/parser.h>
#include<util/util.h>
#include<tokenizer.h>
#include<ctype.h> // isalpha, isalnum


bool Token_isValidIdentifier(Token*token){
	if(token->tag!=TOKEN_TAG_SYMBOL)
		return false;

	if(token->len==0)
		return false;

	if(!isalpha(token->p[0])&&token->p[0]!='_')
		return false;

	for(int i=1;i<token->len;i++){
		if(!isalnum(token->p[i])&&token->p[i]!='_')
			return false;
	}

	return true;
}
