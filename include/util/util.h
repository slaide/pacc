#pragma once

#include<stdio.h> // fprintf
#include<stdlib.h> // exit

#define discard (void)

#define EOFc (char)-1

#define fprint(F,...) {fprintf(F,"%s:%d | ",__FILE__,__LINE__); fprintf(F,__VA_ARGS__);}
#define fprintln(F,...) {fprint(F,__VA_ARGS__); fprintf(F,"\n");}
#define println(...) fprintln(stdout,__VA_ARGS__)
#define print(...) fprint(stdout,__VA_ARGS__)
#define fatal(...) {fprintln(stderr,__VA_ARGS__);exit(-1);}

#ifdef DEVELOP
#define DEBUG
#endif

#define assert(STMT) if(!(STMT)){fatal("assert failed");}
#ifdef DEVELOP
#define dev_assert(STMT) if(!(STMT)){fatal("development assert failed");}
#else
#define dev_assert(_STMT) ;
#endif
#ifdef DEBUG
#define debug_assert(STMT) if(!(STMT)){fatal("debug assert failed");}
#else
#define debug_assert(STMT) ;
#endif

typedef struct String{
	char*str;
	int str_len;
}String;

/// @brief allocates size bytes memory and copies bytes memory from src into it
void* allocAndCopy(size_t size,void*src);

char* makeStringn(int len);
char* makeString();
/// @brief appends fmtstr to str (appends to the last character before the null terminator)
void stringAppend(char*str,char*fmtstr,...);
/// get indentation string of length n
char*ind(int n);
