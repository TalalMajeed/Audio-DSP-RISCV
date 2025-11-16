############################################################
#                 PicoRV32 Top-Level Makefile
#            (Modified for custom minimal firmware)
############################################################

RISCV_GNU_TOOLCHAIN_GIT_REVISION = 411d134
RISCV_GNU_TOOLCHAIN_INSTALL_PREFIX = /opt/riscv32

SHELL = bash
PYTHON = python3
VERILATOR = verilator
ICARUS_SUFFIX =
IVERILOG = iverilog$(ICARUS_SUFFIX)
VVP = vvp$(ICARUS_SUFFIX)

#-------------------------
# Firmware configuration
#-------------------------

# Compile only your firmware sources:
#   firmware/crt0.S
#   firmware/main.c
FIRMWARE_OBJS = firmware/crt0.o firmware/main.o

# Keep the test objects — required for testbench operations
TEST_OBJS =

# Toolchain
TOOLCHAIN_PREFIX ?= riscv64-unknown-elf-

# Aggressive compiler warnings
GCC_WARNS  = -Werror -Wall -Wextra -Wshadow -Wundef -Wpointer-arith -Wcast-qual -Wcast-align -Wwrite-strings
GCC_WARNS += -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes -pedantic

COMPRESSED_ISA = C


############################################################
#                        TESTBENCHES
############################################################

test: testbench.vvp firmware/firmware.hex
	$(VVP) -N $<

test_vcd: testbench.vvp firmware/firmware.hex
	$(VVP) -N $< +vcd +trace +noerror

test_rvf: testbench_rvf.vvp firmware/firmware.hex
	$(VVP) -N $< +vcd +trace +noerror

test_wb: testbench_wb.vvp firmware/firmware.hex
	$(VVP) -N $<

test_wb_vcd: testbench_wb.vvp firmware/firmware.hex
	$(VVP) -N $< +vcd +trace +noerror

test_ez: testbench_ez.vvp
	$(VVP) -N $<

test_ez_vcd: testbench_ez.vvp
	$(VVP) -N $< +vcd

test_sp: testbench_sp.vvp firmware/firmware.hex
	$(VVP) -N $<

test_axi: testbench.vvp firmware/firmware.hex
	$(VVP) -N $< +axi_test

test_synth: testbench_synth.vvp firmware/firmware.hex
	$(VVP) -N $<

# The Verilator testbench produces: ./testbench_verilator
test_verilator: testbench_verilator firmware/firmware.hex
	./testbench_verilator


############################################################
#                 TESTBENCH BUILD RULES
############################################################

testbench.vvp: testbench.v picorv32.v
	$(IVERILOG) -o $@ $(subst C,-DCOMPRESSED_ISA,$(COMPRESSED_ISA)) $^
	chmod -x $@

testbench_rvf.vvp: testbench.v picorv32.v rvfimon.v
	$(IVERILOG) -o $@ -D RISCV_FORMAL $(subst C,-DCOMPRESSED_ISA,$(COMPRESSED_ISA)) $^
	chmod -x $@

testbench_wb.vvp: testbench_wb.v picorv32.v
	$(IVERILOG) -o $@ $(subst C,-DCOMPRESSED_ISA,$(COMPRESSED_ISA)) $^
	chmod -x $@

testbench_ez.vvp: testbench_ez.v picorv32.v
	$(IVERILOG) -o $@ $(subst C,-DCOMPRESSED_ISA,$(COMPRESSED_ISA)) $^
	chmod -x $@

testbench_sp.vvp: testbench.v picorv32.v
	$(IVERILOG) -o $@ $(subst C,-DCOMPRESSED_ISA,$(COMPRESSED_ISA)) -DSP_TEST $^
	chmod -x $@

testbench_synth.vvp: testbench.v synth.v
	$(IVERILOG) -o $@ -DSYNTH_TEST $^
	chmod -x $@


############################################################
#                     VERILATOR TESTBENCH
############################################################

testbench_verilator: testbench.v picorv32.v testbench.cc
	$(VERILATOR) --cc --exe -Wno-lint -trace \
		--top-module picorv32_wrapper \
		testbench.v picorv32.v testbench.cc \
		$(subst C,-DCOMPRESSED_ISA,$(COMPRESSED_ISA)) \
		--Mdir testbench_verilator_dir
	$(MAKE) -C testbench_verilator_dir -f Vpicorv32_wrapper.mk
	cp testbench_verilator_dir/Vpicorv32_wrapper testbench_verilator


############################################################
#                   FIRMWARE BUILD RULES
############################################################

firmware/firmware.hex: firmware/firmware.bin firmware/makehex.py
	$(PYTHON) firmware/makehex.py $< 32768 > $@

firmware/firmware.bin: firmware/firmware.elf
	$(TOOLCHAIN_PREFIX)objcopy -O binary $< $@
	chmod -x $@

firmware/firmware.elf: $(FIRMWARE_OBJS) firmware/sections.lds
	$(TOOLCHAIN_PREFIX)gcc -Os -mabi=ilp32 -march=rv32im$(subst C,c,$(COMPRESSED_ISA)) -ffreestanding -nostdlib \
		-o $@ \
		-Wl,--build-id=none,-Bstatic,-T,firmware/sections.lds,-Map,firmware/firmware.map,--strip-debug \
		$(FIRMWARE_OBJS) -lgcc
	chmod -x $@

# Build startup code (crt0)
firmware/crt0.o: firmware/crt0.S
	$(TOOLCHAIN_PREFIX)gcc -c -mabi=ilp32 -march=rv32im$(subst C,c,$(COMPRESSED_ISA)) -o $@ $<

# Build C files (main.c)
firmware/%.o: firmware/%.c
	$(TOOLCHAIN_PREFIX)gcc -c -mabi=ilp32 -march=rv32i$(subst C,c,$(COMPRESSED_ISA)) \
		-Os --std=c99 $(GCC_WARNS) -ffreestanding -nostdlib -o $@ $<

# Build test objects
tests/%.o: tests/%.S tests/riscv_test.h tests/test_macros.h
	$(TOOLCHAIN_PREFIX)gcc -c -mabi=ilp32 -march=rv32im -o $@ \
		-DTEST_FUNC_NAME=$(notdir $(basename $<)) \
		-DTEST_FUNC_TXT='"$(notdir $(basename $<))"' \
		-DTEST_FUNC_RET=$(notdir $(basename $<))_ret $<

############################################################
#                        CLEAN RULES
############################################################

clean:
	rm -rf riscv-gnu-toolchain-riscv32i riscv-gnu-toolchain-riscv32ic \
	       riscv-gnu-toolchain-riscv32im riscv-gnu-toolchain-riscv32imc
	rm -vrf $(FIRMWARE_OBJS) check.smt2 check.vcd synth.v synth.log \
		firmware/firmware.elf firmware/firmware.bin firmware/firmware.hex firmware/firmware.map \
		testbench.vvp testbench_sp.vvp testbench_synth.vvp testbench_ez.vvp \
		testbench_rvf.vvp testbench_wb.vvp testbench.vcd testbench.trace \
		testbench_verilator testbench_verilator_dir

.PHONY: test test_vcd test_sp test_axi test_wb test_wb_vcd test_ez test_ez_vcd test_synth clean