#include <stdio.h>
#include <stdlib.h>

int main(int argc, const char**argv){
	if(argc<2){
		fprintf(stderr,"Usage: %s <file>\n",argv[0]);
		exit(1);
	}
	
	return 0;
}
