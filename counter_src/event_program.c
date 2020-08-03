//	Define event programming and viable scheduling for win_pmu
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
#define _AMD64_ 1

//#define DBUG_EVPROG 1

#include <windows.h>
#include <winnt.h>

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <inttypes.h>

typedef unsigned __int32 uint;
typedef unsigned __int64 uint64_t;
typedef int bool;

#include "example_pmu.h"
#include "event_util.h"
#define PES_UPPER_SHIFT 16
#define PES_UPPER_MASK 0xFFFF

extern void err(int i, const char * s, ...);
void print_table_row(event_table_struc_cptr table_row, umask_table_struc_cptr current_umask);
void print_programmed_event_list(counter_program_struc_cptr linked_program_event_list);
void copy_counter_program_struc(counter_program_struc_ptr from, counter_program_struc_ptr to);
int hash_name(const char* name);
event_group_data * create_event_group(counter_program_struc_ptr linked_program_event_list, perf_args_ptr arguments);

counter_program_struc_ptr
create_counter_program(name_hash_table_ptr this_table,perf_args_ptr arguments)
{

	event_struc_ptr current_input_event;
	umask_cptr current_umask_list;
	umask_cptr current_input_umask;
	event_table_struc_ptr this_table_event;
	umask_table_struc_cptr this_table_umask=NULL;
	event_table_hash_data * event_name_hash_table, *this_table_entry;
	char const* input_event_name;
	char const* table_event_name;
	char const* current_input_umask_name;
	int hash_val, string_compare_val;
#ifdef DBUG_EVPROG
	int len1, len2;
#endif
//	uint64_t one = 1;
	int index, current_list_element, hash_table_size;

	counter_program_struc_ptr current_program_event, linked_program_event_list, 
		this_program_event;

//	loop through input event list and find the entries in the event table
//	break them into compatibly scheduled groups maintaining the order
 

	current_input_event = arguments->first_event;
	event_name_hash_table = this_table->event_name_hash_table;
	hash_table_size = this_table->hash_table_size;



	current_list_element = 0;
	linked_program_event_list = NULL;
	current_program_event = NULL;
#ifdef DBUG_EVPROG
	fprintf(stderr,"entering create_counter_program\n");
	fprintf(stderr," hash_table_size = %d, sizeof(event_table_hash_data) = %u\n",
		this_table->hash_table_size,(uint) sizeof(event_table_hash_data));
#endif

	while(current_input_event != NULL)
		{
		input_event_name = current_input_event->event_name;
#ifdef DBUG_EVPROG
		fprintf(stderr,"create_counter_program: input event name = %s\n",input_event_name);
#endif
		hash_val = hash_name((const char *)current_input_event->event_name);
#ifdef DBUG_EVPROG2
		fprintf(stderr," hash_val = %d, hash_table_size = %d, mod = %d\n",hash_val,hash_table_size,hash_val%hash_table_size);
#endif
		index = hash_val % hash_table_size;
#ifdef DBUG_EVPROG2
		fprintf(stderr,"create_counter_program: input event hash_val = %d, index = %d\n",hash_val, index);
#endif
		this_table_entry = &event_name_hash_table[index];
		if(this_table_entry->this_event_ptr == NULL)
			{
			fprintf(stderr," create_counter_program: bad event name: full name = %s\n",current_input_event->full_event_name);
			err(1,"create_counter_program: bad input event name = %s\n",input_event_name);
			exit(1);
			}
		this_table_event = this_table_entry->this_event_ptr;
		table_event_name = this_table_event->event_name;
#ifdef DBUG_EVPROG
		fprintf(stderr,"create_counter_program: table event name = %s\n",table_event_name);
#endif
		while(_stricmp(table_event_name,input_event_name) != 0)
			{
//			if a match does not occur then the hashing had better have produced duplicate indecies
//			for multiple event names in spite of the table being 100X larger than the event name list
//
			this_table_entry = this_table_entry->next;
			if(this_table_entry == NULL)
				{
				fprintf(stderr,"failed to match event name for event %s for index %d and a table base name\n",
					input_event_name, index);
				err(1,"failed to match event name for event %s for index %d and a table base name",
					input_event_name, index);
				exit(1);
				}
			this_table_event = this_table_entry->this_event_ptr;
			table_event_name = this_table_event->event_name;
			}
//		create the counter_program_struc
		this_program_event = (counter_program_struc_ptr) calloc(1, sizeof(counter_program_data));
		if(this_program_event == NULL)
			{
			err(1,"failed to allocate program_struc for event %d",current_list_element);
			exit(1);
			}
		if(current_program_event != NULL)current_program_event->next = this_program_event;
		current_program_event = this_program_event;

		if(linked_program_event_list == NULL)linked_program_event_list = current_program_event;

		current_program_event->perf_event_select.raw = 0x0UL;

		current_program_event->perf_event_select.reg.ev_code = this_table_event->event_code;
#ifdef DBUG_EVPROG
		fprintf(stderr," program_event_1: name = %s, perf_event_select = 0x%"PRIx64"\n", table_event_name,current_program_event->perf_event_select.raw);
#endif
		current_program_event->this_table_event = this_table_event;
		current_program_event->this_input_event = current_input_event;

//		now match the umasks...the input event can have multiple umask names
		current_umask_list = current_input_event->umask_list;
		current_input_umask = current_umask_list;
		while(current_input_umask != NULL)
			{
			current_input_umask_name = current_input_umask->umask;
			this_table_umask = this_table_event->umask_list;
			if(this_table_umask == NULL)
				{
				fprintf(stderr,"you will never see this failure of this_table_umask = Null, oacr requirement\n");
				exit(1);
				}
			while((string_compare_val = (int)_stricmp(this_table_umask->umask_name,current_input_umask_name)) != 0)
				{
//				walk the linked list of umasks for this_table_event
#ifdef DBUG_EVPROG
				len1 = (int)strlen(this_table_umask->umask_name);
				len2 = (int)strlen(current_input_umask_name);
				fprintf(stderr,"create_counter_program: table umask = %s, len = %d, arg umask = %s len = %d, compare_val = %d\n",
					this_table_umask->umask_name, len1, current_input_umask_name, len2, string_compare_val);
#endif
				this_table_umask = this_table_umask->next;
				if(this_table_umask == NULL)
					{
//					unmatched input umask string
					err(1,"invalid input umask string = %s for event %s",
						current_input_umask_name,input_event_name);
					exit(1);
					}
				}
//			matched input event name/umask name with table event name/umask name
//			construct event programming
#ifdef DBUG_EVPROG
			print_table_row(this_table_event,this_table_umask);
#endif
			current_program_event->full_event_name = current_input_event->full_event_name;

			current_program_event->perf_event_select.reg.umask = this_table_umask->umask;
#ifdef DBUG_EVPROG
			fprintf(stderr," program_event_2: name = %s, perf_event_select = 0x%"PRIx64"\n", 
				table_event_name,current_program_event->perf_event_select.raw);
#endif
			current_program_event->perf_event_select.raw |= (this_table_umask->pes_upper & PES_UPPER_MASK) << PES_UPPER_SHIFT;
//			fprintf(stderr," program_event_3: name = %s, perf_event_select = 0x%"PRIx64"\n", 
//				table_event_name,current_program_event->perf_event_select.raw);

				
			current_program_event->msr_value |= this_table_umask->msr_value;
#ifdef DBUG_EVPROG
			fprintf(stderr," program_event_msr: name = %s, msr_value = 0x%"PRIx64", this umask msr value = 0x%"PRIx64"\n", 
				table_event_name,current_program_event->msr_value, this_table_umask->msr_value);
#endif

			current_program_event->offcore_field |= this_table_umask->offcore_field;

//				last umask sets the period, fixed, secondary msr index, counter constraint
			current_program_event->period = this_table_umask->overflow;
			current_program_event->fixed = this_table_umask->fixed;
			current_program_event->msr_index = this_table_umask->msr_index;
//				just use ht on counter constraint for now
			current_program_event->allowed_counters = this_table_umask->counter_mask_ht_on;
#ifdef DBUG_EVPROG
			fprintf(stderr," umask_name = %s, current_overflow = %d\n",
				this_table_umask->umask_name, current_program_event->period);
#endif

			current_input_umask = current_input_umask->next;
			}

//			command line input modifiers
//				user_kernel
		if((current_input_event->user != 0) || (current_input_event->kernel != 0))
			{
			current_program_event->perf_event_select.reg.user = current_input_event->user;
			current_program_event->user = current_input_event->user;
			current_program_event->perf_event_select.reg.kernel = current_input_event->kernel;
			current_program_event->kernel = current_input_event->kernel;
#ifdef DBUG_EVPROG
		fprintf(stderr," program_event_4: name = %s, perf_event_select = 0x%"PRIx64"\n", 
			table_event_name,current_program_event->perf_event_select.raw);
#endif
			}

//				inv
		if(current_input_event->inv != 0)current_program_event->perf_event_select.reg.inv = 1;
#ifdef DBUG_EVPROG
		fprintf(stderr," program_event_4: name = %s, perf_event_select = 0x%"PRIx64"\n", 
			table_event_name,current_program_event->perf_event_select.raw);
		fprintf(stderr," program_event_5: name = %s, perf_event_select = 0x%"PRIx64"\n", 
			table_event_name,current_program_event->perf_event_select.raw);
#endif

//				cmask
		if(current_input_event->cmask != 0)current_program_event->perf_event_select.reg.cmask = current_input_event->cmask;
#ifdef DBUG_EVPROG
		fprintf(stderr," program_event_6: name = %s, perf_event_select = 0x%"PRIx64"\n", 
			table_event_name,current_program_event->perf_event_select.raw);
#endif

//				period
#ifdef DBUG_EVPROG
		fprintf(stderr," umask_name = %s, input_overflow = %d\n",
			this_table_umask->umask_name, current_input_event->period);
#endif
		if(current_input_event->period != 0)current_program_event->period = current_input_event->period;
#ifdef DBUG_EVPROG
		fprintf(stderr," umask_name = %s, current_overflow = %d\n",
			this_table_umask->umask_name, current_program_event->period);
#endif

//			disable interupts in PES if running in STAT mode
		if(arguments->mode == 1)current_program_event->perf_event_select.reg.interupt = 0;
#ifdef DBUG_EVPROG
		fprintf(stderr," program_event_7: name = %s, perf_event_select = 0x%"PRIx64"\n", 
			table_event_name,current_program_event->perf_event_select.raw);
#endif
//
		current_list_element++;
		current_input_event = current_input_event->next;
		}
#ifdef DBUG_EVPROG
	print_programmed_event_list(linked_program_event_list);
#endif
	return linked_program_event_list;				
}

