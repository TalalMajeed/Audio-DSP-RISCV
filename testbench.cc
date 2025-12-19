#include "Vpicorv32_wrapper.h"
#include "Vpicorv32_wrapper_picorv32_wrapper.h"
#include "Vpicorv32_wrapper_axi4_memory.h"
#include "verilated_vcd_c.h"

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>

#include "firmware/wav_demo.h"

/* Helper: get plusarg of form +key=VALUE */
static const char *get_plusarg(const char *key, int argc, char **argv)
{
	size_t key_len = strlen(key);
	for (int i = 1; i < argc; i++) {
		const char *arg = argv[i];
		if (arg[0] == '+' && strncmp(arg + 1, key, key_len) == 0 && arg[1 + key_len] == '=') {
			return arg + 2 + key_len;
		}
	}
	return NULL;
}

/* Byte-level memory access into axi4_memory.memory */
static void mem_write_byte(Vpicorv32_wrapper *top, uint32_t addr, uint8_t value)
{
	Vpicorv32_wrapper_axi4_memory *mem = top->picorv32_wrapper->mem;
	uint32_t word_index = addr >> 2;
	uint32_t byte_index = addr & 3u;
	if (word_index >= 32768u)
		return;
	uint32_t w = mem->memory[word_index];
	uint32_t mask = 0xFFu << (8u * byte_index);
	w = (w & ~mask) | ((uint32_t)value << (8u * byte_index));
	mem->memory[word_index] = w;
}

static uint8_t mem_read_byte(Vpicorv32_wrapper *top, uint32_t addr)
{
	Vpicorv32_wrapper_axi4_memory *mem = top->picorv32_wrapper->mem;
	uint32_t word_index = addr >> 2;
	uint32_t byte_index = addr & 3u;
	if (word_index >= 32768u)
		return 0;
	uint32_t w = mem->memory[word_index];
	return (uint8_t)((w >> (8u * byte_index)) & 0xFFu);
}

/* Load host WAV file into simulated RAM at wav_base. */
static bool load_wav_into_memory(Vpicorv32_wrapper *top, const char *path, uint32_t wav_base)
{
	if (!path)
		return false;

	FILE *f = fopen(path, "rb");
	if (!f) {
		std::fprintf(stderr, "ERROR: cannot open input WAV '%s'\n", path);
		return false;
	}

	if (std::fseek(f, 0, SEEK_END) != 0) {
		std::fclose(f);
		return false;
	}
	long fsize = std::ftell(f);
	if (fsize < 0) {
		std::fclose(f);
		return false;
	}
	std::rewind(f);

	if ((size_t)fsize < sizeof(WavHeader)) {
		std::fprintf(stderr, "ERROR: input WAV too small\n");
		std::fclose(f);
		return false;
	}

	/* Limit to the demo buffer size in firmware. */
	const size_t max_bytes = sizeof(ExampleWavMono16);
	size_t to_read = (size_t)fsize;
	if (to_read > max_bytes)
		to_read = max_bytes;

	uint8_t *buf = (uint8_t *)std::malloc(to_read);
	if (!buf) {
		std::fclose(f);
		return false;
	}

	size_t nread = std::fread(buf, 1, to_read, f);
	std::fclose(f);
	if (nread != to_read) {
		std::free(buf);
		return false;
	}

	WavHeader *hdr = (WavHeader *)buf;
	if (std::memcmp(hdr->riff_id, "RIFF", 4) != 0 ||
	    std::memcmp(hdr->wave_id, "WAVE", 4) != 0 ||
	    std::memcmp(hdr->fmt_id, "fmt ", 4) != 0 ||
	    std::memcmp(hdr->data_id, "data", 4) != 0) {
		std::fprintf(stderr, "ERROR: input file is not a simple PCM WAV\n");
		std::free(buf);
		return false;
	}

	if (hdr->audio_format != 1u || hdr->bits_per_sample != 16u || hdr->num_channels != 1u) {
		std::fprintf(stderr, "ERROR: WAV must be 16-bit mono PCM\n");
		std::free(buf);
		return false;
	}

	if (hdr->data_size > to_read - sizeof(WavHeader)) {
		hdr->data_size = (uint32_t)(to_read - sizeof(WavHeader));
	}

	for (size_t i = 0; i < to_read; i++) {
		mem_write_byte(top, wav_base + (uint32_t)i, buf[i]);
	}

	std::free(buf);
	return true;
}

