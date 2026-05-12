#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIXS
#include "nob.h"

#define cc "cc"
#define warnings "-Wextra","-Wall"
#define Olevel "-o0"

int main(int argc, char **argv){	
	GO_REBUILD_URSELF(argc,argv);
	Cmd cmd = {0};
	cmd_append(&cmd,cc,warnings,Olevel,"ods-parser.c","-o","ods-parser","-L./include/miniz/","-lminiz");
	if(!cmd_run(&cmd)) return 1;
	cmd_append(&cmd,"./ods-parser");
	if(!cmd_run(&cmd)) return 1;
}
