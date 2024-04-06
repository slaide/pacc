#include<parser/parser.h>
#include<util/util.h>
#include<tokenizer.h>
#include<ctype.h> // isalpha, isalnum

void Type_init(Type**type){
	*type=allocAndCopy(sizeof(Type),&(Type){});
}
