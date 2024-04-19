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

bool test1(){
	Type* intType=allocAndCopy(sizeof(Type),&(Type){
		.kind=TYPE_KIND_REFERENCE,
		.reference=(Token){.len=3,.p="int",},
	});

	File testFile;
	File_fromString("testfile.c","int main(){}",&testFile);

	array_append(&expected_module.statements,&statements[0]);

	Module_print(&expected_module);
	return Module_equal(&module,&expected_module);
}
