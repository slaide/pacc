#pragma once

#include<tokenizer.h>
#include<util/array.h>

typedef struct Module Module;

enum MODULE_PARSE_RESULT{
	MODULE_INVALID,
	MODULE_PRESENT,
};
enum MODULE_PARSE_RESULT Module_parse(Module* module,struct TokenIter*token_iter_in);
struct Module{
	/* list of statements, so item type is struct Statement* */
	array statements;
	/* list of defined types, i.e. item type is struct Type* */
	array types;
};
void Module_init(Module*module);
/*
find a type by name, returns nullptr if none is found

the returned pointer is valid until the module is destroyed
*/
struct Type* Module_findType(Module*module,Token*name);
bool Module_equal(Module*a,Module*b);
