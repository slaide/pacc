#include <stdio.h>
enum Language{
    LANGUAGE_ENGLISH,
    LANGUAGE_GERMAN,
    LANGUAGE_SPANISH
};
const char* test(enum Language a){
    switch(a){
        case LANGUAGE_ENGLISH:
            goto RET_0;
        case LANGUAGE_GERMAN:
            goto RET_1;
        case LANGUAGE_SPANISH:
            goto RET_2;
    }
    return "unknown language";

RET_0:
    return "hello";
RET_1:
    return "hallo";
RET_2:
    return "hola";
}
int main(){
    printf("%s\n",test(LANGUAGE_ENGLISH));
    printf("%s\n",test(LANGUAGE_GERMAN));
    printf("%s\n",test(LANGUAGE_SPANISH));
    return 0;
}