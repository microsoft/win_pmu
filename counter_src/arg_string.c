//
//	argument string parser for win_pmu
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

typedef unsigned __int32 uint;
typedef unsigned __int64 uint64_t;
typedef int bool;

#include "example_pmu.h"
#include "event_util.h"

#define TEMP_SIZE 128

event_struc_ptr first_event = NULL, last_event = NULL;
pmu_event_list_ptr first_core_event=NULL, first_cbox_event=NULL, first_imc_event=NULL;
pmu_event_list_ptr first_qpi_event=NULL, first_pcie_event=NULL;
pmu_event_list_ptr this_core_event=NULL, this_cbox_event=NULL, this_imc_event=NULL;
pmu_event_list_ptr this_qpi_event=NULL, this_pcie_event=NULL;
int num_events=0, core_events=0, cbox_events=0, imc_events=0, qpi_events=0, pcie_events=0;


char STAT[] = "stat";

void err(int i, const char * s, ...)
{
        va_list arg;
        va_start(arg,s);
        vfprintf(stderr,s,arg);
        exit(i);
}


event_struc_ptr
decode_event_string(const char* event_string)
{
//      -eS1,S2     S1 and S2 are event definition strings 
//              S1  s1.s2.s3:c=X:i=Y:u:k:p=P:lbr=SL:P=N
//                  s1 is event name
//                  s2,s3..sN are umask names, programming fields are OR'd together
//                  c=X    X is cmask < 0xFF
//                  i=Y    Y = [0,1]
//                  u=1    user mode  (default set=1)
//                  k=1    kernel mode (default set=1)
//                         when only u=1 or k=1 is present, other is set to 0
//                  p=P    P = [0,1]  default is 0, for stat mode option is ignored
//                  L=SL    SL is a string defining the LBR filtering mode, ignored in stat mode
//		    P=N       N is the sampling period, ignored in stat mode
//
	int i, event_string_len;
	int event_str_pos = 1, string_pos_increment, len;
	int first_period = 0, first_colon = 0, event_field_start, event_field_len, event_start;
	int calloc_len;
	char comma = ',';
	char colon = ':';
	char equal = '=';
	char period = '.';
	char  c, *temp_string;
	int  arg_user=0, arg_kernel=0;


	event_struc_ptr current_event = 0;
	umask_ptr current_umask = NULL;
	pmu_event_list_ptr new_core_event=NULL, new_cbox_event=NULL, new_imc_event=NULL;
	pmu_event_list_ptr new_qpi_event=NULL, new_pcie_event=NULL;


	event_string_len = (int) strlen(event_string);
#ifdef DBUG_ARG
	fprintf(stderr,"decode_event_string: len = %d, string = %s\n",event_string_len,event_string);
#endif
	if(first_event == NULL)
		{
		first_event = calloc(1,sizeof(event_data));
		if(first_event == NULL)
			{
			err(1,"failed to create first_event struct in decode_event_string");
			exit(1);
			}
		current_event = first_event;
		last_event = current_event;
		}
	else
		{
		current_event = calloc(1,sizeof(event_data));
		if(current_event == NULL)
			{
			err(1,"failed to create current_event struct for new string in decode_event_string");
			exit(1);
			}
		last_event->next = current_event;
		last_event = current_event;
		}

	event_field_start = 0;
	event_start = 0;
	while((event_string[event_str_pos-1] != '\0') && (event_str_pos-1 <= event_string_len))
		{				//2
		string_pos_increment = 1;
		if(event_string[event_str_pos] == period)
			{                                //3
			//event string name seperator
			num_events++;
			event_field_len = event_str_pos - event_field_start;
			if(first_period == 0)
				{                          //4
				// event name field
				first_period = 1;
				current_event->event_name = (char *) calloc(1,event_field_len + 1);
				if(current_event->event_name == 0)
					{
					err(1,"failed to create current_event->event_name string in decode_event_string");
					exit(1);
					}
				for(i=0;i<event_field_len; i++)
					{
					current_event->event_name[i] = event_string[event_field_start + i];
					}
				current_event->event_name[event_field_len] = '\0';
				num_events++;
#ifdef DBUG_ARG
				fprintf(stderr,"decode_event_string_1: %s\n",current_event->event_name);
#endif
//		count events by core, cbox, imc, qpi and pcie PMU groups
//				make linked list for each
				if((current_event->event_name[0] == 'u') && (current_event->event_name[1] == 'n') &&
					(current_event->event_name[2] == 'c'))
					{
					if((current_event->event_name[4] == 'c') && (current_event->event_name[5] == 'b'))
//						cbox event
						{
						cbox_events++;
						new_cbox_event = (pmu_event_list_ptr) calloc(1,sizeof(pmu_event_list_data));
						if(new_cbox_event == NULL)
							{
							err(1,"decode_event_string: failed to allocate pmu_event_list for cbox");
							exit(1);
							}
						new_cbox_event->this_event = current_event;
						if(this_cbox_event == NULL)
							{
							this_cbox_event = new_cbox_event;
							first_cbox_event = this_cbox_event;
							}
						else
							this_cbox_event->next = new_cbox_event;
						}
					if((current_event->event_name[4] == 'i') && (current_event->event_name[5] == 'm'))
//						imc event
						{
						imc_events++;
						new_imc_event = (pmu_event_list_ptr) calloc(1,sizeof(pmu_event_list_data));
						if(new_imc_event == NULL)
							{
							err(1,"decode_event_string: failed to allocate pmu_event_list for imc");
							exit(1);
							}
						new_imc_event->this_event = current_event;
						if(this_imc_event == NULL)
							{
							this_imc_event = new_imc_event;
							first_imc_event = this_imc_event;
							}
						else
							this_imc_event->next = new_imc_event;
						}
					if((current_event->event_name[4] == 'q') && (current_event->event_name[5] == 'p'))
//						qpi event
						{
						qpi_events++;
						new_qpi_event = (pmu_event_list_ptr) calloc(1,sizeof(pmu_event_list_data));
						if(new_qpi_event == NULL)
							{
							err(1,"decode_event_string: failed to allocate pmu_event_list for qpi");
							exit(1);
							}
						new_qpi_event->this_event = current_event;
						if(this_qpi_event == NULL)
							{
							this_qpi_event = new_qpi_event;
							first_qpi_event = this_qpi_event;
							}
						else
							this_qpi_event->next = new_qpi_event;
						}
					if((current_event->event_name[4] == 'p') && (current_event->event_name[5] == 'c'))
//						pcie event
						{
						pcie_events++;
						new_pcie_event = (pmu_event_list_ptr) calloc(1,sizeof(pmu_event_list_data));
						if(new_pcie_event == NULL)
							{
							err(1,"decode_event_string: failed to allocate pmu_event_list for pcie");
							exit(1);
							}
						new_pcie_event->this_event = current_event;
						if(this_pcie_event == NULL)
							{
							this_pcie_event = new_pcie_event;
							first_pcie_event = this_pcie_event;
							}
						else
							this_pcie_event->next = new_pcie_event;
						}
					}
				else
//					core event
					{
						core_events++;
						new_core_event = (pmu_event_list_ptr) calloc(1,sizeof(pmu_event_list_data));
						if(new_core_event == NULL)
							{
							err(1,"decode_event_string: failed to allocate pmu_event_list for core");
							exit(1);
							}
						new_core_event->this_event = current_event;
						if(this_core_event == NULL)
							{
							this_core_event = new_core_event;
							first_core_event = this_core_event;
							}
						else
							this_core_event->next = new_core_event;
					}
				}                        //3
			else
				{                         //4
				// event string umask seperator
				if(current_event->umask_list == 0)
					{                   //5
					current_event->umask_list = (umask_ptr)calloc(1,sizeof(umask_data));
					if(current_event->umask_list == 0)
						{
						err(1,"failed to create current_event->umask_list string in decode_event_string");
						exit(1);
						}
					current_event->umask_list->umask = (char *)calloc(1,event_field_len + 1);
					if(current_event->umask_list->umask == 0)
						{
						err(1,"failed to create current_event->umask_list->umask string in decode_event_string");
						exit(1);
						}
					for(i=0;i<event_field_len; i++)
						{
//						if((event_string[event_field_start + i] <= 'Z') && (event_string[event_field_start + i] >= 'A'))
//							event_string[event_field_start + i] += case_correction;
							current_event->umask_list->umask[i] = event_string[event_field_start + i];
						}
					current_event->umask_list->umask[i] = '\0';
					current_umask = current_event->umask_list;
#ifdef DBUG_ARG
					fprintf(stderr,"decode_event_string_2_umask: %s\n",current_event->umask_list->umask);
#endif
					}                     //4
				else
					{                      //5
					// multiple umasks
					if(current_umask == NULL)
						{
						err(1,"OACR test for impossible error");
						exit(1);
						}
					current_umask->next = (umask_ptr)calloc(1,sizeof(umask_data));
					if(current_umask->next  == 0)
						{
						err(1,"failed to create current_umask->next struct in decode_event_string");
						exit(1);
						}
					current_umask = current_umask->next;
					current_umask->umask = (char *)calloc(1,event_field_len + 1);
					if(current_umask->umask == 0)
						{
						err(1,"failed to create current_umask->umask string in decode_event_string, multiple umasks");
						exit(1);
						}
					for(i=0;i<event_field_len; i++)
						{
//						if((event_string[event_field_start + i] <= 'Z') && (event_string[event_field_start + i] >= 'A'))
//							event_string[event_field_start + i] += case_correction;
						current_umask->umask[i] = event_string[event_field_start + i];
						}
					current_umask->umask[i] = '\0';
#ifdef DBUG_ARG
					fprintf(stderr,"decode_event_string_3_umask, multi umasks: %s\n",current_umask->umask);
#endif
					}                               //4
				}                              //3
			event_field_start = event_str_pos + 1;
			string_pos_increment = 1;
			}                          //2
		if((event_string[event_str_pos] == colon) || (event_string[event_str_pos] == comma) 
			|| (event_string[event_str_pos] == '\0'))
//		end of event name + umasks
			{                                 //3
			current_event->full_event_name = (char *) calloc(1, event_str_pos-event_start + 1);
			if(current_event->full_event_name == NULL)
				{
				err(1,"decode_event_string: failed to allocate full event name len = %d",
					event_str_pos-event_start + 1);
				exit(1);
				}
			for(i=0; i<event_str_pos-event_start; i++)current_event->full_event_name[i] =
						event_string[event_start + i];
			current_event->full_event_name[event_str_pos-event_start] = '\0';
#ifdef DBUG_ARG
			fprintf(stderr," decode_event_string: full_event_name = %s\n",current_event->full_event_name);
#endif
			event_field_len = event_str_pos - event_field_start;
			if(first_colon == 0)
				{                            //4
				if(event_string[event_str_pos] == colon)first_colon = 1;
			// process last umask
				if(current_event->umask_list == 0)
					{                          //5
					current_event->umask_list = (umask_ptr)calloc(1,sizeof(umask_data));
					if(current_event->umask_list == 0)
						{
						err(1,"failed to create current_event->umask_list string in decode_event_string");
						exit(1);
						}
					calloc_len = event_field_len + 2;
					current_event->umask_list->umask = (char *)calloc(1,calloc_len);
					if(current_event->umask_list->umask == 0)
						{
						err(1,"failed to create current_event->umask_list->umask string in decode_event_string");
						exit(1);
						}
					for(i=0;i<event_field_len; i++)current_event->umask_list->umask[i] =
						event_string[event_field_start + i];
					current_event->umask_list->umask[calloc_len - 1] = '\0';
#ifdef DBUG_ARG
					fprintf(stderr,"decode_event_string_4_umask: %s\n",current_event->umask_list->umask);
#endif
					current_umask = current_event->umask_list;
					}                               //4
				else
					{                                //5
					// multiple umasks so add another
					if(current_umask == NULL)
						{
						err(1,"decode_event_string: impossible error current_umask = NULL..OACR test");
						exit(1);
						}
					current_umask->next = (umask_ptr)calloc(1,sizeof(umask_data));
					if(current_umask->next  == 0)
						{
						err(1,"failed to create current_umask->next struct in decode_event_string");
						exit(1);
						}
					current_umask = current_umask->next;
					calloc_len = event_field_len + 2;
					current_umask->umask = (char *)calloc(1,calloc_len);
					if(current_umask->umask == 0)
						{
						err(1,"failed to create current_umask->umask string in decode_event_string, final multiple umasks");
						exit(1);
						}
					for(i=0;i<event_field_len; i++)current_umask->umask[i] =
						event_string[event_field_start + i];
					current_umask->umask[calloc_len-1] = '\0';
#ifdef DBUG_ARG
					fprintf(stderr,"decode_event_string_5_umask: %s\n",current_umask->umask);
#endif
					}                               //4

				}                               //3
			else 
				{                                //4
//				process event modifiers
				if(current_event->umask_list == 0)
					{
					err(1," event with no umask %s",current_event->event_name);
					exit(1);
					}
				if(event_string[event_field_start + 1] != equal)
					{
					err(1,"bad syntax in event modifier, no = sign for event =  %s ... %s",
						 current_event->event_name,current_umask->umask);
					exit(1);
					}
				len = event_str_pos - event_field_start - 2;
				temp_string = (char*) calloc(1,len+2);
				if(temp_string == NULL)
					{
					err(1,"failed to allocate temp_string in decode_event");
					exit(1);
					}
				for(i=0; i < len; i++)temp_string[i] = event_string[event_field_start + 2 + i];
				temp_string[len] = '\0';
#ifdef DBUG_ARG
				fprintf(stderr,"event_modifier flag = %c, value = %s\n",event_string[event_field_start],temp_string);
#endif
			//   parse event modifiers
				c = event_string[event_field_start];
				switch(c) 
					{                           //6
					case 'c':
						current_event->cmask = atoi(temp_string);
						break;
					case 'i' :
						current_event->inv = atoi(temp_string);
						break;
					case 'u' :
						arg_user = atoi(temp_string);
						break;
					case 'k' :
						arg_kernel = atoi(temp_string);
						break;
					case 'p' :
						current_event->precise = atoi(temp_string);
						break;
					case 'P' :
						current_event->period = atoi(temp_string);
						break;
					case 'L' :
						current_event->LBR_string = (char *) calloc(1,len+1);
						if(current_event->LBR_string == 0)
							{
							err(1,"failed to allocate buffer for LBR string for event =  %s ... %s",
							 	current_event->event_name,current_umask->umask);
							exit(1);
							}
						for(i=0; i <= len; i++)current_event->LBR_string[i] = temp_string[i];
						break;
					default:
						err(1, "unknown option %c", c);
						exit(1);
					}                            //5
				free(temp_string);
				}
			event_field_start = event_str_pos + 1;
			string_pos_increment = 1;
			if(event_string[event_str_pos] == comma)
				{
//				reset for next event
				event_start = event_field_start;
				first_period = 0;
				first_colon = 0;
                if (arg_user == 0 && arg_kernel == 0)
                    {
                        arg_user = arg_kernel = 1;
                    }
				current_event->user = (uint)arg_user;
				current_event->kernel = (uint)arg_kernel;
//				create struc for next event and add to linked list
				current_event = calloc(1,sizeof(event_data));
				if(current_event == NULL)
					{
					err(1,"failed to create current_event struct for next event in decode_event_string");
					exit(1);
					}
				last_event->next = current_event;
				last_event = current_event;
				arg_user = 0;
				arg_kernel = 0;
				}
			}
		event_str_pos += string_pos_increment;
#ifdef DBUG_ARG
		fprintf(stderr,"decode_event_string: increment event_str_pos %c\n",event_string[event_str_pos]);
#endif
		}
	if (arg_user == 0 && arg_kernel == 0)
		{
		arg_user = arg_kernel = 1;
		}
	current_event->user = (uint)arg_user;
	current_event->kernel = (uint)arg_kernel;

	return current_event;
}

