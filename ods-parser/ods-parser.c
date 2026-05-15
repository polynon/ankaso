#include <stdio.h>
#include <stdlib.h>
//#include <string.h>

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
#define TABLE_STR "table:table" 

#define DICT_SHEET_SV        sv_from_cstr("Dictionary")
#define TABLE_SV             sv_from_cstr("table:table")
#define OFFICE_ANNOTATION_SV sv_from_cstr("office:annotation")
#define COLUMN_SV            sv_from_cstr("table:table-column")
#define ROW_SV               sv_from_cstr("table:table-row")
#define CELL_SV              sv_from_cstr("table:table-cell")
#define TEXTP_SV             sv_from_cstr("text:p")
#define TEXTSPAN_SV          sv_from_cstr("text:span")

#define SV_JS_NULL sv_from_cstr("null")

#define SV_EMPTY (String_View){.data = 0,.count = 0};
#define IS_SV_EMPTY(sv) ((sv).data == 0 && (sv).count == 0)
// :empty switch for your copypasting needs
/*
static_assert(__XmlTabType_count == 0,"update FUNCTION_NAME\n");
switch(tab.tab_type){
	case XM_OPEN:{
		tab.indent = indent++;
		da_append(tabs,tab);
		TODO("OPEN");
		break;
	}
	case XM_CLOSE:{
		tab.indent = --indent;
		da_append(tabs,tab);
		TODO("CLOSE");
		break;
	}
	case XM_CONTENT:{
		tab.indent = indent;
		da_append(tabs,tab);
		TODO("CONTENT");
		break;
	}
	case XM_SELF_CONTAINED:{
		tab.indent = indent;
		da_append(tabs,tab);
		TODO("SELF_CONTAINED");
		break;
	}
	case __XmlTabType_count: UNREACHABLE("__XmlTabType_count in parse_count");
}
*/

// :langs
#define ROOT sv_from_cstr("root")
//TODO: have this a x macro or some kinda fo code gen
#define EN_GENERAL sv_from_cstr("general")
//this name kinda suck 
#define EN_ROOT sv_from_cstr("root meaning")

// :output buffers
String_Builder ankaso_js_buffer = {0};
String_Builder en_js_buffer = {0};

// :structs
typedef struct{//implements da
	String_View *items;
	size_t count;
	size_t capacity;
}String_Views;

typedef struct{
	String_View root;
	String_Views general;
	String_Views root_meaning;
}EN_Word;

typedef struct{
	String_View root;
}Root_Word;

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
	uint64_t indent;
	union{
		String_View type;
		String_View content;
	};
	Attributes atts;
}XmlTab;

typedef struct{
	XmlTab *items;
	size_t count;
	size_t capacity;
}XmlTabs;


// :forward decs
static inline bool is_white_space(char c);
static inline bool inc_sv(String_View *sv);
bool parse_textspan(String_View *content,XmlTabs *tabs,String_Builder *out,size_t indent,String_View end);
bool parse_content(String_View *content,XmlTabs *tabs,String_Builder *out,size_t indent,String_View end);

static inline String_Builder sv_to_sb(String_View sv){
	return (String_Builder){.count = sv.count,.capacity = sv.count,.items = (void*)sv.data};
}

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

// :json output




String_Views get_entrys_from_sv(String_View sv){
	String_Views result = {0};
	//String_View start = sv;//TODO:memory leak
	String_View comp  = sv;
	for(;;){
		//TODO: get rid of the commas
		if(!inc_sv(&sv)) break;	
		if(is_white_space(sv.data[0])){
			comp.count = comp.count - sv.count - 1;
			sv_chop_prefix(&comp,sv_from_cstr(","));
			da_append(&result,comp);
			sv = sv_trim_left(sv);//get rid of white spaces
			comp = sv;
		}
	}
	if(result.count == 0) da_append(&result,comp);//case for one entry
	return result;
}

// :xml parsing

String_View sv_cpy(String_View sv){
	char *temp = malloc(sv.count);
	if(temp == NULL) return (String_View){.data = NULL};
	memcpy(temp,sv.data,sv.count);
	return (String_View){.count = sv.count,.data = temp};
	
}

