#include <stdint.h>
#define UART ((volatile uint32_t*)0x10000000)
#define PASS ((volatile uint32_t*)0x20000000)

int main(void) {
    *UART = 'H';
    *UART = 'i';
    *UART = '\n';

    *PASS = 123456789;           // signal pass to testbench
    __asm__ volatile("ebreak");  // stop simulation
    while (1);
}