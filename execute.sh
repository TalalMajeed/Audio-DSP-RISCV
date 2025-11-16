#!/bin/bash
set -e

if [ $# -lt 1 ]; then
    echo "Usage: $0 input.wav [output.wav]"
    exit 1
fi

IN_WAV="$1"
OUT_WAV="${2:-output.wav}"

echo "=========================================="
echo " Building firmware (firmware.hex) "
echo "=========================================="

make firmware/firmware.hex TOOLCHAIN_PREFIX=riscv64-unknown-elf-

echo ""
echo "=========================================="
echo " Resolving wav_buffer base address "
echo "=========================================="

WAV_BASE_HEX=$(riscv64-unknown-elf-nm firmware/firmware.elf | awk '/wav_buffer/{print $1; exit}')
if [ -z "$WAV_BASE_HEX" ]; then
    echo "ERROR: could not find wav_buffer symbol in firmware.elf"
    exit 1
fi

echo "wav_buffer base address: 0x$WAV_BASE_HEX"

echo ""
echo "=========================================="
echo " Building Verilator Testbench "
echo "=========================================="
echo ""

if [ ! -d "testbench_verilator_dir" ]; then
    echo "ERROR: testbench_verilator_dir not found."
    echo "Please run 'make testbench_verilator' once from a path without spaces,"
    echo "then re-run this script."
    exit 1
fi

VERILATOR_ROOT="/opt/homebrew/Cellar/verilator/5.042/share/verilator"
if [ ! -d "$VERILATOR_ROOT" ]; then
    echo "ERROR: Expected Verilator root at $VERILATOR_ROOT"
    exit 1
fi

echo "Rebuilding testbench_verilator (C++ wrapper only)..."
c++ -std=c++17 \
    -Itestbench_verilator_dir \
    -I"$VERILATOR_ROOT/include" \
    -I"$VERILATOR_ROOT/include/vltstd" \
    -c testbench.cc -o testbench_verilator_dir/testbench.o

c++ -std=c++17 \
    -Itestbench_verilator_dir \
    -I"$VERILATOR_ROOT/include" \
    -I"$VERILATOR_ROOT/include/vltstd" \
    testbench_verilator_dir/testbench.o \
    testbench_verilator_dir/Vpicorv32_wrapper__ALL.o \
    testbench_verilator_dir/verilated.o \
    testbench_verilator_dir/verilated_vcd_c.o \
    testbench_verilator_dir/verilated_threads.o \
    testbench_verilator_dir/verilated_dpi.o \
    -pthread -o testbench_verilator

echo ""
echo "=========================================="
echo " Running Verilator Testbench "
echo "=========================================="
echo ""

./testbench_verilator +noerror +inwav="$IN_WAV" +outwav="$OUT_WAV" +wavbase=0x"$WAV_BASE_HEX"
