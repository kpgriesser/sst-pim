//
// Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
// See LICENSE in the top level directory for licensing details
//

// clang-format off
#include <sst/core/sst_config.h>
#include <sst/core/params.h>

#include "PIMMemController.h"
#include "PIMBackend.h"
#include "memEventCustom.h"
// clang-format on

#include "kgdbg.h"

using namespace std;
using namespace SST;
using namespace SST::MemHierarchy;

namespace SST::PIM {

PIMMemController::PIMMemController( ComponentId_t id, Params& params ) : MemControllerKG( id, params ) {
  PIMBackend* backend = static_cast<PIMBackend*>( memory_ );
  backend->setComponentName( getName() );  // used to generate src identifier for requests
  using std::placeholders::_1;
  backend->setEventInjectionHandler( std::bind( &MemControllerKG::handlePIMEvent, this, _1 ) );

  // TODO int node_id = params.find<unsigned>( "node_id", -1 );
  // TODO assert( node_id >= 0 );
  int node_id = 0;
  mmio_decoder = new PIMDecoder( node_id );
}

PIMMemController::~PIMMemController() {
  if( mmio_decoder )
    delete mmio_decoder;
}

void PIMMemController::setup() {
  MemControllerKG::setup();
}

void PIMMemController::handleEvent( SST::Event* event ) {
  MemEvent* ev = static_cast<MemEvent*>( event );
  // If not locally generated PIM DRAM request check for MMIO
  if( !ev->queryFlag( PIMMemEvent::F_PIM ) ) {
    if( mmio_decoder->decode( ev->getAddr() ).isIO ) {
      ev->setFlag( PIMMemEvent::F_MMIO );
      this->out.verbose(CALL_INFO, 3, 0, "Received MMIO event %s\n", ev->toString().c_str());
      // All MMIO must be non-cachable accesses 
      assert(ev->queryFlag ( MemEventBase::F_NONCACHEABLE ));
    }
  }
  MemControllerKG::handleEvent( event );
}

void PIMMemController::handleMemResponse( SST::Event::id_type id, uint32_t flags ) {
  // passthrough since base class modified to handle the new local flags.
  MemControllerKG::handleMemResponse( id, flags );
}

}  //namespace SST::PIM
