//
// Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
// See LICENSE in the top level directory for licensing details
//

#ifndef _H_SST_MEMH_PIM_
#define _H_SST_MEMH_PIM_

#include <queue>
#include <vector>

#include "PIMBackend.h"
#include "PIMDecoder.h"
#include "pimdef.h"
#include "kgdbg.h"

using namespace SST::MemHierarchy;

namespace SST::PIM {

// TODO static uint64_t getPayload( std::vector<uint8_t> payload );

struct PIMReq_t {
  PIMReq_t( MemBackend::ReqId _id, Addr _addr, bool _isWrite, unsigned _numBytes, MemEventBase::dataVec _payload );
  PIMReq_t( MemBackend::ReqId _id, Addr _addr, bool _isWrite, unsigned _numBytes, uint64_t data = 0 );
  MemBackend::ReqId     id;
  Addr                  addr;
  bool                  isWrite;
  unsigned              numBytes;
  MemEventBase::dataVec payload;
  void                  setPayload( uint8_t* data, unsigned bytes );

  friend std::ostream& operator<<( std::ostream& os, const PIMReq_t r ) {
    return os << hex << "id=0x" << r.id << " addr=0x" << r.addr << " isWrite=" << r.isWrite << " numBytes=" << r.numBytes << endl;
  }
};

class PIM {
public:
  PIM( SST::Output* o ) : output( o ) {
    kgdbg::spinner("PIM_SPINNER");
  };

  virtual ~PIM() {}

  virtual void     setup()                                             = 0;
  virtual bool     clock( SST::Cycle_t )                               = 0;
  virtual uint64_t getCycle()                                          = 0;
  // IO access functions
  virtual bool isMMIO( uint64_t addr )                                 = 0;
  virtual void read( Addr, uint64_t numBytes, std::vector<uint8_t>& )  = 0;
  virtual void write( Addr, uint64_t numBytes, std::vector<uint8_t>* ) = 0;
  // DRAM request callback (uint64_t a, MemEventBase::dataVec* d, unsigned bytes, bool isWrite, std::function<void(const uint64_t&)> completion)
  std::function<void( uint64_t, MemEventBase::dataVec*, bool, std::function<void( const MemEventBase::dataVec& )> )>
    m_issueDRAMRequest;

  void setCallback(
    std::function<void( uint64_t, MemEventBase::dataVec*, bool, std::function<void( const MemEventBase::dataVec& )> )> handler
  ) {
    m_issueDRAMRequest = handler;
  }

  //TODO protect and friend FSM
  SST::Output*          output;
  MemEventBase::dataVec buffer;

};  // class PIMifc

class TestPIM : public PIM {
public:
  TestPIM( uint64_t node, SST::Output* o );
  virtual ~TestPIM();
  void     setup() override {};
  bool     clock( SST::Cycle_t ) override;
  uint64_t getCycle() override;
  bool     isMMIO( uint64_t addr ) override;
  // IO access functions
  void read( Addr, uint64_t numBytes, std::vector<uint8_t>& ) override;
  void write( Addr, uint64_t numBytes, std::vector<uint8_t>* ) override;

private:
  uint64_t   id;
  PIMDecoder* pimDecoder;
  uint64_t   cycle = 0;

  // memory mapped IO
  std::vector<std::shared_ptr<PIMMemSegment>> PIMSegs;
  uint64_t                                   sramArray[8] = { 0 };
  uint64_t                                   ctl_buf     = 0;
  uint64_t                                   ctl_event   = 0;
  std::deque<uint64_t>                       ctl_ops;
  bool                                       ctl_exec   = false;
  bool                                       ctl_locked = false;
  void                                       ctl_write( unsigned offset );

  // dma sequencer
  class SimpleDMA {
  public:
    SimpleDMA( TestPIM* p );
    virtual ~SimpleDMA() {};
    void     start( uint64_t src, uint64_t dst, uint64_t numWords );
    unsigned clock();  // return 1 when busy, 0 when idle

    inline bool active() { return ( dma_state != DMA_STATE::IDLE ); };

  private:
    TestPIM* parent;

    enum DMA_STATE { IDLE, READ, WRITE, WAITING, DONE };

    DMA_STATE dma_state    = DMA_STATE::IDLE;
    uint64_t  total_words  = 0;
    uint64_t  word_counter = 0;
    uint64_t  src          = 0;
    uint64_t  dst          = 0;
  };  //class SimpleDMA

  SimpleDMA* dma;

};  //class TestPIM

}  // namespace SST::PIM

#endif  // _H_SST_MEMH_PIM_
