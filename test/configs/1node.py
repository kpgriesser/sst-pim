#
# Copyright (C) 2017-2024 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
#
# See LICENSE in the top level directory for licensing details
#

#
# pim-simple.py
#
# Single CPU, Single Memory
#

import os
import sst

print("pim-simple.py")

#
# Output controls
#

debug = {
    "cpubus"  : 0, # 0: No debugging, 1: STDOUT, 2: STDERR, 3: FILE
    "l1"      : 0,
    "l2"      : 0,
    "mem"     : 0,
    "memnic"  : 0,
    "local_network" : 0,
    "debug_addr"    : []  # address selections for caches, memnic, memctrl, ...
}

debug_level = {
    "cpubus"  : 9, # 1-2:nada?, 3-9:Events(New), 10:Broadcasts
    "l1"      : 4, # 1-2:info, 3:Events(New,Recv), 4:Events(Send), 5:Alloc/Get*/Dealloc/Writeback
                   # 6-9:hit/miss/respond/stall, 10:Init/MSHR(InsEv/RemFr/Erase)
    "l2"      : 4,
    "mem"     : 8, # 1-2:empty, 3:Event, 4-7:Backend, 8:PIMBackend, 9-10:InitEvent
    "memnic"  : 8,
    "local_network" : 8,
}

verbose = {
    "rev"           :  1,  # 1:rev_fast_printf 5:tracer
    "rev_mem_ctrl"  :  0,  # 1-4:nada?, 5-10:info
    "l1"            :  0,  # 0[fatal error only], 1[warnings], 2[full state dump on fatal error]
    "l2"            :  0,  # 0[fatal error only], 1[warnings], 2[full state dump on fatal error]
    "mem"           :  3,  # 3: PIM
    "miranda"       :  1,  # 1: Application messages, 3: sequencer
}

#
# Timing Parameters
#
timing_params = {
    "global_clock"                 : "1.0GHz",
    "link_latency_default"         : "1ns",
    "rev_max_txn"                  : "16",
    "l1_access_latency_cycles"     : "8",
    "l2_access_latency_cycles"     : "30",
    "memnic_network_bw"            : "100GiB/s",

    "local_network_xbar_bw"        : "1TiB/s",
    "local_network_link_bw"        : "1TiB/s",
    "local_network_input_latency"  : "1ns",
    "local_network_output_latency" : "1ns",
    # MEMORY_MODEL=simplemem
    "simplemem_access_time"     : "100ns",
    # MEMORY_MODEL=dramsim3
    "dramsim3_config_ini"       : "HBM2e_8Gbx128_UP.ini",
}

#
# User configuration settings
#


# REV machine type
ARCH = os.getenv("ARCH","rv64g_zicntr")

# Application Driver Test
APP = os.getenv("APP")
if APP:
    print(f"APP={APP}")

SUPPORTED_INTERLEAVING = ["none","wide","small"]
INTERLEAVE = os.getenv("INTERLEAVE","wide")
if INTERLEAVE not in SUPPORTED_INTERLEAVING:
    sys.exit(f"INTERLEAVE must be one of: {SUPPORTED_INTERLEAVING}")
print(f"INTERLEAVE={INTERLEAVE}")

# Command line arguments for elf
ARGS = os.getenv("ARGS","");

FORCE_NONCACHEABLE_REQS = os.getenv("FORCE_NONCACHEABLE_REQS",0)
print(f"FORCE_NONCACHEABLE_REQS={FORCE_NONCACHEABLE_REQS}")

# Memory: 0x0000_0400_0000_0000
TOTAL_GB = 8*512
mem_info = {
    "gb"     : TOTAL_GB,
    "sz_str" : f"{TOTAL_GB}GiB",
    "sz"     : TOTAL_GB * 1024 * 1024 * 1024,
    "last"   : TOTAL_GB * 1024 * 1024 * 1024 - 1,
    "sz_rev" : TOTAL_GB * 1024 * 1024 * 1024,
}
print(f"TotalMem={mem_info['gb']}GiB")
print(f"RevMem={mem_info['sz_rev']/(1024*1024*1024)}GiB")
print(f"LastByte=0x{mem_info['last']:X}")

