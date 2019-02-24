// Bus Raider
// Rob Dobson 2018

#include "uartMini.h"
#include "BCM2835.h"
#include "irq.h"
#include "nmalloc.h"
#include "lowlev.h"
#include "lowlib.h"
#include <stddef.h>

volatile unsigned int UartMini::rxhead;
volatile unsigned int UartMini::rxtail;
volatile unsigned char UartMini::rxbuffer[RXBUFMASK + 1];

UartMini::UartMini()
{

}

UartMini::~UartMini()
{

}

//------------------------------------------------------------------------
// unsigned int UartMini::uart_get_line_status(void)
// {
//     return (RD32(AUX_MU_LSR_REG));
// }
// //------------------------------------------------------------------------

int UartMini::read()
{
    if (rxtail != rxhead) {
        int ch = rxbuffer[rxtail];
        rxtail = (rxtail + 1) & RXBUFMASK;
        return ch;
    }
    return 0;
}

//------------------------------------------------------------------------
bool UartMini::poll(void)
{
    return rxtail != rxhead;
}

//------------------------------------------------------------------------
int UartMini::write(unsigned int c)
{
    while (1) {
        if (RD32(ARM_AUX_MU_LSR_REG) & 0x20)
            break;
    }
    WR32(ARM_AUX_MU_IO_REG, c);
    return 1;
}

//------------------------------------------------------------------------
void UartMini::write(const char* data, unsigned int size)
{
    for (unsigned int i = 0; i < size; i++)
        write(data[i]);
}

//------------------------------------------------------------------------
void UartMini::writeStr(const char* data)
{
    const char* pD = data;
    while (*pD)
        write(*pD++);

}
//------------------------------------------------------------------------
void UartMini::clear(void)
{
    // Ensure we don't wait here forever
    for (int i=0; i < 100000; i++) {
        if (RD32(ARM_AUX_MU_LSR_REG) & 0x40)
            break;
    }
}

void UartMini::isrStatic(void* pParam)
{
	UartMini *pStatic = (UartMini*) pParam;
	pStatic->isr();
}

void UartMini::isr()
{
    unsigned int rb, rc;

    // An interrupt has occurred, find out why
    while (1)
    {
        rb = RD32(ARM_AUX_MU_IIR_REG);
        if ((rb & 1) == 1)
            break; // No more interrupts
        if ((rb & 6) == 4) {
            // Receiver holds a valid byte
            rc = RD32(ARM_AUX_MU_IO_REG); // Read byte from rx fifo
            rxbuffer[rxhead] = rc & 0xFF;
            rxhead = (rxhead + 1) & RXBUFMASK;
        }
    }
}

//------------------------------------------------------------------------
bool UartMini::setup(unsigned int baudRate, [[maybe_unused]]int rxBufSize, [[maybe_unused]]int txBufSize)
{
    unsigned int ra;

    WR32(ARM_AUX_ENABLE, 1);
    WR32(ARM_AUX_MU_IER_REG, 0);
    WR32(ARM_AUX_MU_CNTL_REG, 0);
    WR32(ARM_AUX_MU_LCR_REG, 3);
    WR32(ARM_AUX_MU_MCR_REG, 0);
    // Enable RX interrupt
    WR32(ARM_AUX_MU_IER_REG, 0x05);
    WR32(ARM_AUX_MU_IIR_REG, 0xC6);
    // WR32(AUX_MU_BAUD_REG,270);
    //WR32(AUX_MU_BAUD_REG, 33); // 921,600 baud
    WR32(ARM_AUX_MU_BAUD_REG, ((ARM_AUX_MU_CLOCK/(baudRate*8))-1));

    ra = RD32(ARM_GPIO_GPFSEL1);
    ra &= ~(7 << 12); //gpio14
    ra |= 2 << 12; //alt5
    ra &= ~(7 << 15); //gpio15
    ra |= 2 << 15; //alt5
    WR32(ARM_GPIO_GPFSEL1, ra);

    WR32(ARM_AUX_MU_CNTL_REG, 3);

    // // Initialise the interrupt handler
    WR32(ARM_IC_ENABLE_IRQS_1, 1 << ARM_IRQ_AUX);
    lowlev_enable_irq();
    irq_set_auxMiniUart_handler(isrStatic, NULL);
    // irq_attach_handler(57, uart_irq_handler, NULL);
    return true;
}
