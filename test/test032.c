#include<file.h>

#include<stdlib.h>
#include<stdio.h>
#include<string.h>

#include<util/util.h>

void File_read(const char*filepath,File*out){
	FILE* file=fopen(filepath,"r");
	if(!file){
		fatal("could not open file %s",filepath);
	}
	fseek(file,0,SEEK_END);
	int file_len=ftell(file);
	fseek(file,0,SEEK_SET);
	char*file_contents=malloc(file_len+1);
	fread(file_contents,1,file_len,file);
	file_contents[file_len]=0;
	fclose(file);
	
	*out=(File){
		.filepath=filepath,
		.contents=file_contents,
		.contents_len=file_len,
	};
}
void File_fromString(const char*filepath,const char*str,File*out){
	*out=(File){
		.filepath=filepath,
		.contents=str,
		.contents_len=strlen(str),
	};
}

// TESTFOR dereference operator at start of statement
