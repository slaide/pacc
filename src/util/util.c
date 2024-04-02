#include<util/util.h>

#include<stdlib.h>
#include<string.h>

void* allocAndCopy(size_t size,const void*src){
    void*mem=malloc(size);
    if(!mem)
        return nullptr;

    memcpy(mem,src,size);
    return mem;
}
