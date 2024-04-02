#pragma once

#include<stdlib.h>
#include<string.h>

typedef struct array{
    void*data;
    int elem_size;
    int len;
    int cap;
}array;

void array_init(array*a,int elem_size);
void array_free(array*a);

enum ARRAY_APPEND_RESULT{
    ARRAY_APPEND_OK=0,
    ARRAY_APPEND_ALLOC_FAIL,
};
enum ARRAY_APPEND_RESULT array_append(array*a,void*elem);

void array_pop_front(array*a);
void array_pop_back(array*a);
void* array_get(array*a,int index);
