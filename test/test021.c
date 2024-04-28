int main(int argc, const char**argv){
	if(!(argc==1)){
		print_test_result("test1",test1());
		print_test_result("test2",test2());

		fatal("no input file given. aborting.");
	}
	if(2==~2)
		fatal("this is literally impossible");
	
	return 0;
}
