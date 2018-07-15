// Bus Raider V1.3

#include "pgm_config.h"
#include "utils.h"
#include "uart.h"
#include "timer.h"
#include "framebuffer.h"
#include "console.h"
#include "gfx.h"
#include "irq.h"
#include "dma.h"
#include "nmalloc.h"
#include "ee_printf.h"
#include "piwiring.h"
#include "../uspi\include\uspi\types.h"
#include "../uspi/include/uspi.h"
#include "busraider.h"

// int ledVal = 0;
// int ledCount = 0;
// int firstCharReceived = 0;
// uint32_t rdAddr = 0x8000;
// uint32_t wrAddr = 0x8000;
// int dataVal = 0;
// int errCount = 0;

extern unsigned int pheap_space;
extern unsigned int heap_sz;

#if ENABLED(SKIP_BACKSPACE_ECHO)
volatile unsigned int backspace_n_skip;
volatile unsigned int last_backspace_t;
#endif

static void _keypress_handler(const char* str )
{
    const char* c = str;
    char CR = 13;

    while( *c )
    {
         char ch = *c;
         //ee_printf("CHAR 0x%x\n",ch );

#if ENABLED(SEND_CR_LF)
        if( ch == 10 )
        {
            // Send CR first
            uart_write( &CR, 1 );

        }
#endif

#if ENABLED( SWAP_DEL_WITH_BACKSPACE )
        if( ch == 0x7F ) 
        {
            ch = 0x8;
        }
#endif

#if ENABLED( BACKSPACE_ECHO )
        if( ch == 0x8 )
            gfx_term_putstring( "\x7F" );
#endif

#if ENABLED(SKIP_BACKSPACE_ECHO)
        if( ch == 0x7F )
        {
            backspace_n_skip = 2;
            last_backspace_t = time_microsec();
        }
#endif
        uart_write( &ch, 1 ); 
        ee_printf("%02x\n", ch);
        ++c;
    }
} 

void initialize_framebuffer()
{
    usleep(10000);
    fb_release();

    unsigned char* p_fb=0;
    unsigned int fbsize;
    unsigned int pitch;

    unsigned int p_w = 640;
    unsigned int p_h = 480;
    unsigned int v_w = p_w;
    unsigned int v_h = p_h;

    fb_init( p_w, p_h, 
             v_w, v_h,
             8, 
             (void*)&p_fb, 
             &fbsize, 
             &pitch );

    fb_set_xterm_palette();

    if( fb_get_phisical_buffer_size( &p_w, &p_h ) != FB_SUCCESS )
    {
        //cout("fb_get_phisical_buffer_size error");cout_endl();
    }
    //cout("phisical fb size: "); cout_d(p_w); cout("x"); cout_d(p_h); cout_endl();

    usleep(10000);
    gfx_set_env( p_fb, v_w, v_h, pitch, fbsize ); 
    gfx_clear();
}

void main_loop()
{
    ee_printf("Waiting for UART data (921600,8,N,1)\n");

    char strb[2] = {0,0};
    while(1)
    {
        if (uart_poll())
        {
            // // Switch screen to terminal view
            // if (!firstCharReceived)
            // {
            //     gfx_term_putstring( "\x1B[2J" );
            //     firstCharReceived = 1;
            // }
            // Show char received
            strb[0] = uart_read_byte();
            gfx_term_putstring( strb );
        }

        timer_poll();

        // Blink LED
        // const int NUM_TEST_LOCS = 10;
        // ledCount++;
        // if (ledCount > 100000)
        // {
        //     ledCount = 0;
        //     ledVal = !ledVal;

        //     // Show edge detect
        //     ee_printf("Edge %08x\n", R32(GPEDS0));
        //     // Check bus ack initially
        //     if (br_bus_acknowledged())
        //     {
        //         ee_printf("BusAck Initially ????\n");
        //         continue;
        //     }
        //     // Request bus
        //     br_request_bus();
        //     // Wait for ack
        //     for (int i = 0; i < 100000; i++)
        //     {
        //         if (br_bus_acknowledged())
        //         {
        //             ee_printf("Ack\n");
        //             break;
        //         }
        //     }
        //     if (!br_bus_acknowledged())
        //     {
        //         br_release_control();
        //         ee_printf("Failed to acquire bus\n");
        //         continue;
        //     }

        //     // Take control
        //     br_take_control();

        //     for (int i = 0; i < NUM_TEST_LOCS; i++)
        //     {
        //         busSetAddr(((uint32_t)0x0100+i));
        //         busWriteData(i);
        //     }


        //     uint8_t readVals[NUM_TEST_LOCS];
        //     for (int i = 0; i < NUM_TEST_LOCS; i++)
        //     {
        //         busSetAddr(((uint32_t)0x0100)+i);
        //         readVals[i] = busReadData();
        //     }
        //     for (int i = 0; i < NUM_TEST_LOCS; i++)
        //     {
        //         ee_printf("%02x ",readVals[i]);
        //     }
        //     ee_printf(" %08x\n", R32(GPLEV0));
        //     // Release bus

        //     br_release_control();
        //     ee_printf("Edge2 %08x\n", R32(GPEDS0));

        // }

        // busSetAddr(wrAddr);
        // busWriteData(dataVal);

        // wrAddr++;
        // dataVal++;

        // if (wrAddr > 10)
        // {
        //     busSetAddr(rdAddr);
        //     uint8_t val = busReadData();
        //     if (val != (rdAddr & 0xff))
        //     {
        //         errCount++;
        //     }
        //     rdAddr++;
        // }
    }

}


void entry_point()
{
    // Heap init
    nmalloc_set_memory_area( (unsigned char*)( pheap_space ), heap_sz );

    // UART
    uart_init();

    // Frame buffer    
    initialize_framebuffer();

    // Initial message
    gfx_term_putstring( "\x1B[2J" ); // Clear screen
    gfx_set_bg(27);
    gfx_term_putstring( "\x1B[2K" ); // Render blue line at top
    ee_printf(" ===  PiGFX ===  v.%s\n", PIGFX_VERSION );
    gfx_term_putstring( "\x1B[2K" );
    //gfx_term_putstring( "\x1B[2K" ); 
    ee_printf(" Copyright (c) 2016 Filippo Bergamasco\n\n");
    gfx_set_bg(0);

    timers_init();

    // USB
    ee_printf("Initializing USB\n");
    if( USPiInitialize() )
    {
        ee_printf("Initialization OK!\n");
        ee_printf("Checking for keyboards...\n");

        if ( USPiKeyboardAvailable () )
        {
            USPiKeyboardRegisterKeyPressedHandler( _keypress_handler );
            gfx_set_fg(10);
            ee_printf("Keyboard found.\n");
            gfx_set_fg(15);
        }
        else
        {
            gfx_set_fg(9);
            ee_printf("No keyboard found.\n");
            gfx_set_fg(15);
        }
    }
    else
    {
        ee_printf("USB initialization failed.\n");
    }
    ee_printf("---------\n");

    // Bus raider setup
    br_init();

    // Start the main loop
    main_loop();
}