#pragma once

#include<util/util.h>
#include<util/array.h>
#include<tokenizer.h>

#include<parser/type.h>
#include<parser/value.h>
#include<parser/symbol.h>
#include<parser/statement.h>

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

bool Token_isValidIdentifier(Token*token);
