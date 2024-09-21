#!/bin/bash -x
make clean
mkdir -p rev-output/bin

riscv64-unknown-elf-g++ -O2 -g -Wall -Wextra -Wuninitialized -pedantic-errors -Wno-unused-function -Wno-unused-parameter  -static -lm -fpermissive -o rev-output/bin/checkdram.o -g -c rev-test-src/checkdram.cc -march=rv64g  -I/home/kgriesser/work/rev/common/syscalls -I/home/kgriesser/work/rev/test/include -I/home/kgriesser/work/sst-pim/sstcomp/include

riscv64-unknown-elf-ld -o rev-output/bin/checkdram.exe -T ./rev-test-src/pim.lds rev-output/bin/checkdram.o

PIM_TYPE=3 OUTPUT_DIRECTORY=rev-output/checkdram ARCH=rv64g_zicntr REV_EXE=rev-output/bin/checkdram.exe sst --add-lib-path=/home/kgriesser/work/sst-pim/sstcomp/PIMBackend --add-lib-path=/home/kgriesser/work/rev/build/src  --output-json=rev-output/checkdram/rank.json /home/kgriesser/work/sst-pim/test/configs/1node.py

