#define FUNC(A,...) {(A);{__VA_ARGS__;};}

#define TEXT_COLOR_GREEN "GREEN"
#define TEXT_COLOR_RED "RED"
#define TEXT_COLOR_RESET "RESET"

void print_test_result(const char*testname,bool passed){
	if(passed){
		FUNC(TEXT_COLOR_GREEN "%s passed" TEXT_COLOR_RESET,testname);
	}else{
		FUNC(TEXT_COLOR_RED "%s failed" TEXT_COLOR_RESET,testname);
	}
}