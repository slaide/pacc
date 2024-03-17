#pragma once

typedef struct File{
	const char* contents;
	int contents_len;
}File;
void File_read(const char*filepath,File*out);
