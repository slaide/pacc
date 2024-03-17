#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>

#include<util.h>
#include<parser.h>

int main(int argc, const char**argv){
	if(argc<2)
		fatal("no input file given");

	File code_file={};
	File_read(argv[1],&code_file);

	Tokenizer tokenizer={};
	Tokenizer_init(&tokenizer,&code_file);

	Module module={};
	Module_parse(&module,&tokenizer);
	
	return 0;
}
