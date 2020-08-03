//
//	utility header of structure definitions, declarations etc for win_pmu
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
#pragma once
#define MAX_EVENT_CODE 255

#define max_general_counter_mask 8

typedef struct umask * umask_ptr;
typedef struct umask const* umask_cptr;
typedef struct umask{
	char* umask;
	umask_ptr next;
	uint umask_value;
	} umask_data;

typedef struct event_struc * event_struc_ptr;
typedef struct event_struc const* event_struc_cptr;
typedef struct event_struc{
	event_struc_ptr 	next;
	char* 			event_name;
	char* 			full_event_name;
	umask_ptr 		umask_list;
	char* 			LBR_string;
	uint64_t 		secondary_msr_value;
	uint64_t 		counter_value;
	uint 			event_code;
	uint 			umask_code;
	uint 			secondary_msr;
	uint 			counter_mask;
	uint 			offcore_resp_mask;
	uint 			period;
	uint 			cmask;
	uint 			inv;
	uint 			user;
	uint 			kernel;
	uint 			precise;
	uint 			loop_count;
	uint 			data_latency;
	uint 			other;
	} event_data;

typedef struct pmu_event_list * pmu_event_list_ptr;
typedef struct pmu_event_list{
	event_struc_ptr 	this_event;
	pmu_event_list_ptr 	next;
	} pmu_event_list_data;

typedef struct perf_args * perf_args_ptr;
typedef struct perf_args const* perf_args_cptr;
typedef struct perf_args{
	FILE*			outfile;
	event_struc_ptr 	first_event;
	event_struc_ptr 	last_event;
	char*			core_array;
	char*			user_app;
	char*			user_app_string;
	int			run_time;
	int			full_iterations;
	int			multiplex_time;
	int			add_fixed_counters;
	int			fixed_cntr_user;
	int			fixed_cntr_kernel;
	int			mode;
	char			field_seperator;
	int			num_event_groups;
	int			num_general_events;
	int			num_events;
	int			core_count;
	int			verbose;
	int			details;
	int			summary;
	} perf_args_data;

typedef struct input_args * input_args_ptr;
typedef struct input_args{
	int 			argc1;
	char **			argv1;
	}input_args_data;
	
typedef struct umask_table_struc * umask_table_struc_ptr;
typedef struct umask_table_struc const* umask_table_struc_cptr;
typedef struct umask_table_struc{
	umask_table_struc_ptr 	next;
	char* 			umask_name;
	char* 			description;
	uint64_t 		msr_value;
	uint64_t 		pes_upper;
	uint 			umask;
	uint 			counter_mask_ht_on;
	uint 			counter_mask_ht_off;
	uint 			overflow;
	uint 			msr_index;
	uint 			precise;
	uint 			data_lat;
	uint 			offcore_field;
	uint 			fixed;
	} umask_table_data;

typedef struct event_table_struc * event_table_struc_ptr;
typedef struct event_table_struc const* event_table_struc_cptr;
typedef struct event_table_struc{
	event_table_struc_ptr 	next;
	char* 			event_name;
	uint 			event_code;
	umask_table_struc_ptr 	umask_list;
	umask_table_struc_ptr	old_umask;
	} event_table_data;

typedef struct arg_event_struc * arg_event_struc_ptr;
typedef struct arg_event_struc{
	pmu_event_list_ptr 	first_core_event;
	pmu_event_list_ptr 	first_cbox_event;
	pmu_event_list_ptr 	first_imc_event;
	pmu_event_list_ptr 	first_qpi_event;
	pmu_event_list_ptr 	first_pcie_event;
	int 			core_events;
	int 			cbox_events;
	int 			imc_events;
	int 			qpi_events;
	int 			pcie_events;
	} arg_event_data;

typedef struct arch_event_tables_struc * arch_event_tables_struc_ptr;
typedef struct arch_event_tables_struc{
	event_table_struc_ptr 	first_core_event;
	event_table_struc_ptr 	first_cbox_event;
	event_table_struc_ptr 	first_imc_event;
	event_table_struc_ptr 	first_qpi_event;
	event_table_struc_ptr 	first_pcie_event;
	} arch_event_tables_data;

event_table_struc_ptr event_pointers[MAX_EVENT_CODE];

typedef struct counter_program_struc *counter_program_struc_ptr;
typedef struct counter_program_struc const* counter_program_struc_cptr;
typedef struct counter_program_struc{
	counter_program_struc_ptr	next;
	event_table_struc_ptr 		this_table_event;
	event_struc_ptr 		this_input_event;
	perf_event_select_data 		perf_event_select;
	char*				full_event_name;
	uint64_t 			msr_value;
	uint64_t 			value;
	uint64_t 			live_time;
	uint	 			period;
	uint				perf_event_select_msr;
	uint				counter_msr;
	uint				msr_index;
	uint				offcore_field;
	int				multiplex_group;
	int				allowed_counters;
	int				fixed;
	int				user;
	int				kernel;
	} counter_program_data;

typedef struct event_group_struc *event_group_struc_ptr;
typedef struct event_group_struc{
	global_ctrl		group_global_ctrl;
	fixed_ctr_ctrl		group_fixed_ctr_ctrl;
	uint64_t		fixed_counter_msr_index[FIXED_COUNTERS];
	perf_event_select_data 	perf_event_select[GENERAL_COUNTERS];
	uint64_t		general_PES_msr_index[GENERAL_COUNTERS];
	uint64_t		general_counter_msr_index[GENERAL_COUNTERS];
	uint64_t		extra_msr_index[GENERAL_COUNTERS];
	uint64_t		extra_msr_value[GENERAL_COUNTERS];
	uint64_t		event_total[GENERAL_COUNTERS+FIXED_COUNTERS];
	uint64_t		*event_count_sum;
	uint64_t		total_run_time;
	char 			*full_event_name[GENERAL_COUNTERS + FIXED_COUNTERS];
	int			num_general_events;
	int			num_fixed_events;
	} event_group_data;	

typedef struct event_table_hash_list* event_table_hash_list_ptr;
typedef struct event_table_hash_list{
	event_table_struc_ptr 		this_event_ptr;
	event_table_hash_list_ptr 	next;
	} event_table_hash_data;

typedef struct name_hash_table* name_hash_table_ptr;
typedef struct name_hash_table{
	event_table_hash_data * 	event_name_hash_table;
	int 				hash_table_size;
	}hash_table_data;

typedef struct topology_struc * topology_struc_ptr;
typedef struct topology_struc const* topology_struc_cptr;
typedef struct topology_struc{
	char*				core_map;
	int*				processors_in_group;
	int*				smt_id;
	int*				phys_core_id;
	int*				socket_id;
	int				num_logical_cores;
	int				num_groups;
	int				num_physical_cores;
	int				num_sockets;
	int				family;
	int				model;
	int				cpuid_logical_cores;
	} topology_data;

