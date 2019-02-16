// Bus Raider
// Rob Dobson 2018

#include "uartMini.h"
#include "bare_metal_pi_zero.h"
#include "../System/ee_printf.h"
#include "irq.h"
#include "nmalloc.h"
#include "lowlib.h"
#include <stddef.h>

volatile unsigned int rxhead;
volatile unsigned int rxtail;
#define RXBUFMASK 0x7FFF
volatile unsigned char rxbuffer[RXBUFMASK + 1];

//------------------------------------------------------------------------
unsigned int uart_get_line_status(void)
{
    return (RD32(AUX_MU_LSR_REG));
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
        if (RD32(AUX_MU_LSR_REG) & 0x20)
            break;
    }
    WR32(AUX_MU_IO_REG, c);
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
    // Ensure we don't wait here forever
    for (int i=0; i < 100000; i++) {
        if (RD32(AUX_MU_LSR_REG) & 0x40)
            break;
    }
}

volatile int globalUartCount = 0;
volatile int globalUartDebug = 0;

void uart_irq_handler(__attribute__((unused)) void* data)
{
    unsigned int rb, rc;

    globalUartDebug++;

    // An interrupt has occurred, find out why
    while (1)
    {
        rb = RD32(AUX_MU_IIR_REG);
        if ((rb & 1) == 1)
            break; // No more interrupts
        if ((rb & 6) == 4) {
            // Receiver holds a valid byte
            rc = RD32(AUX_MU_IO_REG); // Read byte from rx fifo
            rxbuffer[rxhead] = rc & 0xFF;
            rxhead = (rxhead + 1) & RXBUFMASK;
            globalUartCount++;
        }
    }
}

void uart_init_irq()
{
    WR32(IRQ_ENABLE1, 1 << 29);
   //  enable_irq();
   irq_set_auxMiniUart_handler(uart_irq_handler, NULL);
    // irq_attach_handler(57, uart_irq_handler, NULL);
}

//------------------------------------------------------------------------
void uart_init(unsigned int baudRate, bool use_interrupts)
{
    unsigned int ra;

    WR32(AUX_ENABLES, 1);
    WR32(AUX_MU_IER_REG, 0);
    WR32(AUX_MU_CNTL_REG, 0);
    WR32(AUX_MU_LCR_REG, 3);
    WR32(AUX_MU_MCR_REG, 0);
    // Enable RX interrupt
    WR32(AUX_MU_IER_REG, 0x05);
    WR32(AUX_MU_IIR_REG, 0xC6);
    // WR32(AUX_MU_BAUD_REG,270);
    //WR32(AUX_MU_BAUD_REG, 33); // 921,600 baud
    WR32(AUX_MU_BAUD_REG, ((AUX_MU_CLOCK/(baudRate*8))-1));

    ra = RD32(GPFSEL1);
    ra &= ~(7 << 12); //gpio14
    ra |= 2 << 12; //alt5
    ra &= ~(7 << 15); //gpio15
    ra |= 2 << 15; //alt5
    WR32(GPFSEL1, ra);

    WR32(AUX_MU_CNTL_REG, 3);

    if (use_interrupts)
    {
        // Initialise the interrupt handler
        uart_init_irq();
    }
}