#NODES = int(os.getenv("NODES", 1))
NODES = 1
MEM_PER_NODE = int(mem_info['sz'] / NODES)

print(f"NODES={NODES}")
print(f"MEM_PER_NODE=0x{MEM_PER_NODE:X}")
MiB_PER_NODE = f"{(1024*1024)}MiB"

#NUM_CPUS = int(os.getenv("NUM_CPUS", 1))
NUM_CPUS = 1
print(f"NUM_CPUS={NUM_CPUS}")

#NUM_CORES = int(os.getenv("NUM_CORES", 1))
NUM_CORES = 1

SUPPORTED_MEMORY_MODELS = ["simpleMem","dramsim3"]
MEMORY_MODEL = os.getenv("MEMORY_MODEL","simpleMem")
MEMORY_CONTROLLER = "PIM.PIMMemController"

PIM_TYPE = os.getenv("PIM_TYPE","0")  # 0:none, 1:test, 2:reserved, 3:tclpim
print(f"PIM_TYPE={PIM_TYPE}")

if MEMORY_MODEL not in SUPPORTED_MEMORY_MODELS:
    sys.exit(f"MEMORY_MODEL must be one of: {SUPPORTED_MEMORY_MODELS}")
print(f"MEMORY_MODEL={MEMORY_MODEL}")


# Apparently sst --output-directory is not putting statistics output there so...
# TODO check if bug posted, if not, post one
OUTPUT_DIRECTORY=os.getenv("OUTPUT_DIRECTORY","./")
print(f"OUTPUT_DIRECTORY={OUTPUT_DIRECTORY}")
if not os.path.exists(OUTPUT_DIRECTORY):
    os.makedirs(OUTPUT_DIRECTORY);

#
# Common Component parameters
#
# Groups are used by NIC to determine which attached components
# are recognized by the NIC.
cpu_group = 1               # L2 cache level
node_group = 2              # Memory Controllers

# Miranda/Appx
miranda_params = {
    "verbose" : verbose['miranda'],
    "max_reqs_cycle" : 2,
    "cache_line_size" : 64,
    "maxmemreqpending" : 1,
    "clock"   : timing_params['global_clock'],
}

# REV CPU
rev_params = {
    "verbose" : verbose['rev'],                      # Verbosity
    "numCores" : NUM_CORES,                          # Number of cores
    "clock" : timing_params['global_clock'],
                                                     # Adjust top of memory per stack
    "maxHeapSize" : (1024*1024*1024)>>4,             # Default is 1/4 mem size
    "machine" : f"[CORES:{ARCH}]",                   # Core:Config
    "memCost" : "[0:1:10]",                          # Memory loads required 1-10 cycles
    "program" : os.getenv("REV_EXE", "sanity.exe"),  # Target executable
    "enableMemH" : 1,                                # Enable memHierarchy support
    #"trcStartCycle" : 1,                            # Begin cycle for tracing
    #"trcLimit" : 100,                               # End cycle for tracing
    "splash" : 0,                                    # Display the splash message
}

revmemctrl_params = {
    "verbose"         : verbose['rev_mem_ctrl'],
    "clock"           : timing_params['global_clock'],
    "max_loads"       : timing_params['rev_max_txn'],
    "max_stores"      : timing_params['rev_max_txn'],
    "max_flush"       : timing_params['rev_max_txn'],
    "max_llsc"        : timing_params['rev_max_txn'],
    "max_readlock"    : timing_params['rev_max_txn'],
    "max_writeunlock" : timing_params['rev_max_txn'],
    "max_custom"      : timing_params['rev_max_txn'],
    "ops_per_cycle"   : timing_params['rev_max_txn'],
}