/* Dump processed WAV buffer from simulated RAM to host file. */
static bool dump_wav_from_memory(Vpicorv32_wrapper *top, const char *path, uint32_t wav_base)
{
	if (!path)
		return false;

	uint8_t raw_hdr[sizeof(WavHeader)];
	for (size_t i = 0; i < sizeof(WavHeader); i++)
		raw_hdr[i] = mem_read_byte(top, wav_base + (uint32_t)i);

	WavHeader *hdr = (WavHeader *)raw_hdr;
	if (std::memcmp(hdr->riff_id, "RIFF", 4) != 0 ||
	    std::memcmp(hdr->wave_id, "WAVE", 4) != 0 ||
	    std::memcmp(hdr->fmt_id, "fmt ", 4) != 0 ||
	    std::memcmp(hdr->data_id, "data", 4) != 0) {
		std::fprintf(stderr, "WARNING: firmware did not leave a valid WAV header; skipping output\n");
		return false;
	}

	uint32_t total_bytes = sizeof(WavHeader) + hdr->data_size;
	if (total_bytes > sizeof(ExampleWavMono16))
		total_bytes = sizeof(ExampleWavMono16);

	FILE *f = std::fopen(path, "wb");
	if (!f) {
		std::fprintf(stderr, "ERROR: cannot open output WAV '%s'\n", path);
		return false;
	}

	for (uint32_t i = 0; i < total_bytes; i++) {
		uint8_t b = mem_read_byte(top, wav_base + i);
		if (std::fwrite(&b, 1, 1, f) != 1) {
			std::fclose(f);
			return false;
		}
	}

	std::fclose(f);
	return true;
}

int main(int argc, char **argv, char **env)
{
	std::printf("Built with %s %s.\n", Verilated::productName(), Verilated::productVersion());
	std::printf("Recommended: Verilator 4.0 or later.\n");

	const char *in_wav  = get_plusarg("inwav", argc, argv);
	const char *out_wav = get_plusarg("outwav", argc, argv);
	const char *wavbase_str = get_plusarg("wavbase", argc, argv);

	uint32_t wav_base = 0;
	if (wavbase_str && wavbase_str[0]) {
		wav_base = (uint32_t)strtoul(wavbase_str, NULL, 0);
	}

	Verilated::commandArgs(argc, argv);
	Vpicorv32_wrapper* top = new Vpicorv32_wrapper;

	// Tracing (vcd)
	VerilatedVcdC* tfp = NULL;
	const char* flag_vcd = Verilated::commandArgsPlusMatch("vcd");
	if (flag_vcd && 0==strcmp(flag_vcd, "+vcd")) {
		Verilated::traceEverOn(true);
		tfp = new VerilatedVcdC;
		top->trace (tfp, 99);
		tfp->open("testbench.vcd");
	}

	// Tracing (data bus, see showtrace.py)
	FILE *trace_fd = NULL;
	const char* flag_trace = Verilated::commandArgsPlusMatch("trace");
	if (flag_trace && 0==strcmp(flag_trace, "+trace")) {
		trace_fd = fopen("testbench.trace", "w");
	}

	// Optional register logging for progress (+reglog)
	bool reglog = false;
	const char* flag_reglog = Verilated::commandArgsPlusMatch("reglog");
	if (flag_reglog && 0==strcmp(flag_reglog, "+reglog")) {
		reglog = true;
	}

	top->clk = 0;
	top->resetn = 0;

	/* Run one eval so Verilog initial blocks (including $readmemh) execute. */
	top->eval();

	if (in_wav && wav_base != 0) {
		if (!load_wav_into_memory(top, in_wav, wav_base)) {
			std::fprintf(stderr, "WARNING: failed to load input WAV; running without it\n");
		}
	} else if (in_wav && wav_base == 0) {
		std::fprintf(stderr, "WARNING: +inwav provided but +wavbase missing; ignoring input\n");
	}

	int t = 0;
	uint64_t cycle_count = 0;
	while (!Verilated::gotFinish()) {
		if (t > 200)
			top->resetn = 1;
		top->clk = !top->clk;
		top->eval();
		if (tfp) tfp->dump (t);
		if (trace_fd && top->clk && top->trace_valid) std::fprintf(trace_fd, "%9.9llx\n", (unsigned long long)top->trace_data);

		if (top->clk && top->resetn) {
			cycle_count++;
			if (reglog && (cycle_count % 10000ull) == 0ull) {
				Vpicorv32_wrapper_picorv32_wrapper *core = top->picorv32_wrapper;
				uint32_t pc = core->__PVT__uut__DOT__picorv32_core__DOT__reg_pc;
				uint32_t x1 = core->__PVT__uut__DOT__picorv32_core__DOT__cpuregs[1];
				uint32_t x10 = core->__PVT__uut__DOT__picorv32_core__DOT__cpuregs[10];
				uint32_t x11 = core->__PVT__uut__DOT__picorv32_core__DOT__cpuregs[11];
				std::printf("REGLOG cycle=%llu pc=0x%08x x1=0x%08x x10=0x%08x x11=0x%08x\n",
				            (unsigned long long)cycle_count, pc, x1, x10, x11);
			}
		}

		t += 5;
	}

	if (out_wav && wav_base != 0) {
		if (!dump_wav_from_memory(top, out_wav, wav_base)) {
			std::fprintf(stderr, "WARNING: failed to dump output WAV\n");
		}
	}

	if (tfp) tfp->close();
	delete top;
	return 0;
}

/* Required by some Verilated models when not using SystemC. */
double sc_time_stamp()
{
	return 0.0;
}
