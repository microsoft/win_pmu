//	MSR programming and data collection loop for win_pmu
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

//#define DBUG_COLLECT 1

#include <windows.h>
#include <winnt.h>

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <inttypes.h>
#include <tchar.h>
#include <Processthreadsapi.h>

typedef unsigned __int32 uint;
typedef unsigned __int64 uint64_t;
typedef int bool;

#include "example_pmu.h"
#include "event_util.h"
#include "RwMsrCtrlWrapper.h"   //-rk-
extern void *rwmsrObj;          //-rk-



extern void err(int i, const char * s, ...);

void*
clear_pmu(ULONG proc_count, const char*logicalProcBitVector)
{
//	reset PMU control registers to clean state
	USHORT msrAddr[NUM_PMU_CONTROL];
	ULONGLONG write_msrValue[NUM_PMU_CONTROL];
	ULONG msr_count;
	LONG msr_errno;
	int i;

	msr_count = NUM_PMU_CONTROL;
	msrAddr[0] = IA32_PERF_GLOBAL_CTRL;
	write_msrValue[0] = GLOBAL_CTR_DISABLE;

	for(i=0; i<GENERAL_COUNTERS; i++)
		msrAddr[i+1] = PERF_EVENT_SELECT_BASE + (USHORT)i;
	msrAddr[GENERAL_COUNTERS] = IA32_FIXED_CTR_CTRL;
	for(i=1; i<NUM_PMU_CONTROL; i++)
		write_msrValue[i] = 0;

	msr_errno = WriteMsrsOnLogicalProcsVector(rwmsrObj, proc_count, logicalProcBitVector, msr_count, msrAddr, write_msrValue);
	if(msr_errno != 0)
		{
		fprintf(stderr,
			"event_collection_loop: msrWrite for clear_pmu returned non zero value of %d\n",msr_errno);
		exit(1);
		}
	return NULL;
}

