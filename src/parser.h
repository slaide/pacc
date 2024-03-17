#pragma once

#include<util.h>
#include<tokenizer.h>

typedef struct Module Module;

void Module_parse(Module* module,Tokenizer*tokenizer);

struct Module{
	int num_statements;
};
