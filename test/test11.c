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
	Type* intType=allocAndCopy(sizeof(Type));
    /*
    ,&(Type){
		.kind=TYPE_KIND_REFERENCE,
		.reference=(Token){.len=3,.p="int",},
	});
    */

	File testFile;
	File_fromString("testfile.c","int main(){}",&testFile);

    /*
	Tokenizer tokenizer={};
	Tokenizer_init(&tokenizer,&testFile);

	struct TokenIter token_iter;
	TokenIter_init(&token_iter,&tokenizer,(struct TokenIterConfig){.skip_comments=true,});

	Module module={};
	Module_parse(&module,&token_iter);

	Module expected_module={};
	array_init(&expected_module.statements,sizeof(Statement));

	Statement statements[]={(Statement){
		.tag=STATEMENT_FUNCTION_DEFINITION,
		.functionDef={
			.symbol={
				.name=&(Token){.len=4,.p="main",},
				.kind=SYMBOL_KIND_DECLARATION,
				.type=allocAndCopy(sizeof(Type),&(Type){
					.kind=TYPE_KIND_FUNCTION,
					.function={
						.ret=intType,
					},
				}),
			},
			.bodyStatements={},
		},
	}};
    */
		array_init(&statements[0].functionDef.symbol.type->function.args,sizeof(Statement));
		array_init(&statements[0].functionDef.bodyStatements,sizeof(Statement));

	array_append(&expected_module.statements,&statements[0]);

	Module_print(&expected_module);
	return Module_equal(&module,&expected_module);
}
