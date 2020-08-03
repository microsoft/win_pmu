//	logical core affinity pinning and processor group interaction code for win_pmu
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

#define INCL_BASE

# include <stdio.h>
# include <stdlib.h>
# include <windows.h>

int NumGroups=0, aggragateCount[16], totalCpuCount=-1;

int 
getCpuGroups()
{
    int i,  ActiveGroups, TotalProcessors=0;
    int* ProcessorsInGroup;

    ActiveGroups = GetActiveProcessorGroupCount();
    if(ActiveGroups < 1)
	{
	fprintf(stderr," getCPUGroups: exit   Active_groups less than 1\n");
	exit(1);
	}
    NumGroups = ActiveGroups;

    ProcessorsInGroup = (int*)malloc( sizeof(int) * ActiveGroups );
    if(ProcessorsInGroup == NULL)
	{
	fprintf(stderr,"failed to malloc buffer in getCPUGroups\n");
	exit(1);
	}

    for( i=0; i<ActiveGroups; i++ )
	{
        ProcessorsInGroup[i] = GetActiveProcessorCount((WORD)i);
        aggragateCount[i] = ProcessorsInGroup[i];
	if(i > 0)aggragateCount[i] += aggragateCount[i-1];
        TotalProcessors += ProcessorsInGroup[i];
        totalCpuCount = TotalProcessors;
//        printf("Group %d has %d processors\n", i, ProcessorsInGroup[i]);
	}
#ifdef DBUG_AFFINITY
    printf("%d groups, %d processors total\n", ActiveGroups, TotalProcessors);
#endif
    free(ProcessorsInGroup);
    return NumGroups;
}


int
pin_affinity(int cpu)
{
    int retval, i,mygroup=0, cpu_in_group;
    UINT64 one=1UL, affinityMask;
    GROUP_AFFINITY GroupAffinityNew,GroupAffinityOld;


    // if all 3 aren't zero, the API fails w/ error 87
    GroupAffinityNew.Reserved[0] = 0;
    GroupAffinityNew.Reserved[1] = 0;
    GroupAffinityNew.Reserved[2] = 0;

    cpu_in_group = cpu - 1;
    if(NumGroups == 0)NumGroups = getCpuGroups();
    if(NumGroups == 0)
	{
	fprintf(stderr," pin affinity fails NumGroups = 0\n");
	return -1;
	}
    cpu++;
    if(NumGroups == 1)
	{
	affinityMask = one << (cpu);
	mygroup = 0;
	cpu_in_group = cpu - 1;
	}
    else
	{
	for(i=0; i<NumGroups; i++)
		{
		if(cpu <= aggragateCount[i])
			{
			mygroup = i;
			if (i > 0)
				cpu_in_group = cpu - aggragateCount[i-1] - 1;
			else
				cpu_in_group = cpu - 1;
			break;
			}
		}
	}
    affinityMask = one << (cpu_in_group);
    GroupAffinityNew.Group = (WORD)mygroup;
    GroupAffinityNew.Mask = affinityMask;

    retval = SetThreadGroupAffinity(GetCurrentThread(), &GroupAffinityNew, &GroupAffinityOld);
    if( !retval)
	{
	fprintf(stderr," affinity pin failed for cpu %d, group = %d, cpu in group = %d, mask = 0x%I64x\n",
		cpu-1, mygroup, cpu_in_group, affinityMask);
	return -1;
	}
    return 0;				
}
