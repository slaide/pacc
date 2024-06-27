#pragma once

typedef struct Module Module;

#include<tokenizer.h>
#include<parser/stack.h>

enum MODULE_PARSE_RESULT{
	MODULE_INVALID,
	MODULE_PRESENT,
};
enum MODULE_PARSE_RESULT Module_parse(Module* module,struct TokenIter*token_iter_in);
struct Module{
	Stack stack;
};
void Module_init(Module*module);

bool Module_equal(Module*a,Module*b);
