//
//	event table reader for win_pmu
//	Copyright (c) 2016 Microsoft Corporation
//	Permission is hereby granted, free of charge, to any person obtaining a copy
//	of this software and associated documentation files (the "Software"), to deal
//	in the Software without restriction, including without limitation the rights
//	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//	copies of the Software, and to permit persons to whom the Software is
//	furnished to do so, subject to the following conditions:
//	The above copyright notice and this permission notice shall be included in
//	all copies or substantial portions of the Software.
//	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//	THE SOFTWARE.
//
//
#define _AMD64_ 1

#include <windows.h>
#include <winnt.h>

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <tchar.h>

typedef unsigned __int32 uint;
typedef unsigned long long uint64_t;
typedef int bool;

#include "example_pmu.h"
#include "event_util.h"

extern void err(int i, const char * s, ...);
event_table_struc_ptr read_event_file(const char* input_file);

#define MAX_FILE_PATH 256
#define MAX_STRING 2048
#define LINE_SIZE 4112
#define CODE_SIZE 16
#define UMASK_SIZE 12
#define EVENT_NAME_SIZE 128
#define UMASK_NAME_SIZE 256
#define DESCRIPTION_SIZE 2048
#define COUNTER_HT_ON_SIZE 8
#define COUNTER_HT_OFF_SIZE 8
#define MSR_INDEX_SIZE 16
#define MSR_VALUE_SIZE 64
#define OVERFLOW_SIZE 24
#define PRECISE_SIZE 8
#define PES_UPPER_SIZE 16
#define DATA_LAT_SIZE 8
#define OFFCORE_FIELD_SIZE 8
#define FIXED_SIZE 8
#define MAX_BASE_PATH 1024
#define MAX_TOTAL_PATH 2048
#define MAX_TABLE_NAME 256
#define MAX_FAM 8
#define NUM_PMU 5

char base_dir[] = "event_files\\";
char input_core[MAX_FILE_PATH];
char map_file[] = "file_map.csv";

char mode[]="r";

int table_length = 0, table_event_count = 0;



