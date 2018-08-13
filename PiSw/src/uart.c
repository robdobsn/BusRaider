// Bus Raider
// Rob Dobson 2018

#include "uart.h"
#include "bare_metal_pi_zero.h"
#include "ee_printf.h"
#include "irq.h"
#include "nmalloc.h"

volatile unsigned int rxhead;
volatile unsigned int rxtail;
#define RXBUFMASK 0x7FFF
volatile unsigned char rxbuffer[RXBUFMASK + 1];

//------------------------------------------------------------------------
unsigned int uart_lcr(void)
{
    return (R32(AUX_MU_LSR_REG));
}
//------------------------------------------------------------------------
unsigned int uart_read_byte(void)
{
    if (rxtail != rxhead) {
        unsigned int ch = rxbuffer[rxtail];
        rxtail = (rxtail + 1) & RXBUFMASK;
        return ch;
    }
    return 0;
}
//------------------------------------------------------------------------
unsigned int uart_poll(void)
{
    return rxtail != rxhead;
}
//------------------------------------------------------------------------
void uart_send(unsigned int c)
{
    while (1) {
        if (R32(AUX_MU_LSR_REG) & 0x20)
            break;
    }
    W32(AUX_MU_IO_REG, c);
}
//------------------------------------------------------------------------
void uart_write(const char* data, unsigned int size)
{
    for (unsigned int i = 0; i < size; i++)
        uart_send(data[i]);
}
//------------------------------------------------------------------------
void uart_write_str(const char* data)
{
    const char* pD = data;
    while (*pD)
        uart_send(*pD++);
}
//------------------------------------------------------------------------
void uart_purge(void)
{
    while (1) {
        if (R32(AUX_MU_LSR_REG) & 0x40)
            break;
    }
}
//------------------------------------------------------------------------
void uart_write_hex_u8(unsigned int d)
{
    //unsigned int ra;
    unsigned int rb;
    unsigned int rc;

    rb = 8;
    while (1) {
        rb -= 4;
        rc = (d >> rb) & 0xF;
        if (rc > 9)
            rc += 0x37;
        else
            rc += 0x30;
        uart_send(rc);
        if (rb == 0)
            break;
    }
    // uart_send(0x20);
}
//------------------------------------------------------------------------
void uart_write_hex_u32(unsigned int d)
{
    //unsigned int ra;
    unsigned int rb;
    unsigned int rc;

    rb = 32;
    while (1) {
        rb -= 4;
        rc = (d >> rb) & 0xF;
        if (rc > 9)
            rc += 0x37;
        else
            rc += 0x30;
        uart_send(rc);
        if (rb == 0)
            break;
    }
    // uart_send(0x20);
}
//------------------------------------------------------------------------
void uart_dump_mem(unsigned char* start_addr, unsigned char* end_addr)
{
    unsigned char* pAddr = start_addr;
    int linPos = 0;
    for (long i = 0; i < end_addr - start_addr; i++) {
        uart_write_hex_u8(*pAddr++);
        linPos++;
        if (linPos == 8)
            uart_write_str("\r\n");
        else
            uart_send(' ');
    }
    uart_write_str("\r\n");
}
//------------------------------------------------------------------------
void uart_load_ihex(void)
{
}
//------------------------------------------------------------------------
unsigned int uart_read_hex()
{
    return 0;
}

void uart_irq_handler(__attribute__((unused)) void* data)
{
    unsigned int rb, rc;

    //an interrupt has occurred, find out why
    while (1) //resolve all interrupts to uart
    {
        rb = R32(AUX_MU_IIR_REG);
        if ((rb & 1) == 1)
            break; //no more interrupts
        if ((rb & 6) == 4) {
            //receiver holds a valid byte
            rc = R32(AUX_MU_IO_REG); //read byte from rx fifo
            rxbuffer[rxhead] = rc & 0xFF;
            rxhead = (rxhead + 1) & RXBUFMASK;
        }
    }
}

void uart_init_irq()
{
    W32(IRQ_ENABLE1, 1 << 29);
    enable_irq();
    irq_uart_handler(uart_irq_handler);
}

//------------------------------------------------------------------------
void uart_init(unsigned int baudRate, bool use_interrupts)
{
    unsigned int ra;

    W32(AUX_ENABLES, 1);
    W32(AUX_MU_IER_REG, 0);
    W32(AUX_MU_CNTL_REG, 0);
    W32(AUX_MU_LCR_REG, 3);
    W32(AUX_MU_MCR_REG, 0);
    // Enable RX interrupt
    W32(AUX_MU_IER_REG, 0x05);
    W32(AUX_MU_IIR_REG, 0xC6);
    // W32(AUX_MU_BAUD_REG,270);
    //W32(AUX_MU_BAUD_REG, 33); // 921,600 baud
    W32(AUX_MU_BAUD_REG, ((AUX_MU_CLOCK/(baudRate*8))-1));

    ra = R32(GPFSEL1);
    ra &= ~(7 << 12); //gpio14
    ra |= 2 << 12; //alt5
    ra &= ~(7 << 15); //gpio15
    ra |= 2 << 15; //alt5
    W32(GPFSEL1, ra);

    W32(AUX_MU_CNTL_REG, 3);

    if (use_interrupts)
    {
        // Initialise the interrupt handler
        uart_init_irq();
    }
}
