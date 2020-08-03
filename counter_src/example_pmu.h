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
//PMU MSRs
#define FIXED_COUNTER_BASE        0x309
#define GENERAL_COUNTER_BASE      0xC1
#define PERF_EVENT_SELECT_BASE    0x186
#define IA32_DEBUGCTRL            0x1D9
#define IA32_FIXED_CTR_CTRL       0x38D
#define IA32_PERF_GLOBAL_STATUS   0x38E
#define IA32_PERF_GLOBAL_CTRL     0x38F
#define IA32_PERF_GLOBAL_OVF_CTRL 0x390

//PMU mask values
#define GLOBAL_CTR_ENABLE         0x70000000F
#define GLOBAL_CTR_DISABLE        0x0
#define FIXED_COUNTER_PROGRAM_ALL 0x333


#define FIXED_COUNTERS 3
#define GENERAL_COUNTERS 4
#define MAX_GENERAL_COUNTER_MASK 0x8
#define NUM_PERF_EVENT_SELECT 5
#define NUM_PMU_CONTROL 6



typedef union tagFixedCtrl{
	struct {
		unsigned f0_kern:1;
		unsigned f0_user:1;
		unsigned f0_anythread:1;
		unsigned f0_pmi:1;
		unsigned f1_kern:1;
		unsigned f1_user:1;
		unsigned f1_anythread:1;
		unsigned f1_pmi:1;
		unsigned f2_kern:1;
		unsigned f2_user:1;
		unsigned f2_anythread:1;
		unsigned f2_pmi:1;
		unsigned reserved:32;
		unsigned reserved2:20;
		}reg;
		uint64_t raw;
	}fixed_ctr_ctrl;
typedef union tagPerf_evt_select{
	struct{
		unsigned ev_code:8;
		unsigned umask:8;
		unsigned user:1;
		unsigned kernel:1;
		unsigned edge:1;
		unsigned pc:1;
		unsigned interupt:1;
		unsigned anythread:1;
		unsigned enable:1;
		unsigned inv:1;
		unsigned cmask:8;
		unsigned reserved:32;
		} reg;
		uint64_t raw;
	}perf_event_select_data;
typedef union tagGlobalCtrl{
	struct {
		unsigned long	general_counter_enable:4;
		unsigned long	reserved:28;
		unsigned long 	fixed_counter_enable:3;
		unsigned long	reserved2:29;
		}reg;
		uint64_t raw;
	}global_ctrl;