arch_event_tables_struc_ptr 
read_arch_event_files(int model, int family)
{
//
//	The lack of getdelim in windows makes this quite a bit uglier than it could be
//	but this is probably faster   :-)
//
	FILE* input;
	int i, input_family, input_model, found_model=0;
	char* input_map_file;
	char lineptr[LINE_SIZE],map_family[MAX_FAM],map_model[MAX_FAM],map_pmu_files[NUM_PMU][MAX_TABLE_NAME];
	int current_field_start=0, current_field_pos=0, row_count=0;
	int line_len, line_size=LINE_SIZE;
	int pmu_index=0;
	char const* return_val;
	char* input_file;
	int  path_len, base_len, file_dir_len, map_file_len, pmu_file_len;
	TCHAR tbase_path[MAX_BASE_PATH];
	char  base_path[MAX_BASE_PATH];
	char back_slash = '\\';
	char comma = ',';
	char eol = '\n';
	errno_t errno;
	size_t ret_val, size_in_bytes=MAX_BASE_PATH;

	arch_event_tables_struc_ptr this_arch_tables;

	path_len = GetModuleFileName(NULL, tbase_path, MAX_BASE_PATH);
#ifdef DBUG_TABLE
	fprintf(stderr," read_arch_event_files: path_len = %d, path = %s\n",path_len,tbase_path);
#endif
	if((path_len <= 0) || (path_len >= MAX_BASE_PATH))
		{
		fprintf(stderr," read_arch_event_files: path_len = %d is too large for predefined buffer base_path, base_path = %s\n",path_len,base_path);
		exit(1);
		}
	errno = wcstombs_s(&ret_val, base_path, size_in_bytes, tbase_path, MAX_BASE_PATH-2);
	if(errno != 0)
		{
		fprintf(stderr,"read_arch_event_files: failed to copy tchar exec path to char base_path array,errno = %ld\n",errno);
		exit(1);
		}
	base_path[MAX_BASE_PATH-1] ='\0';
#ifdef DBUG_TABLE
	fprintf(stderr," read_arch_event_files: path_len = %d, base_path = %s\n",path_len,base_path);
#endif

//	trim the string back to the last \, that is the path to the install directory
	path_len = (int) strlen((const char *)base_path);
	for(i=path_len; i>0; i--)
		if(base_path[i] == back_slash)break;
	base_len = i+1;

	input_file = (char*)calloc(1,MAX_TOTAL_PATH*sizeof(char));
	if(input_file == NULL)
		{
		fprintf(stderr," read_arch_event_files: failed to allocating input_file buffer\n");
		exit(1);
		}
//	set up the full path to the file, open it and reader the header row
	for(i=0; i<base_len; i++)input_file[i] = base_path[i];
	input_file[base_len+1] = '\0';
#ifdef DBUG_TABLE
	fprintf(stderr,"read_arch_event_files: base_path  = %s\n",input_file);
#endif

//	append the file repository subdirectory to the path
	file_dir_len = (int) strlen(base_dir);
	for(i=0; i<file_dir_len; i++)input_file[base_len+i] = base_dir[i];
	input_file[base_len+file_dir_len] = '\0';

#ifdef DBUG_TABLE
	fprintf(stderr,"read_arch_event_files: file_repository = %s\n",input_file);
#endif

//	get the map file to deal with the family and model
	map_file_len = (int) strlen(map_file);
	input_map_file = calloc(1,(base_len + file_dir_len + map_file_len + 2)*sizeof(char));
	if(input_map_file == NULL)
		{
		fprintf(stderr,"read_arch_event_files: failed to allocate input_file_map\n");
		exit(1);
		}
	for(i=0; i<base_len+file_dir_len; i++)input_map_file[i] = input_file[i];
	for(i=0; i<map_file_len; i++)input_map_file[base_len+file_dir_len + i] = map_file[i];
	input_map_file[base_len+file_dir_len + map_file_len] = '\0';
#ifdef DBUG_TABLE
	fprintf(stderr,"read_arch_event_files: map_file = %s\n",input_map_file);
#endif

//	open and read the map file and find the matching family and model
	errno = fopen_s(&input, input_map_file, mode);
	if(errno != 0)
		err(1,"read_arch_event_files: failed to open input table in read_core_event_file %s",input_file);

//	line read loop starts with extracting the family and model and on a match get the file names
//	terminate lineptr to avoid idiotic OACR error
	lineptr[LINE_SIZE-1] = '\0';
	while((return_val = fgets(lineptr,line_size,input)) != NULL)
		{
		line_len = (int)strlen(lineptr);
		row_count++;
#ifdef DBUG_TABLE
		fprintf(stderr,"read_arch_event_files:row %d: %s\n",row_count,lineptr);
#endif
		current_field_start=0;
		current_field_pos = 0;
		while(lineptr[current_field_pos] != comma)
			{
			if(current_field_pos > line_len)
				{
				fprintf(stderr,"read_arch_event_files: char match loop went beyond line length on family current_field_pos = %d, file = %s\n",
					current_field_pos,input_map_file);
				exit(1);
				}
			if((current_field_pos - current_field_start) >= MAX_FAM)
				{
				fprintf(stderr,"read_arch_event_files: reading map file failed to terminate family field %s\n",input_map_file);
				exit(1);
				}
			map_family[current_field_pos - current_field_start] = lineptr[current_field_pos];
			current_field_pos++;
			}

		if((current_field_pos - current_field_start) >= MAX_FAM)
			{
			fprintf(stderr,"read_arch_event_files: reading map file failed to terminate model field, %s\n",input_map_file);
			exit(1);
			}
		map_family[current_field_pos - current_field_start] = '\0';
		input_family = atoi(map_family);
#ifdef DBUG_TABLE
		fprintf(stderr,"read_arch_event_files: row = %d, family = %d\n",row_count, input_family);
#endif
		current_field_pos++;
		current_field_start = current_field_pos;

		while(lineptr[current_field_pos] != comma)
			{
			if(current_field_pos > line_len)
				{
				fprintf(stderr,"read_arch_event_files: char match loop went beyond line length on model current_field_pos = %d for file %s\n",
					current_field_pos,input_map_file);
				exit(1);
				}
			if((current_field_pos - current_field_start) >= MAX_FAM)
				{
				fprintf(stderr,"read_arch_event_files: reading map file failed to terminate model field, %s\n",input_map_file);
				exit(1);
				}
			map_model[current_field_pos - current_field_start] = lineptr[current_field_pos];
			current_field_pos++;
			}

		if((current_field_pos - current_field_start) >= MAX_FAM)
			{
			fprintf(stderr,"read_arch_event_files: reading map file failed to terminate model field, %s\n",input_map_file);
			exit(1);
			}
		map_model[current_field_pos - current_field_start] = '\0';
		input_model = atoi(map_model);
#ifdef DBUG_TABLE
		fprintf(stderr,"read_arch_event_files:  row = %d, family = %d, input_model = %d\n",row_count, input_family, input_model);
#endif
		current_field_pos++;
		current_field_start = current_field_pos;

//		test family and model, if there is no match continue to next line in map file
		if((model != input_model) || (family != input_family)) continue;

//		set found_model flag
		found_model = 1;

//		read files names into map_file 2D array
		pmu_index = 0;
		while((lineptr[current_field_pos] != comma) && (lineptr[current_field_pos] != eol))
			{
			if(pmu_index == NUM_PMU)
				{
				fprintf(stderr,"read_arch_event_files: too many fields in map file %s\n",input_map_file);
				exit(1);
				}

			if(current_field_pos > line_len)
				{
				fprintf(stderr,"read_arch_event_files: char match loop went beyond line length on model current_field_pos = %d, file = %s\n",
					current_field_pos,input_map_file);
				exit(1);
				}
			if((current_field_pos - current_field_start) >= MAX_TABLE_NAME)
				{
				fprintf(stderr,"read_arch_event_files: reading map file failed to terminate family field %s\n",input_map_file);
				exit(1);
				}
			map_pmu_files[pmu_index][current_field_pos - current_field_start] = lineptr[current_field_pos];
			current_field_pos++;
			}

		map_pmu_files[pmu_index][current_field_pos - current_field_start] = '\0';
		pmu_file_len = (int)strlen(map_pmu_files[pmu_index]);
		if(pmu_index == 0)
			{
			for(i=0; i<pmu_file_len; i++)input_file[base_len+file_dir_len + i] = map_pmu_files[pmu_index][i];
			input_file[base_len+file_dir_len + pmu_file_len] = '\0';
#ifdef DBUG_TABLE
			fprintf(stderr,"read_arch_event_files: map_file = %s\n",input_file);
#endif
			}
		pmu_index++;
		current_field_pos++;
		current_field_start = current_field_pos;
		}
	if(found_model != 1)
		{
		fprintf(stderr,"read_arch_event_files: failed to locate family = %d and model = %d in file = %s\n", family,model,input_map_file);
		exit(1);
		}


//	create struc of event files
	this_arch_tables = (arch_event_tables_struc_ptr) calloc(1,sizeof(arch_event_tables_data));
	if(this_arch_tables == NULL)
		{
		err(1,"read_arch_event_files: table_reader failed to allocate this_arch_tables");
		exit(1);
		}

#ifdef DBUG
	fprintf(stderr,"read_arch_event_files: core event file path = %s, event file = %s\n",input_file);
#endif

	this_arch_tables->first_core_event = read_event_file(input_file);

//	uncore PMU not supported at this time
/*
	if(cbox_file_name != NULL)
		{
		sprintf_s(input_file,string_size,"%s%s",base_dir,cbox_file_name);
		this_arch_tables->first_cbox_event = read_event_file(input_file);
		}

	if(imc_file_name != NULL)
		{
		sprintf_s(input_file,string_size,"%s%s",base_dir,imc_file_name);
		this_arch_tables->first_imc_event = read_event_file(input_file);
		}

	if(pcie_file_name != NULL)
		{
		sprintf_s(input_file,string_size,"%s%s",base_dir,pcie_file_name);
		this_arch_tables->first_pcie_event = read_event_file(input_file);
		}
*/
	free(input_file);
	free(input_map_file);
	return this_arch_tables;
}

