//	Winpmu
//	PMU data collection tool for Windows
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
//      Argument processing for win_pmu
//      process arguments after mode = stat or record
//      -tXXX       XXX = time in seconds
//      -dXXX       XXX = multiplex time in milliseconds
//	-iXXX       XXX = number of multiplex iterations
//      -CX,Y,Z,A-B X,Y,Z individual cores, A-B is core range inclusive of A & B
//      -XC         C = field separation character used for stat output lines
//	-F	    add fixed counters to every collection group (stat only)
//	-h	    call usage, print these comments and terminate
//      -eS1,S2     S1 and S2 are event definition strings 
//              S1  s1.s2.s3:c=X:i=Y:u:k:p=P:lbr=SL:P=N
//                  s1 is event name
//                  s2,s3..sN are umask names, programming fields are OR'd together
//                  c=X    X is cmask < 0xFF
//                  i=Y    Y = [0,1]
//                  u      user mode  (default set=1)
//                  k      kernel mode (default set=1)
//                         when only u or k is present, other is set to 0
//                  p=P    P = [0,1]  default is 0, for stat mode option is ignored
//                  L=SL    SL is a string defining the LBR filtering mode, ignored in stat mode
//		    P=N       N is the sampling period, ignored in stat mode
//       -- AS      AS is a string defining application to be launched by utility
//                    if this field exists and there is no -t option, collection time is set by duration of application
//
#define _AMD64_ 1
#include <windows.h>
#include <winnt.h>

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

typedef unsigned __int32 uint;
typedef unsigned __int64 uint64_t;
typedef int bool;

#include "example_pmu.h"
#include "event_util.h"
#include "RwMsrCtrlWrapper.h"   //-rk-

extern void err(int i, const char * s, ...);
extern void* usage();

input_args_ptr command_file_reader(int argc, char ** argv);
topology_struc_ptr get_topology();
int print_topology(const topology_struc_cptr this_topology);

perf_args_ptr arg_string(int argc, char **argv, int core_count);
void* dump_arg_string(perf_args_cptr arguments);

arch_event_tables_struc_ptr read_arch_event_files(int model, int family);
void* dump_event_file(event_table_struc_cptr event_file);

name_hash_table_ptr create_event_name_hash_table(event_table_struc_ptr event_file);

counter_program_struc_ptr create_counter_program(name_hash_table_ptr this_table,
	perf_args_ptr arguments);

event_group_data* create_event_group(counter_program_struc_ptr linked_program_event_list, 
	perf_args_ptr arguments);
void* print_event_group_array(event_group_data const* event_group_array, int num_groups);

void* event_collection_loop( event_group_data const* this_group_array, perf_args_ptr arguments);

void *rwmsrObj = NULL;  //-rk-

int
main(int argc, char **argv)
{

	topology_struc_cptr this_topology;
	perf_args_ptr arguments;
	input_args_ptr input_args;
	int model, family, core_count, ret_val;
	event_table_struc_ptr core_event_file;
	arch_event_tables_struc_ptr this_arch_tables;
	name_hash_table_ptr this_hash_table;
	counter_program_struc_ptr this_program_list;
	event_group_data const* this_group_array;
	char ** argv1;
	int argc1;
#ifdef DBUG
	int i;
#endif
//	force stderr to be unbuffered
	if( setvbuf( stderr, NULL, _IONBF, 0 ) != 0 )
		fprintf(stderr, "Incorrect type or size of buffer for stderr\n" );
#ifdef DBUG
	else
		fprintf(stderr, "'stderr' now has no buffer\n" );
#endif
#ifdef DBUG
	fprintf(stderr," argc = %d\n",argc);
	for(i=0; i<argc; i++)fprintf(stderr," arg %d = %s\n",i,argv[i]);
#endif
//	test for minimal arguments, let user know the usage
	if(argc < 3)
		{
		usage();
		exit(1);
		}
	if((argv[1][0] == '-') && (argv[1][1] == 'f'))
		{
//		command line in file
		input_args = command_file_reader(argc, argv);
		argv1 = input_args->argv1;
		argc1 = input_args->argc1;
		}
	else
		{
		argv1 = argv;
		argc1 = argc;
		}

//	instantiate driver
//-rk-
    rwmsrObj = RwMsrAllocControllerObj();
    if (!rwmsrObj)
    {
        err(1, "Failed to allocate RwMsr object");
        exit(1);
    }

    if (!RwMsrOpenDriver(rwmsrObj))
    {
        err(1, "Failed to open RwMsr Driver");
        exit(1);
    }

    // Set our driver controller mode (RwOnly, PrintOnly, RwAndPrint)
    enum ControllerMode mode = RwOnly;
    RwMsrSetControllerMode(rwmsrObj, mode);

//-rk-

	this_topology = get_topology();
	if(this_topology == NULL)
		{
		ret_val = print_topology(this_topology);
		fprintf(stderr,"stupid print to beat OACR %d\n",ret_val);
		exit(1);
		}
	core_count = this_topology->num_logical_cores;

	family = this_topology->family;
	model = this_topology->model;

	arguments = arg_string(argc1, argv1, core_count);
#ifdef DBUG
	dump_arg_string(arguments);
#endif

	this_arch_tables = read_arch_event_files(model, family);
	core_event_file = this_arch_tables->first_core_event;

#ifdef DEBUG_FILE
	dump_event_file (core_event_file);
#endif

	this_hash_table = create_event_name_hash_table(core_event_file);

	this_program_list = create_counter_program(this_hash_table, arguments);

	this_group_array = create_event_group(this_program_list, arguments);

#ifdef DBUG
	print_event_group_array( this_group_array, arguments->num_event_groups);
#endif
	
	event_collection_loop( this_group_array, arguments);

//	fprintf(stderr,"returned from event collection loop, about to call RwMsrDeleteControllerObj\n");

    RwMsrDeleteControllerObj(rwmsrObj); //-rk-
//	fprintf(stderr,"returned from RwMsrDeleteControllerObj program should exit\n");
}