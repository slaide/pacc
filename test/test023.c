void doathing(){}
int main(int argc, const char**argv){
	do{
		doathing();
	}while(0);
	while(true);

	for(int i=0;i<argc;){
		doathing();
	}
	int i;
	for(;i<argc;i++){
		doathing();
	}
	for(;i<argc;){
		doathing();
	}

	for(;;){
		doathing();
	}
	
	return 0;
}

// TESTGOAL do while loop, multiple versions of a for loop
