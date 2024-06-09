#include <stdio.h>

struct Statement{
	int _padding;
};
const char*Statement_asString(struct Statement*statement){
	return "statement";
}
struct Module{
	int num_statements;
	struct Statement*statements;
};
void Module_print(struct Module*module){
	for(int i=0;i<module->num_statements;i++){
		struct Statement*statement=&module->statements[i];
		printf("statement %d is : %s\n",i,Statement_asString(statement));
	}
}
