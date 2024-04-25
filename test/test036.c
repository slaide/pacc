#include <stdarg.h>
int funcWithVarargArguments(int a,char*b,...){
    // var arg args are ints here, so we calculate the sum and return that+a, if b is not a nullptr
    if(b!=nullptr){
        int sum=a;
        va_list args;
        va_start(args,b);
        while(1){
            int arg=va_arg(args,int);
            if(arg==0){
                break;
            }
            sum+=arg;
        }
        va_end(args);
        return sum;
    }
    return 0;
}
// TESTFOR vararg function arguments