l1cache_params = {
    "cache_frequency" : timing_params['global_clock'],
    "cache_size" : "16KiB",
    "associativity" : "4",
    "access_latency_cycles" : timing_params['l1_access_latency_cycles'],
    "L1" : "1",
    "cache_line_size" : "64",
    "coherence_protocol" : "MESI", # MESI, MSI, NONE
    "cache_type" : "inclusive",    # required for L1s
    "max_requests_per_cycle" : -1, # -1 unconstrained
    "maxRequestDelay" : 0,         # (in ns) disabled
    "debug" : debug['l1'],
    "debug_level" : debug_level['l1'],
    "debug_addr" : debug['debug_addr'],
    "verbose" : verbose['l1'],
    "force_noncacheable_reqs" : FORCE_NONCACHEABLE_REQS,
    "replacement_policy" : "lru",
}

PIM_REGION_BASE=0x0E000000             # FUNC_BASE
PIM_REGION_BOUND=0x0f800000+0x00100000 # DRAM_BASE + DRAM_SIZE
l1cache_ifc_params = {
    "noncacheable_regions": [PIM_REGION_BASE, PIM_REGION_BOUND-1]
}

l2cache_params = {
    "cache_frequency" : timing_params['global_clock'],
    "cache_size" : "64 KiB",
    "associativity" : "16",
    "access_latency_cycles" : timing_params['l2_access_latency_cycles'],
    "cache_line_size" : "64",
    "coherence_protocol" : "MESI",
    "max_requests_per_cycle" : -1, # -1 unconstrained
    "request_link_width"  : "0B",  # 0B unconstrained
    "response_link_width" : "0B",  # 0B unconstrained
    "maxRequestDelay" : 0,         # (in ns) disabled
    "mshr_num_entries" : 8,        # Miss Status Holding Registers
    "mshr_latency_cycles" : 16,    # cycles to process responses in cache and replay requests
    "debug" : debug['l2'],
    "debug_level" : debug_level['l2'],
    "debug_addr" : debug['debug_addr'],
    "verbose" : verbose['l2'],
    "force_noncacheable_reqs" : FORCE_NONCACHEABLE_REQS,
}

cpubus_params = {
    "bus_frequency" : timing_params['global_clock'],
    "debug"         : debug['cpubus'],
    "debug_level"   : debug_level['cpubus'],
    "debug_addr"    : debug['debug_addr']
}

memlink_params = {
    "debug"         : debug['cpubus'],
    "debug_level"   : debug_level['cpubus'],
    "debug_addr"    : debug['debug_addr'],
    "latency"       : "0ps"
}

memctrl_params = {
    "pim_type" : PIM_TYPE,           # 1:test mode, 2:reserved, 3:tclpim
    "clock" : timing_params['global_clock'],
    "request_width" : 64,
    "verbose" : verbose['mem'],
    "debug" : debug['mem'],
    "debug_level" : debug_level['mem'],
    "debug_addr"  : debug['debug_addr'],
    "listenercount" : 0,
    "backing" : "malloc",
    "customCmdHandler" : "memHierarchy.defCustomCmdHandler",
}

memnic_params = {
    "debug" : debug['memnic'],
    "debug_level" : debug_level['memnic'],
    "debug_addr"  : debug['debug_addr'],
    "network_bw" : timing_params['memnic_network_bw'],
    "network_input_buffer_size" : "1KiB",
    "network_output_buffer_size" : "1KiB",
    "range_check" : "0",
}

# backend
backend_params = {
    "verbose"    : verbose['mem'],
    "debug"      : debug['mem'],
    "debug_level" : debug_level['mem'],
    "mem_size"   : mem_info['sz_str'], # TODO should be per node memory
    "pim_type"   : PIM_TYPE,
    "num_nodes"  : NODES,
    "output_directory" : OUTPUT_DIRECTORY, # location for perflog.tsv files
}

