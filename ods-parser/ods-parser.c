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

typedef struct{
	//<TYPE x=value...\>
	String_View type;
	Attributes atts;
	//uint64_t stack_index;//should be enough for everybody
	bool self_closing;//temp
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
	bool result = sv->count > 0;
	++(sv->data);
	return result;
}

//TODO make this work with stack
bool get_next_tab(String_View *sv,XmlTab* out){
	//TODO: add better error messages
	bool result = true; //defer
	bool first = false;
	*sv = sv_trim_left(*sv);
	if(sv->data[0] != '<') defer(false);
	if(!inc_sv(sv)) defer(false);
	if(sv->data[0] == '?'){
		first = true;
		if(!inc_sv(sv)) defer(false);
	}
	*sv = sv_trim_left(*sv);
	String_Builder sb = {0};
	// :first
	if(first) out->self_closing = true;
	if(first) for(;;){
		if(sv->data[0] == '=' || sv->data[0] == '\\' || sv->data[0] == '>') defer(false);
		if(is_white_space(sv->data[0]) || sv->data[0] == '?'){
			if(!inc_sv(sv)) defer(false);
			//TODO: bit of a memory leak 
			//sv_cpy should be replaced with 
			//a better memory system
			//this is not that bad right now since the content.xml
			//is small but is could be a problum later
			out->type = sv_cpy(sb_to_sv(sb));
			if(out->type.data == NULL){
				printf("OUT OF RAM OH NO!!!!");
				defer(1);
			}
			sb.count = 0;//clean sb
			break;
		}
		sb_append(&sb,sv->data[0]);
		if(!inc_sv(sv)) defer(false);
	}
	else for(;;){
		if(sv->data[0] == '=' || sv->data[0] == '?') defer(false);
		if(is_white_space(sv->data[0]) || sv->data[0] == '\\' || sv->data[0] == '>'){
			if(!inc_sv(sv)) defer(false);
			//TODO: bit of a memory leak 
			//sv_cpy should be replaced with 
			//a better memory system
			//this is not that bad right now since the content.xml
			//is small but is could be a problum later
			out->type = sv_cpy(sb_to_sv(sb));
			if(out->type.data == NULL){
				printf("OUT OF RAM OH NO!!!!");
				defer(1);
			}
			sb.count = 0;//clean sb
			break;
		}
		sb_append(&sb,sv->data[0]);
		if(!inc_sv(sv)) defer(false);
	}
	// :attributes parsing
	for(;;){
		*sv = sv_trim_left(*sv);
		if(first && sv->data[0] == '?'){
			out->self_closing = true;
			if(!inc_sv(sv)) defer(false);
			*sv = sv_trim_left(*sv);
			if(sv->data[0] != '>') defer(false);
			if(!inc_sv(sv)) defer(false);
			break;
		}
		else if(!first && sv->data[0] == '\\'){
			out->self_closing = true;
			if(!inc_sv(sv)) defer(false);
			*sv = sv_trim_left(*sv);
			if(sv->data[0] != '>') defer(false);
			if(!inc_sv(sv)) defer(false);
			break;
		}
		//this covers the 'first' case
		else if(sv->data[0] == '>'){
			if(!inc_sv(sv)) defer(false); 
			break;
		}
	//att_end end
		String_View key;
		String_View value;
		// :key
		for(;;){
			if(sv->data[0] == '>' || sv->data[0] == '\\' || sv->data[0] == '?') defer(false); 
			if(is_white_space(sv->data[0]) || sv->data[0] == '='){
				//printf("hellow\n");
				//TODO: bit of a memory leak 
				//sv_cpy should be replaced with 
				//a better memory system
				//this is not that bad right now since the content.xml
				//is small but is could be a problum later
				key = sv_cpy(sb_to_sv(sb));
				if(out->type.data == NULL){
					printf("OUT OF RAM OH NO!!!!");
					defer(1);
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
				if(out->type.data == NULL){
					printf("OUT OF RAM OH NO!!!!");
					defer(1);
				}	
				if(!inc_sv(sv)) defer(false); 
				sb.count = 0;//clean sb
				break;
			}
			sb_append(&sb,sv->data[0]);
			if(!inc_sv(sv)) defer(false);
		}
		//printf(SV_Fmt":\'"SV_Fmt"\'\n",SV_Arg(key),SV_Arg(value));
		if(key.count == 0) defer(false);
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
	printf("?type = "SV_Fmt"\n",SV_Arg(tab.type));
	printf("    ?self_closing = %d\n",tab.self_closing);
	da_foreach(Attribute,att,&tab.atts){
		printf("    "SV_Fmt" = ""\'"SV_Fmt"\'""\n",SV_Arg(att->name),SV_Arg(att->value));
	}
}

bool parse_tabs(String_View content){
	bool result = true;
	XmlTab tab = {0};//needed for the da
	if(!get_next_tab(&content,&tab)) defer(false);
	print_tab(tab);
	if(!get_next_tab(&content,&tab)) defer(false);
	print_tab(tab);
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
