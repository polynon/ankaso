#include <stdio.h>
#include <stdlib.h>

//#define NOB_IMPLEMENTATION
//#define NOB_STRIP_PREFIXS
//#include "nob.h"

#include "./include/miniz/miniz.h"

#define defer(x) do{          \
		result = (x); \
		goto defer;   \
	}while(0)

#define DICT_PATH "Ankaso.ods"

int main(void){
	int result = 0;
	mz_zip_archive zip_archive;
	memset(&zip_archive, 0, sizeof(zip_archive));
defer:
	mz_zip_reader_end(&zip_archive);
	return result;
}
