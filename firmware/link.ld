ENTRY(_start)

MEMORY {
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 32K
    RAM   (rw)  : ORIGIN = 0x00008000, LENGTH = 32K
}

SECTIONS {
    .text : {
        *(.text*)
        *(.rodata*)
    } > FLASH

    .data : {
        *(.data*)
    } > RAM AT > FLASH

    .bss : {
        *(.bss*)
        *(COMMON)
    } > RAM
}