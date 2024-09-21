//
// Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
// See LICENSE in the top level directory for licensing details
//

#include "tclpim_functions.h"

namespace SST::PIM {

MemCopy::MemCopy( TCLPIM* p ) : FSM( p ) {};

// Param 0: Destination Address
// Param 1: Source Address
// Param 2: Number of Bytes to transfer ( must by divisible by 8 ) 

void MemCopy::start( uint64_t params[NUM_FUNC_PARAMS] ) {
  uint64_t numBytes = params[2];
  if( numBytes == 0 )
    return;
  assert((numBytes%8)==0);
  dst    = params[0];
  src    = params[1];
  total_words  = numBytes/8;
  word_counter = numBytes/8;
  dma_state    = DMA_STATE::READ;
  parent->output->verbose(
    CALL_INFO, 3, 0, "start dma: dst=0x%" PRIx64 " src=0x%" PRIx64 " total_words=%" PRId64 "\n", dst, src, total_words
  );

  // TODO check for overlapping ranges
  auto dst_inf = parent->getDecodeInfo(dst);
  auto src_inf = parent->getDecodeInfo(src);
  if ( dst_inf.isIO && (dst_inf.pimAccType != PIM_ACCESS_TYPE::SRAM ) )
    parent->output->fatal(CALL_INFO, -1, "Destination address must by SRAM or DRAM\n");
  if ( src_inf.isIO && (src_inf.pimAccType != PIM_ACCESS_TYPE::SRAM ) )
    parent->output->fatal(CALL_INFO, -1, "Source address must by SRAM or DRAM\n");

  dst_is_sram = dst_inf.isIO && (dst_inf.pimAccType == PIM_ACCESS_TYPE::SRAM );
  src_is_sram = src_inf.isIO && (src_inf.pimAccType == PIM_ACCESS_TYPE::SRAM );
}

bool MemCopy::clock() {
#if 1
  unsigned words = word_counter >= 64 ? 64 : word_counter;
#else
  unsigned words = 1;
#endif
  unsigned bytes = words * sizeof( uint64_t );
  parent->buffer.resize( bytes );
  switch (dma_state) {
    case DMA_STATE::READ:
      if (src_is_sram) {
        parent->read( src, bytes, parent->buffer);
        dma_state = DMA_STATE::WRITE;
      } else {
        sequence_dram_read(DMA_STATE::WRITE);
        dma_state = DMA_STATE::WAITING;
      }
      src += bytes;
      break;
    case DMA_STATE::WRITE:
    {
      assert( word_counter >= words );
      word_counter = word_counter - words;
      DMA_STATE nextState = (word_counter > 0) ? DMA_STATE::READ : DMA_STATE::DONE;
      if (dst_is_sram) {
        parent->write( dst, bytes, &parent->buffer);
        dma_state = nextState;
      } else {
        sequence_dram_write(nextState);
        dma_state = DMA_STATE::WAITING;
      }
      dst += bytes;
      break;
    }
    case DMA_STATE::DONE:
      parent->output->verbose( CALL_INFO, 1, 0, "DMA Done\n" );
      dma_state = DMA_STATE::IDLE;
      return true;  // finished!
    default:
      break;
  }
  return false;
}

void MemCopy::sequence_dram_read(DMA_STATE nextState)
{
  parent->m_issueDRAMRequest(src, &parent->buffer, false, [this, nextState]( const MemEventBase::dataVec& d ) 
    {
      assert( parent->buffer.size() == d.size() );
      for( size_t i = 0; i < d.size(); i++ ) {
        parent->buffer[i] = d[i];
      }
      dma_state = nextState;
    });
}

void MemCopy::sequence_dram_write(DMA_STATE nextState)
{
  parent->m_issueDRAMRequest( dst, &parent->buffer, true, [this, nextState]( const MemEventBase::dataVec& d ) {
    dma_state = nextState;
  } );
}

} // namespace