/*
 * checksram.cpp
 *
 * Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
 * All Rights Reserved
 * contact@tactcomplabs.com
 *
 * See LICENSE in the top level directory for licensing details
 */

// Standard includes
#include <cinttypes>
#include <cstdlib>
#include <cstring>

// PIM definitions
#include "revpim.h"

// Select one and only one
// #define DO_LOOP 1
#define DO_PIM 1

// Globals
const int xfr_size = 128;  // dma transfer size in dwords
uint64_t check_data[xfr_size];
uint64_t buffer[xfr_size] = {0};

// PIM Memories (non-cachable)
uint64_t dram_dst[xfr_size] __attribute__((section(".pimdram")));
uint64_t dram_src[xfr_size] __attribute__((section(".pimdram")));
uint64_t sram[PIM::SRAM_SIZE] __attribute__((section(".pimsram")));

size_t checkPIM() {
  size_t time1, time2;
  // sram offset 0 initialized by PIM hardware
  REV_TIME( time1 );
  if (sram[0] != ( uint64_t( PIM_TYPE_TCL ) << 56 )) {
    printf("Unexpected PIM TYPE 0x%lx\n", sram[0]);
    assert(false);
  }
  REV_TIME( time2 );
  return time2 - time1;
}

size_t configure() {
  size_t time1, time2;
  REV_TIME( time1 );
  // Generate source and check data
  for (int i=0; i<xfr_size ;i++) {
    uint64_t d = (0xaced << 16) | i;
    check_data[i] = d;
    dram_src[i] = d;
  }
  REV_TIME( time2 );
  return time2 - time1;
}

#if DO_LOOP
size_t theApp() {
  size_t time1, time2;
  REV_TIME( time1 );
  for (int i=0; i<xfr_size; i++) {
    buffer[i] = dram_src[i];
  }
  for (int i=0; i<xfr_size; i++) {
    dram_dst[i] = buffer[i];
  }
  REV_TIME( time2 );
  return time2 - time1;
}
#endif

#if DO_PIM
size_t theApp() {
  size_t time1, time2;
  REV_TIME( time1 );
  // TODO should be able to queue these functions up in the PIM and synchronize them using semaphore.
  // For now, run 1, poll until it's done, run the next... 

  // PIM DRAM to SRAM
  revpim::init(PIM::FUNC_NUM::F1, sram, dram_src, xfr_size*sizeof(uint64_t));
  revpim::run(PIM::FUNC_NUM::F1);
  revpim::finish(PIM::FUNC_NUM::F1);

  // PIM SRAM to DRAM
  revpim::init(PIM::FUNC_NUM::F1, dram_dst, sram, xfr_size*sizeof(uint64_t));
  revpim::run(PIM::FUNC_NUM::F1);
  revpim::finish(PIM::FUNC_NUM::F1);

  REV_TIME( time2 );
  return time2 - time1;
}
#endif


size_t check() {
  size_t time1, time2;
  REV_TIME( time1 );
  for (int i=0; i<xfr_size; i++) {

    if (check_data[i] != dram_src[i]) {
      printf("Failed: check_data[%d]=0x%lx dram_src[%d]=0x%lx\n",
              i, check_data[i], i, dram_src[i]);
      assert(false);
    }

    #if DO_LOOP
    if (check_data[i] != buffer[i]) {
      printf("Failed: check_data[%d]=0x%lx buffer[%d]=0x%lx\n",
              i, check_data[i], i, buffer[i]);
      assert(false);
    }
    #else 
    if (check_data[i] != sram[i]) {
      printf("Failed: check_data[%d]=0x%lx sram[%d]=0x%lx\n",
              i, check_data[i], i, sram[i]);
      assert(false);
    }
    #endif

    if (check_data[i] != dram_dst[i]) {
      printf("Failed: check_data[%d]=0x%lx dram_dst[%d]=0x%lx\n",
              i, check_data[i], i, dram_dst[i]);
      assert(false);
    }

  }
  REV_TIME( time2 );
  return time2 - time1;
}

int main( int argc, char** argv ) {
  printf("Starting checksram\n");
  #ifdef DO_LOOP
  printf("Using simple loop (no PIM)\n");
  #elif DO_PIM
  printf("Using PIM functions\n");
  #else
  assert(0);
  #endif

  size_t time_id, time_config, time_exec, time_check;

#if DO_LOOP
  printf("\nbuffer=0x%lx\ndram_dst=0x%lx\ndram_src=0x%lx\nxfr_size=%d\n",
    reinterpret_cast<uint64_t>(buffer), reinterpret_cast<uint64_t>(dram_dst), reinterpret_cast<uint64_t>(dram_src), xfr_size
  );
#else
  printf("\nsram=0x%lx\ndram_dst=0x%lx\ndram_src=0x%lx\nxfr_size=%d\nsram_size=%d\n",
    reinterpret_cast<uint64_t>(sram), reinterpret_cast<uint64_t>(dram_dst), reinterpret_cast<uint64_t>(dram_src), xfr_size, PIM::SRAM_SIZE
  );
  if ((8*xfr_size) > PIM::SRAM_SIZE) {
    printf("xfr_size in bytes %d exceeds sram size %ld. Data will wrap.\n", 8*xfr_size, PIM::SRAM_SIZE);
  }
#endif

  printf("Checking PIM ID...\n");
  time_id = checkPIM();
  printf("Configuring...\n");
  time_config = configure();
  printf("Executing...\n");
  time_exec = theApp(); 
  printf("Checking...\n");
  time_check = check();

  printf("Results:\n");
  printf("cycles: id_check=%d, config=%d, exec=%d, check=%d\n", time_id, time_config, time_exec, time_check);
  printf("checksram completed normally\n");
  return 0;
}
