#pragma once

#include<stdio.h>
#include<stdlib.h>

#define discard (void)

#define EOFc (char)-1

#define fprintln(F,...) {fprintf(F,"%s:%d | ",__FILE__,__LINE__); fprintf(F,__VA_ARGS__); fprintf(F,"\n");}
#define println(...) fprintln(stdout,__VA_ARGS__)
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
