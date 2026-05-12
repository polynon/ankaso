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
#define DICT_SHEET_NAME "Dictionary"

#define SV_JS_NULL (String_View){.count = 4,.data = "NULL"};

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

// :xml parsing
typedef struct{
	String_View name;
	String_View value;
}Attribute;

//interfaces with nob::da
typedef struct{	
	Attribute *items;
	size_t count;
	size_t capacity;
}Attributes;

typedef enum{
	XM_OPEN,
	XM_CLOSE,
	XM_CONTENT,
	XM_SELF_CONTAINED,
	__XmlTabType_count
}XmlTabType;

typedef struct{
		XmlTabType tab_type;
	//uint64_t stack_index;//should be enough for everybody
	union{
		String_View type;
		String_View content;
	};
	Attributes atts;
}XmlTab;

String_View sv_cpy(String_View sv){
	char *temp = malloc(sv.count);
	if(temp == NULL) return (String_View){.data = NULL};
	memcpy(temp,sv.data,sv.count);
	return (String_View){.count = sv.count,.data = temp};
	
}

static inline bool is_white_space(char c){
	return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}

#define INC_SV(sv) (((sv)->data)++ || (sv)->count >= 0)
static inline bool inc_sv(String_View *sv){
	bool result = sv->count != 0;//so we can check the last char
	++(sv->data);
	--(sv->count);
	return result;
}

//TODO make this work with stack
bool get_next_tab(String_View *sv,XmlTab* out){
	//TODO: add better error messages
	bool result = true; //defer

	String_Builder sb = {0};
	bool first = false;
	*sv = sv_trim_left(*sv);
	if(sv->data[0] != '<'){
		out->tab_type = XM_CONTENT;
		if(!inc_sv(sv)) defer(false);
		for(;;){
			//TODO: parse quotes
			if(sv->data[0] == '<') break;
			sb_append(&sb,sv->data[0]);
			if(!inc_sv(sv)) defer(false);
		}
		out->content = sv_cpy(sb_to_sv(sb));
		if(out->content.data == NULL){
			printf("OUT OF RAM OH NO!!!!");
			defer(false);
		}	
		//sb.count = 0;//clean sb
		defer(true);
	}
	if(!inc_sv(sv)) defer(false);
	if(sv->data[0] == '?'){
		out->tab_type = XM_SELF_CONTAINED;
		first = true;
		if(!inc_sv(sv)) defer(false);
	}
	else if(sv->data[0] == '/'){
		out->tab_type = XM_CLOSE;
		if(!inc_sv(sv)) defer(false);
	}
	else out->tab_type = XM_OPEN;
	*sv = sv_trim_left(*sv);
	// :first
	if(first) for(;;){
		if(sv->data[0] == '=' || sv->data[0] == '/' || sv->data[0] == '>'){
			printf("unexepected \'%c\' for tab\n",sv->data[0]);
			defer(false);
		}
		if(is_white_space(sv->data[0]) || sv->data[0] == '?'){
			//TODO: bit of a memory leak 
			//sv_cpy should be replaced with 
			//a better memory system
			//this is not that bad right now since the content.xml
			//is small but is could be a problum later
			out->type = sv_cpy(sb_to_sv(sb));
			if(out->type.data == NULL){
				printf("OUT OF RAM OH NO!!!!");
				defer(false);
			}
			if(sv->data[0] != '?') if(!inc_sv(sv)) defer(false);
			sb.count = 0;//clean sb
			break;
		}
		sb_append(&sb,sv->data[0]);
		if(!inc_sv(sv)) defer(false);
	}
	else for(;;){
		if(sv->data[0] == '=' || sv->data[0] == '?'){
			printf("unexepected \'%c\' for tab\n",sv->data[0]);
			defer(false);
		}
		if(is_white_space(sv->data[0]) || sv->data[0] == '/' || sv->data[0] == '>'){
			//TODO: bit of a memory leak 
			//sv_cpy should be replaced with 
			//a better memory system
			//this is not that bad right now since the content.xml
			//is small but is could be a problum later
			out->type = sv_cpy(sb_to_sv(sb));
			if(out->type.data == NULL){
				printf("OUT OF RAM OH NO!!!!");
				defer(false);
			}
			if(sv->data[0] != '/' && sv->data[0] != '>') if(!inc_sv(sv)) defer(false);
			sb.count = 0;//clean sb
			break;
		}
		sb_append(&sb,sv->data[0]);
		if(!inc_sv(sv)) defer(false);
	}
	//printf("type = "SV_Fmt"\n",SV_Arg(out->type));
	// :attributes parsing
	for(;;){
		*sv = sv_trim_left(*sv);
		if(first && sv->data[0] == '?'){
			if(!inc_sv(sv)) defer(false);
			*sv = sv_trim_left(*sv);
			if(sv->data[0] != '>'){
				printf("exepected \'>\' for tab\n");
				defer(false);
			}
			if(!inc_sv(sv)) defer(false);
			break;
		}
		else if(!first && sv->data[0] == '/'){
			if(out->tab_type == XM_OPEN) out->tab_type = XM_SELF_CONTAINED;
			else{
				printf("only tabtype open can be self closed\n");
				defer(false);
			}
			//defer(false);
			if(inc_sv(sv)){//this is end of file we are done
				*sv = sv_trim_left(*sv);
			}
			if(sv->data[0] != '>'){
				printf("exepected \'>\' for tab\n");
				defer(false);
			}
			if(!inc_sv(sv)){/*defer(false);*/}
			break;
		}
		//this covers the 'first' case
		else if(sv->data[0] == '>'){
			if(!inc_sv(sv)){ /*defer(false);*/ }//EOF
			break;
		}
	//att_end end
		String_View key;
		String_View value;
		// :key
		for(;;){
			if(sv->data[0] == '>' || sv->data[0] == '/' || sv->data[0] == '?'){
				printf("unexpected char \'%c\' for attribute name\n",sv->data[0]);
				defer(false);
			}
			if(is_white_space(sv->data[0]) || sv->data[0] == '='){
				//printf("hellow\n");
				//TODO: bit of a memory leak 
				//sv_cpy should be replaced with 
				//a better memory system
				//this is not that bad right now since the content.xml
				//is small but is could be a problum later
				key = sv_cpy(sb_to_sv(sb));
				if(key.data == NULL){
					printf("OUT OF RAM OH NO!!!!");
					defer(false);
				}	
				sb.count = 0;//clean sb
				if(sv->data[0] == '=') break;//break early so we can check for it
				if(!inc_sv(sv)) defer(false); 
				break;
			}
			sb_append(&sb,sv->data[0]);
			if(!inc_sv(sv)) defer(false); 
		}
		*sv = sv_trim_left(*sv);
		if(sv->data[0] != '=') defer(false);
		if(!inc_sv(sv)) defer(false); 
		// :value
		*sv = sv_trim_left(*sv);
		char quote = sv->data[0];
		if(quote != '\"' && quote != '\'') defer(1);
		if(!inc_sv(sv)) defer(false); 
		*sv = sv_trim_left(*sv);
		for(;;){
			if(sv->data[0] == quote){
				//printf("values\n");
				//TODO: bit of a memory leak 
				//sv_cpy should be replaced with 
				//a better memory system
				//this is not that bad right now since the content.xml
				//is small but is could be a problum later
				value = sv_cpy(sb_to_sv(sb));
				if(value.data == NULL){
					printf("OUT OF RAM OH NO!!!!");
					defer(false);
				}	
				if(!inc_sv(sv)) defer(false); 
				sb.count = 0;//clean sb
				break;
			}
			sb_append(&sb,sv->data[0]);
			if(!inc_sv(sv)) defer(false);
		}
		//printf(SV_Fmt":\'"SV_Fmt"\'\n",SV_Arg(key),SV_Arg(value));
		if(key.count == 0){
			printf("ERROR: attribute name is empty\n");
			defer(false);
		}
		if(value.count == 0) value = SV_JS_NULL;
		da_append(&out->atts,((Attribute){.name = key,.value = value}));
	}
defer:
	sb_free(sb);//could avoid this with static
	return result;
}

