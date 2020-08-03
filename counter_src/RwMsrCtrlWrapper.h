#include <windows.h>

enum ControllerMode { RwOnly, PrintOnly, RwAndPrint};

#ifdef __cplusplus

#include "RwMsrController.h"

using namespace autopilot::hardware;

class RwMsrControllerWrapper
{
public:
    RwMsrControllerWrapper();
    ~RwMsrControllerWrapper();
    bool OpenDriver();
    void SetMode(enum ControllerMode mode) { m_mode = mode; };
    enum ControllerMode GetMode() const { return m_mode; };

    LONG WriteMsrsOnLogicalProcsVector(ULONG logicalProcCount,
        const CHAR *logicalProcBitVector,
        ULONG msrCount,
        const USHORT *msrAddr,
        const ULONGLONG *msrValue);

    LONG ReadMsrsFromLogicalProcsVector(ULONG logicalProcCount,
                                        const CHAR *logicalProcBitVector,
                                        ULONG msrCount,
                                        const USHORT *msrAddr,
                                        _Out_bytecap_(retSize) ULONGLONG *msrValue, ULONG retSize);

private:
    enum ControllerMode m_mode;
    RwMsrController m_rwmsrController;
};
 
#endif
 
#ifdef __cplusplus
extern "C" {
#endif

void* RwMsrAllocControllerObj();
void  RwMsrDeleteControllerObj(void *obj);
bool  RwMsrOpenDriver(void *obj);

ULONG GetMaxSupportedLpCount();
ULONG GetMaxSupportedMsrCount();

void RwMsrSetControllerMode(const void *obj, enum ControllerMode mode);

LONG
WriteMsrsOnLogicalProcsVector(const void *obj,
    ULONG logicalProcCount,
    const CHAR *logicalProcBitVector,
    ULONG msrCount,
    const USHORT *msrAddr,
    const ULONGLONG *msrValue);

LONG
ReadMsrsFromLogicalProcsVector(const void *obj, 
    ULONG logicalProcCount,
    const CHAR *logicalProcBitVector,
    ULONG msrCount,
    const USHORT *msrAddr,
    _Out_bytecap_(retSize) ULONGLONG *msrValue, ULONG retSize);

#ifdef __cplusplus
}
#endif