char *
decode_core_string(const char* core_string, int core_count)
{
	int i,len, core, core2;
	char comma = ',';
	char dash = '-';
	int core_field_start=0, core_field_index=0, field_has_dash=0, field_len;
	char temp[TEMP_SIZE];
	char * core_array;

	core_field_index=0;
	core_array = (char *)calloc(1,sizeof(char)*core_count);
	if(core_array == NULL)
		{
		err(1,"decode_core_string failed to allocate core_array for %d cores",core_count);
		exit(1);
		}
	if(core_string == NULL)
		{
		for(i=0; i<core_count; i++)core_array[i] = 1;
		return core_array;
		}
#ifdef DBUG
	fprintf(stderr,"decode_core_string: core_string = %s\n",core_string);
#endif
	for(i=0; i<core_count; i++)core_array[i] = 0;
	len = (int)strlen(core_string);
	while(core_field_index <= len)
		{
		if((core_string[core_field_index] == comma) || (core_field_index == len))
			{
			//   end of a core string
			if(field_has_dash == 0)
				{
//				simple single core
				field_len = core_field_index - core_field_start;
				if(field_len >= TEMP_SIZE)
					{
					fprintf(stderr,"decode_core_string: field len >= temp_size, %d\n",field_len);
					exit(1);
					}
				for(i=0; i < field_len; i++)temp[i] = core_string[core_field_start + i];
				temp[field_len] = '\0';
				core = atoi(temp);
				core_array[core] = 1;
				}
			else
				{
//				field has a dash so it represents a range of cores
				field_len = field_has_dash - core_field_start;
				if(field_len >= TEMP_SIZE)
					{
					fprintf(stderr,"decode_core_string: field len >= temp_size, %d\n",field_len);
					exit(1);
					}
				for(i=0; i<field_len; i++)temp[i] = core_string[core_field_start + i];
				temp[field_len] = '\0';
				core = atoi(temp);
				field_len = core_field_index - field_has_dash - 1;
				if(field_len >= TEMP_SIZE)
					{
					fprintf(stderr,"decode_core_string: field len >= temp_size, %d\n",field_len);
					exit(1);
					}
				for(i=0; i<field_len; i++)temp[i] = core_string[field_has_dash + 1 + i];
				temp[field_len] = '\0';
				core2 = atoi(temp);
				field_has_dash = 0;
				if(core2 < core)
					{
					err(1,"bad core string %s, core2 = %d < core = %d",core_string,core2,core);
					exit(1);
					}
				for(i=core; i<=core2; i++)core_array[i] = 1;
				}
			core_field_start = core_field_index + 1;
			}
		if(core_string[core_field_index] == dash)field_has_dash = core_field_index;
		core_field_index++;
		}
#ifdef DBUG_ARG
	fprintf(stderr," decode_core_string: core_array = %d",(int)core_array[0]);
	for(i=1; i<core_count; i++)fprintf(stderr,", %d",(int)core_array[i]);
	fprintf(stderr,"\n");
#endif
	return core_array;
}
void*
usage()
{
	fprintf(stderr," Argument processing for perf_win\n");
	fprintf(stderr," process arguments after mode = stat or record\n");
	fprintf(stderr," -tXXX       XXX = time in seconds\n");
	fprintf(stderr," -mXXX       XXX = multiplex time in milliseconds\n");
	fprintf(stderr," -iXXX       XXX = number of multiplex iterations\n");
	fprintf(stderr," -v	     disable verbose printout of each core's data for each multiplex iteration\n");
	fprintf(stderr," -d	     disable detailed printout of each core's data summed over multiplex iterations\n");
	fprintf(stderr," -s	     disable summary printout of each event's data summed over cores and multiplex iterations\n");
	fprintf(stderr," -CX,Y,Z,A-B X,Y,Z individual cores, A-B is core range inclusive of A & B\n");
	fprintf(stderr," -XC         C = field separation character used for stat output lines, default is tab\n");
	fprintf(stderr," -F	    add fixed counters to every collection group (stat only)\n");
	fprintf(stderr," -h	    call usage, print these comments and terminate\n");
	fprintf(stderr," -eS1,S2     S1 and S2 are event definition strings\n");
	fprintf(stderr,"         S1  s1.s2.s3:c=X:i=Y:u:k:p=P:L=SL:P=N\n");
	fprintf(stderr,"             s1 is event name\n");
	fprintf(stderr,"             s2,s3..sN are umask names, programming fields are OR'd together\n");
	fprintf(stderr,"             c=X    X is cmask < 0xFF\n");
	fprintf(stderr,"             i=Y    Y = [0,1]\n");
	fprintf(stderr,"             u      user mode  (default set=1)\n");
	fprintf(stderr,"             k      kernel mode (default set=1)\n");
	fprintf(stderr,"                    when only u or k is present, other is set to 0\n");
	fprintf(stderr,"             p=P    P = [0,1]  default is 0, for stat mode option is ignored\n");
	fprintf(stderr,"             L=SL    SL is a string defining the LBR filtering mode, ignored in stat mode\n");
	fprintf(stderr," 	     P=N       N is the sampling period, ignored in stat mode\n");
	fprintf(stderr," -- AS      AS is a string defining application to be launched by utility\n");
	fprintf(stderr,"             if this field exists, collection time is set by duration of application\n");
	fprintf(stderr," -finfile   infile is a full or relative path to a file that contains all the arguments desired\n");
	fprintf(stderr,"	    for the run. If this option is used it must be the only option other that output redirection\n");
	fprintf(stderr,"	    this option is required for cases where the command line exceeds 8191 characters\n");
	fprintf(stderr," -ooutfile  By default the output is sent to stderr and stderr is set to be unbuffered\n");
	fprintf(stderr,"            the data will be written to the file defined by outfile\n"); 
	exit(1);
}
//      Argument processing for win_perf
//      process arguments after mode = stat or record
//      -tXXX       XXX = time in seconds
//      -dXXX       XXX = multiplex time in milliseconds
//	-iXXX       XXX = number of multiplex iterations
//	-v	     disable verbose printout of each core's data for each multiplex iteration
//	-d	     disable detailed printout of each core's data summed over multiplex iterations
//	-s	     disable summary printout of each event's data summed over cores and multiplex iterations
//      -CX,Y,Z,A-B X,Y,Z individual cores, A-B is core range inclusive of A & B
//      -XC         C = field separation character used for stat output lines
//	-F	    add fixed counters to every collection group (stat only)
//	-h	    call usage, print these comments and terminate
//      -eS1,S2     S1 and S2 are event definition strings 
//              S1  s1.s2.s3:c=X:i=Y:u:k:p=P:L=SL:P=N
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
//       Outputs
//          linked list of event structures
//          integer values for time, multiplex period
//          boolean array for cores to be used
//          application string
//
perf_args_ptr
arg_string(int argc, char **argv, int core_count)
{
	int i,j,k;
	int len, user_app_len, user_app_index;
	char c;
	char  *core_string=NULL, *event_string, *user_app_string;
	char *outfile_name;
	char field_seperator = '\t';
	char minus = '-';
	char space = ' ';
	char term = '\0';
	perf_args_ptr arguments;
	arg_event_struc_ptr arg_events;
	char case_correction;
	case_correction = 'a' - 'A';
	errno_t errno;
	FILE * outfile;

#ifdef DBUG_ARG
	fprintf(stderr,"arrived in arg_string, argc = %d\n",argc);
	for(i=0; i<argc; i++)fprintf(stderr," arg %d = %s\n",i,argv[i]);
#endif

	arguments = (perf_args_ptr) calloc(1,sizeof(perf_args_data));
	if(arguments == 0)
		{
		err(1,"arg_string: failed to allocate perf_args_data structure");
		exit(1);
		}
//		defaults
	arguments->core_count = core_count;
	arguments->run_time = 0;
	arguments->multiplex_time = 100;
	arguments->full_iterations = 0;
	arguments->verbose = 1;
	arguments->details = 1;
	arguments->summary = 1;
	arguments->field_seperator = '\t';
	arguments->outfile = stderr;

	if(strcmp(argv[1], STAT) == 0)
		{
//		stat counting mode
		arguments->mode = 1;
	
	        for( i=2; i< argc; i++)
			{
			if(argv[i][0] != minus)
				{
				err(1,"field does not start with minus %s",argv[i]);
				exit(1);
				}
			c = argv[i][1];
        	        switch(c) 
				{
				case 't':
					arguments->run_time = atoi(&argv[i][2]);
					break;
				case 'm':
					arguments->multiplex_time = atoi(&argv[i][2]);
					break;
				case 'i':
					arguments->full_iterations = atoi(&argv[i][2]);
					break;
				case 'v':
					arguments->verbose = 0;;
					break;
				case 'd':
					arguments->details = 0;;
					break;
				case 's':
					arguments->summary = 0;;
					break;
				case 'o':
					len = (int)strlen(argv[i]) - 2;
					outfile_name = (char*)calloc(1,len+2);
					if(outfile_name == 0)
						{
						err(1,"allocation of outfile_name failed for argc %d, string = %s", i,argv[i]);
						exit(1);
						}
					for(j=0; j<len; j++)outfile_name[j] = argv[i][j+2];
					outfile_name[len] = '\0';

					errno = fopen_s(&outfile, (const char*) outfile_name, "w");
					if(errno != 0)
						{
						fprintf(stderr,"arg_string: failed to open outfile %s\n",outfile_name);
						exit(1);
						}
//				OACR seems to insist on this here..
					if(outfile == 0)
						{
						fprintf(stderr," rarg_string FILE outfile = 0\n");
						exit(1);
						}
					arguments->outfile = outfile;
					free(outfile_name);
					break;
				case 'C':
					len = (int)strlen(argv[i]) - 2;
					core_string = (char*)calloc(1,len+2);
					if(core_string == 0)
						{
						err(1,"allocation of core string failed for argc %d, string = %s", i,argv[i]);
						exit(1);
						}
					for(j=0; j<len; j++)core_string[j] = argv[i][j+2];
					core_string[len] = '\0';
//		still need to set up boolean array for this from family, model, msr's

					break;
				case 'e':
					len = (int)strlen(argv[i]);
					event_string = (char*)calloc(1,len+1);
					if(event_string == 0)
						{
						err(1,"allocation of event string failed for argc %d, string = %s", i,argv[i]);
						exit(1);
						}
					for(j=0; j<len-2; j++)
						{
						event_string[j] = argv[i][j+2];
						if((event_string[j] <= 'Z') && (event_string[j] >= 'A'))
							event_string[j] += case_correction;
						}
					event_string[len] = '\0';
//					fprintf(stderr,"arg_string: len = %d,event_string = %s  argv = %s\n",len,event_string,argv[i]);
					arguments->last_event = decode_event_string(event_string);
					arguments->first_event = first_event;
					free(event_string);
					break;
				case 'X':
					field_seperator = argv[i][2];
					break;
				case 'F':
					arguments->add_fixed_counters = 1;
					arguments->fixed_cntr_user = 1;
					arguments->fixed_cntr_kernel = 1;
					len = (int)strlen(argv[i]);
					if(len != 2)
						{
						err(1," -F had a value attached, %s. Presence of -F adds fixed counters to all groups",argv[i]);
						exit(1);
						}
					break;
				case 'h':
					usage();
					break;
				case '-':
//                string for application to be launched
					len = (int)strlen(argv[i]);
					if(len != 2)
						{
						err(1,"user application string identifier must be -- followed by space but is %s",argv[i]);
						exit(1);
						}
					if(argc == i)
						err(1," user app seperator entered with no following arguments");
					len = 0;
					for(j = i+1; j < argc; j++)len += (int)strlen(argv[j]) + 1;
					user_app_len = len+1;
					user_app_string = (char*)calloc(1,sizeof(char)*(user_app_len+3));
					if(user_app_string == 0)
						{
						err(1,"allocation of user_app string failed for argc %d, string = %s %s", i+1,argv[i+1]);
						exit(1);
						}
					user_app_index = 0;
					for(j = i+1; j < argc; j++)
						{
						len = (int)strlen(argv[j]);
						if(user_app_index + len < user_app_len)
							for(k = 0; k < len; k++)user_app_string[user_app_index+k] = argv[j][k];
						user_app_index += len;
						if(user_app_index < user_app_len)
							{
							user_app_string[user_app_index] = space;
							if(j == argc-1)user_app_string[user_app_index] = term;
							}
						else
							{
							err(1,"user_app_index = %d exceeds size of app_string buffer = %d",user_app_index,user_app_len);
							exit(1);
							}
						user_app_index++;
						}
					arguments->user_app_string = user_app_string;
					i = argc;
					break;
				default:
					err(1, "unknown option %c", c);
					exit(1);
				}
			}
//		check for inconsistent arguments
		if((arguments->run_time != 0 ) && (arguments->full_iterations != 0 ))
			err(1," cannot set both run_time = %d and number of iterations = %d on the command line",
				arguments->run_time, arguments->full_iterations);
//		create arguments->core_array
		arguments->core_array = decode_core_string(core_string, core_count);

//		set fields here in case of defaults
		arguments->field_seperator = field_seperator;
//		save argument event list data
		arg_events =(arg_event_struc_ptr) calloc(1,sizeof(arg_event_data));
		if(arg_events == NULL)
			{
			err(1,"failed to allocate arg_events in arg_string");
			exit(1);
			}
		arg_events->first_core_event = first_core_event;
		arg_events->first_cbox_event = first_cbox_event;
		arg_events->first_imc_event = first_imc_event;
		arg_events->first_qpi_event = first_qpi_event;
		arg_events->first_pcie_event = first_pcie_event;
		arg_events->core_events = core_events;
		arg_events->cbox_events = cbox_events;
		arg_events->imc_events = imc_events;
		arg_events->qpi_events = qpi_events;
		arg_events->pcie_events = pcie_events;
		}
	else
		err(1,"first argument must be stat to enable counting mode");

	return arguments;
}