# Local Network Parameters (Merlin)
local_network_params = {
    "num_ports" : f"{1+NODES}",
    "topology" : "merlin.singlerouter",
    "link_bw" : timing_params['local_network_link_bw'],
    "xbar_bw" : timing_params['local_network_xbar_bw'],
    "flit_size" : "72B",
    "input_latency" : timing_params['local_network_input_latency'],
    "output_latency" : timing_params['local_network_output_latency'],
    "input_buf_size" : "1KB",
    "output_buf_size" : "1KB",
    "debug" : debug['local_network'],
    "debug_level" : debug_level['local_network'],
    "debug_addr"  : debug['debug_addr']
}

# REVCPU with L1 class
#

class REV_CPU():
    def __init__(self, cpu_num):
        # Rev
        self.comp = sst.Component(f"cpu{cpu_num}", "revcpu.RevCPU")
        self.comp.addParams(rev_params)
        # Buffering for private stack and heap (match memory stride so each CPU heap starts at corresponding node)
        mem_top = mem_info['sz_rev'] - cpu_num*0x10000
        self.comp.addParams({ "memSize" : mem_top })
        self.comp.addParams({ "heapBuffer" : cpu_num * 0x10000 })

        # append cpu number to args. Tests must support this convention for multiple REV CPUs
        if ARGS == "":
            cpu_args = f"{cpu_num}"
        else:
            cpu_args = f"{ARGS} {cpu_num}"
        self.comp.addParams( {"args" : cpu_args } );
        print(f"REV_CPU[{cpu_num}] args={cpu_args} mem_top=0x{mem_top:x}")

        # Rev Memory Controller
        self.revmemctrl = self.comp.setSubComponent("memory", "revcpu.RevBasicMemCtrl")
        self.revmemctrl.addParams(revmemctrl_params)
        # Rev Memory Interface
        self.ifc = self.revmemctrl.setSubComponent("memIface", "memHierarchy.standardInterface")
        self.ifc.addParams(l1cache_ifc_params)
        # L1 Cache
        self.l1 = sst.Component(f"l1_{cpu_num}", "memHierarchy.Cache")
        self.l1.addParams(l1cache_params)
        self.l1.addParams({
            "node": cpu_num,
        })
        ## connect CPU to L1
        self.link_cpu_l1 = sst.Link(f"link_cpu_l1_{cpu_num}")
        lat = timing_params['link_latency_default']
        self.link_cpu_l1.connect( (self.ifc, "port", lat), (self.l1, "high_network_0", lat) )

    def enableStatistics():
        self.comp.enableAllStatistics()
        self.comp_revmemctrl.enableAllStatistics({"type":"sst.AccumulatorStatistic"})
        self.comp_l1.enableAllStatistics({"type":"sst.AccumulatorStatistic"})

class APP_CPU():
    def __init__(self, cpu_num):
        # Miranda
        self.comp = sst.Component(f"cpu{cpu_num}", "AppGen.RequestGenCPU_KG")
         # append cpu number to args. Tests must support this convention for multiple REV CPUs
        if ARGS == "":
            cpu_args = f"{cpu_num}"
        else:
            cpu_args = f"{ARGS} {cpu_num}"
        self.comp.addParams(miranda_params)
        self.comp.addParams( { "args" : cpu_args } )
        print(f"APP_CPU[{cpu_num}] args={cpu_args}")

        # TODO: non-cacheable ranges for L1 interface then remove FORCE_NONCACHEABLE_REQS from mkinc/appx.mk

        # Miranda application transactor / generator
        self.gen = self.comp.setSubComponent("generator", f"AppGen.{APP}")
        self.gen.addParams(miranda_params)
        # L1 Cache
        self.l1 = sst.Component(f"l1_{cpu_num}", "memHierarchy.Cache")
        self.l1.addParams(l1cache_params)
        self.l1.addParams({
            "node": cpu_num,
        })
        # connect CPU to L1
        self.link_cpu_l1 = sst.Link(f"link_cpu_l1_{cpu_num}")
        lat = timing_params['link_latency_default']
        self.link_cpu_l1.connect( (self.comp, "cache_link", lat), (self.l1, "high_network_0", lat) )

    def enableStatistics():
        self.comp.enableAllStatistics()
        self.comp_revmemctrl.enableAllStatistics({"type":"sst.AccumulatorStatistic"})
        self.comp_l1.enableAllStatistics({"type":"sst.AccumulatorStatistic"})