static inline bool is_white_space(char c){
	return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}

//#define INC_SV(sv) (((sv)->data)++ || (sv)->count >= 0)
static inline bool inc_sv(String_View *sv){
	bool result = sv->count != 0;//so we can check the last char
	++(sv->data);
	--(sv->count);
	return result;
}

//TODO make this work with stack
//does not set indent
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

void print_tab(XmlTab tab,uint64_t indent){
	//TODO: make it work
	(void)indent;
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
			TODO("close");
			printf("</");
			printf(SV_Fmt,SV_Arg(tab.type));
			printf(">\n");
			break;
		}
		case XM_CONTENT:{
			TODO("content");
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

int get_attribute(XmlTab tab,String_View neddle){
	//TODO: we assum atts.count can fit in int
	for(int i = 0; i < (int)tab.atts.count;++i) if(sv_eq(neddle, tab.atts.items[i].name)) return i;
	return -1;
}


bool parse_first_row(String_View *content,XmlTabs *tabs,uint64_t indent){
	//TODO: this is where we can unhard code the order of the columns
	bool result = true;
	++indent;
	XmlTab tab = {0};
	for(;;){
		if(content->count == 0) defer(false); //means we ended premucutly
		if(!get_next_tab(content,&tab)){
			defer(false);
		}
		static_assert(__XmlTabType_count == 4,"update parse_first_row\n");
		switch(tab.tab_type){
			case XM_OPEN:{
				tab.indent = indent++;
				da_append(tabs,tab);
				if(sv_eq(tab.type,ROW_SV)) defer(false);
				break;
			}
			case XM_CLOSE:{
				tab.indent = --indent;
				da_append(tabs,tab);
				if(sv_eq(tab.type,ROW_SV)) defer(true);
				break;
			}
			case XM_CONTENT:{
				tab.indent = indent;
				da_append(tabs,tab);
				break;
			}
			case XM_SELF_CONTAINED:{
				tab.indent = indent;
				da_append(tabs,tab);
				break;
			}
			case __XmlTabType_count:UNREACHABLE("XmlTabType_count found in parse column in parse_first_row");
		}
	}
defer:
	return result;
}

bool close_tab(XmlTabs *tabs,XmlTab tab){
	//TODO: when this cleans memory
	//we need to make sure we don't have use after frees
	assert(tab.tab_type == XM_CLOSE);
	bool result = true;
	for(;;){
		XmlTab comp = da_pop(tabs);//TODO:this will assert if there are no items
		//TODO: free the memory
		if(comp.indent < tab.indent){
			nob_log(NOB_ERROR,"indent in close_tab is below passed tab");
			defer(false);
		}
		if(comp.indent == tab.indent
		&& sv_eq(comp.type,tab.type) 
		&& comp.tab_type == XM_OPEN) defer(true); 
	}
defer:
	return result;
}

bool parse_content(String_View *content,XmlTabs *tabs,String_Builder *out,size_t indent,String_View end){
	bool result = true;
	String_Builder sb = {0};
	XmlTab tab = {0};
	for(;;){
		if(content->count == 0) defer(false); //means we ended premucutly
		if(!get_next_tab(content,&tab))	defer(false);
		static_assert(__XmlTabType_count == 4,"update parse_content\n");
		switch(tab.tab_type){
			case XM_OPEN:{
				tab.indent = indent++;
				da_append(tabs,tab);
				if(sv_eq(tab.type,end)) defer(false);
				if(sv_eq(tab.type,TEXTSPAN_SV)){ 
					if(!parse_content(content,tabs,&sb,indent,TEXTSPAN_SV)) defer(false);
					--indent;
					break;
				}
				else UNREACHABLE("unknown type");
				break;
			}
			case XM_CLOSE:{
				tab.indent = --indent;
				da_append(tabs,tab);
				if(!close_tab(tabs,tab)) defer(false);
				if(sv_eq(tab.type,end)) defer(true);
				TODO("close");
				break;
			}
			case XM_CONTENT:{
				tab.indent = indent;
				da_append(tabs,tab);
				sb_append_sv(&sb,tab.content);//does a memcpy
				sb_append(&sb,' ');//does a memcpy
				break;
			}
			case XM_SELF_CONTAINED:{
				tab.indent = indent;
				da_append(tabs,tab);
				TODO("SELF_CONTAINED");
				break;
			}
			case __XmlTabType_count: UNREACHABLE("__XmlTabType_count in parse_count");
		}
	}
defer:
	sb_append_sv(out,sb_to_sv(sb));//a bit hacky
	sb_free(sb);
	//printf("result = %d\n",result);
	return result;
}

bool parse_cell(String_View *content,XmlTabs *tabs,String_View *out,size_t indent){
	bool result = true;

	XmlTab tab = {0};
	String_Builder sb_out = {0};

	if(content->count == 0) TODO("EOF"); //means we ended premucutly
	if(!get_next_tab(content,&tab))	TODO("FAILED TO GET TAB");
	switch(tab.tab_type){
		case XM_OPEN:{
			tab.indent = indent++;
			da_append(tabs,tab);
			if(!sv_eq(tab.type,CELL_SV)) TODO("not a cell");

			size_t saved_indent = indent;
			for(;;){
				if(content->count == 0) defer(false); //means we ended premucutly
				if(!get_next_tab(content,&tab))	defer(false);
		
				static_assert(__XmlTabType_count == 4,"update parse_cell\n");
				switch(tab.tab_type){
					case XM_OPEN:{
						//NOTE: this just keeps apending content idk if this is a good idea
						tab.indent = indent++;
						da_append(tabs,tab);
						//this + 1 kinda sucks but oh well
						if(indent == saved_indent + 1 && sv_eq(tab.type,TEXTP_SV)){
							if(!parse_content(content,tabs,&sb_out,indent,TEXTP_SV)) 
								TODO("parse content failed");
							--indent;//go back becauce it auto closes
						}
						break;
					}
					case XM_CLOSE:{
						tab.indent = --indent;
						da_append(tabs,tab);
						if(!close_tab(tabs,tab)) defer(false);
						if(sv_eq(tab.type,CELL_SV)) defer(true);
						break;
					}
					case XM_CONTENT:{
						tab.indent = indent;
						da_append(tabs,tab);
						if(indent == saved_indent) UNREACHABLE("content not expected in cell");
						//printf("|"SV_Fmt"|\n",SV_Arg(tab.content));
						//TODO("content");
						break;
					}
					case XM_SELF_CONTAINED:{
						tab.indent = indent;
						da_append(tabs,tab);
						//printf("|"SV_Fmt"|\n",SV_Arg(tab.type));
						//TODO("sc");
						break;
					}
					case __XmlTabType_count:UNREACHABLE("xmltabtype_count in parse_cell");
				}
			}
			break;
		}
		case XM_CLOSE:{
			tab.indent = --indent;
			da_append(tabs,tab);
			UNREACHABLE("expected cell but found a close type tab");
			//printf("|"SV_Fmt"|\n",SV_Arg(tab.type));
			//if(!close_tab(tabs,tab)) defer(false);
			//if(sv_eq(tab.type,ROW_SV)) defer(true);//don't append anything
			//if(!sv_eq(tab.type,CELL_SV)) TODO("not a cell");
			break;
		}
		case XM_SELF_CONTAINED:{
			tab.indent = indent;
			da_append(tabs,tab);
			if(!sv_eq(tab.type,CELL_SV)) TODO("not a cell");

			if(!sv_eq(tab.type,CELL_SV)) defer(false);
			//*out = SV_JS_NULL;
			break;
		}
		case XM_CONTENT:{
			tab.indent = indent;
			da_append(tabs,tab);
			if(!sv_eq(tab.type,CELL_SV)) TODO("not a cell");
			TODO("content");
			break;
		}
		case __XmlTabType_count:UNREACHABLE("xmltabtype_count in parse_cell");
	}
defer:
	*out = sb_to_sv(sb_out);
	return result; 
}

bool pull_cell(String_View *content,XmlTabs *tabs,uint64_t indent,String_View *out){
	String_View sv_saved = *content;
	XmlTab tab = {0};
	if(content->count == 0){
		nob_log(NOB_ERROR,"Premature EOF");//TODO: put location of __line__ __file__
		*out = SV_EMPTY;
		return true;//we return true so any loops can catch the errors
	}
	if(!get_next_tab(content,&tab)){
		*out = SV_EMPTY;
		return true;//we return true so any loops can catch the errors
	}

	static_assert(__XmlTabType_count == 4,"update parse_row\n");
	if(tab.tab_type == XM_CLOSE){
		tab.indent = --indent;
		da_append(tabs,tab);
		if(!close_tab(tabs,tab)){
			*out = SV_EMPTY;
			return true;//we return true so any loops can catch the errors
		}
		if(sv_eq(tab.type,ROW_SV)) return false; 
		else UNREACHABLE("unexpected in pull_cell");
	}

	*content = sv_saved;
	if(!parse_cell(content,tabs,out,indent)){
		*out = SV_EMPTY;
		return true;//we return true so any loops can catch the errors
	}
	if(IS_SV_EMPTY(*out)) *out = SV_JS_NULL;
	return true;
}

bool parse_row(String_View *content,XmlTabs *tabs,uint64_t indent){
	bool result = true;
	//EN_Word en_word = {0};
	//Root_Word root_word = {0};
	//TODO: unhard code this
	//TODO: writejson
	String_View text;
	if(!pull_cell(content,tabs,indent,&text)) defer(true);//junk
	if(IS_SV_EMPTY(text)) defer(false);
	if(!pull_cell(content,tabs,indent,&text)) defer(true);//general
	if(IS_SV_EMPTY(text)) defer(false);
	if(!pull_cell(content,tabs,indent,&text)) defer(true);//root
	if(IS_SV_EMPTY(text)) defer(false);
	if(!pull_cell(content,tabs,indent,&text)) defer(true);//root-meaning
	if(IS_SV_EMPTY(text)) defer(false);
	
	while(pull_cell(content,tabs,indent,&text)){//junk...
		if(IS_SV_EMPTY(text)) defer(false);
	}
defer:
	//printf("result = %d\n",result);
	//TODO("parse_row");
	return result;
}

bool parse_dict(String_View *content,XmlTabs *tabs,uint64_t indent){
	//TODO: unhard code the columns by reading the first row
	bool result = true;
	XmlTab tab = {0};
	String_View last_tab = *content;
	size_t start = indent;
	// :skip columns
	for(;;){	
		if(content->count == 0){
			printf("premuture EOF\n");
			defer(false); //means we ended premucutly
		}
		if(!get_next_tab(content,&tab)) defer(false);
		static_assert(__XmlTabType_count == 4,"update parse_dict\n");
		switch(tab.tab_type){
			case XM_OPEN:{
				tab.indent = indent++;
				da_append(tabs,tab);
				if(sv_eq(tab.type,ROW_SV)){
					if(indent - 1 != start) defer(false);
					--indent;
					goto over_columns;
				}
				break;
			}
			case XM_CLOSE:{
				tab.indent = --indent;
				da_append(tabs,tab);
				TODO("close");
				if(sv_eq(tab.type,ROW_SV)) goto over_columns;
				break;
			}
			case XM_CONTENT:{
				tab.indent = indent;
				da_append(tabs,tab);
				TODO("content");
				if(sv_eq(tab.type,ROW_SV)) goto over_columns;
				break;
			}
			case XM_SELF_CONTAINED:{
				tab.indent = indent;
				da_append(tabs,tab);
				if(sv_eq(tab.type,ROW_SV)){
					if(indent != start) defer(false);
					goto over_columns;
				}
				break;
			}
			case __XmlTabType_count:UNREACHABLE("XmlTabType_count found in parse column in parse_dict");
		}
		last_tab = *content;
	}
	over_columns:
	// :rows
	bool first_row = true;
	*content = last_tab;
	for(;;){
		if(content->count == 0){
			printf("premuture EOF\n");
			defer(false); //means we ended premucutly
		}
		if(!get_next_tab(content,&tab)){
			printf("failed to get_next_tab\n");
			defer(false);
		}
		static_assert(__XmlTabType_count == 4,"update parse_dict\n");
		switch(tab.tab_type){
			case XM_OPEN:{
				tab.indent = indent++;
				da_append(tabs,tab);
				if(sv_eq(tab.type,ROW_SV)){	
					if(first_row){ 
						if(!parse_first_row(content,tabs,indent)) defer(false);
						first_row = false;
					}
					else if(!parse_row(content,tabs,indent)) defer(false);
					--indent;
				}
				else UNREACHABLE("unexpected in parse_dict");
				break;
			}
			case XM_CLOSE:{
				tab.indent = --indent;
				da_append(tabs,tab);
				if(!close_tab(tabs,tab)) defer(false);
				//this -1 is kinda ugly
				if(tab.indent == start - 1){
					if(sv_eq(tab.type,TABLE_SV)) defer(true);
				}
				break;
			}
			case XM_CONTENT:{
				tab.indent = indent;
				da_append(tabs,tab);
				TODO("OPEN row");
				break;
			}
			case XM_SELF_CONTAINED:{
				tab.indent = indent;
				da_append(tabs,tab);

				TODO("sc");
				if(sv_eq(tab.type,ROW_SV)){}
				printf(SV_Fmt"\n",SV_Arg(tab.type));
				break;
			}
			case __XmlTabType_count:UNREACHABLE("XmlTabType_count found in parse column in parse_dict");
		}
	}
defer:
	return result;
}

bool parse_tabs(String_View content){
	bool result = true;
	//needed for the da
	XmlTabs tabs = {0};//do we need this?
	XmlTab tab = {0};	
	uint64_t indent = 0;
	for(;;){
		if(content.count == 0) defer(true); //TODO: EOF
		if(!get_next_tab(&content,&tab)) defer(false);
		static_assert(__XmlTabType_count == 4,"update parse_tabs\n");
		switch(tab.tab_type){
			case XM_OPEN:{
				tab.indent = indent++;
				da_append(&tabs,tab);
				if(sv_eq(tab.type, sv_from_cstr(TABLE_STR))){
					//TODO: less hardcoding
					int i = get_attribute(tab,sv_from_cstr("table:name"));
					if(i < 0) defer(false);
					if(sv_eq(tab.atts.items[i].value,DICT_SHEET_SV)){
						printf("parsing dict\n");
						if(!parse_dict(&content,&tabs,indent)) defer(false);
						--indent;
						break;
					}
				}
				break;
			}
			case XM_CLOSE:{
				tab.indent = --indent;
				da_append(&tabs,tab);
				if(!close_tab(&tabs,tab)) defer(false);
				if(sv_eq(tab.type, sv_from_cstr(TABLE_STR))){
					int i = get_attribute(tab,sv_from_cstr("table:name"));
					if(i < 0) defer(false);//TODO:error reporting
					if(sv_eq(tab.atts.items[i].value,sv_from_cstr(DICT_SHEET_NAME)))
						UNREACHABLE("got dict close in parse tabs");
				}
				break;
			}
			case XM_CONTENT:{
				tab.indent = indent;
				da_append(&tabs,tab);
				break;
			}
			case XM_SELF_CONTAINED:{
				tab.indent = indent;
				da_append(&tabs,tab);
				if(sv_eq(tab.type, sv_from_cstr(TABLE_STR))) UNREACHABLE("FOUND EMPTY TABLE");
				break;
			}
			case __XmlTabType_count:UNREACHABLE("XmlTabType_count found in parse_tabs");
		}
	}
defer:
	//TODO: make it not complain
	//otherwise MEMORYLEAK is fine tho
	//mz_free((void*)content.data);
	//free(content.data);
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
