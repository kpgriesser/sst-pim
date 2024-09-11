#ifndef _SST_PIMDEF_H_
#define _SST_PIMDEF_H_

#include <cstdint>

#define PIM_TYPE_NONE    0
#define PIM_TYPE_TEST    1
#define PIM_TYPE_RESERVE 2
#define PIM_TYPE_TCL     3
        
namespace SST::PIM
{    
    // Important: Keep memory map in sync with pim.lds

    // 16 built-in functions
    const uint64_t FUNC_SIZE = 0x00000010llu;
    const uint64_t FUNC_BASE = 0x0e000000llu;

    // 16 user functions
    const uint64_t USRFUNC_SIZE = 0x00000010llu;
    const uint64_t USRFUNC_BASE = 0x0e000100llu;

    // 1k sram
    const uint64_t SRAM_SIZE = 0x00000400llu;
    const uint64_t SRAM_BASE = 0x0f000000llu;

    // 1M DRAM
    const uint64_t DRAM_SIZE = 0x00100000llu;
    const uint64_t DRAM_BASE = 0x0f800000llu;

    // SRAM Access
    enum class SRAM_CMD : int { NOP, READ, WRITE, DONE };
    
    // FUNC Access
    enum class FUNC_CMD : int { INIT, RUN, FINISH, EOD };

    // Enumerated functions
    enum class FUNC    : int { F0, F1, F2, F3, F4, F5, F6, F7};
    enum class USRFUNC : int { F0, F1, F2, F3, F4, F5, F6, F7};

    // Function states
    enum class FSTATE  : int { INVALID, INITIALIZED, RUNNING, DONE };

} //namespace SST::PIM

#endif //_SST_PIMDEF_H_
