## PicoRV32 AUX Audio Extension

This fork of PicoRV32 adds a small set of custom **audio‑oriented instructions** implemented via the Pico Co‑Processor Interface (PCPI) and exposes them as C inline helpers in `firmware/main.c`.

The goal is to accelerate fixed‑point audio kernels (MACs, convolutions, LMS, complex ops) while keeping the core vanilla RV32I from the toolchain’s point of view (all ops live in the CUSTOM‑0 opcode space).

---

## Hardware mapping

- Core parameters (in `picorv32.v:62`):
  - `ENABLE_AUDIO` – new boolean parameter. When set, the core exposes an internal **audio PCPI** unit and treats unknown instructions in CUSTOM‑0 as candidates for that unit instead of trapping.
  - `WITH_PCPI` (internal) now includes `ENABLE_AUDIO`, so the PCPI handshake is active whenever MUL/DIV/GENERIC PCPI or AUDIO is enabled (`picorv32.v:169`).

- PCPI audio block:
  - Module: `picorv32_pcpi_audio` (`picorv32.v` near the existing `picorv32_pcpi_mul`/`_div` modules).
  - Connected in the core alongside MUL/DIV:
    - New wires `pcpi_audio_wr/rd/wait/ready` and a `generate if (ENABLE_AUDIO)` instance (`picorv32.v:257-322`).
    - The PCPI result mux includes audio in `pcpi_int_wr/rd` and contributes to `pcpi_int_wait/pcpi_int_ready` (`picorv32.v:326-343`).
  - This block is **combinational**: it never asserts `pcpi_wait`; it returns results in a single cycle whenever it recognizes a valid AUX instruction.

- AXI / Wishbone wrappers:
  - `picorv32_axi` and `picorv32_wb` now forward the `ENABLE_AUDIO` parameter into the core (`picorv32.v:2480+` and `picorv32.v:2880+`).
  - To use AUX instructions in any SoC or test, make sure your `picorv32_axi`/`picorv32_wb` instantiation sets `.ENABLE_AUDIO(1'b1)`.

- Default testbench:
  - The main Verilog testbench enables the audio extension:
    - In `testbench.v:136-172`, the `picorv32_axi` instance has `.ENABLE_AUDIO(1)` added.

---

## Instruction encodings

All AUX instructions are encoded as **R‑type** instructions in the **CUSTOM‑0** opcode space:

- `opcode[6:0] = 0b0001011` (0x0b) – CUSTOM‑0
- `funct3[14:12] = 0b000` – currently ignored by hardware
- `funct7[31:25]` selects the specific AUX instruction
- `rs1`, `rs2`, and `rd` are standard R‑type register fields

### Funct7 map

To avoid colliding with PicoRV32’s built‑in CUSTOM‑0 IRQ instructions
(`getq/setq/maskirq/retirq/waitirq/timer`), which occupy funct7 = 0..5,
the AUX instructions use funct7 values starting at 0x20:

| Name     | funct7 | Description (summary)                                          |
|---------:|:------:|----------------------------------------------------------------|
| MAC16    | 0x20   | 2×16‑bit signed MAC: `(a0*b0 + a1*b1)`                         |
| MSUB16   | 0x21   | 2×16‑bit signed MSUB: `(a0*b0 − a1*b1)`                        |
| ABS16    | 0x22   | Lane‑wise abs on 16‑bit lanes with saturation                  |
| CONV4    | 0x23   | 4×8‑bit signed dot product                                     |
| CONV8    | 0x24   | Currently identical to CONV4 (reserved for longer taps later)  |
| LMSSTEP  | 0x25   | Currently identical to MAC16 (LMS update done in software)     |
| CMAC     | 0x26   | Complex 16‑bit multiply, saturating 16‑bit outputs             |
| ABS2     | 0x27   | Complex magnitude squared from 16‑bit lanes                    |
| CLIP16   | 0x28   | Symmetric lane‑wise 16‑bit clipping                            |
| SHIFTN   | 0x29   | Signed fixed‑point scaling shift with rounding                 |

### Lane semantics

- 16‑bit lanes:
  - `rsX[15:0]`  – low lane (often “left” channel)
  - `rsX[31:16]` – high lane (often “right” channel)
- 8‑bit lanes (CONV4/CONV8):
  - `rsX[7:0]`, `[15:8]`, `[23:16]`, `[31:24]` – four signed 8‑bit values.

### Operation details

- **MAC16 (funct7 = 0x00)**  
  `rs1 = {a1,a0}`, `rs2 = {b1,b0}` (all signed 16‑bit)  
  `rd = (a0*b0 + a1*b1)` as a full 32‑bit signed sum.

- **MSUB16 (0x01)**  
  Same packing as MAC16, but `rd = (a0*b0 − a1*b1)`.

- **ABS16 (0x02)**  
  Signed 16‑bit abs on each lane with saturation:
  - `rd.lo16 = min(|rs1.lo16|, 0x7FFF)`
  - `rd.hi16 = min(|rs1.hi16|, 0x7FFF)`

- **CONV4 (0x03)**  
  Interpret `rs1` and `rs2` as packed signed 8‑bit values `{x3,x2,x1,x0}` and `{h3,h2,h1,h0}`:  
  `rd = x0*h0 + x1*h1 + x2*h2 + x3*h3` (32‑bit signed).

