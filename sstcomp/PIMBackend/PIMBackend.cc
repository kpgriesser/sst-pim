// Copyright 2009-2023 NTESS. Under the terms
// of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2009-2023, NTESS
// All rights reserved.
//
// Portions are copyright of other developers:
// See the file CONTRIBUTORS.TXT in the top level directory
// of the distribution for more information.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.

// clang-format off
#include <sst/core/sst_config.h>
#include <sst/core/link.h>
#include "PIMBackend.h"
#include "tclpim.h"
#include "sst/elements/memHierarchy/util.h"
#include "kgdbg.h"
// clang-format on

using namespace SST;
using namespace SST::MemHierarchy;

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

namespace SST::PIM {

/*------------------------------- Simple Backend ------------------------------- */
PIMBackend::PIMBackend( ComponentId_t id, Params& params ) : SimpleMemBackend( id, params ) {

  kgdbg::spinner( "PIMBACKEND_SPINNER" );

  // Get parameters
  fixupParams( params, "clock", "backend.clock" );

  UnitAlgebra delay = params.find<UnitAlgebra>( "request_delay", UnitAlgebra( "0ns" ) );

  if( !( delay.hasUnits( "s" ) ) ) {
    output->fatal(
      CALL_INFO,
      -1,
      "Invalid param(%s): request_delay - must have units of 's' "
      "(seconds). You specified %s.\n",
      getName().c_str(),
      delay.toString().c_str()
    );
  }

  // PIM output
  const int Verbosity = params.find<int>( "verbose", 0 );
  pimOutput.init( "PIMBackend[" + getName() + ":@p:@t]: ", Verbosity, 0, SST::Output::STDOUT );

  // Create our backend
  // TODO check
  backend = loadUserSubComponent<SimpleMemBackend>( "backend" );
  if( !backend ) {
    std::string backendName   = params.find<std::string>( "backend", "memHierarchy.simpleDRAM" );
    Params      backendParams = params.get_scoped_params( "backend" );
    backendParams.insert( "mem_size", params.find<std::string>( "mem_size" ) );
    backend = loadAnonymousSubComponent<SimpleMemBackend>(
      backendName, "backend", 0, ComponentInfo::SHARE_PORTS | ComponentInfo::INSERT_STATS, backendParams
    );
  }
  backend->setResponseHandler( std::bind( &PIMBackend::handleMemResponse, this, _1 ) );

  m_memSize = backend->getMemSize();  // inherit from backend

  // Set up self links
  if( delay.getValue() != 0 ) {
    delay_self_link = configureSelfLink(
      "DelaySelfLink", delay.toString(), new Event::Handler<PIMBackend>( this, &PIMBackend::handleNextRequest )
    );
  } else {
    delay_self_link = NULL;
  }

  // Create the PIM
  pim_type            = params.find<uint32_t>( "pim_type", PIM_TYPE_TEST );
  num_nodes = params.find<unsigned>( "num_nodes", 0 );
  assert( num_nodes > 0 );
  node_id = params.find<unsigned>( "node_id", 0 );

  if( pim_type == PIM_TYPE_TEST ) {
    pimsim = new TestPIM( node_id, &pimOutput );
    pimOutput.verbose( CALL_INFO, 1, 0, "pim_type=%" PRIu32 " Node=%" PRIu32 " Using Test Mode\n", PIM_TYPE_TEST, node_id );
  } else if( pim_type == PIM_TYPE_TCL ) {
    pimsim = new TCLPIM( node_id, &pimOutput );
    pimOutput.verbose( CALL_INFO, 1, 0, "pim_type=%" PRIu32 " Node=%" PRIu32 " Using TCL PIM\n", PIM_TYPE_TCL, node_id );
  } else if( pim_type == PIM_TYPE_RESERVE ) {
    pimOutput.fatal( CALL_INFO, -1, "PIM_TYPE_RESERVED not supported\n" );
  } else {
    pimOutput.verbose( CALL_INFO, 1, 0, "Warning: no PIM specified (pim_type=0)\n" );
    pimsim = nullptr;
  }

  // simulator callback to access DRAM
  if( pimsim )
    pimsim->setCallback( std::bind( &PIMBackend::issueDRAMRequest, this, _1, _2, _3, _4 ) );

  // TODO multiple controllers per memory
  uint64_t Loff = 0;
  spdBase       = SRAM_BASE + Loff;
}

PIMBackend::~PIMBackend() {
  if( pimsim )
    delete pimsim;
}

void PIMBackend::handleNextRequest( SST::Event* event ) {
  Req& req = requestBuffer.front();
  if( !backend->issueRequest( req.id, req.addr, req.isWrite, req.numBytes ) ) {
    delay_self_link->send( 1, NULL );
  } else
    requestBuffer.pop();
}

bool PIMBackend::issueRequest( ReqId req, Addr addr, bool isWrite, unsigned numBytes ) {
  // Normal DRAM path. Keep the DelayBuffer functionality.
  if( delay_self_link != NULL ) {
    requestBuffer.push( Req( req, addr, isWrite, numBytes ) );
    delay_self_link->send( 1, NULL );  // Just need a wakeup
    return true;
  } else {
    return backend->issueRequest( req, addr, isWrite, numBytes );
  }
}

/*
 * Call throughs to our backend
 */

bool PIMBackend::clock( Cycle_t cycle ) {

  // Clock our PIM and Backend
  // TODO check if clocking on before clocking
  bool unclockPIM = false;
  if( pimsim )
    unclockPIM = pimsim->clock( cycle );
  bool unclockBackend = backend->clock( cycle );

  if( pimsim && !initDRAMDone ) {
    initDRAMDone                 = true;
    this->output->verbose(CALL_INFO, 3, 0, "Running initial PIM memory test\n");
    // Write test data to SRAM base + 64
    MemEventBase::dataVec wrData = { 0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe };
    MemEvent*             evw    = new MemEvent( getName(), spdBase + 64, spdBase + 64, PIM_WRITE, wrData );
    evw->setFlags( MemEvent::F_NONCACHEABLE | MemEvent::F_NORESPONSE );
    m_pimRequest( evw );

    //Read it back. The first read response will be check to clear self-test done.
    MemEventBase::dataVec rdData = { 0 };
    rdData.resize( 8 );
    MemEvent* evr = new MemEvent( getName(), spdBase + 64, spdBase + 64, PIM_READ, rdData );
    evr->setFlags( MemEvent::F_NONCACHEABLE );
    m_pimRequest( evr );
  }

  return unclockPIM && unclockBackend;
}

void PIMBackend::handleMemReponse( ReqId id ) {
  SimpleMemBackend::handleMemResponse( id );
}

void PIMBackend::setComponentName( const std::string& name ) {
  componentName = name;
}

void PIMBackend::issueDRAMRequest(
  uint64_t a, MemEventBase::dataVec* vec, bool isWrite, std::function<void( const MemEventBase::dataVec& )> completion
) {

  kgdbg::spinner( "PIMREQ_SPINNER" );

  MemEvent* ev;
  Command   cmd = isWrite ? PIM_WRITE : PIM_READ;
  ev            = new MemEvent(
    getComponentName(),
    a,
    a,  // base address matches address for noncacheable accesses
    cmd,
    *vec
  );

  // TODO remote node decode
  assert(num_nodes==1);
  ev->setDst( "memory" + std::to_string( ( a >> 27 ) % num_nodes ) );
  ev->setFlags( MemEvent::F_NONCACHEABLE );
  m_pimRequest( ev );
  pendingPIMEvents.insert( std::make_pair( ev->getID(), completion ) );
  pimOutput.verbose( CALL_INFO, 3, 0, "%s\n", ev->toString().c_str() );
}

void PIMBackend::handlePIMCompletion( SST::Event* resp ) {
  MemEvent* mev = static_cast<MemEvent*>( resp );

  // TODO use self-check completion function
  if( !selfCheckDone ) {
    uint64_t payload = 0;
    // PIM should only need response for DRAM Reads (GETS) consumed by PIM.
    if( mev->getCmd() == Command::GetSResp ) {
      size_t                sz  = mev->getPayloadSize();
      MemEventBase::dataVec vec = mev->getPayload();
      uint8_t*              p   = (uint8_t*) &payload;
      for( size_t i = 0; i < sz; i++ ) {
        p[i] = vec[i];
      }
    }
    selfCheckDone = true;
    if( payload != 0xfedcba9876543210 )
      pimOutput.fatal( CALL_INFO, -1, "Failed self-check\n" );
    delete resp;  // Event completed.
    return;
  }
  // Find corresponding pending event
  auto it = pendingPIMEvents.find( mev->getID() );
  if( it == pendingPIMEvents.end() ) {
    pimOutput.fatal( CALL_INFO, -1, "Could not match ID for PIM returning PIM event [ %s ]\n", mev->toString().c_str() );
  }
  // Invoke its completion function
  auto payload = mev->getPayload();
  it->second( mev->getPayload() );

  pimOutput.verbose( CALL_INFO, 3, 0, "%s\n", mev->toString().c_str() );
  
  // Erase the entry
  pendingPIMEvents.erase( it );
  delete resp;  // Event completed.
}

void PIMBackend::handleMMIOReadCompletion( SST::Event* ev ) {
  assert( pimsim );
  MemEvent* mev = static_cast<MemEvent*>( ev );
  // place PIM data in event payload
  buffer.resize( mev->getSize() );
  pimsim->read( mev->getAddr(), mev->getSize(), buffer );
  mev->setPayload( buffer );
  pimOutput.verbose(CALL_INFO,3,0,"MMIO read a=0x%" PRIx64 "d[0]=%" PRId32 "\n", mev->getAddr(), (int)buffer[0]);
}

void PIMBackend::handleMMIOWriteCompletion( SST::Event* ev ) {
  MemEvent* mev = static_cast<MemEvent*>( ev );
  // write event payload to PIM
  buffer        = mev->getPayload();
  // TODO: Fix elf / linker / loader to not write initial values to MMIO ranges to avoid side effects
  if (mev->getSize() !=8 ) {
    pimOutput.verbose(CALL_INFO, 3, 0, "Warning: Dropping MMIO write to function handler with numBytes=%" PRIx32 "\n", mev->getSize() );
    return;
  }
  pimsim->write( mev->getAddr(), mev->getSize(), &buffer );
  pimOutput.verbose(CALL_INFO,3,0,"MMIO write a=0x%" PRIx64 " d[0]=%" PRId32 "\n", mev->getAddr(), (int)buffer[0]);
}

void PIMBackend::setup() {
  backend->setup();
}

void PIMBackend::finish() {
  backend->finish();
}

}  //namespace SST::PIM