#
# CPU_COMPLEX
#   cpu[0]   -- L1$ --|
#   cpu[1]   -- L1$ --| \
#                     |  L2$ -- nic
#                     | /
#   cpu[n-1] -- L1$ --|
#
class CPU_COMPLEX():
    def __init__(self,node):

        # CPU vector
        self.cpu = []
        for i in range(NUM_CPUS):
            if APP:
                print(f"Creating APP_CPU({APP})[{i}]")
                self.cpu.append(APP_CPU(i))
            else:
                print(f"Creating REV_CPU[{i}]")
                self.cpu.append(REV_CPU(i))

        # Bus connecting CPU L1 to next level of the memory hierarchy
        self.cpubus = sst.Component("cpubus", "memHierarchy.Bus")
        self.cpubus.addParams(cpubus_params)

        # Level2 Cache
        self.l2cache = sst.Component("l2cache", "memHierarchy.Cache")
        self.l2cache.addParams(l2cache_params)
        self.l2cache.addParams({"node" : node})

        # L2 Cache interface to cpu bus
        self.cpulink = self.l2cache.setSubComponent("cpulink","memHierarchy.MemLink")
        self.cpulink.addParams(memlink_params)

        # L2 Cache interface to nic
        self.nic = self.l2cache.setSubComponent("memlink","memHierarchy.MemNIC")
        self.nic.addParams({
            "group" : cpu_group,
        })
        self.nic.addParams(memnic_params)

        # Connect cpubus to L2
        self.link_cpubus_l2 = sst.Link("link_cpubus_l2")
        lat = timing_params['link_latency_default']
        self.link_cpubus_l2.connect(
            (self.cpubus, "low_network_0", lat),
            (self.cpulink, "port", lat) )

        # Connect CPUs (L1) to cpubus
        self.link_l1_cpubus = []
        for i in range(NUM_CPUS):
            self.link_l1_cpubus.append(sst.Link(f"link_l1_{i}_cpubus"))
            lat = timing_params['link_latency_default']
            self.link_l1_cpubus[i].connect(
                (self.cpu[i].l1, "low_network_0",  lat),
                (self.cpubus, f"high_network_{i}", lat)
            )


