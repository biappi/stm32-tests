MEMORY
{
    ram (rwx) : ORIGIN = 0x20000000, LENGTH = 64K
    rom (rx)  : ORIGIN = 0x08000000, LENGTH = 256K
}

INCLUDE libucmx_stm32f4.ld