event_group_data*
create_event_group(counter_program_struc_ptr linked_program_event_list, perf_args_ptr arguments)
{

	int i,index, group_index, len;
	event_group_data * event_group_array;
	counter_program_struc_ptr this_program_event, last_program_event, 
		first_program_event, first_event_in_group;
	int counter_in_use[GENERAL_COUNTERS], decode_allowed_counter_mask[8], allowed_counter;
	int fixed_counter_in_use[FIXED_COUNTERS], num_event_groups=0;
	int num_general_events=0, num_fixed_events=0;
	unsigned long global_enable_fc[FIXED_COUNTERS], global_enable_gc[GENERAL_COUNTERS];
	uint64_t  zero =0x0ULL;
	uint64_t fixed_ctrl_enable_user[3],fixed_ctrl_enable_kernel[3];
	char* fixed_event_names[FIXED_COUNTERS], *name1, *name2, *name3;
	const char *fixed_name1 = "fixed_counter.inst_retired";
	const char *fixed_name2 = "fixed_counter.cpu_clk_unhalted";
	const char *fixed_name3 = "fixed_counter.ref_clk_unhalted";

#ifdef DBUG_EVPROG
	fprintf(stderr," entered create_event_group\n");
#endif
	for(i=0; i<MAX_GENERAL_COUNTER_MASK; i++)decode_allowed_counter_mask[i]=-1;
	decode_allowed_counter_mask[0]=0;
	decode_allowed_counter_mask[1]=1;
	decode_allowed_counter_mask[3]=2;
	decode_allowed_counter_mask[7]=3;

	global_enable_gc[0] = 1;
	global_enable_gc[1] = 2;
	global_enable_gc[2] = 4;
	global_enable_gc[3] = 8;
	global_enable_fc[0] = 1;
	global_enable_fc[1] = 2;
	global_enable_fc[2] = 4;

	fixed_ctrl_enable_user[0] = 0x2UL;
	fixed_ctrl_enable_user[1] = 0x20UL;
	fixed_ctrl_enable_user[2] = 0x200UL;
	fixed_ctrl_enable_kernel[0] = 0x1UL;
	fixed_ctrl_enable_kernel[1] = 0x10UL;
	fixed_ctrl_enable_kernel[2] = 0x100UL;

	len = (int) strlen(fixed_name1);	
	name1 = (char *) calloc(1, (len+1)*sizeof(char));
	if(name1 == NULL)
		{
		fprintf(stderr,"create_event_group: failed to allocate buffer for fixed name 1\n");
		exit(1);
		}
	for(i=0; i<len; i++)name1[i] = fixed_name1[i];
	name1[len] = '\0';

	len = (int) strlen(fixed_name2);	
	name2 = (char *) calloc(1, (len+1)*sizeof(char));
	if(name2 == NULL)
		{
		fprintf(stderr,"create_event_group: failed to allocate buffer for fixed name 1\n");
		exit(1);
		}
	for(i=0; i<len; i++)name2[i] = fixed_name2[i];
	name2[len] = '\0';

	len = (int) strlen(fixed_name3);
	name3 = (char *) calloc(1, (len+1)*sizeof(char));
	if(name3 == NULL)
		{
		fprintf(stderr,"create_event_group: failed to allocate buffer for fixed name 1\n");
		exit(1);
		}
	for(i=0; i<len; i++)name3[i] = fixed_name3[i];
	name3[len] = '\0';

	fixed_event_names[0] = name1;
	fixed_event_names[1] = name2;
	fixed_event_names[2] = name3;


//	break events into compatible groups
	this_program_event = linked_program_event_list;
	first_program_event = this_program_event;
	last_program_event = this_program_event;
//	walk through the events and find first incompatibility within a set of general counters or fixed counters
//		then restart at the first incompatible event;
	while(last_program_event != NULL)
		{
		index = 0;
		for(i=0; i<GENERAL_COUNTERS; i++)counter_in_use[i]=0;
		for(i=0; i<FIXED_COUNTERS; i++)fixed_counter_in_use[i]=0;
		first_event_in_group = this_program_event;
#ifdef DBUG_EVPROG
		fprintf(stderr," starting group %d, first event = %s\n",num_event_groups,
				this_program_event->this_input_event->full_event_name);
#endif
		while((index < GENERAL_COUNTERS) && (this_program_event != NULL))
			{
			if(this_program_event->fixed >= 1)
				{
//			fixed events can be added until the same fixed event shows up a second time
				allowed_counter = this_program_event->fixed - 1;
#ifdef DBUG_EVPROG
				fprintf(stderr," fixed event allowed_counter = %d, event = %s\n",allowed_counter,
					this_program_event->this_input_event->full_event_name);
#endif
				if(fixed_counter_in_use[allowed_counter] == 1)
					{
//					same fixed event already in group -> exit loop, end of group
					last_program_event = this_program_event;
					break;
					}
				if((allowed_counter < 0) || (allowed_counter >= FIXED_COUNTERS))
					{
					fprintf(stderr,"create_event_groups: fixed counter out of range in table %d\n",allowed_counter);
					exit(1);
					}
				fixed_counter_in_use[allowed_counter] = 1;
				this_program_event = this_program_event->next;
				last_program_event = this_program_event;
				continue;
				}
			if(this_program_event->allowed_counters <= MAX_GENERAL_COUNTER_MASK)
				{
//				event is constrained to a particular counter
				allowed_counter = decode_allowed_counter_mask[this_program_event->allowed_counters - 1];
#ifdef DBUG_EVPROG
				fprintf(stderr," constrained event allowed_counter = %d, event = %s\n",allowed_counter,
					this_program_event->this_input_event->full_event_name);
#endif
				if((allowed_counter < 0) || (allowed_counter >= GENERAL_COUNTERS))
					{
					fprintf(stderr,"create_event_groups: general counter out of range in table %d\n",allowed_counter);
					exit(1);
					}
				if(counter_in_use[allowed_counter] == 1)
					{
//					found incompatible event with previous event in group -> exit loop, end of group
#ifdef DBUG_EVPROG
					fprintf(stderr,"found incompatible event, allowed counter = %d, counter_mask = %d, event name = %s\n",
						allowed_counter, this_program_event->allowed_counters, 
						this_program_event->this_input_event->full_event_name);
#endif
					last_program_event = this_program_event;
					break;
					}
				else
					counter_in_use[allowed_counter] = 1;
				}
			index++;
#ifdef DBUG_EVPROG
			if(this_program_event != NULL)
				fprintf(stderr,"  event allowed_counter = %d, event = %s\n",this_program_event->allowed_counters,
					this_program_event->this_input_event->full_event_name);
#endif
			this_program_event = this_program_event->next;
			last_program_event = this_program_event;


			}
		index = 0;
		
//	walk through this set of events and construct the group, assign counters to events
		this_program_event = first_event_in_group;
#ifdef DBUG_EVPROG
		fprintf(stderr," starting counter scheduling for group %d, first event = %s\n",num_event_groups,
				this_program_event->this_input_event->full_event_name);
#endif
		while(this_program_event != last_program_event)
			{
			this_program_event->multiplex_group = num_event_groups;
			if(this_program_event->fixed >= 1)
				{
				this_program_event->counter_msr = this_program_event->allowed_counters;
				this_program_event = this_program_event->next;
				continue;
				}
			if(this_program_event->allowed_counters > MAX_GENERAL_COUNTER_MASK)
				{
//				event to be assigned first unused general counter
				while((counter_in_use[index] == 1) && (index < GENERAL_COUNTERS))index++;
				if(counter_in_use[index] == 1)
					err(1, "badly constructed group, no available counter for event %s",
						this_program_event->this_input_event->full_event_name);
				this_program_event->counter_msr = index;
				counter_in_use[index] = 1;
				}
			else
				this_program_event->counter_msr = 
					decode_allowed_counter_mask[this_program_event->allowed_counters - 1];
			this_program_event = this_program_event->next;
			}
		index = 0;
		this_program_event = last_program_event;
		num_event_groups++;
		}

	arguments->num_event_groups = num_event_groups;

#ifdef DBUG_EVPROG
	fprintf(stderr," num_group_events determined in first loop over events = %d\n",arguments->num_event_groups);
#endif
#ifdef DBUG_EVPROG2
	print_programmed_event_list(linked_program_event_list);
#endif

//	create the event group structure array

	event_group_array = (event_group_data *) calloc(num_event_groups, sizeof(event_group_data));
	if(event_group_array == NULL)
		{
		fprintf(stderr,"create_event_group: failed to allocate event_group_array\n");
		exit(1);
		}

//	initialize first group
	num_general_events = 0;
	num_fixed_events = 0;
	group_index = 0;
	if(group_index >= num_event_groups)
		{
		fprintf(stderr," create_event_group: num_event_groups was less than 1\n");
		exit(1);
		}
	event_group_array[group_index].group_global_ctrl.raw = zero;

	for(i=0; i<GENERAL_COUNTERS; i++)
		{
		event_group_array[group_index].perf_event_select[i].raw = zero;
		event_group_array[group_index].general_counter_msr_index[i] = zero;
		event_group_array[group_index].extra_msr_index[i] = zero;
		event_group_array[group_index].extra_msr_value[i] = zero;
		event_group_array[group_index].full_event_name[i] = NULL;
		}
	for(i=0; i<FIXED_COUNTERS; i++)
		{
		event_group_array[group_index].fixed_counter_msr_index[i] = zero;
		event_group_array[group_index].full_event_name[GENERAL_COUNTERS + i] = NULL;
		}
	if(arguments->add_fixed_counters == 1)
		{
//		add fixed counters to all groups
		num_fixed_events = FIXED_COUNTERS;
		event_group_array[group_index].num_fixed_events = FIXED_COUNTERS;
		for(i=0; i<FIXED_COUNTERS; i++)
			{
			event_group_array[group_index].full_event_name[GENERAL_COUNTERS + i] = fixed_event_names[i];
			event_group_array[group_index].fixed_counter_msr_index[i] = FIXED_COUNTER_BASE + i;
			event_group_array[group_index].group_global_ctrl.reg.fixed_counter_enable += global_enable_fc[i];
#ifdef DBUG_EVPROG
			fprintf(stderr,"global_control = 0x%"PRIx64", fixed_counter_enable = 0x%"PRIx64"\n",
				event_group_array[group_index].group_global_ctrl.raw,
				(ULONGLONG) event_group_array[group_index].group_global_ctrl.reg.fixed_counter_enable);
#endif
			if(arguments->fixed_cntr_user == 1)
				event_group_array[group_index].group_fixed_ctr_ctrl.raw += fixed_ctrl_enable_user[i];
			if(arguments->fixed_cntr_kernel == 1)
				event_group_array[group_index].group_fixed_ctr_ctrl.raw += fixed_ctrl_enable_kernel[i];
			}
		}

	this_program_event = first_program_event;
	while(this_program_event != NULL)
		{
		if(this_program_event->multiplex_group != group_index)
			{
#ifdef DBUG_EVPROG
			fprintf(stderr,"new event group as index changed, old group index = %d\n",group_index);
#endif
			group_index++;
			if(group_index != this_program_event->multiplex_group)
				{
				fprintf(stderr,"create_event_group: error should never occur group index != this_program_event->multiplex_group\n");
				exit(1);
				}
			if(group_index >= num_event_groups)
				{
				fprintf(stderr,"create_event_group: error should never occur group index >= num_event_groups\n");
				exit(1);
				}
			num_general_events = 0;
			num_fixed_events = 0;
			event_group_array[group_index].group_global_ctrl.raw = zero;

			for(i=0; i<GENERAL_COUNTERS; i++)
				{
				event_group_array[group_index].perf_event_select[i].raw = zero;
				event_group_array[group_index].general_counter_msr_index[i] = zero;
				event_group_array[group_index].extra_msr_index[i] = zero;
				event_group_array[group_index].extra_msr_value[i] = zero;
				event_group_array[group_index].full_event_name[i] = NULL;
				}
			for(i=0; i<FIXED_COUNTERS; i++)
				{
				event_group_array[group_index].fixed_counter_msr_index[i] = zero;
				event_group_array[group_index].full_event_name[GENERAL_COUNTERS + i] = NULL;
				}
			if(arguments->add_fixed_counters == 1)
				{
//			add fixed counters to all groups
				num_fixed_events = FIXED_COUNTERS;
				event_group_array[group_index].num_fixed_events = FIXED_COUNTERS;
				for(i=0; i<FIXED_COUNTERS; i++)
					{
					event_group_array[group_index].full_event_name[GENERAL_COUNTERS + i] = fixed_event_names[i];
					event_group_array[group_index].fixed_counter_msr_index[i] = FIXED_COUNTER_BASE + i;
					event_group_array[group_index].group_global_ctrl.reg.fixed_counter_enable += global_enable_fc[i];
#ifdef DBUG_EVPROG
					fprintf(stderr,"global_control = 0x%"PRIx64", fixed = %d, fixed_counter_enable = 0x%x\n",
						event_group_array[group_index].group_global_ctrl.raw, 
                        this_program_event->fixed,
						event_group_array[group_index].group_global_ctrl.reg.fixed_counter_enable);
#endif
					if(arguments->fixed_cntr_user == 1)
						event_group_array[group_index].group_fixed_ctr_ctrl.raw += fixed_ctrl_enable_user[i];
					if(arguments->fixed_cntr_kernel == 1)
						event_group_array[group_index].group_fixed_ctr_ctrl.raw += fixed_ctrl_enable_kernel[i];
					}
				}
			}

//		member of this group, increment fixed count or general count as appropriate
		if(this_program_event->fixed == 0)
			{
			if(group_index >= num_event_groups)
				{
				fprintf(stderr,"create_event_group: error should never occur group index >= num_event_groups in fixed block\n");
				exit(1);
				}
#ifdef DBUG_EVPROG
			fprintf(stderr," create_event_group: fixed = 0, event name = %s\n",this_program_event->this_input_event->event_name);
#endif
			event_group_array[group_index].num_general_events++;
			num_general_events++;
			if(num_general_events > GENERAL_COUNTERS)
				{
				err(1,"num_general_events for group %d is greater than GENERAL_COUNTERS",group_index);
				exit(1);
				}
			event_group_array[group_index].full_event_name[num_general_events - 1] = this_program_event->full_event_name;
			event_group_array[group_index].perf_event_select[num_general_events - 1] = this_program_event->perf_event_select;
			event_group_array[group_index].general_PES_msr_index[num_general_events - 1] = 
				this_program_event->counter_msr + PERF_EVENT_SELECT_BASE;
			event_group_array[group_index].general_counter_msr_index[num_general_events - 1] = 
				this_program_event->counter_msr + GENERAL_COUNTER_BASE;
			event_group_array[group_index].extra_msr_index[num_general_events - 1] = this_program_event->msr_index;
			event_group_array[group_index].extra_msr_value[num_general_events - 1] = this_program_event->msr_value;
			if(this_program_event->counter_msr >= GENERAL_COUNTERS)
				{
				err(1,"this_program_event->counter_msr for group %d is greater than GENERAL_COUNTERS",group_index);
				exit(1);
				}
			event_group_array[group_index].group_global_ctrl.reg.general_counter_enable += global_enable_gc[this_program_event->counter_msr];
			}
		else if (arguments->add_fixed_counters == 0)
//		event is fixed counter event and adding fixed counters to all groups was not requested
			{
#ifdef DBUG_EVPROG
			fprintf(stderr," create_event_group: fixed != 0, event name = %s\n",this_program_event->this_input_event->event_name);
#endif
			event_group_array[group_index].full_event_name[GENERAL_COUNTERS + num_fixed_events] = this_program_event->full_event_name;
			event_group_array[group_index].fixed_counter_msr_index[num_fixed_events] = FIXED_COUNTER_BASE + this_program_event->fixed - 1;
			event_group_array[group_index].num_fixed_events++;
			num_fixed_events++;
//	this test will never fail but is required by OACR
			if((this_program_event->fixed - 1 < 0) || (this_program_event->fixed - 1 >= 3))
				{
				fprintf(stderr," create_event_group: impossible test on fixed required by OACR\n");
				exit(1);
				}
			event_group_array[group_index].group_global_ctrl.reg.fixed_counter_enable += global_enable_fc[this_program_event->fixed - 1];
#ifdef DBUG_EVPROG
			fprintf(stderr,"global_control = 0x%"PRIx64", fixed = %d, fixed_counter_enable = 0x%x\n",
				event_group_array[group_index].group_global_ctrl.raw, this_program_event->fixed,
				event_group_array[group_index].group_global_ctrl.reg.fixed_counter_enable);
#endif
			if(this_program_event->user == 1)
				event_group_array[group_index].group_fixed_ctr_ctrl.raw += fixed_ctrl_enable_user[this_program_event->fixed - 1];
			if(this_program_event->kernel == 1)
				event_group_array[group_index].group_fixed_ctr_ctrl.raw += fixed_ctrl_enable_kernel[this_program_event->fixed - 1];

			}
		this_program_event = this_program_event->next;
		
		}
	
	return event_group_array;				
}


