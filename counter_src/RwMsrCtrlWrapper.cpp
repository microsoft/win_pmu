#include "RwMsrCtrlWrapper.h"

using namespace autopilot::hardware;

//
// Debug print
//
static int getid(int maxY, int x, int y)
{
    return (maxY*x + y);
}

static void PrintRwMsrParams(bool isWrite,
    ULONG logicalProcCount,
    const CHAR *logicalProcBitVector,
    ULONG msrCount,
    const USHORT *msrAddr,
    const ULONGLONG *msrValue,
    const ULONG retSize)
{
    ULONG maxMsrCount = retSize / (logicalProcCount * sizeof(ULONGLONG));

    printf("lpCount: %u, msrCount: %u, retSize: %u\n", logicalProcCount, msrCount, retSize);

    for (ULONG i = 0; i < logicalProcCount; ++i)
        if (logicalProcBitVector[i])
            for (ULONG j = 0; j < msrCount; ++j)
                if (isWrite)
                {
                    printf("Writing 0x%016I64X to MSR 0x%X on LP %u\n", msrValue[j], msrAddr[j], i);
                }
                else
                {
                    ULONGLONG val = msrValue[getid(maxMsrCount, i, j)];
                    printf("Read 0x%016I64X from MSR 0x%X on LP %u\n", val, msrAddr[j], i);
                }
}

RwMsrControllerWrapper::RwMsrControllerWrapper()
{
    m_mode = RwOnly;
}

RwMsrControllerWrapper::~RwMsrControllerWrapper()
{

}

bool RwMsrControllerWrapper::OpenDriver()
{
    return(m_rwmsrController.OpenDriver());
}

LONG RwMsrControllerWrapper::WriteMsrsOnLogicalProcsVector(ULONG logicalProcCount,
    const CHAR *logicalProcBitVector,
    ULONG msrCount,
    const USHORT *msrAddr,
    const ULONGLONG *msrValue) //const
{
    LONG retVal = RWMSR_OK;
    if (GetMode() != PrintOnly)
        retVal = m_rwmsrController.WriteMsrsOnLogicalProcsVector(logicalProcCount,
                                                                 logicalProcBitVector,
                                                                 msrCount,
                                                                 msrAddr,
                                                                 msrValue);
    if (GetMode() != RwOnly)
        PrintRwMsrParams(true, logicalProcCount,
                         logicalProcBitVector,
                         msrCount,
                         msrAddr,
                         msrValue,
                         0);
    return retVal;
}

LONG RwMsrControllerWrapper::ReadMsrsFromLogicalProcsVector(ULONG logicalProcCount,
    const CHAR *logicalProcBitVector,
    ULONG msrCount,
    const USHORT *msrAddr,
    ULONGLONG *msrValue,
    ULONG retSize)
{
    LONG retVal = true;
    if (GetMode() != PrintOnly)
        retVal = m_rwmsrController.ReadMsrsFromLogicalProcsVector(logicalProcCount,
                                                                  logicalProcBitVector,
                                                                  msrCount,
                                                                  msrAddr,
                                                                  msrValue,
                                                                  retSize);
      
    if (GetMode() != RwOnly)
        PrintRwMsrParams(false, logicalProcCount,
                                  logicalProcBitVector,
                                  msrCount,
                                  msrAddr,
                                  msrValue,
                                  retSize);

    return retVal;
}


//
// C wrappers
//
void *RwMsrAllocControllerObj()
{
    RwMsrControllerWrapper *rwmsrCtrlW = new RwMsrControllerWrapper;
    return (void *)rwmsrCtrlW;
}

void RwMsrDeleteControllerObj(void *obj)
{
    if (obj)
        delete obj;
}

bool RwMsrOpenDriver(void *obj)
{
    RwMsrControllerWrapper *rwmsrCtrlW = (RwMsrControllerWrapper *)obj;
    return rwmsrCtrlW->OpenDriver();
}

LONG WriteMsrsOnLogicalProcsVector(const void *obj,
    ULONG logicalProcCount,
    const CHAR *logicalProcBitVector,
    ULONG msrCount,
    const USHORT *msrAddr,
    const ULONGLONG *msrValue)
{
//    const RwMsrControllerWrapper *rwmsrCtrlW = (const RwMsrControllerWrapper *)obj;
    RwMsrControllerWrapper *rwmsrCtrlW = (RwMsrControllerWrapper *)obj;

    LONG retVal = rwmsrCtrlW->WriteMsrsOnLogicalProcsVector(logicalProcCount,
        logicalProcBitVector,
        msrCount,
        msrAddr,
        msrValue);

    return retVal;
}

LONG ReadMsrsFromLogicalProcsVector(const void *obj,
    ULONG logicalProcCount,
    const CHAR *logicalProcBitVector,
    ULONG msrCount,
    const USHORT *msrAddr,
    ULONGLONG *msrValue,
    ULONG retSize)
{
    RwMsrControllerWrapper *rwmsrCtrlW = (RwMsrControllerWrapper *)obj;

    LONG retVal = rwmsrCtrlW->ReadMsrsFromLogicalProcsVector(logicalProcCount,
        logicalProcBitVector,
        msrCount,
        msrAddr,
        msrValue,
        retSize);

    return retVal;
}

#define MAX_LPS 128
#define MAX_MSR 32
ULONG GetMaxSupportedLpCount() 
{ 
    return (ULONG)MAX_LPS;
}

ULONG GetMaxSupportedMsrCount() 
{ 
    return (ULONG)MAX_MSR; 
}

void RwMsrSetControllerMode(const void *obj, enum ControllerMode mode)
{
    RwMsrControllerWrapper *rwmsrCtrlW = (RwMsrControllerWrapper *)obj;
    rwmsrCtrlW->SetMode(mode);
}
