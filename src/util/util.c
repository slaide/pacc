#include<util/util.h>

#include<stdarg.h>
#include<stdlib.h>
#include<string.h>

void* allocAndCopy(size_t size,const void*src){
    void*mem=malloc(size);
    if(!mem)
        return nullptr;

    memcpy(mem,src,size);
    return mem;
}

char* makeStringn(int len){
    char*str=calloc(len,1);
    return str;
}
char* makeString(){
    return makeStringn(4096);
}
void stringAppend(char*str,char*fmtstr,...){
    va_list args;
    va_start(args,fmtstr);
    vsprintf(str+strlen(str),fmtstr,args);
    va_end(args);
}
char*ind(int n){
    char*str=makeStringn(n+1);
    for(int i=0;i<n;i++){
        str[i]=' ';
    }
    str[n]='\0';
    return str;
}
