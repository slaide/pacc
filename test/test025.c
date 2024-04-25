#include<stdio.h>
int main(int argc, const char**argv){
	if(argc<0)return-1;
	switch(argc){
		default:
			{
				printf();
				char*progname=*argv;
				printf("progname is %s",progname);
	/*
				for(int ac=0;ac<argc;ac++){
					printf("arg %d is %s",ac,argv[ac]);
				}
			*/
			}
			break;
		case 0:
			break;
		case 1:
			{
				char*progname=*argv;
			/*
				printf("progname is %s",progname);
			}
			break;
		case 2:
			{
				char*progname=*argv;
				printf("progname is %s",progname);
				printf("arg 1 is %s",argv[1]);
			*/
			}
			break;
	}
	
	return 0;
}

// TESTFOR dereference a pointer