- **CONV8 (0x04)**  
  Currently **implemented identically to CONV4** (same 4‑tap dot product). This leaves room to extend to a true 8‑tap form later without changing the existing encoding.

- **LMSSTEP (0x05)**  
  Implemented as an alias for MAC16: `rd = a0*b0 + a1*b1`.  
  The LMS weight update itself is expected to be done in software using this MAC as the inner product.

- **CMAC (0x06)**  
  `rs1 = ar + j*ai`, `rs2 = br + j*bi` (all signed 16‑bit).  
  - `real = ar*br − ai*bi`  
  - `imag = ar*bi + ai*br`  
  Each is saturated to signed 16‑bit using symmetric saturation, and packed as:  
  `rd = {imag16, real16}`.

- **ABS2 (0x07)**  
  From `rs1 = ar + j*ai` (signed 16‑bit):  
  `rd = ar*ar + ai*ai` (32‑bit signed).

- **CLIP16 (0x08)**  
  `rs1` holds data, `rs2.lo16` holds the clip limit `L` (signed).  
  The hardware uses `L = |rs2.lo16|` and clips each signed 16‑bit lane of `rs1` into `[-L, +L]`.  
  High 16 bits of `rs2` are ignored.

- **SHIFTN (0x09)**  
  `rs1` is a signed 32‑bit value, `rs2[4:0]` is the shift amount `s`.  
  - If `s == 0`: `rd = rs1`  
  - Else, arithmetic right shift with round to nearest:
    - For positive `x`: `rd = (x + 2^(s−1)) >>> s`
    - For negative `x`: `rd = −(((−x) + 2^(s−1)) >>> s)`

---

## C wrappers

All C wrappers live in `firmware/aux.h` and `firmware/aux.c`. They encode the AUX instructions as raw `.word` values using fixed registers:

- Wrapper convention (implemented in `aux.c`):
  - `a0` (x10) – used as both `rs1` and `rd` inside the instruction.
  - `a1` (x11) – used as `rs2` for two‑operand ops.
  - Wrappers move their arguments into `a0`/`a1`, emit the encoded instruction word, then move `a0` back into a C variable.

### Available helpers

- 16‑bit lane ops:
  - `uint32_t aux_mac16(uint32_t a, uint32_t b);`  
    - `a` and `b` pack `{hi16, lo16}` as signed 16‑bit channels.
  - `uint32_t aux_msub16(uint32_t a, uint32_t b);`
  - `uint32_t aux_abs16(uint32_t x);`
  - `uint32_t aux_clip16(uint32_t x, int16_t limit);`

- Convolution:
  - `uint32_t aux_conv4(uint32_t x_packed, uint32_t h_packed);`
  - `uint32_t aux_conv8(uint32_t x_packed, uint32_t h_packed);` (currently same as `aux_conv4`)
  - Pack 4 signed 8‑bit values into a 32‑bit word:
    - `x_packed = (uint8_t)x0 | ((uint32_t)(uint8_t)x1 << 8) | ...`

- Adaptive / LMS:
  - `uint32_t aux_lmsstep(uint32_t x_packed, uint32_t h_packed);`  
    - Currently identical to `aux_mac16`; you typically use it as the error‑*x* dot product inside an LMS loop.

- Complex operations:
  - `uint32_t aux_cmac(uint32_t x_complex, uint32_t h_complex);`  
    - `x_complex = (uint16_t)real | ((uint32_t)(uint16_t)imag << 16)`
  - `uint32_t aux_abs2(uint32_t x_complex);`

- Fixed‑point scaling:
  - `uint32_t aux_shiftn(uint32_t x, uint32_t shamt);`

### Example usage

Simple stereo MAC + magnitude + scaling (already in `main()`):

- Pack two samples:
  - `stereo_a = (int16_t)left | ((uint32_t)(int16_t)right << 16);`
  - `stereo_b` similarly.
- Compute:
  - `mac = aux_mac16(stereo_a, stereo_b);`
  - `mag2 = aux_abs2(stereo_a);`
  - `scaled = aux_shiftn(mac, 4);`

Clipping a stereo frame:

- `uint32_t y = aux_clip16(stereo_a, 12000);`  
  Clips both 16‑bit lanes of `stereo_a` to ±12000.

4‑tap 8‑bit convolution:

- Pack inputs:
  - `uint32_t x = (uint8_t)x0 | ((uint32_t)(uint8_t)x1 << 8) | ...;`
  - `uint32_t h = (uint8_t)h0 | ((uint32_t)(uint8_t)h1 << 8) | ...;`
- Use:
  - `int32_t acc = (int32_t)aux_conv4(x, h);`

---

## Building and running

- Build firmware (compiles `firmware/main.c` and links with `firmware/crt0.S`):
  - `make firmware/firmware.elf`
- Convert to hex and run the Icarus testbench:
  - `make test`
  - UART output is printed on stdout; PASS/FAIL is signaled via the memory‑mapped `PASS` register at `0x20000000` (see `testbench.v:260+`).

To use these instructions in your own SoC or top‑level:

- Instantiate `picorv32_axi` or `picorv32_wb` with `.ENABLE_AUDIO(1'b1)`.
- Ensure your firmware is linked for RV32I/M as usual; the toolchain sees AUX ops as opaque `.word` values in CUSTOM‑0 and does not require any ISA extension flags.
