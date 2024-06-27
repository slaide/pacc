#pragma once

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
/* append element pointed to by elem to array */
enum ARRAY_APPEND_RESULT array_append(array*a,const void*elem);

void array_pop_front(array*a);
void array_pop_back(array*a);
/* get address of element at index, only valid until next array mutation! */
void* array_get(array*a,int index);
