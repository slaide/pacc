#include<stdio.h>
#include<stdlib.h>

int main(int argc, const char**argv){
	int a[2]={1,2};
	if(argc<a[1]){
		fprintf(stderr,"Usage: %s <file>\n",argv[0]);
		exit(a[0]);
	}
	
	return 0;
}
