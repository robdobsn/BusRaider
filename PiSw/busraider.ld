
ENTRY(_start)

MEMORY
{
    ram : ORIGIN = 0x8000, LENGTH = 512M
}

heap_size = 50M;
otaUpdateBufferSize = 10M;

SECTIONS
{
    .init : { *(.init) } > ram
    .text : { *(.text*) _etext = .; } > ram
    .rodata ALIGN(4096) : { *(.rodata*) } > ram

	.init_array ALIGN(4) : { 
        __init_start = .; 
        *(.init_array*)
        *(.ctors)
        __init_end = .; 
    } > ram

	.ARM.exidx : {
		__exidx_start = .;
		*(.ARM.exidx*)
		__exidx_end = .;
	} > ram

	.eh_frame : {
		*(.eh_frame*)
	} > ram

    .data ALIGN(4096): { *(.data*) } > ram

    /* Uninitialized data section */
    .bss ALIGN(4): {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        ._end = .;
    } > ram 

    . = ALIGN(4);
    __bss_end = .;
    __bss_size__ = __bss_end - __bss_start;

    .heap ALIGN(4): {
        _heap_start = .;
        . = . + heap_size;
        _heap_end = .;
    } > ram 

    .otaUpdateBuffer ALIGN(4): {
        _otaUpdateBufferStart = .;
        . = . + otaUpdateBufferSize;
        _otaUpdateBufferEnd = .;
    } > ram
    
    .ARM.attributes 0 : { *(.ARM.attributes) }

}

