#include <stdio.h>
#include <stdlib.h>

int main(int argc, const char**argv){
	if(argc<2){
		fprintf(stderr,"not enough inputs. aborting.");
		exit(1);
	}

	int values[6]={1,2,3,4,5,6};

	int index=atoi(argv[1]);
	if(index>=0&&index<6)
		return values[index];
	
	return 0; 
}