void
print_table_row(event_table_struc_cptr table_row, umask_table_struc_cptr current_umask)
{
			fprintf(stderr,"0x%x,", table_row->event_code);
			fprintf(stderr,"0x%x,%s,%s,\"%s\",%u,%u,%u,0x%x,",current_umask->umask,table_row->event_name,
				current_umask->umask_name,current_umask->description,current_umask->counter_mask_ht_on,
				current_umask->counter_mask_ht_off,current_umask->overflow,current_umask->msr_index);
			fprintf(stderr,"0x%"PRIx64",%u,0x%"PRIx64",%u,%u,%u\n",current_umask->msr_value,current_umask->precise,
				current_umask->pes_upper,current_umask->data_lat,current_umask->offcore_field,
				current_umask->fixed);
}		
void
print_programmed_event_list(counter_program_struc_cptr linked_program_event_list)
{
	event_struc_cptr this_input_event;
	event_table_struc_cptr this_table_event;
	counter_program_struc_cptr current_program_event;
	
	if(linked_program_event_list == NULL)	
		{
		err(1,"print_programmed_event_list called with NULL pointer");
		exit(1);
		}

//		walk the list and print out all the info
	current_program_event = linked_program_event_list;
	while(current_program_event != NULL)
		{
		this_table_event = current_program_event->this_table_event;
		if(this_table_event == NULL)
			{
			err(1,"print_programmed_event: badly constructed programmed event struc this_table_event = NULL");
			exit(1);
			}
		this_input_event = current_program_event->this_input_event;
		if(this_input_event == NULL)
			{
			err(1,"print_programmed_event: badly constructed programmed event struc this_input_event = NULL");
			exit(1);
			}
		fprintf(stderr,"Input event: %s programmed as 0x%"PRIx64"\n",this_input_event->full_event_name, current_program_event->perf_event_select.raw);
		fprintf(stderr,"     additional MSR index = 0x%x, value = 0x%"PRIx64", offcore_field = %u, period = %u\n",
			current_program_event->msr_index, current_program_event->msr_value, current_program_event->offcore_field, current_program_event->period);
		fprintf(stderr,"     assigned counter = %u, allowed counters = %d, fixed counter flag = %d, multiplex group = %d\n",
			current_program_event->counter_msr, current_program_event->allowed_counters, current_program_event->fixed,
			current_program_event->multiplex_group);
		if(current_program_event->next == NULL)fprintf(stderr,"print_programmed_event_list: end of linked list\n");
		current_program_event = current_program_event->next;
		}

}

