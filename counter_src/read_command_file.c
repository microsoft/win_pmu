//
//	command file reader for win_pmu
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

typedef unsigned __int32 uint;
typedef unsigned long long uint64_t;
typedef int bool;

#include "example_pmu.h"
#include "event_util.h"

typedef struct command_line * command_line_ptr;
typedef struct command_line {
	command_line_ptr	next;
	char*			this_string;
	int			this_len;
	}command_line_data;

#define LINE_SIZE 16384
char mode2[]="r";
char argv0[] = "perf_win.exe ";

extern void err(int i, const char * s, ...);

input_args_ptr
command_file_reader(int argc, char ** argv)
{
	FILE* input;
	char lineptr[LINE_SIZE];
	int line_size=LINE_SIZE;
	char * input_file, *command_line, *current_string;
	char** arg_strings;
	command_line_ptr first_line=NULL, this_line, old_line=NULL;
	int i,j, arg_count=0, input_file_len, len, arg_len=0, argv0_len;
	int row_count = 0, command_line_pos=0, command_line_len, num_valid_char, first_non_blank;
//	char comma = ',';
//	char quote = '"';
	const char space = ' ';
	const char dash = '-';
	const char tab   = '\t';
	const char controlM = '\r';
//	const char backslash = '\\';
	const char eol = '\n';
	errno_t errno;
	char const* return_val;
	input_args_ptr input_args;

//	this includes the trailing space
	argv0_len = (int) strlen(argv0);

	input_args = (input_args_ptr)calloc(1,sizeof(input_args_data));
	if(input_args == NULL)
		{
		fprintf(stderr,"command_file_reader: failed to allocate input_args buffer\n");
		exit(1);
		}

//	argv[1] has already been checked to be -f
//	therefore argv[2] must be full relative path and file name for command file
	if(argc != 3)
		{
		fprintf(stderr,"command_file_reader: argc != 3, argc = %d\n",argc);
		exit(1);
		}

	input_file_len = (int)strlen(argv[2]);
	input_file = (char *) calloc(1, input_file_len + 2);
	if(input_file == NULL)
		{
		fprintf(stderr,"command_file_reader: failed to allocate input_file buffer\n");
		exit(1);
		}
	for(i=0; i<input_file_len; i++)input_file[i] = argv[2][i];
	input_file[input_file_len] = '\0';

	errno = fopen_s(&input, (const char*) input_file, mode2);
	if(errno != 0)
		{
		fprintf(stderr,"command_file_reader: failed to open file %s\n",input_file);
		exit(1);
		}
//	OACR seems to insist on this here..but nowhere else
	if(input == 0)
		{
		fprintf(stderr,"command_file_reader: FILE input = 0\n");
		exit(1);
		}
//	read command lines
	while((return_val = fgets(lineptr,line_size,input)) != NULL)
		{
//		create linked list of structures for the command lines from the file
//		terminate lineptr to avoid idiotic OACR error
		lineptr[LINE_SIZE-1] = '\0';
		len = (int) strlen(lineptr);
#ifdef DBUG
		fprintf(stderr,"command_file_reader: line %d: len = %d, string = %s\n",row_count, len, lineptr);
#endif
		this_line = (command_line_ptr) calloc(1, sizeof(command_line_data));
		if(this_line == NULL)
			{
			fprintf(stderr,"command_file_reader: failed to allocate command line struct\n");
			exit(1);
			}
		if(first_line == NULL)first_line = this_line;
		this_line->this_string = (char*) calloc(1,len+1);
		if(this_line->this_string == NULL)
			{
			fprintf(stderr,"command_file_reader: failed to allocate command line struct string element\n");
			exit(1);
			}
		for(i=0; i<len; i++)this_line->this_string[i] = lineptr[i];
		this_line->this_string[len] = '\0';
		this_line->this_len = len;
		if(old_line != NULL)old_line->next = this_line;
		old_line = this_line;
		row_count++;
		arg_len += len;
		}

//	create a single buffer for the command line and replace tabs with spaces, backslashes with spaces 
//		and then compress out multiple spaces
	command_line_len = arg_len + argv0_len + 30;
	if(command_line_len < 30)
		{
		fprintf(stderr,"command_file_reader: failed on impossible OACR test, command_line_len < 30, len = %d\n",command_line_len);
		exit(1);
		}
	command_line = (char*) calloc(1,command_line_len);
	if(command_line == NULL)
		{
		fprintf(stderr,"read_command_line: failed to allocate single command_line buffer\n");
		exit(1);
		}
//	copy the command file into the command_line buffer suppressing and converting characters on the fly
	for(i=0; i<argv0_len; i++)
		if(i < command_line_len)command_line[i] = argv0[i];
	command_line_pos = argv0_len+1;

	if((command_line_pos < command_line_len) && (command_line_len >= 30))
		{
		command_line[command_line_pos] = '\0';
#ifdef DBUG
		fprintf(stderr,"read_command_line:buffer position = %d, command line = %s\n",command_line_pos, command_line);
#endif
		command_line_pos--;
		}


	this_line = first_line;
	row_count = 0;

	while(this_line != NULL)
		{
		len = this_line->this_len;
		current_string = this_line->this_string;
		num_valid_char = 0;
#ifdef DBUG
		fprintf(stderr,"read_command_line: copy loop: buffer pos = %d, len = %d, copied version = %s\n",command_line_pos, len, current_string);
#endif
		for(i=0; i<len; i++)
			{
			if((command_line_pos) >= command_line_len)
				{
				fprintf(stderr,"read_command_file: command_line_pos >= command_line_len\n");
				fprintf(stderr,"     command_line_pos = %d, command_line_len = %d, current line = %s\n",
					command_line_pos, command_line_len, current_string);
				exit(1);
				}
			if(current_string[i] == tab)current_string[i] = space;
			if(current_string[i] == controlM)current_string[i] = space;
//			if(current_string[i] == backslash)current_string[i] = space;
			if(current_string[i] == eol)current_string[i] = space;
			if((current_string[i] == space) && (current_string[i+1] == space))continue;
			if((current_string[i] == space) && (current_string[i+1] != dash))continue;
			if((current_string[i] == dash) && (current_string[i+1] == dash))
				{
//				application command_string
				for(j=i; j<len; j++)
					{
					if((command_line_pos) >= command_line_len)
						{
						fprintf(stderr,"read_command_file: command_line_pos >= command_line_len during copy of app string\n");
						fprintf(stderr,"     command_line_pos = %d, command_line_len = %d, current line = %s\n",
							command_line_pos, command_line_len, current_string);
						exit(1);
						}
					command_line[command_line_pos] = current_string[j];	
					command_line_pos++;
					num_valid_char++;
					}
				break;
				}
			command_line[command_line_pos] = current_string[i];	
			command_line_pos++;
			num_valid_char++;
			}
		old_line = this_line;
		this_line = this_line->next;
		free(old_line->this_string);
		free(old_line);
#ifdef DBUG
		if(command_line_pos < command_line_len)
			{
			command_line[command_line_pos] = '\0';
			fprintf(stderr,"read_command_line: line = %d, new valid char = %d, command line = %s\n",row_count, num_valid_char, command_line);
			}
#endif
		row_count++;
		}

//	second test required by OACR for no obvious reason
	if(((command_line_pos) >= command_line_len) || (command_line_len < 33))
		{
		fprintf(stderr,"read_command_file: command_line_pos + 3 >= command_line_len after line loop\n");
		fprintf(stderr, "    command_line_pos = %d, command_line_len = %d\n",
				command_line_pos, command_line_len);
		exit(1);
		}

//	terminate the final command line
		command_line[command_line_pos] = '\0';

#ifdef DBUG
	fprintf(stderr,"read_command_file: full command line = %s\n",command_line);
#endif
	arg_count = 0;
	len = (int)strlen(command_line);
	for(i=0; i<len; i++)
		if(command_line[i] == space)arg_count++;
	if(command_line[len-1] != space)arg_count++;
#ifdef DBUG
	fprintf(stderr,"read_command_line: space count = arg count = %d, string len = %d\n",arg_count,len); 
	fprintf(stderr,"read_command_file: full command line = %s\n",command_line);
#endif
	arg_strings = (char**) calloc(1, (arg_count+2) * sizeof(char*));
	if(arg_strings == NULL)
		{
		fprintf(stderr,"read_command_file failed to allocate arg_strings\n");
		exit(1);
		}
	num_valid_char = 0;
	row_count = 0;
	command_line_pos = 0;
	for(i=0; i<len; i++)
		{
		num_valid_char++;
		if((command_line[i] == space) || (i == len-1))
			{
//			end of an argument
			if(row_count >= arg_count)
				{
				fprintf(stderr,"read_command_line: walk through command line second time found more spaces, arg_count = %d, row_count = %d\n",
					arg_count, row_count);
				exit(1);
				}
			arg_strings[row_count] = (char*)calloc(1,num_valid_char+2);
			if(arg_strings[row_count] == NULL)
				{
				fprintf(stderr,"read_command_line: failed to allocate arg_strings for argument %d\n",row_count);
				exit(1);
				}
			first_non_blank = 0;
			if(command_line[command_line_pos] == space)first_non_blank = 1;
			for(j=first_non_blank; j<num_valid_char; j++)
				{
//					OACR required test
				if((command_line_pos+j) >= len)
					{
					fprintf(stderr,"read_command_line: command_line_pos+j >= len\n");
					exit(1);
					}
				arg_strings[row_count][j - first_non_blank] = command_line[command_line_pos + j];
				}
			arg_strings[row_count][num_valid_char] = '\0';
#ifdef DBUG
			fprintf(stderr,"read_command_line: processed arg count = %d, len = %d, string = %s\n",row_count, num_valid_char, arg_strings[row_count]);
#endif
			row_count++;
			command_line_pos = i;
			num_valid_char = 0;
			}
		}
	if(row_count > arg_count)
		{
		fprintf(stderr,"read_command_line: row_count = %d > arg_count = %d\n",row_count,arg_count);
		exit(1);
		}
#ifdef DBUG
	for(i=0; i< row_count; i++)
		fprintf(stderr,"read_command_line: argument %d, string = %s\n",i,arg_strings[i]);
#endif

	input_args->argc1 = row_count;
	input_args->argv1 = arg_strings;


	errno = fclose(input);
	if(errno != 0)
		{
		fprintf(stderr,"command_file_reader: failed to close file %s\n",input_file);
		exit(1);
		}
	free(input_file);
	free(command_line);
	return input_args;
}