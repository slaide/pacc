#include<stdlib.h>
#include<string.h>

#include<util/array.h>

void array_init(array*a,int elem_size){
    *a=(array){.data=0,.elem_size=elem_size,.len=0,.cap=0};
}
void array_free(array*a){
    free(a->data);
    a->data=0;
    a->len=0;
    a->cap=0;
}

enum ARRAY_APPEND_RESULT array_append(array*a,const void*elem){
    if(a->len==a->cap){
        a->cap=a->cap*2+1;
        void*newmem=realloc(a->data,a->cap*a->elem_size);
        if(!newmem){
            return ARRAY_APPEND_ALLOC_FAIL;
        }

        a->data=newmem;
    }

    char* cdata=a->data;
    memcpy(&cdata[a->len*a->elem_size],elem,a->elem_size);
    a->len++;

    return ARRAY_APPEND_OK;
}
void array_pop_front(array*a){
    if(a->len==0){
        return;
    }

    a->len--;
    char* data=a->data;
    data+=a->elem_size;
    a->data=data;
}
void array_pop_back(array*a){
    if(a->len==0){
        return;
    }

    a->len--;
}
void* array_get(array*a,int index){
    if(index<0||index>=a->len){
        return nullptr;
    }

    return &((char*)a->data)[index*a->elem_size];
}
