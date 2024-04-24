#include<stdio.h>
static const int NUM_KEYWORDS=14;
const int static const static a;
static thread_local const static int tg;
int inc(){
    thread_local static int t=0;
    return t++;
}
int main(void){
    for(int i=0;i<NUM_KEYWORDS;i++){
        printf("%d\n",inc());
    }
    return 0;
}
static const int const static thread_local c;
// TESTFOR unnamed function arguments, const type qualifiers, static and thread_local storage class specifier