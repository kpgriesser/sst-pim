//
// Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
// See LICENSE in the top level directory for licensing details
//

#ifndef _SST_PIMBACKEND_TCL_PIM_FUNCTIONS_
#define _SST_PIMBACKEND_TCL_PIM_FUNCTIONS_

#include "tclpim.h"

namespace SST::PIM {

class MemCopy : public FSM {
public:
  MemCopy( TCLPIM* p );
  virtual ~MemCopy() {};
  void start( uint64_t params[NUM_FUNC_PARAMS] ) override;
  bool clock() override;
private:
  enum DMA_STATE { IDLE, READ, WRITE, WAITING, DONE };
  DMA_STATE dma_state    = DMA_STATE::IDLE;
  uint64_t  total_words  = 0;
  uint64_t  word_counter = 0;
  uint64_t  src          = 0;
  uint64_t  dst          = 0;
  bool src_is_sram          = false;
  bool dst_is_sram          = false;
  void sequence_dram_read(DMA_STATE nextState);
  void sequence_dram_write(DMA_STATE nextState);
};  //class MemCopy


} // namespace SST::PIM


#endif //_SST_PIMBACKEND_TCL_PIM_FUNCTIONS_