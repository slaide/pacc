#pragma once

#include<util/util.h>
#include<util/array.h>
#include<tokenizer.h>

#include<parser/type.h>
#include<parser/value.h>
#include<parser/symbol.h>
#include<parser/statement.h>

typedef struct Module Module;
void Module_parse(Module* module,Tokenizer*tokenizer);
struct Module{
	array statements;
};
bool Module_equal(Module*a,Module*b);

bool Token_isValidIdentifier(Token*token);