void*
print_event_group_array(event_group_data const* event_group_array, int num_groups)
{
	int i,j;
	if(event_group_array == NULL)
		{
		err(1," print_event_group_array called with null pointer");
		exit(1);
		}
	if(num_groups == 0)
		{
		err(1," print_event_group_array called with numgroups = 0");
		exit(1);
		}
	for(i = 0; i < num_groups; i++)
		{
		fprintf(stderr, "group %d, global enable = 0x%"PRIx64", num_general_events = %d, num_fixed_events = %d\n",
			i,event_group_array[i].group_global_ctrl.raw,event_group_array[i].num_general_events
			,event_group_array[i].num_fixed_events);
		for(j=0; j< event_group_array[i].num_general_events; j++)
			{
			fprintf(stderr," event %d, perf_event_select = 0x%"PRIx64", PES index = 0x%"PRIx64"\n",
				j,event_group_array[i].perf_event_select[j].raw, event_group_array[i].general_PES_msr_index[j]);
			fprintf(stderr,"     counter index = 0x%"PRIx64", extra msr = 0x%"PRIx64", extra msr value = 0x%"PRIx64"\n",
				event_group_array[i].general_counter_msr_index[j], event_group_array[i].extra_msr_index[j],
				event_group_array[i].extra_msr_value[j]);
			}
		fprintf(stderr," fixed event ctr_ctrl = 0x%"PRIx64"\n",event_group_array[i].group_fixed_ctr_ctrl.raw);
		for(j=0; j< event_group_array[i].num_fixed_events; j++)
			{
			fprintf(stderr," fixed counter MSR = 0x%"PRIx64"\n",event_group_array[i].fixed_counter_msr_index[j]);
			}
		}

	return NULL;
}