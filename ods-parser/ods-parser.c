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

int main(void){
	//TODO: factor out the zip archive to it's own function
	int result = 0;
	mz_zip_archive zip_archive;
	memset(&zip_archive, 0, sizeof(zip_archive));

	mz_bool status = mz_zip_reader_init_file(&zip_archive, DICT_PATH, 0);
	if (!status) {
		//TODO: factor out ankaso.ods
		printf("Could not open Ankaso.ods as zip file.\n");
		defer(1);
	}
	
	int file_i = mz_zip_reader_locate_file(&zip_archive,"content.xml",NULL,0);
	if(file_i < 0){
		//TODO: factor out sheet_name
		printf("could not find content.xml in ankaso.ods\n");
		defer(1);
	}

	size_t size;
	char* pBuf = mz_zip_reader_extract_to_heap(&zip_archive, file_i, &size, 0);	
	if(!pBuf){
		//TODO: factor out content.xml
		printf("failed to move content.xml to heap\n");
		defer(1);
	}
	String_View sv = {.count = size,.data = pBuf};
	printf(SV_Fmt,SV_Arg(sv));
defer:
	//if the order matters then a memcp will need to be done
	if(pBuf) mz_free(pBuf);
	mz_zip_reader_end(&zip_archive);
	return result;
}
