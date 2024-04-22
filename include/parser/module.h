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
	array statements;
};
bool Module_equal(Module*a,Module*b);
