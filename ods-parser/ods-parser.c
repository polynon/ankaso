#include <stdio.h>
#include <stdlib.h>

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIXS
#include "nob.h"

#include "./include/miniz/miniz.h"

#define defer(x) do{	      \
		result = (x); \
		goto defer;   \
	}while(0)

#define DICT_PATH "Ankaso.ods"
#define DICT_SHEET_NAME "Dictionary.xml"

//TODO: document .ods
bool get_content(String_View* out){
	/*
	 * miniz is only used with this function
	 * and it's a rather big thing for what we
	 * so TODO: replace miniz with a smaller lib
	 *
	 * this is the unziper of ther zip
	 * which also grabs the content.xml
	 */
	bool result = true;//defer
	mz_zip_archive zip_archive;
	memset(&zip_archive, 0, sizeof(zip_archive));

	mz_bool status = mz_zip_reader_init_file(&zip_archive, DICT_PATH, 0);
	if (!status) {
		//TODO: factor out ankaso.ods
		printf("Could not open Ankaso.ods as zip file.\n");
		defer(false);
	}
	
	int file_i = mz_zip_reader_locate_file(&zip_archive,"content.xml",NULL,0);
	if(file_i < 0){
		//TODO: factor out sheet_name
		printf("could not find content.xml in ankaso.ods\n");
		defer(false);
	}

	size_t size;
	//TODO: maybe this needs to use custom alocator
	//or memcpy
	char* pBuf = mz_zip_reader_extract_to_heap(&zip_archive, file_i, &size, 0);	
	if(!pBuf){
		//TODO: factor out content.xml
		printf("failed to move content.xml to heap\n");
		defer(false);
	}
	String_View sv = {.count = size,.data = pBuf};
	*out = sv;
defer:
	mz_zip_reader_end(&zip_archive);
	return result;
}

int main(void){
	int result = 0;//defer
	String_View content;
	if(!get_content(&content)) defer(1);
	printf("count = %zu\n",content.count);
defer:
	//TODO: make this better
	mz_free((void*)content.data);
	return result;
}