#
# Memory Subsystems (Nodes)
#
class NODE():
    def __init__(self,node):

        node_mem_params = {}
        if NODES==1 or INTERLEAVE=="none":
            memBot = 0;
            memTop = mem_info['sz']
            node_mem_params = {
                "node_id"          : 0,
                "backend.mem_size" : MiB_PER_NODE,
                "addr_range_start" : f"{memBot}",
                "addr_range_end"   : f"{memTop-1}"
            }
            print(f"memory{node} start=0x{memBot:X} end=0x{memTop:X} size=0x{MEM_PER_NODE:X}")
        elif INTERLEAVE=="wide":
            # every 128MB switch memories
            istride = 0x8000000
            isize = "128MiB"
            istep = f"{128*NODES}MiB"
            memBot = (node%NODES) * istride;
            memTop = mem_info['sz'] - ((NODES-node-1)*istride) - 1
            node_mem_params = {
                "node_id"          : node,
                "backend.mem_size" : MiB_PER_NODE,
                "addr_range_start" : f"{memBot}",
                "addr_range_end"   : f"{memTop}",
                "interleave_size"  : isize,
                "interleave_step"  : istep
            }
            print(f"memory{node} start=0x{memBot:X} end=0x{(memTop):X} size=0x{MEM_PER_NODE:X} interleave_size={isize} interleave_step={istep}")
        elif INTERLEAVE=="small":
            istride = 64
            isize = "64B"
            istep = f"{istride*NODES}B"
            memBot = (node%NODES) * istride;
            memTop = mem_info['sz'] - ((NODES-node-1)*istride) - 1
            node_mem_params = {
                "node_id"          : node,
                "backend.mem_size" : MiB_PER_NODE,
                "addr_range_start" : f"{memBot}",
                "addr_range_end"   : f"{memTop}",
                "interleave_size"  : isize,
                "interleave_step"  : istep
            }
            print(f"memory{node} start=0x{memBot:X} end=0x{(memTop):X} size=0x{MEM_PER_NODE:X} interleave_size={isize} interleave_step={istep}")


        self.memctrl = sst.Component(f"memory{node}", MEMORY_CONTROLLER)

        # python 3.9+
        #self.memctrl.addParams( memctrl_params | node_mem_params)
        # python2.7/python3
        memctrl_params.update(node_mem_params)
        self.memctrl.addParams( memctrl_params )

        print(mem_info)

        # insert PIM here
        self.pimbackend = self.memctrl.setSubComponent("backend", "PIM.PIMBackend")
        self.pimbackend.addParams({
            "node_id" : node,
            "max_requests_per_cycle" : 128,
            "request_delay" : "1ns",
            "pim_type" : PIM_TYPE
        })
        self.pimbackend.addParams( backend_params )

        # connect memory controller backend
        if MEMORY_MODEL == "simpleMem":
            self.memory = self.pimbackend.setSubComponent("backend", "memHierarchy.simpleMem")
            self.memory.addParams({"access_time" : timing_params['simplemem_access_time']})
        elif MEMORY_MODEL == "dramsim3":
            self.memory = self.pimbackend.setSubComponent("backend", "memHierarchy.dramsim3")
            self.memory.addParams({"config_ini" : timing_params['dramsim3_config_ini']})

        self.memory.addParams(backend_params)

        # The memory controller NIC
        self.memNIC = self.memctrl.setSubComponent("cpulink", "memHierarchy.MemNIC")
        self.memNIC.addParams(memnic_params)
        self.memNIC.addParams({
            "group" : node_group,
            "sources" : [cpu_group, node_group],
            "destinations" : [node_group],
        })

if __name__ == "__main__":

    #
    # Instantiate and connect NODES to the Internode Network
    #

    # cpu_l1[n:0] <-> cpu_bus <-> l2 <..> nic
    cpucomplex = CPU_COMPLEX(0)

    # Memory Subsystems containing PIMs
    node = []
    for n in range(NODES):
        node.append(NODE(n))

    # Local interconnect
    local_network = sst.Component("local_network", "merlin.hr_router")
    local_network.addParams( {"id" : 0 });
    local_network.addParams(local_network_params)
    local_network.setSubComponent("topology",local_network_params['topology'])

    # connnect L2 NIC to Local Interconnect Network
    link_cache_net_0 = sst.Link("link_cache_net_0")
    link_cache_net_0.connect(
        (cpucomplex.nic, "port", timing_params['link_latency_default']),
        (local_network, "port0", timing_params['link_latency_default'])
    )

    # connect local_network to directory controllers
    link_dir_net = []
    for n in range(NODES):
         link_dir_net.append(sst.Link(f"link_dir_net_{n}"))
         link_dir_net[n].connect(
             (local_network, f"port{n+1}", timing_params['link_latency_default']),
             (node[n].memNIC, "port", timing_params['link_latency_default'])
         )


    #
    # Statistics Controls
    #

    sst.setStatisticLoadLevel(7)
    sst.setStatisticOutput("sst.statOutputCSV")
    sst.setStatisticOutputOptions({
        "filepath" : f"{OUTPUT_DIRECTORY}/sst-stats.csv",
        "separator" : ","
    })
    sst.enableAllStatisticsForAllComponents()


# EOF