void print_tab(XmlTab tab){
	//TODO: care about depth
	//printf("atts_count = %zu\n",tab.atts.count);
	//TODO: print type;
	//printf("    ?self_closing = %d\n",tab.self_closing);
	static_assert(__XmlTabType_count == 4,"update print tab\n");
	switch(tab.tab_type){
		case XM_OPEN:{
			printf("<");
			printf(SV_Fmt"\n",SV_Arg(tab.type));
			da_foreach(Attribute,att,&(tab.atts)){
				printf("    "SV_Fmt":""\'"SV_Fmt"\'""\n",SV_Arg(att->name),SV_Arg(att->value));
			}
			printf(">\n");
			break;
		}
		case XM_CLOSE:{
			printf("</");
			printf(SV_Fmt,SV_Arg(tab.type));
			printf(">\n");
			break;
		}
		case XM_CONTENT:{
			printf("    "SV_Fmt"\n",SV_Arg(tab.content));
			break;
		}
		case XM_SELF_CONTAINED:{
			printf("<");
			printf(SV_Fmt"\n",SV_Arg(tab.type));
			da_foreach(Attribute,att,&(tab.atts)){
				printf("    "SV_Fmt":""\'"SV_Fmt"\'""\n",SV_Arg(att->name),SV_Arg(att->value));
			}
			printf("/>\n");
			break;
		}
		case __XmlTabType_count:UNREACHABLE("XmlTabTpye_count found in print_tab");
	}
}

bool parse_tabs(String_View content){
	bool result = true;
	XmlTab tab = {0};//needed for the da
	int tab_i = 1;
	for(;;){
		if(content.count == 0) defer(true); //TODO: EOF
		if(!get_next_tab(&content,&tab)){
			printf("i = %d\n",tab_i);
			print_tab(tab);
			defer(false);
		}
		++tab_i;
		//printf("i = %d\n",tab_i++);
		//print_tab(tab);
	}
defer:
	//TODO: make it not complain
	//mz_free((void*)content.data);
	return result;
}

int main(void){
	int result = 0;//defer
	String_View content;
	if(!get_content(&content)) defer(1);
	//printf(SV_Fmt,SV_Arg(content));	
	if(!parse_tabs(content)) defer(1);
defer:
	return result;
}
