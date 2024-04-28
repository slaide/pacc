#include<stdlib.h>
#include<string.h>
struct Token{int len;char*p;};
typedef struct Token Token;
bool test1(){
	Token;
	struct Token token={.len=0,.p=0};
	struct Token token2=(Token){.len=0,.p=0};
	struct Token token3=(struct Token){.len=0,.p=0};
	struct Token*tokenp1;
	struct Token*tokenp2=&token;
	struct Token*tokenp3=&(Token){.len=0,.p=0};
	struct Token*tokenp4=&(struct Token){.len=0,.p=0};

	return true;
}
