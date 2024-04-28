#include <stdio.h>

int main(int argc, const char**argv){
	if(argc>=2 && argc<3)
		return 0;
	
	printf("%s",argv[argc]);
	return 0;
}
