#include <parser/parser.h>

int main(int argc, const char**argv){
	if(argc<2){
		print_test_result("test1",test1());
		print_test_result("test2",test2());

		fatal("no input file given. aborting.");
	}
	
	return 0;
}

// TESTGOAL if statement