event_table_struc_ptr
read_event_file(const char* input_file)
{
//	The lack of getdelim in windows makes this quite a bit uglier than it could be
//	but this is probably faster   :-)
//
	FILE* input;
	char lineptr[LINE_SIZE],code[CODE_SIZE],umask[UMASK_SIZE],event_name[EVENT_NAME_SIZE],umask_name[UMASK_NAME_SIZE];
	char description[DESCRIPTION_SIZE],counter_ht_on[COUNTER_HT_ON_SIZE],counter_ht_off[COUNTER_HT_OFF_SIZE];
	char overflow[OVERFLOW_SIZE],msr_index[MSR_INDEX_SIZE],msr_value[MSR_VALUE_SIZE];
	char precise[PRECISE_SIZE],pes_upper[PES_UPPER_SIZE],data_lat[DATA_LAT_SIZE];
	char offcore_field[OFFCORE_FIELD_SIZE],fixed[FIXED_SIZE];
	int current_field_start=0, current_field_pos=0;
	int line_size=LINE_SIZE;
	char const* return_val;
	int i,field_len=0, row_count=0, line_len=0;
	char comma = ',';
	char quote = '"';
	char comment_delimeter;
	event_table_struc_ptr table_row, current_event, this_table, last_event=NULL;
	umask_table_struc_ptr current_umask;
	int first_event=0;
	uint	event_code;
	errno_t errno;

	char case_correction; 

	case_correction = 'a' - 'A';

	errno = fopen_s(&input, input_file, mode);
	if(errno != 0)
		err(1,"read_event_file: failed to open input table in read_core_event_file %s",input_file);

//	read first line
	return_val = fgets(lineptr,line_size,input);
	if(return_val == NULL)
		err(1,"read_event_file: failed to read first line from event table %s",input_file);
	row_count++;
#ifdef DBUG_TABLE
	fprintf(stderr,"read_event_file: first row %s\n",lineptr);
#endif

//	create the first event table structure which anchors the linked list
	table_row = (event_table_struc_ptr) calloc(1,sizeof(event_table_data));
	if(table_row == NULL)
		err(1,"read_event_file: failed to allocate first event table struc");
	this_table = table_row;

//	read the event rows
//	line read loop starts with extracting the event code
//	terminate lineptr to avoid idiotic OACR error
	lineptr[LINE_SIZE-1] = '\0';
	while((return_val = fgets(lineptr,line_size,input)) != NULL)
		{
		line_len = (int)strlen(lineptr);
		row_count++;
#ifdef DBUG_TABLE
		fprintf(stderr,"read_event_file: row %d: %s\n",row_count,lineptr);
#endif
		current_field_start=0;
		current_field_pos = 0;
		while(lineptr[current_field_pos] != comma)
			{
			if(current_field_pos > line_len)
				err(1,"read_event_file: char match loop went beyond line length on code current_field_pos = %d\n",current_field_pos);
			if(current_field_pos-current_field_start >= CODE_SIZE)
				{
				err(1,"read_event_file: code field length = %d larger than CODE_SIZE = %d",
					current_field_pos-current_field_start, CODE_SIZE);
				exit(1);
				}
			code[current_field_pos-current_field_start] = lineptr[current_field_pos];
			current_field_pos++;
			}
		code[current_field_pos-current_field_start] = '\0';
		current_field_pos++;
		current_field_start = current_field_pos;
		event_code = (uint)strtol(code, NULL,0);
#ifdef DBUG_TABLE
		fprintf(stderr,"read_event_file: event code = %u\n",event_code);
#endif

		if(event_pointers[event_code] == NULL)
			{
//			first occurence of this event code
			if(first_event == 0)
				{
//				first data row in table..must use anchor of linked list
				event_pointers[event_code] = this_table;
				first_event = 1;
				last_event = this_table;
#ifdef DBUG_TABLE
				fprintf(stderr,"read_event_file: first event, code = %d\n", event_code);
#endif
				}
			else
				{
				table_row = (event_table_struc_ptr) calloc(1,sizeof(event_table_data));
				if(table_row == NULL)
					err(1,"read_event_file: failed to allocate new event table struc for event %d, row %d",
						event_code, row_count);
				event_pointers[event_code] = table_row;
				last_event->next = table_row;
#ifdef DBUG_TABLE
				fprintf(stderr,"read_event_file: new event, event_code = %d\n",event_code);
#endif
				last_event = table_row;
				}
			current_event = event_pointers[event_code];
			current_event->event_code = event_code;
			table_event_count++;
			}
		else
			{
			current_event = event_pointers[event_code];
#ifdef DBUG_TABLE
			fprintf(stderr,"read_event_file: event already exists, name = %s, code = %d\n",current_event->event_name, current_event->event_code);
#endif
			}


//		create next umask in event struc's list
		current_umask = (umask_table_struc_ptr)calloc(1,sizeof(umask_table_data));
		if(current_umask == NULL)
			err(1,"read_event_file: failed to allocate new umask struc for event %d, row %d",
				event_code, row_count);
		if(current_event->umask_list == 0)
			{
			current_event->umask_list = current_umask;
			current_event->old_umask = current_umask;
			}
		else
			{
			current_event->old_umask->next = current_umask;
			}
#ifdef DBUG_TABLE
		fprintf(stderr,"read_event_file: current_event->old_umask->umask_name = %s\n",current_event->old_umask->umask_name);
#endif
		current_event->old_umask = current_umask;

//		next field is the umask code
		while(lineptr[current_field_pos] != comma)
			{
			if(current_field_pos > line_len)
				err(1,"read_event_file: char match loop went beyond line length on umask current_field_pos = %d\n",current_field_pos);
			if(current_field_pos-current_field_start >= UMASK_SIZE)
				{
				err(1,"read_event_file: code field length = %d larger than UMASK_SIZE = %d",
					current_field_pos-current_field_start, UMASK_SIZE);
				exit(1);
				}
			umask[current_field_pos-current_field_start] = lineptr[current_field_pos];
			current_field_pos++;
			}
//	the following test cannot fail by design but is required by OACR
		if(current_field_pos-current_field_start >= UMASK_SIZE)
			{
			fprintf(stderr,"read_event_file: umask field exceeds UMASK_SIZE\n");
			exit(1);
			}
			umask[current_field_pos-current_field_start] = '\0';

		current_umask->umask = (uint)strtol(umask, NULL,0);
		current_field_pos++;
		current_field_start = current_field_pos;
//		fprintf(stderr,"read_event_file:  umask = %d\n",current_umask->umask);

//		next field is the event_name
		while(lineptr[current_field_pos] != comma)
			{
			if(current_field_pos > line_len)
				err(1,"read_event_file: char match loop went beyond line length on event_name current_field_pos = %d\n",current_field_pos);
			if(current_field_pos-current_field_start >= EVENT_NAME_SIZE)
				{
				err(1,"read_event_file: code field length = %d larger than EVENT_NAME_SIZE = %d",
					current_field_pos-current_field_start, EVENT_NAME_SIZE);
				exit(1);
				}
			if((lineptr[current_field_pos] <= 'Z') && (lineptr[current_field_pos] >= 'A'))
				lineptr[current_field_pos]+= case_correction;
			event_name[current_field_pos-current_field_start] = lineptr[current_field_pos];
			current_field_pos++;
			}
//	this test cannot fail by design but required by OACR
		if(current_field_pos-current_field_start >= EVENT_NAME_SIZE)
			{
			fprintf(stderr,"read_event_file: event_name field exceeds EVENT_NAME_SIZE\n");
			exit(1);
			}
			event_name[current_field_pos-current_field_start] = '\0';
		field_len = current_field_pos-current_field_start;
		current_field_pos++;
		current_field_start = current_field_pos;

		if(current_event->event_name == NULL)
			{
			current_event->event_name = (char*)calloc(1,field_len+1);
			if(current_event->event_name == NULL)
				{
				err(1,"read_event_file: failed to allocate buffer for event name, event_code %d, row %d",event_code,row_count);
				exit(1);
				}
			for(i=0; i< field_len; i++)current_event->event_name[i] = event_name[i];
			}
//		fprintf(stderr,"read_event_file:  current_event_name = %s\n",current_event->event_name);

//		next field is the umask_name
		while(lineptr[current_field_pos] != comma)
			{
			if(current_field_pos > line_len)
				err(1,"read_event_file: char match loop went beyond line length on umask_name current_field_pos = %d\n",current_field_pos);
			if(current_field_pos-current_field_start >= UMASK_NAME_SIZE)
				{
				err(1,"read_event_file: code field length = %d larger than UMASK_NAME_SIZE = %d",
					current_field_pos-current_field_start, UMASK_NAME_SIZE);
				exit(1);
				}
			if((lineptr[current_field_pos] <= 'Z') && (lineptr[current_field_pos] >= 'A'))
				lineptr[current_field_pos]+= case_correction;
			umask_name[current_field_pos-current_field_start] = lineptr[current_field_pos];
			current_field_pos++;
			}
//	this test cannot fail by design but required by OACR
		if(current_field_pos-current_field_start >= UMASK_NAME_SIZE)
			{
			fprintf(stderr,"read_event_file: umask_name field exceeds UMASK_NAME_SIZE\n");
			exit(1);
			}
			umask_name[current_field_pos-current_field_start] = '\0';
		field_len = current_field_pos-current_field_start;
		current_field_pos++;
		current_field_start = current_field_pos;
		current_umask->umask_name = (char*)calloc(1,field_len+1);
		if(current_umask->umask_name == NULL)
			{
			err(1,"read_event_file: failed to allocate buffer for umask name, event_code %d, row %d",event_code,row_count);
			exit(1);
			}
		for(i=0; i< field_len; i++)current_umask->umask_name[i] = umask_name[i];
//		fprintf(stderr," current_umask_name = %s\n",current_umask->umask_name);


//		next field is the description
//		   field can be delimited by either a ',' or '"'
//		   the '"' is the delimeter if the file is edited in excel and and there is a ',' within the text
//		   if there is no ',' then excel will delete the '"' when a save is executed
//		        making editing the event files on windows a bit of a pain
		comment_delimeter = comma;
		if( lineptr[current_field_pos] == quote )
			{
//                 		first " is at the beginning of the string
			current_field_pos++;
			comment_delimeter = quote;
			}
		current_field_start = current_field_pos;

//		second " is at the end of the string
		while(lineptr[current_field_pos] != comment_delimeter)
			{
			if(current_field_pos > line_len)
				{
				fprintf(stderr,"read_event_file: char match loop went beyond line length on description current line = %s\n",lineptr);
				fprintf(stderr,"read_event_file: char match loop went beyond line length on description current_field_pos = %d, line len = %d\n",current_field_pos, line_len);
				exit(1);
				}
			if(current_field_pos-current_field_start >= DESCRIPTION_SIZE)
				{
				err(1,"read_event_file: code field length = %d larger than DESCRIPTION_SIZE = %d",
					current_field_pos-current_field_start, DESCRIPTION_SIZE);
				exit(1);
				}
			description[current_field_pos-current_field_start] = lineptr[current_field_pos];
			current_field_pos++;
			}
//	the following test cannot fail by design but is required by OACR
		if(current_field_pos-current_field_start >= DESCRIPTION_SIZE)
			{
			fprintf(stderr,"read_event_file: description field exceeds DESCRIPTION_SIZE\n");
			exit(1);
			}
		description[current_field_pos-current_field_start] = '\0';
		field_len = current_field_pos-current_field_start;
		current_field_pos++;
		current_field_start = current_field_pos;
		current_umask->description = (char*)calloc(1,field_len+1);
		if(current_umask->description == NULL)
			err(1,"read_event_file: failed to allocate buffer for description, event_code %d, row %d",event_code,row_count);
		for(i=0; i< field_len; i++)current_umask->description[i] = description[i];
#ifdef DBUG_TABLE
		fprintf(stderr,"read_event_file:  current_description = %s\n",current_umask->description);
#endif

//                 and advance over the trailing comma at the end of the string
		if(comment_delimeter == quote)current_field_pos++;
		current_field_start = current_field_pos;

//		next field is the counter_mask_ht_on
		while(lineptr[current_field_pos] != comma)
			{
			if(current_field_pos > line_len)
				err(1,"read_event_file: char match loop went beyond line length on ht_on counter mask current_field_pos = %d\n",current_field_pos);
			if(current_field_pos-current_field_start >= COUNTER_HT_ON_SIZE)
				err(1,"read_event_file: code field length = %d larger than COUNTER_HT_ON_SIZE = %d",
					current_field_pos-current_field_start, COUNTER_HT_ON_SIZE);
			counter_ht_on[current_field_pos-current_field_start] = lineptr[current_field_pos];
			current_field_pos++;
			}
		counter_ht_on[current_field_pos-current_field_start] = '\0';
		current_field_pos++;
		current_field_start = current_field_pos;
		current_umask->counter_mask_ht_on = atoi(counter_ht_on);
#ifdef DBUG_TABLE
		fprintf(stderr,"read_event_file:  current_ht_on mask = %d\n",current_umask->counter_mask_ht_on);
#endif

//		next field is the counter_mask_ht_off
		while(lineptr[current_field_pos] != comma)
			{
			if(current_field_pos > line_len)
				err(1,"read_event_file: char match loop went beyond line length on ht_off counter mask current_field_pos = %d\n",current_field_pos);
			if(current_field_pos-current_field_start >= COUNTER_HT_OFF_SIZE)
				err(1,"read_event_file: code field length = %d larger than COUNTER_HT_OFF_SIZE = %d",
					current_field_pos-current_field_start, COUNTER_HT_OFF_SIZE);
			counter_ht_off[current_field_pos-current_field_start] = lineptr[current_field_pos];
			current_field_pos++;
			}
		counter_ht_off[current_field_pos-current_field_start] = '\0';
		current_field_pos++;
		current_field_start = current_field_pos;
		current_umask->counter_mask_ht_off = atoi(counter_ht_off);
#ifdef DBUG_TABLE
		fprintf(stderr,"read_event_file:  current_ht_off mask = %d\n",current_umask->counter_mask_ht_off);
#endif

//		next field is the overflow
		while(lineptr[current_field_pos] != comma)
			{
			if(current_field_pos > line_len)
				err(1,"char match loop went beyond line length on overflow current_field_pos = %d\n",current_field_pos);
			if(current_field_pos-current_field_start >= OVERFLOW_SIZE)
				err(1,"code field length = %d larger than OVERFLOW_SIZE = %d",
					current_field_pos-current_field_start, OVERFLOW_SIZE);
			overflow[current_field_pos-current_field_start] = lineptr[current_field_pos];
			current_field_pos++;
			}
		overflow[current_field_pos-current_field_start] = '\0';
		current_field_pos++;
		current_field_start = current_field_pos;
		current_umask->overflow = atoi(overflow);
#ifdef DBUG_TABLE
		fprintf(stderr,"read_event_file:  umask_name = %s, current_overflow = %d\n",current_umask->umask_name, current_umask->overflow);
#endif

//		next field is the msr_index
		while(lineptr[current_field_pos] != comma)
			{
			if(current_field_pos > line_len)
				err(1,"read_event_file: char match loop went beyond line length on msr_index current_field_pos = %d\n",current_field_pos);
			if(current_field_pos-current_field_start >= MSR_INDEX_SIZE)
				err(1,"read_event_file: code field length = %d larger than MSR_INDEX_SIZE = %d",
					current_field_pos-current_field_start, MSR_INDEX_SIZE);
			msr_index[current_field_pos-current_field_start] = lineptr[current_field_pos];
			current_field_pos++;
			}
		msr_index[current_field_pos-current_field_start] = '\0';
		current_field_pos++;
		current_field_start = current_field_pos;
		current_umask->msr_index = (uint)strtol(msr_index,NULL,0);
#ifdef DBUG_TABLE
		fprintf(stderr,"read_event_file:  current_msr_index = 0x%x\n",current_umask->msr_index);
#endif

//		next field is the msr_value
		while(lineptr[current_field_pos] != comma)
			{
			if(current_field_pos > line_len)
				{
				err(1,"read_event_file: char match loop went beyond line length on msr_value current_field_pos = %d\n",current_field_pos);
				exit(1);
				}
			if(current_field_pos-current_field_start >= MSR_VALUE_SIZE)
				{
				err(1,"read_event_file: code field length = %d larger than MSR_VALUE_SIZE = %d",
					current_field_pos-current_field_start, MSR_VALUE_SIZE);
				exit(1);
				}
			msr_value[current_field_pos-current_field_start] = lineptr[current_field_pos];
			current_field_pos++;
			}
		msr_value[current_field_pos-current_field_start] = '\0';
		current_field_pos++;
		current_field_start = current_field_pos;
		current_umask->msr_value = (uint64_t)strtoll(msr_value, NULL,0);
#ifdef DBUG
		fprintf(stderr,"read_event_file:  current_msr_value = 0x%lx\n",current_umask->msr_value);
#endif

//		next field is the precise
		while(lineptr[current_field_pos] != comma)
			{
			if(current_field_pos > line_len)
				{
				err(1,"read_event_file: char match loop went beyond line length on precise current_field_pos = %d\n",current_field_pos);
				exit(1);
				}
			if(current_field_pos-current_field_start >= PRECISE_SIZE)
				{
				err(1,"read_event_file: code field length = %d larger than PRECISE_SIZE = %d",
					current_field_pos-current_field_start, PRECISE_SIZE);
				exit(1);
				}
			precise[current_field_pos-current_field_start] = lineptr[current_field_pos];
			current_field_pos++;
			}
		precise[current_field_pos-current_field_start] = '\0';
		current_field_pos++;
		current_field_start = current_field_pos;
		current_umask->precise = atoi(precise);
#ifdef DBUG_TABLE
		fprintf(stderr,"read_event_file:  current_precise = %d\n",current_umask->precise);
#endif

//		next field is the pes_upper
		while(lineptr[current_field_pos] != comma)
			{
			if(current_field_pos > line_len)
				{
				err(1,"read_event_file: char match loop went beyond line length on pes_upper current_field_pos = %d\n",current_field_pos);
				exit(1);
				}
			if(current_field_pos-current_field_start >= PES_UPPER_SIZE)
				{
				err(1,"read_event_file: code field length = %d larger than PES_UPPER_SIZE = %d",
					current_field_pos-current_field_start, PES_UPPER_SIZE);
				exit(1);
				}
			pes_upper[current_field_pos-current_field_start] = lineptr[current_field_pos];
			current_field_pos++;
			}
		pes_upper[current_field_pos-current_field_start] = '\0';
		current_field_pos++;
		current_field_start = current_field_pos;
		current_umask->pes_upper = (uint64_t)strtoll(pes_upper, NULL,0);
#ifdef DBUG_TABLE
		fprintf(stderr,"read_event_file:  current_pes_upper = 0x%x\n",current_umask->pes_upper);
#endif

//		next field is the data_lat
		while(lineptr[current_field_pos] != comma)
			{
			if(current_field_pos > line_len)
				{
				err(1,"read_event_file: char match loop went beyond line length on data_lat current_field_pos = %d\n",current_field_pos);
				exit(1);
				}
			if(current_field_pos-current_field_start >= DATA_LAT_SIZE)
				{
				err(1,"read_event_file: code field length = %d larger than DATA_LAT_SIZE = %d",
					current_field_pos-current_field_start, DATA_LAT_SIZE);
				exit(1);
				}
			data_lat[current_field_pos-current_field_start] = lineptr[current_field_pos];
			current_field_pos++;
			}
		data_lat[current_field_pos-current_field_start] = '\0';
		current_field_pos++;
		current_field_start = current_field_pos;
		current_umask->data_lat = atoi(data_lat);
#ifdef DBUG_TABLE
		fprintf(stderr,"read_event_file:  current_data_lat = %d\n",current_umask->data_lat);
#endif

//		next field is the offcore_field
		while(lineptr[current_field_pos] != comma)
			{
			if(current_field_pos > line_len)
				{
				err(1,"read_event_file: char match loop went beyond line length on offcore_field current_field_pos = %d\n",current_field_pos);
				exit(1);
				}
			if(current_field_pos-current_field_start >= OFFCORE_FIELD_SIZE)
				{
				err(1,"read_event_file: code field length = %d larger than OFFCORE_FIELD_SIZE = %d",
					current_field_pos-current_field_start, OFFCORE_FIELD_SIZE);
				exit(1);
				}
			offcore_field[current_field_pos-current_field_start] = lineptr[current_field_pos];
			current_field_pos++;
			}
		offcore_field[current_field_pos-current_field_start] = '\0';
		current_field_pos++;
		current_field_start = current_field_pos;
		current_umask->offcore_field = atoi(offcore_field);
#ifdef DBUG_TABLE
		fprintf(stderr,"read_event_file:  current_offcore_field = %d\n",current_umask->offcore_field);
#endif

//		next field is the fixed
//		end of line
		while(lineptr[current_field_pos] != '\n')
			{
			if(current_field_pos > line_len)
				{
				err(1,"read_event_file: char match loop went beyond line length on fixed current_field_pos = %d\n",current_field_pos);
				exit(1);
				}
			if(current_field_pos-current_field_start >= FIXED_SIZE)
				{
				err(1,"read_event_file: code field length = %d larger than FIXED_SIZE = %d",
					current_field_pos-current_field_start, FIXED_SIZE);
				exit(1);
				}
			fixed[current_field_pos-current_field_start] = lineptr[current_field_pos];
			current_field_pos++;
			}
		fixed[current_field_pos-current_field_start] = '\0';
		current_field_pos++;
		current_field_start = current_field_pos;
		current_umask->fixed = atoi(fixed);
#ifdef DBUG_TABLE
		fprintf(stderr,"read_event_file:  current_fixed = %d\n",current_umask->fixed);
#endif

		}
	table_length = row_count;

	return this_table;

}

