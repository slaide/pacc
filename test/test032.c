#include<stdlib.h>
#include<stdio.h>
#include<string.h>
typedef struct File{
	const char*filepath;
	const char*contents;
	int contents_len;
}File;
void File_read(const char*filepath,File*out){
	FILE* file=fopen(filepath,"r");
	if(!file){
		fprintf(stderr,"could not open file %s",filepath);
		exit(1);
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