void*
event_collection_loop(event_group_data* this_group_array, perf_args_ptr arguments)
{
	int i,j,k, iter, group_index, events_in_group, name_index;
	int app_string_len=0;
	ULONG max_proc_count, max_msr_count, proc_count, msr_count, MAX_MSRs, msr_index;
	ULONG retSize;
	CHAR *logicalProcBitVector;
	USHORT *msrAddr;
	ULONGLONG *write_msrValue;
	ULONGLONG *read_msrValue;
	ULONGLONG start_time, stop_time, run_time, overflow = 0xFFFFFFFFFFFFFFFFULL;
	wchar_t *user_app=NULL;
	errno_t return_code;
	size_t retval, copy_len;
	rsize_t dstsz_app=0;
	FILE *outfile;
	LONG msr_errno;
	

//	for starting user application
    	STARTUPINFO si;
    	PROCESS_INFORMATION pi;

#ifdef DBUG_COLLECT
//	force stderr to be unbuffered
	if( setvbuf( stderr, NULL, _IONBF, 0 ) != 0 )
		fprintf(stderr, "Incorrect type or size of buffer for stderr\n" );
	else
		fprintf(stderr, "'stderr' now has no buffer\n" );
#endif

    	ZeroMemory( &si, sizeof(si) );
    	si.cb = sizeof(si);
    	ZeroMemory( &pi, sizeof(pi) );

	MAX_MSRs = 3 * GENERAL_COUNTERS + FIXED_COUNTERS + 2;
#ifdef DBUG_COLLECT
	fprintf(stderr," event_collection_loop: entering\n");
#endif
//	set up local variable to make this a little easier to read
	outfile = arguments->outfile;

	if(arguments->user_app_string != NULL)
		{
		app_string_len = (int) strlen(arguments->user_app_string);
#ifdef DBUG_COLLECT
		fprintf(stderr,"event_collection_loop: app_string_len = %d, app_string = %s\n",
			app_string_len, arguments->user_app_string);
#endif

		user_app = (wchar_t*) calloc(1,sizeof(wchar_t)*app_string_len + 20);
		if(user_app == NULL)
			{
			fprintf(stderr,"event_collection_loop: failed to allocate buffer for wide string version of user_app\n");
			exit(1);
			}
		copy_len = app_string_len;
		dstsz_app = copy_len+3;
		if(copy_len != 0)
			{
#ifdef DBUG_COLLECT
			fprintf(stderr,"  call to mbstowcs_s for %s, copy_len = %llu\n",arguments->user_app, copy_len);
			fprintf(stderr,"other arguments to mbtowcs_s &retval = %p, address of user_app = %p, dstsz_app = %llu\n",
				&retval, &user_app[0], dstsz_app);
#endif
			return_code = mbstowcs_s((size_t *) &retval, &user_app[0], dstsz_app, (const char *) arguments->user_app_string, copy_len);
#ifdef DBUG_COLLECT
			fprintf(stderr,"event_collection_loop: mbtowcs_s for user_app returned, retval = %llu, dstsz = %llu\n",retval,dstsz_app);
#endif
			if(return_code != 0)
				{
				fprintf(stderr,"event_collection_loop: mbtowcs_s for user_app failed, retval = %llu\n",retval);
				exit(1);
				}
			}

		user_app[app_string_len+2] = L'\0';
#ifdef DBUG_COLLECT
		fprintf(stderr,"event_collection_loop: W_app_string_len = %llu, W_app_string = %S\n",
			dstsz_app, user_app);
#endif
		fflush(stderr);

		arguments->full_iterations = 1;
		}
	else if(arguments->run_time != 0)
		{
		arguments->full_iterations = (int) (1000 * arguments->run_time/(arguments->num_event_groups*arguments->multiplex_time));
		if(arguments->full_iterations < 1)
			{
			err(1,"run_time is less than num_event_groups * multiplex_time/1000");
			exit(1);
			}
		}

#ifdef DBUG_COLLECT
	fprintf(stderr,"event_collection_loop: full_iterations = %d\n",arguments->full_iterations);
#endif

	max_proc_count = GetMaxSupportedLpCount();
	proc_count = arguments->core_count;
	if(max_proc_count < proc_count)
		{
		err(1," GetMaxSupportedLpCount() returned %d which is less that topology core count of %d",max_proc_count,proc_count); 
		exit(1);
		}
	max_msr_count = GetMaxSupportedMsrCount();
	if(max_msr_count < MAX_MSRs)
		{
		err(1," GetMaxSupportedMsrCount() returned %d which is less than what is needed, %d, for all supported counters",max_msr_count, MAX_MSRs);
		exit(1);
		}

	retSize = max_msr_count * proc_count * sizeof(ULONGLONG);
#ifdef DBUG_COLLECT
	fprintf(stderr," event_collection_loop:  retSize = %u, max_msr_count = %u, proc_count = %u\n",
		retSize, max_msr_count, proc_count);
#endif

//	get the array pointers

	msrAddr = (USHORT*)calloc(1,max_msr_count*sizeof(USHORT));
	if(msrAddr == NULL)
		{
		err(1,"event_collection_loop fails to allocate msrAddr array of length %d",MAX_MSRs);
		exit(1);
		}

	write_msrValue = (ULONGLONG*) calloc(1,max_msr_count*sizeof(ULONGLONG));
	if(write_msrValue == NULL)
		{
		err(1,"event_collection_loop fails to allocate write_msrValue array of length %d",MAX_MSRs);
		exit(1);
		}

	logicalProcBitVector = (char*) calloc(1,proc_count*sizeof(char));
	if(logicalProcBitVector == NULL)
		{
		err(1,"event_collection_loop fails to allocate logicalProcBitVector array of length %d",proc_count);
		exit(1);
		}

	read_msrValue = (ULONGLONG*) calloc(1,max_msr_count * proc_count*sizeof(ULONGLONG));
	if(read_msrValue == NULL)
		{
		err(1,"event_collection_loop fails to allocate read_msrValue array of length %d",MAX_MSRs*proc_count);
		exit(1);
		}
#ifdef DBUG_COLLECT
	fprintf(stderr,"event_collection_loop: finished calloc calls\n");
#endif

//	initialize the core array
	for(i = 0; i < (int)proc_count; i++)logicalProcBitVector[i] = arguments->core_array[i];
#ifdef DBUG_COLLECT
	fprintf(stderr,"event_collection_loop: initialized logicalProcBitVector\n");
#endif
//	create the data arrays per group
	for(group_index = 0; group_index < arguments->num_event_groups; group_index++)
		{
		this_group_array[group_index].total_run_time = 0;
		this_group_array[group_index].event_count_sum = (uint64_t *)calloc(1,proc_count*(GENERAL_COUNTERS+FIXED_COUNTERS)*sizeof(uint64_t));
		if(this_group_array[group_index].event_count_sum == NULL)
			{
			fprintf(stderr,"event_collection_loop: failed to allocate event_count_sum for group %d\n",
				group_index);
			exit(1);
			}
		}
//	test that the counters are free
	for(i = 0; i < (int)proc_count; i++)
		for(j = 0; j < NUM_PERF_EVENT_SELECT; j++)
			read_msrValue[i * max_msr_count + j] = 0;
	for(i = 0; i < GENERAL_COUNTERS; i++) msrAddr[i] = PERF_EVENT_SELECT_BASE + (USHORT)i;
	msrAddr[NUM_PERF_EVENT_SELECT - 1] = IA32_FIXED_CTR_CTRL;

#ifdef DBUG_COLLECT
	fprintf(stderr,"event_collection_loop: about to call ReadMsrsFromLogicalProcsVector to check perf_event_selct registers\n");
#endif
	msr_errno = ReadMsrsFromLogicalProcsVector(rwmsrObj, proc_count, logicalProcBitVector, NUM_PERF_EVENT_SELECT, msrAddr, read_msrValue, retSize);
#ifdef DBUG_COLLECT
	fprintf(stderr,"event_collection_loop: returned from ReadMsrsFromLogicalProcsVector to check perf_event_selct registers\n");
#endif
	if(msr_errno != 0)
		{
		fprintf(stderr,"event_collection_loop: msrRead for initial test read returned non zero value of %d\n",msr_errno);
		exit(1);
		}
//	test that all perf_event_select were zero and PMU is available on the cores of interest

	for(i = 0; i < (int)proc_count; i++)
		{
		if(logicalProcBitVector[i] == 0) continue;
		for(j = 0; j < NUM_PERF_EVENT_SELECT; j++)
			if(read_msrValue[i * max_msr_count + j] != 0)
				{
				err(1," perf_event_select[%d] was non zero = 0x%"PRIx64" on core %d",j,read_msrValue[i * max_msr_count + j],i);
				exit(1);
				}
		}
#ifdef DBUG_COLLECT
	fprintf(stderr,"event_collection_loop: all perf_event_selects are zero\n");
#endif

//	from here until the end of the routine all exit conditions 
//		should be preceeded by zeroing the perf_event_selects

//	freeze the counters
	msr_count = 1;
	write_msrValue[0] = GLOBAL_CTR_DISABLE;
	msrAddr[0] = IA32_PERF_GLOBAL_CTRL;
	msr_errno = WriteMsrsOnLogicalProcsVector(rwmsrObj, proc_count, logicalProcBitVector, msr_count, msrAddr, write_msrValue);

#ifdef DBUG_COLLECT
	fprintf(stderr,"event_collection_loop: returned from freezing counters\n");
#endif
	if(msr_errno != 0)
		{
		fprintf(stderr,"event_collection_loop: msrWrite for initial pmu freeze read returned non zero value of %d\n",msr_errno);
		exit(1);
		}
//	loop over iterations
	for(iter = 0; iter < arguments->full_iterations; iter++)
		{
//		loop over the event groups
		for(group_index = 0; group_index < arguments->num_event_groups; group_index++)
			{
			msr_index = 0;
//			set up the array of MSRs to be written and their values

//			general_counters
			for(k = 0; k < this_group_array[group_index].num_general_events; k++)
				{


				write_msrValue[msr_index] = this_group_array[group_index].perf_event_select[k].raw;
				msrAddr[msr_index] = (USHORT)this_group_array[group_index].general_PES_msr_index[k];
#ifdef DBUG_COLLECT
				fprintf(stderr,"event_collection_loop: msr index = 0x%x, value = 0x%"PRIx64"\n",
					msrAddr[msr_index], write_msrValue[msr_index]);
#endif
				msr_index++;
				write_msrValue[msr_index] = 0;
				msrAddr[msr_index] = (USHORT)this_group_array[group_index].general_counter_msr_index[k];
#ifdef DBUG_COLLECT
				fprintf(stderr,"event_collection_loop: msr index = 0x%x, value = 0x%"PRIx64"\n",
					msrAddr[msr_index], write_msrValue[msr_index]);
#endif
				msr_index++;
				if(this_group_array[group_index].extra_msr_index[k] != 0)
					{
					write_msrValue[msr_index] = this_group_array[group_index].extra_msr_value[k];
					msrAddr[msr_index] = (USHORT)this_group_array[group_index].extra_msr_index[k];
#ifdef DBUG_COLLECT
					fprintf(stderr,"event_collection_loop: msr index = 0x%x, value = 0x%"PRIx64"\n",
						msrAddr[msr_index], write_msrValue[msr_index]);
#endif
					msr_index++;
					}
				}

//			fixed_counters
			for(k = 0; k < this_group_array[group_index].num_fixed_events; k++)
				{
				write_msrValue[msr_index] = 0;
				msrAddr[msr_index] = (USHORT)this_group_array[group_index].fixed_counter_msr_index[k];
#ifdef DBUG_COLLECT
				fprintf(stderr,"event_collection_loop: msr index = 0x%x, value = 0x%"PRIx64"\n",
					msrAddr[msr_index], write_msrValue[msr_index]);
#endif
				msr_index++;
				}
			msrAddr[msr_index] = IA32_FIXED_CTR_CTRL;
			write_msrValue[msr_index] = this_group_array[group_index].group_fixed_ctr_ctrl.raw;
#ifdef DBUG_COLLECT
			fprintf(stderr,"event_collection_loop: msr index = 0x%x, value = 0x%"PRIx64"\n",
				msrAddr[msr_index], write_msrValue[msr_index]);
#endif
			msr_index++;
			msr_errno = WriteMsrsOnLogicalProcsVector(rwmsrObj, proc_count, logicalProcBitVector, msr_index, msrAddr, write_msrValue);
			if(msr_errno != 0)
				{
				fprintf(stderr,
					"event_collection_loop: msrWrite for event group %d returned non zero value of %d\n",
						group_index,msr_errno);
				exit(1);
				}

//			enable counters
			msr_count = 1;
			write_msrValue[0] = this_group_array[group_index].group_global_ctrl.raw;
			msrAddr[0] = IA32_PERF_GLOBAL_CTRL;
#ifdef DBUG_COLLECT
			fprintf(stderr,"event_collection_loop: msr index = 0x%x, value = 0x%"PRIx64"\n",
				msrAddr[0], write_msrValue[0]);
#endif
			msr_errno = WriteMsrsOnLogicalProcsVector(rwmsrObj, proc_count, logicalProcBitVector, msr_count, msrAddr, write_msrValue);
			if(msr_errno != 0)
				{
				fprintf(stderr,
					"event_collection_loop: msrWrite for enable of event group %d returned non zero value of %d\n",
						group_index,msr_errno);
				exit(1);
				}

			fflush(stderr);
//			wait for the multiplex_time duration (in milliseconds)
#ifdef DBUG_COLLECT
			fprintf(stderr,"event_collection_loop: programmed msrs for iteration %d, group = %d\n",iter, group_index);
#endif
			start_time = __rdtsc();
			if(app_string_len == 0)
				Sleep(arguments->multiplex_time);
			else
				{
// Start the child process. 
				if(user_app == NULL)
					{
					fprintf(stderr,"event_collection_loop: user_app = NULL before call to create_process\n");
					clear_pmu(proc_count, (const char *) logicalProcBitVector);
					exit(1);
					}
    				if( !CreateProcessW(  NULL,   		// module name must be NULL!!!
				         user_app,        		// application and command options
				        NULL,           		// Process handle not inheritable
				        NULL,         		  	// Thread handle not inheritable
				        FALSE,        		 	// Set handle inheritance to FALSE
				        0,              		// No creation flags
				        NULL,           		// Use parent's environment block
				        NULL,           		// Use parent's starting directory 
				        (LPSTARTUPINFOW)&si,            // Pointer to STARTUPINFO structure
				        &pi )           		// Pointer to PROCESS_INFORMATION structure
				    ) 
    					{
				        fprintf(stderr, "CreateProcess failed %u\n", GetLastError() );
					clear_pmu(proc_count, (const char *) logicalProcBitVector);
					exit(1);
					}
// 				Wait until child process exits.
				WaitForSingleObject( pi.hProcess, INFINITE );

// 				Close process and thread handles. 
				CloseHandle( pi.hProcess );
				CloseHandle( pi.hThread );
				}
			stop_time = __rdtsc();



//			freeze the counters
			msr_count = 1;
			write_msrValue[0] = GLOBAL_CTR_DISABLE;
			msrAddr[0] = IA32_PERF_GLOBAL_CTRL;
			msr_errno = WriteMsrsOnLogicalProcsVector(rwmsrObj, proc_count, logicalProcBitVector, msr_count, msrAddr, write_msrValue);
			if(msr_errno != 0)
				{
				fprintf(stderr,
					"event_collection_loop: msrWrite for freeze of event group %d returned non zero value of %d\n",
						group_index,msr_errno);
				exit(1);
				}

//			read the counters
			msr_index = 0;
//				general_counters
			for(k = 0; k < this_group_array[group_index].num_general_events; k++)
				{
				msrAddr[msr_index] = (USHORT)this_group_array[group_index].general_counter_msr_index[k];
				msr_index++;
				}

//			fixed_counters
			for(k = 0; k < this_group_array[group_index].num_fixed_events; k++)
				{
				msrAddr[msr_index] = (USHORT)this_group_array[group_index].fixed_counter_msr_index[k];
				msr_index++;
				}
			events_in_group = msr_index;

			msr_errno = ReadMsrsFromLogicalProcsVector(rwmsrObj, proc_count, logicalProcBitVector, msr_index, msrAddr, read_msrValue, retSize);
			if(msr_errno != 0)
				{
				fprintf(stderr,
					"event_collection_loop: msrRead for event group %d returned non zero value of %d\n",
						group_index,msr_errno);
				exit(1);
				}

//			process the data for each group
			if(stop_time > start_time)
				run_time = stop_time - start_time;
			else
				run_time = overflow - start_time + stop_time;
			this_group_array[group_index].total_run_time += run_time;

			for(k = 0; k < events_in_group; k++)
				{
				name_index = k;
				if(k >= this_group_array[group_index].num_general_events)
					name_index = GENERAL_COUNTERS + k - this_group_array[group_index].num_general_events;
				if((name_index < 0) || (name_index > GENERAL_COUNTERS + FIXED_COUNTERS - 1))
					{
					fprintf(stderr,"event_collection_loop: name index out of range = %d\n",name_index);
					exit(1);
					}
				if(arguments->verbose == 1)
					fprintf(outfile," %s%c %llu ",this_group_array[group_index].full_event_name[name_index],
						arguments->field_seperator, run_time);
				for(i=0; i<(int)proc_count; i++)
					{
					if(arguments->verbose == 1)
						fprintf(outfile,"%c %llu",
							arguments->field_seperator,read_msrValue[i*max_msr_count + k]);
					if((uint)(i*(GENERAL_COUNTERS + FIXED_COUNTERS) + k) >= proc_count*(GENERAL_COUNTERS + FIXED_COUNTERS))
						{
						fprintf(stderr,"event_count_loop: read_data index %d >= proc_count*total_counters = %u, for event group %d, event %s\n",
							i*(GENERAL_COUNTERS + FIXED_COUNTERS) + k,proc_count*(GENERAL_COUNTERS + FIXED_COUNTERS),
							group_index, this_group_array[group_index].full_event_name[name_index]);
						exit(1);
						}
					this_group_array[group_index].event_count_sum[i*(GENERAL_COUNTERS + FIXED_COUNTERS) + k]
						+= read_msrValue[i*max_msr_count + k];
					}
				fprintf(outfile,"\n");
				}

			}
//		any processing per iteration
		}

//	zero the perf_event_selects to leave things clean for the next user
	for(i = 0; i < GENERAL_COUNTERS; i++)
		{
		msrAddr[i] = PERF_EVENT_SELECT_BASE + (USHORT)i;
		write_msrValue[i] = 0;
		} 
	msrAddr[NUM_PERF_EVENT_SELECT - 1] = IA32_FIXED_CTR_CTRL;
	write_msrValue[NUM_PERF_EVENT_SELECT - 1] = 0;
	msr_errno = WriteMsrsOnLogicalProcsVector(rwmsrObj, proc_count, logicalProcBitVector, NUM_PERF_EVENT_SELECT, msrAddr, write_msrValue);
	if(msr_errno != 0)
		{
		fprintf(stderr,
			"event_collection_loop: msrWrite for pmu reset returned non zero value of %d\n",msr_errno);
		exit(1);
		}

//	print out core totals
	if(arguments->details == 1)
		fprintf(outfile,"total counts per core\n");

//	loop over the event groups
	for(group_index = 0; group_index < arguments->num_event_groups; group_index++)
		{
		events_in_group = this_group_array[group_index].num_general_events + 
			this_group_array[group_index].num_fixed_events;
//		loop over events in group
		for(k = 0; k < events_in_group; k++)
			{
			name_index = k;
			if(k >= this_group_array[group_index].num_general_events)
				name_index = GENERAL_COUNTERS + k - this_group_array[group_index].num_general_events;
			if((name_index < 0) || (name_index > GENERAL_COUNTERS + FIXED_COUNTERS - 1))
				{
				fprintf(stderr,"event_collection_loop: name index out of range = %d\n",name_index);
				exit(1);
				}
			if(arguments->details == 1)
				fprintf(outfile," %s%c %llu ",this_group_array[group_index].full_event_name[name_index],
					arguments->field_seperator, this_group_array[group_index].total_run_time);
//			loop over cores
			for(i=0; i<(int)proc_count; i++)
				{
				if(arguments->details == 1)
					fprintf(outfile,"%c %llu",
						arguments->field_seperator,
						this_group_array[group_index].event_count_sum[i*(GENERAL_COUNTERS + FIXED_COUNTERS) + k]);
				this_group_array[group_index].event_total[name_index] += 
					this_group_array[group_index].event_count_sum[i*(GENERAL_COUNTERS + FIXED_COUNTERS) + k];
				}
				fprintf(outfile,"\n");
			}
		}
//	global summary counts for each event in each group
	if(arguments->summary == 1)
		{
		fprintf(outfile,"total counts per machine\n");
//	loop over the event groups
		for(group_index = 0; group_index < arguments->num_event_groups; group_index++)
			{
			events_in_group = this_group_array[group_index].num_general_events + 
				this_group_array[group_index].num_fixed_events;
//			loop over events in group
			for(k = 0; k < events_in_group; k++)
				{
				name_index = k;
				if(k >= this_group_array[group_index].num_general_events)
					name_index = GENERAL_COUNTERS + k - this_group_array[group_index].num_general_events;
				if((name_index < 0) || (name_index > GENERAL_COUNTERS + FIXED_COUNTERS - 1))
					{
					fprintf(stderr,"event_collection_loop: name index out of range = %d\n",name_index);
					exit(1);
					}
				fprintf(outfile," %s%c %llu%c%llu \n",this_group_array[group_index].full_event_name[name_index],
					arguments->field_seperator, this_group_array[group_index].total_run_time,
					arguments->field_seperator,this_group_array[group_index].event_total[name_index]);
				}
			}
		}
	free(user_app);
	free(msrAddr);
	free(write_msrValue);
	free(read_msrValue);
	free(logicalProcBitVector);		
	return NULL;
}