void*
dump_event_file(event_table_struc_cptr event_file)
{
	event_table_struc_cptr table_row;
	umask_table_struc_cptr current_umask;

	if(event_file == NULL)
		{
		fprintf(stderr," dump_event_file called with NULL pointer\n");
		return NULL;
		}
	table_row = event_file;
//	header row
 	fprintf(stderr," CODE,UMASK,NAME, UMASK_NAME,DESCRIPTION,COUNTER_MASK_HT_ON,COUNTER_MASK_HT_OFF,OVERFLOW,MSR_INDEX,MSR_VALUE,PRECISE_EVENT,PES_UPPER,DATA_LAT,OFFCORE_FIELD\n");
	while(table_row != NULL)
		{
		current_umask = table_row->umask_list;
		while(current_umask != NULL)
			{
			fprintf(stderr,"0x%x,", table_row->event_code);
			fprintf(stderr,"0x%x,%s,%s,\"%s\",%u,%u,%u,%u,",current_umask->umask,table_row->event_name,
				current_umask->umask_name,current_umask->description,current_umask->counter_mask_ht_on,
				current_umask->counter_mask_ht_off,current_umask->overflow,current_umask->msr_index);
			fprintf(stderr,"0x%llx,%u,0x%llx,%u,%u,%u\n",current_umask->msr_value,current_umask->precise,
				current_umask->pes_upper,current_umask->data_lat,current_umask->offcore_field,
				current_umask->fixed);
			current_umask = current_umask->next;
			}
		table_row = table_row->next;
		}
	return NULL;
}
int
hash_name(const char* name)
{
	int i,string_len;
	uint64_t hash_val = 0, char_pow;
	char max_name[EVENT_NAME_SIZE];
	int MAX_VAL=0x7FFFFFFF;
	int root = 31;

	char_pow = root;
	i=0;
	hash_val = 0;
	string_len = (int)strlen(name);
#ifdef DBUG_HASH
	fprintf(stderr," hash_name, name = %s, len = %d\n",name, string_len);
#endif

	while(name[i] != '\0')
		{
		if(i > EVENT_NAME_SIZE)
			{
			for(i=0; i< EVENT_NAME_SIZE-1; i++) max_name[i] = name[i];
			max_name[EVENT_NAME_SIZE-1] = '\0';
			err(1,"hash_name: event name too large first %d characters are %s",EVENT_NAME_SIZE,max_name);
			}
		hash_val += (((uint)name[i])*char_pow) & MAX_VAL;
		char_pow = (char_pow*root) & MAX_VAL;
		i++;
		}
#ifdef DBUG_HASH
	fprintf(stderr," hash_name, name = %s, len = %d, index = %lu\n",name, string_len, hash_val);
#endif
	return abs( (int)hash_val);
}

