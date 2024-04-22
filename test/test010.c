#include "parser/statement.h"
#include <util/util.h>
#include <parser/parser.h>
#include<util/ansi_esc_codes.h>

void Module_print(Module*module){
	for(int i=0;i<module->statements.len;i++){
		Statement*statement=array_get(&module->statements,i);
		println("statement %d is : %s",i,Statement_asString(statement));
	}
}

// TESTGOAL : include directive with quotation marks and angle brackets, function definition, for loop, arrow operator, dot operator