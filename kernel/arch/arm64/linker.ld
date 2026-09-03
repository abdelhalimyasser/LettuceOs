/* SPDX-License-Identifier: Apache-2.0 */

ENTRY(_start)

SECTIONS
{
    . = 0x40080000;
    .text.boot : { KEEP(*(.text.boot)) }
    .text.exception ALIGN(2048) : { KEEP(*(.text.exception)) }
    .text : { *(.text*) }
    .rodata ALIGN(4096) : { *(.rodata*) }
    .data ALIGN(4096) : { *(.data*) }
    .bss (NOLOAD) : ALIGN(4096)
    {
        __bss_start = .;
        *(COMMON)
        *(.bss*)
        . = ALIGN(4096);
        __bss_end = .;
    }

    . = ALIGN(4096);
    __service_text_start = .;
    .service_text : { KEEP(*(.service_text*)) }
    . = ALIGN(4096);
    __service_text_end = .;
}