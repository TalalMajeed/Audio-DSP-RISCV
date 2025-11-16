ENTRY(_start)

MEMORY {
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 32K
    RAM   (rw)  : ORIGIN = 0x00008000, LENGTH = 32K
}

SECTIONS
{
    /* Code + read-only data in FLASH */
    .text :
    {
        KEEP(*(.text.start))
        *(.text*)
        *(.rodata*)
        *(.srodata*)
        . = ALIGN(4);       /* FIX 1 */
    } > FLASH

    /* Writable data: load image in FLASH, run in RAM */
    .data :
    {
        _data_start = .;
        *(.data*)
        _data_end = .;
        . = ALIGN(4);       /* FIX 2 */
    } > RAM AT > FLASH
    _data_lma_start = LOADADDR(.data);

    /* Uninitialized RAM */
    .bss (NOLOAD) :
    {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        _bss_end = .;
        . = ALIGN(4);       /* FIX 3 (optional but recommended) */
    } > RAM

    /* Stack grows downward from end of RAM */
    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
}

/* Ensure final ELF ends on 4-byte boundary */
. = ALIGN(4);                /* FIX 4 */