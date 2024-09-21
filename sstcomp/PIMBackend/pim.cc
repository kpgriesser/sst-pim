//
// Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
// See LICENSE in the top level directory for licensing details
//

#include "pim.h"

namespace SST::PIM {

PIMReq_t::PIMReq_t( MemBackend::ReqId _id, Addr _addr, bool _isWrite, unsigned _numBytes, MemEventBase::dataVec _payload )
  : id( _id ), addr( _addr ), isWrite( _isWrite ), numBytes( _numBytes ), payload( _payload ) {};

PIMReq_t::PIMReq_t( MemBackend::ReqId _id, Addr _addr, bool _isWrite, unsigned _numBytes, uint64_t data )
  : id( _id ), addr( _addr ), isWrite( _isWrite ), numBytes( _numBytes ) {
  uint8_t* p = (uint8_t*) ( &data );
  setPayload( p, numBytes );
};

void PIMReq_t::setPayload( uint8_t* data, unsigned bytes ) {
  if( bytes != payload.size() )
    payload.resize( bytes );
  for( unsigned int i = 0; i < bytes; i++ )
    payload.at( i ) = data[i];
};

/* TestPIM member functions */

TestPIM::TestPIM( uint64_t node, SST::Output* o ) : PIM( o ) {
  // simulator defined identifier
  id          = ( uint64_t( PIM_TYPE_TEST ) << 56 ) | ( node << 12 );
  sramArray[0] = id;
  output->verbose( CALL_INFO, 1, 0, "Creating TestPim node=%" PRId64 " id=0x%" PRIx64 "\n", node, id );
  // proto DMA
  dma       = new SimpleDMA( this );
  // mmio decoder
  pimDecoder = new PIMDecoder( node );
}

TestPIM::~TestPIM() {
  if( dma )
    delete dma;
  if( pimDecoder )
    delete pimDecoder;
}

bool TestPIM::clock( SST::Cycle_t cycle ) {
  this->cycle = cycle;
  if( dma->active() ) {
    uint64_t done = dma->clock();
    sramArray[0]   = done;
  }
  return false;  // do not disable clock
}

uint64_t TestPIM::getCycle() {
  return cycle;
}

bool TestPIM::isMMIO( uint64_t addr ) {
  PIMDecodeInfo inf = pimDecoder->decode( addr );
  return inf.isIO;
}

void TestPIM::read( Addr addr, uint64_t numBytes, std::vector<uint8_t>& payload ) {
  // TODO eleminate extra decode
  PIMDecodeInfo info = pimDecoder->decode( addr );
  if( info.pimAccType == PIM_ACCESS_TYPE::SRAM ) {
    unsigned spdIndex = ( addr & 0x38ULL ) >> 3;
    unsigned byte     = ( addr & 0x7 );
    assert(byte==0); // TODO should we allow unaligned accesses?
    assert( ( byte + numBytes ) <= 8 );  // 8 byte aligned only
    uint8_t* p = (uint8_t*) ( &( sramArray[spdIndex] ) );
    for( unsigned i = 0; i < numBytes; i++ ) {
      payload[i] = p[byte + i];
    }
    output->verbose(
      CALL_INFO, 3, 0, "PIM 0x%" PRIx64 " IO READ SRAM A=0x%" PRIx64 " D=0x%" PRIx64 "\n", id, addr, sramArray[spdIndex]
    );
  } else {
    assert( false );  // FUNC is write-only
  }
}

void TestPIM::write( Addr addr, uint64_t numBytes, std::vector<uint8_t>* payload ) {
  // TODO eliminate extra decode
  PIMDecodeInfo info = pimDecoder->decode( addr );
  if( info.pimAccType == PIM_ACCESS_TYPE::SRAM ) {
    // 8 8-byte entries. Byte Addressable (memcpy -O0 does byte copy).
    unsigned offset = ( addr & 0x38ULL ) >> 3;
    unsigned byte   = ( addr & 0x7 );
    assert( ( byte + numBytes ) <= 8 );  // 8 byte aligned only
    uint8_t* p = (uint8_t*) ( &( sramArray[offset] ) );
    for( unsigned i = 0; i < numBytes; i++ ) {
      p[byte + i] = payload->at( i );
    }
    output->verbose(
      CALL_INFO, 3, 0, "PIM 0x%" PRIx64 " IO WRITE SRAM A=0x%" PRIx64 " D=0x%" PRIx64 "\n", id, addr, sramArray[offset]
    );
  } else if( info.pimAccType == PIM_ACCESS_TYPE::FUNC ) {
    // 4 entries, write-only, DWORD addressable
    // These have hardware effects.
    unsigned offset = ( addr & 0x38ULL ) >> 3;
    unsigned byte   = ( addr & 0x7 );
    assert( numBytes == 8 );
    assert( byte == 0 );
    uint8_t* p = (uint8_t*) ( &( ctl_buf ) );
    for( unsigned i = 0; i < numBytes; i++ ) {
      p[byte + i] = payload->at( i );
    }
    output->verbose( CALL_INFO, 3, 0, "PIM 0x%" PRIx64 " IO WRITE FUNC A=0x%" PRIx64 " D=0x%" PRIx64 "\n", id, addr, ctl_buf );
    ctl_write( offset );
  } else {
    assert( false );
  }
}

void TestPIM::ctl_write( unsigned offset ) {
  assert( offset < 4 );
  PIM_FUNCTION_CONTROL ctloff = static_cast<PIM_FUNCTION_CONTROL>( offset );
  //cout << "FUNC[" << PIM_OFFSET2string.at(ctloff) << "] <- 0x" << hex << ctl_buf << endl;
  switch( ctloff ) {
  case PIM_FUNCTION_CONTROL::EVENTS: ctl_event = ctl_buf; break;
  case PIM_FUNCTION_CONTROL::EXEC:
    ctl_exec = ctl_buf;
    // ctl_ops : node, src, dst, word_count
    dma->start( ctl_ops[1], ctl_ops[2], ctl_ops[3] );
    ctl_ops.clear();
    break;
  case PIM_FUNCTION_CONTROL::LOCK: ctl_locked = ctl_buf; break;
  case PIM_FUNCTION_CONTROL::OPERANDS:
    ctl_ops.push_back( ctl_buf );
    break;
    break;
  default: {
    assert( false );
  } break;
  }
}

TestPIM::SimpleDMA::SimpleDMA( TestPIM* p ) : parent( p ) {};

void TestPIM::SimpleDMA::start( uint64_t src, uint64_t dst, uint64_t numWords ) {
  if( numWords == 0 )
    return;
  this->src    = src;
  this->dst    = dst;
  total_words  = numWords;
  word_counter = numWords;
  dma_state    = DMA_STATE::READ;
  parent->output->verbose(
    CALL_INFO, 3, 0, "start dma: src=0x%" PRIx64 " dst=0x%" PRIx64 " total_words=%" PRId64 "\n", src, dst, total_words
  );
}

unsigned TestPIM::SimpleDMA::clock() {
#if 1
  unsigned words = word_counter >= 64 ? 64 : word_counter;
#else
  unsigned words = 1;
#endif
  unsigned bytes = words * sizeof( uint64_t );
  //if (parent->buffer.size() != bytes)
  parent->buffer.resize( bytes );
  const bool WRITE = true;
  const bool READ  = false;
  if( dma_state == DMA_STATE::READ ) {
    parent->m_issueDRAMRequest( src, &parent->buffer, READ, [this]( const MemEventBase::dataVec& d ) 
      {
        assert( parent->buffer.size() == d.size() );
        for( size_t i = 0; i < d.size(); i++ ) {
          parent->buffer[i] = d[i];
        }
        dma_state = DMA_STATE::WRITE;
      } 
    );
    dma_state = DMA_STATE::WAITING;
    src += bytes;
  } else if( dma_state == DMA_STATE::WRITE ) {
    assert( word_counter >= words );
    word_counter = word_counter - words;
    if( word_counter > 0 ) {
      parent->m_issueDRAMRequest( dst, &parent->buffer, WRITE, [this]( const MemEventBase::dataVec& d ) 
        {
          dma_state = DMA_STATE::READ;
        }
      );
    } else {
      parent->m_issueDRAMRequest( dst, &parent->buffer, WRITE, [this]( const MemEventBase::dataVec& d ) 
        {
          dma_state = DMA_STATE::DONE;
        }
      );
    }
    dst += bytes;
    dma_state = DMA_STATE::WAITING;
  } else if( dma_state == DMA_STATE::DONE ) {
    parent->output->verbose( CALL_INFO, 1, 0, "DMA Done\n" );
    dma_state = DMA_STATE::IDLE;
    return 1;  // finished!
  }
  return 0;
}

uint64_t getPayload(std::vector<uint8_t> payload)
{
    uint64_t data = 0;
    uint8_t* p = (uint8_t*) ( &data );
    for( unsigned i = 0; i < sizeof(uint64_t); i++ )
      p[i] = payload[i];
    return data;
}

} // namespace SST::PIM

// EOF