name_hash_table_ptr
create_event_name_hash_table(event_table_struc_ptr event_file)
{
	int index, hash_table_size;
	int hash_val, int_mult, remainder;
	event_table_struc_ptr current_event;
	char const* current_name;
	event_table_hash_data * event_name_hash_table, *next_link;
	event_table_hash_list_ptr next_table_entry, current_table_entry;
	name_hash_table_ptr this_table;

	this_table = (name_hash_table_ptr)calloc(1,sizeof(hash_table_data));
	if(this_table == NULL)
		{
		err(1,"create_event_name_hash_table: failed to create hash table struct for event file with first event = %s",event_file->event_name);
		exit(1);
		}
	hash_table_size = 100*table_event_count;
#ifdef DBUG_HASH
	fprintf(stderr,"create_event_name_hash_table: hash_table_size = %d, sizeof(event_table_hash_data) = %d \n",
			hash_table_size,sizeof(event_table_hash_data));
#endif
	this_table->hash_table_size = hash_table_size;
	event_name_hash_table = (event_table_hash_data *)calloc(hash_table_size,sizeof(event_table_hash_data));
#ifdef DBUG_HASH
	fprintf(stderr,"create_event_name_hash_table: address of event_name_hash_table = 0x%p \n", &event_name_hash_table[0]);
#endif
	if(event_name_hash_table == NULL)
		{
		err(1,"create_event_name_hash_table: failed to create hash table for event file with first event = %s",event_file->event_name);
		exit(1);
		}
	this_table->event_name_hash_table = event_name_hash_table;
	current_event = event_file;
	while(current_event != NULL)
		{
		current_name = current_event->event_name;
#ifdef DBUG_HASH
		fprintf(stderr,"create_event_name_hash_table: current name = %s\n",current_name);
#endif
		hash_val = hash_name((const char *)current_name);
		int_mult = hash_val/(uint64_t)hash_table_size;
		remainder = hash_val - (uint64_t)(int_mult*hash_table_size);
#ifdef DBUG_HASH
		fprintf(stderr,"create_event_name_hash_table: hash_val = %lu, hash_table_size = %d, mod = %d\n",hash_val,hash_table_size,hash_val%hash_table_size);
#endif
		index = hash_val % hash_table_size;
#ifdef DBUG_HASH
		fprintf(stderr,"create_event_name_hash_table: current name = %s, hash_val = %d, hashed index = %d, remainder = %d, int_mult = %d, address of hash_table[index], %p\n",
			current_name, hash_val, index, remainder, int_mult, &event_name_hash_table[index] );
#endif

		if(event_name_hash_table[index].this_event_ptr == NULL)
			{
			event_name_hash_table[index].this_event_ptr = current_event;
			current_table_entry = &event_name_hash_table[index];
#ifdef DBUG_HASH
			fprintf(stderr,"create_event_name_hash_table: hash entry set for first event = %s, index = %d\n",
					current_table_entry->this_event_ptr->event_name, index);
#endif
			}
		else
			{
			current_table_entry = &event_name_hash_table[index];
			while(current_table_entry->next != NULL)
				current_table_entry = current_table_entry->next;
			next_link = (event_table_hash_data*) calloc(1,sizeof(event_table_hash_data));
			next_table_entry = (event_table_hash_list_ptr) next_link;
			if(next_table_entry == NULL)
				{
				err(1,"create_event_name_hash_table: create_event_name_hash_table: failed to create linked list entry for hash table index %d, for event %s, list root %s",
					index,current_name,current_table_entry->this_event_ptr->event_name);
				exit(1);
				}
			next_table_entry->this_event_ptr = current_event;
			current_table_entry->next = next_table_entry;
#ifdef DBUG_HASH
			fprintf(stderr,"create_event_name_hash_table: duplicate hash index for first event = %s, new event = %s, index = %d\n",
					current_table_entry->this_event_ptr->event_name,current_table_entry->next->this_event_ptr->event_name, index);
#endif

			}
		current_event = current_event->next;
		}
	return this_table;
}