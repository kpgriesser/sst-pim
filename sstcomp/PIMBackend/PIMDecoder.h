//
// Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
// See LICENSE in the top level directory for licensing details
//

#ifndef _H_SST_PIM_DECODER_
#define _H_SST_PIM_DECODER_

#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "pimdef.h"

namespace SST::PIM {
enum class PIM_ACCESS_TYPE : unsigned {
  NONE = 0x0,
  SRAM = 0x2,
  DRAM = 0x3,
  FUNC = 0x10,
};

const std::map<PIM_ACCESS_TYPE, std::string> PIMTYPE2string{
  {PIM_ACCESS_TYPE::NONE, "NONE"},
  {PIM_ACCESS_TYPE::SRAM, "SRAM"},
  {PIM_ACCESS_TYPE::DRAM, "DRAM"},
  {PIM_ACCESS_TYPE::FUNC, "FUNC"},
};

enum class PIM_FUNCTION_CONTROL { EVENTS = 0, OPERANDS = 1, EXEC = 2, LOCK = 3 };

//TODO SETUP, LAUNCH, STATUS, RETRIEVE?
const std::map<PIM_FUNCTION_CONTROL, std::string> PIM_OFFSET2string{
  {  PIM_FUNCTION_CONTROL::EVENTS,   "EVENTS"},
  {PIM_FUNCTION_CONTROL::OPERANDS, "OPERANDS"},
  {    PIM_FUNCTION_CONTROL::EXEC,     "EXEC"},
  {    PIM_FUNCTION_CONTROL::LOCK,     "LOCK"},
};

class PIMMemSegment {
public:
  PIMMemSegment( PIM_ACCESS_TYPE pimAccType, uint64_t baseAddr, uint64_t size ) : pimAccessType( pimAccType ), BaseAddr( baseAddr ), Size( size ) {
    TopAddr = baseAddr + size;
  } 

  uint64_t getTopAddr() const { return BaseAddr + Size; }

  uint64_t getBaseAddr() const { return BaseAddr; }

  uint64_t getSize() const { return Size; }

  void setSize( uint64_t size ) {
    Size    = size;
    TopAddr = BaseAddr + size;
  }

  /// PIMMemSegment: Check if vAddr is included in this segment
  PIM_ACCESS_TYPE decode( const uint64_t& vAddr ) {
    if( vAddr >= BaseAddr && vAddr < TopAddr )
      return this->pimAccessType;
    return PIM_ACCESS_TYPE::NONE;
  };

  PIM_ACCESS_TYPE getType() { return this->pimAccessType; }

  // Override for easy std::cout << *Seg << std::endl;
  friend std::ostream& operator<<( std::ostream& os, const PIMMemSegment& Seg ) {
    return os << " | PIMTYPE: " << PIMTYPE2string.at( Seg.pimAccessType ) << " | BaseAddr:  0x" << std::hex << Seg.getBaseAddr()
              << " | TopAddr: 0x" << std::hex << Seg.getTopAddr() << " | Size: " << std::dec << Seg.getSize() << " Bytes";
  }

private:
  PIM_ACCESS_TYPE pimAccessType;
  uint64_t BaseAddr;
  uint64_t Size;
  uint64_t TopAddr;
};  // class PIMMemSegment


struct PIMDecodeInfo {
  bool     isIO   = false;
  bool     isDRAM = false;
  PIM_ACCESS_TYPE   pimAccType = PIM_ACCESS_TYPE::NONE;
};

class PIMDecoder {
public:
  // Important: must match RevMem::IOMemoryHolePunch() must match
  const uint64_t SEG_SIZE = DRAM_BASE * sizeof( uint64_t );
  PIMDecoder( uint64_t node = 0 );
  PIMDecodeInfo decode( const uint64_t& addr );

private:
  std::vector<std::shared_ptr<PIMMemSegment>> PIMSegs;
  uint64_t                                    node;
  uint64_t                                    nodeOffset;
};  // class PIMDecoder

}  // namespace SST::PIM

#endif  // _H_SST_PIM_DECODER_
