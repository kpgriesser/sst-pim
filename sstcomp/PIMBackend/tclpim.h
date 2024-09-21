//
// Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
// See LICENSE in the top level directory for licensing details
//

#ifndef _SST_PIMBACKEND_TCLPIM_
#define _SST_PIMBACKEND_TCLPIM_

#include "pim.h"

namespace SST::PIM {

class FSM;

class TCLPIM : public PIM {
public:
  TCLPIM( uint64_t node, SST::Output* o );
  virtual ~TCLPIM();
  void     setup() override {};
  bool     clock( SST::Cycle_t ) override;
  uint64_t getCycle() override;
  bool     isMMIO( uint64_t addr ) override;
  PIMDecodeInfo  getDecodeInfo( uint64_t addr);
  // IO access functions
  void read( Addr, uint64_t numBytes, std::vector<uint8_t>& ) override;
  void write( Addr, uint64_t numBytes, std::vector<uint8_t>* ) override;

  // Primary functional state machine
  class FuncState {
  public:
    FuncState( TCLPIM* p, FUNC_NUM fn, std::shared_ptr<FSM> fsm);
    void setFSM(std::shared_ptr<FSM> fsm);
    void writeFSM(FUNC_CMD cmd);
    void writeFSM(uint64_t d);
    uint64_t readFSM();
    bool running();
    std::shared_ptr<FSM> exec();
    
  private:
    TCLPIM* parent;
    FUNC_NUM fnum;
    std::shared_ptr<FSM> exec_ = nullptr;
    uint64_t params[NUM_FUNC_PARAMS] = {0};
    FSTATE fstate = FSTATE::INVALID;
    // TODO bool lock = false;
    int counter = 0;

  }; // class FuncState

private:
  uint64_t   id;
  PIMDecoder* pimDecoder;
  uint64_t   cycle = 0;

  // memory mapped IO
  std::vector<std::shared_ptr<PIMMemSegment>> PIMSegs;
  uint64_t             sramArray[SRAM_SIZE/sizeof(uint64_t)] = { 0 };
  std::deque<uint64_t> ctl_ops;
  void                 function_write( uint64_t data );
  uint64_t             decodeFuncNum( uint64_t address, unsigned numBytes );
  std::map< FUNC_NUM, shared_ptr<FuncState>> funcState;

};  //class TCLPIM

class FSM {
public:
  FSM( TCLPIM* p ) : parent(p) {};
  virtual ~FSM() {};
  virtual void start( uint64_t params[NUM_FUNC_PARAMS] ) = 0;
  virtual bool clock() = 0;  // return true when done

protected:
  TCLPIM* parent;
}; //class FSM

} // namespace SST::PIM

#endif //_SST_PIMBACKEND_TCLPIM_