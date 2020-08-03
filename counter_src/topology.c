//	Event file table reader for win_pmu
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
#include <windows.h>
#include <winnt.h>

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <intrin.h>

typedef unsigned __int32 uint;
typedef unsigned __int64 uint64_t;
typedef int bool;

#include "example_pmu.h"
#include "event_util.h"

extern void err(int i, const char * s, ...);
extern int pin_affinity(int cpu);

topology_struc_ptr
get_topology()
{

	int i, j, k, num_groups=0, num_logical_cores=0;
	int* processors_in_group, registers[4], infocode;
	char  *core_map;
	int *smt_id, *phys_core_id, *socket_id;
	int ht_width, ht_mask, width, mask;
	int ECX_value;
	topology_struc_ptr this_topology;
	int ret_code;

	int model_mask = 0xF0;
	int family_mask = 0xF00;
	int ex_model_mask = 0xF0000;
	int ex_family_mask = 0xF00000;
	int logical_core_count_mask = 0xFF0000;

	num_groups = GetActiveProcessorGroupCount();

	processors_in_group = (int*)calloc(1, sizeof(int) * num_groups );
	if(processors_in_group == NULL)
		{
		err(1,"failed to allocate space for processors in group array");
		exit(1);
		}

	for( i=0; i<num_groups; i++ )
		{
		processors_in_group[i] = GetActiveProcessorCount((WORD)i);
		num_logical_cores += processors_in_group[i];
		}
#ifdef DBUG_TOPO
	fprintf(stderr," number of processor groups = %d, processor group logical cores = %d\n",
		 num_groups,num_logical_cores);
#endif
	core_map = (char*)calloc(1, sizeof(char) * num_logical_cores );
	if(core_map == NULL)
		{
		err(1,"failed to allocate space for core_map array");
		exit(1);
		}
	smt_id = (int*)calloc(1, sizeof(int) * num_logical_cores );
	if(smt_id == NULL)
		{
		err(1,"failed to allocate space for smt_id array");
		exit(1);
		}
	phys_core_id = (int*)calloc(1, sizeof(int) * num_logical_cores );
	if(phys_core_id == NULL)
		{
		err(1,"failed to allocate space for phys_core_id array");
		exit(1);
		}
	socket_id = (int*)calloc(1, sizeof(int) * num_logical_cores );
	if(socket_id == NULL)
		{
		err(1,"failed to allocate space for socket_id array");
		exit(1);
		}

	this_topology = (topology_struc_ptr) calloc(1, sizeof(topology_data));
	if(this_topology == NULL)
		{
		err(1,"failed to allocate space for topology struc");
		exit(1);
		}
	this_topology->core_map = core_map;
	this_topology->processors_in_group = processors_in_group;
	this_topology->num_logical_cores = num_logical_cores;
	this_topology->num_groups = num_groups;

//	get family and model with __cpuid()

	for(j=0; j<4; j++)
		{
		infocode = j;
		__cpuid(registers, infocode);
#ifdef DBUG_TOPO
		fprintf(stderr," infocode = %d,", infocode);
		for(i=0; i<4; i++)fprintf(stderr," register %d = 0x%x ", i,registers[i]);
		fprintf(stderr,"\n");
#endif
		}
	for(j=0; j<4; j++)
		{
		infocode = 4;
		ECX_value = j;
		__cpuidex(registers, infocode, ECX_value);
#ifdef DBUG_TOPO
		fprintf(stderr," infocode = %d, ECX = %d", infocode, ECX_value);
		for(i=0; i<4; i++)fprintf(stderr," register_ex = 0x%x ", registers[i]);
		fprintf(stderr,"\n");
#endif
		}

	infocode = 1;
	__cpuid(registers, infocode);
#ifdef DBUG_TOPO
	for(i=0; i<4; i++)fprintf(stderr," register %d = 0x%x ", i,registers[i]);
	fprintf(stderr,"\n");
#endif
	this_topology->model = (registers[0] & model_mask) >> 4;
	this_topology->family = (registers[0] & family_mask) >> 8;
//	fprintf(stderr," family = %d, model = %d\n", this_topology->family, this_topology->model);
	this_topology->model += (registers[0] & ex_model_mask) >> 12;
	this_topology->family += (registers[0] & ex_family_mask) >> 16;
	this_topology->cpuid_logical_cores = (registers[1] & logical_core_count_mask) >> 16;
	this_topology->smt_id = smt_id;
	this_topology->phys_core_id = phys_core_id;
	this_topology->socket_id = socket_id;

	for(k=0; k<num_logical_cores; k++)
		{
		ret_code = pin_affinity(k);
		if(ret_code != 0)
			err(1,"failed to pin affinity to core %d",k);
		infocode = 0xb;
//		fprintf(stderr," core = %d, eax= infocode = %d\n",k, infocode);
//	smt topology
		ECX_value = 0;
		__cpuidex(registers, infocode, ECX_value);
		ht_width = registers[0]&0x1f;
		ht_mask = ~((-1)<<ht_width);
		smt_id[k] = registers[3] & ht_mask;
//		fprintf(stderr," infocode = %d, ECX = %d", infocode, ECX_value);
//		for(i=0; i<4; i++)fprintf(stderr," register_ex = 0x%x ", registers[i]);
//		fprintf(stderr,"\n");
//	physical core and socket topology
		ECX_value = 1;
		__cpuidex(registers, infocode, ECX_value);
		width = registers[0]&0x1f;
		mask = (~((-1)<<width)^ht_mask)^ht_mask;
		phys_core_id[k] = (registers[3] & mask)>>ht_width;
		mask = (-1)<<width;
		socket_id[k] = (registers[3] & mask)>>width;
//		fprintf(stderr," infocode = %d, ECX = %d", infocode, ECX_value);
//		for(i=0; i<4; i++)fprintf(stderr," register_ex = 0x%x ", registers[i]);
//		fprintf(stderr,"\n");
		}

	return this_topology;
}
int
print_topology(topology_struc_cptr this_topology)
{
	int k, good=1;
	char tab = '\t';

	if(this_topology == NULL)
		{
		err(1,"print_topology called with null pointer");
		exit(1);
		}
	fprintf(stderr," Print topology structure\n");
	fprintf(stderr," family = %d, model = %d, cpuid_logical_cores = %d\n", 
		this_topology->family, this_topology->model, this_topology->cpuid_logical_cores);
	fprintf(stderr," number of processor groups = %d, processor group logical cores = %d\n", 
		this_topology->num_groups, this_topology->num_logical_cores);
	fprintf(stderr,"cpu %c smt  %c phys_core %c socket %c core_map \n",tab,tab,tab,tab);
	for(k = 0; k<this_topology->num_logical_cores; k++)
		{
		fprintf(stderr," %d%c  %d%c   %d%c            %d%c            %c\n",k,tab,this_topology->smt_id[k],tab,
			this_topology->phys_core_id[k],tab, this_topology->socket_id[k],tab, this_topology->core_map[k]);
		}
	return good;
}
