#include<file.h>

#include<stdlib.h>
#include<stdio.h>

void File_read(const char*filepath,File*out){
	FILE* file=fopen(filepath,"r");
	fseek(file,0,SEEK_END);
	int file_len=ftell(file);
	fseek(file,0,SEEK_SET);
	char*file_contents=malloc(file_len+1);
	fread(file_contents,1,file_len,file);
	file_contents[file_len]=0;
	fclose(file);
	
	*out=(File){
		.contents=file_contents,
		.contents_len=file_len,
	};
}
