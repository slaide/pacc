#pragma once

typedef struct File{
	const char*filepath;
	const char* contents;
	int contents_len;
}File;
void File_read(const char*filepath,File*out);
void File_fromString(const char*filepath,const char*str,File*out);