void*
dump_arg_string(perf_args_cptr arguments)
{
	event_struc_cptr current_event;
	umask_cptr current_umask;

//	simple global run options
	fprintf(stderr," runtime = %d, multiplex time = %d, field seperator = %c, fixed_counter = %d, user_app_string = %s\n",
		arguments->run_time, arguments->multiplex_time, arguments->field_seperator, 
		arguments->add_fixed_counters, arguments->user_app_string);
//	events
	fprintf(stderr,"num events = %d\n",num_events);
	if(arguments->first_event == NULL)
		{
		err(1,"dump_arg_string: first_event = NULL");
		exit(1);
		}
	current_event = arguments->first_event;
	while(current_event != 	NULL)
		{
		fprintf(stderr,"  event = %s, full name = %s",current_event->event_name, current_event->full_event_name);
		current_umask = current_event->umask_list;
		while(current_umask != NULL)
			{
			fprintf(stderr," umask = %s",current_umask->umask);
			current_umask = current_umask->next;
			}
		fprintf(stderr,"\n    cmask = %u, inv = %u, user = %u, kernel = %u, precise = %u, period = %u, loop_count = %u,",
			current_event->cmask, current_event->inv, current_event->user, current_event->kernel,
			current_event->precise, current_event->period, current_event->loop_count);
		if(current_event->LBR_string == NULL)
			fprintf(stderr," LBR_string = 0\n");
		else
			fprintf(stderr," LBR_string = 0%s\n",current_event->LBR_string);
		current_event = current_event->next;
		}
	return NULL;
}
