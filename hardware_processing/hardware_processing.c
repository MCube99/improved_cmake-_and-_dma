#include <stdio.h>
#include <stdbool.h>

#include "pico/stdlib.h"

#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/sync.h"
#include "hardware/structs/iobank0.h"

#include "hardware_processing.h"
#include "queue.h"

#include "clocked_input.pio.h"

// -----------------------------------------------------------------------------
// STRUCTURES
// -----------------------------------------------------------------------------

typedef struct {
    PIO pio;
    uint sm;
    int dma_chan;
    uint32_t size;
    uint pio_irq;
    dma_channel_config dma_cfg;
} pio_spi_t;

// -----------------------------------------------------------------------------
// GLOBALS
// -----------------------------------------------------------------------------

static pio_spi_t pio_spi;


// -----------------------------------------------------------------------------
// PIO ISR
// -----------------------------------------------------------------------------

PRIVATE void __not_in_flash_func(my_pio_isr)(void) {

    if(pio_interrupt_get(pio_spi.pio,0)) {
        pio_spi.size = pio_sm_get(pio_spi.pio, pio_spi.sm);
        size_byte_set = true;
        pio_interrupt_clear(pio_spi.pio, 0);
    }

    if(pio_interrupt_get(pio_spi.pio,1)) { // This is the one to use to detect whether its usb or keyboard transfer 
        pio_interrupt_clear(pio_spi.pio, 1);
        dma_start_channel_mask(1u << return_channel()); 
        usb_check = true;
    }

    if(pio_interrupt_get(pio_spi.pio,2)) { // making this for usb use in the future

    }
}

// -----------------------------------------------------------------------------
// GPIO SETUP
// -----------------------------------------------------------------------------

PUBLIC void set_gpio_pins(void)
{
    uint PIO_IRQ = PIO0_IRQ_0;
    set_pio_irq(PIO_IRQ);

    // CSN pin setup
    gpio_init(PICO_DEFAULT_SPI_CSN_PIN);
    gpio_set_dir(PICO_DEFAULT_SPI_CSN_PIN, GPIO_IN);
    gpio_pull_up(PICO_DEFAULT_SPI_CSN_PIN);

    // Map PIO pins
    pio_gpio_init(return_spi_pio(), PICO_DEFAULT_SPI_RX_PIN);
    pio_gpio_init(return_spi_pio(), PICO_DEFAULT_SPI_SCK_PIN);
    pio_gpio_init(return_spi_pio(), PICO_DEFAULT_SPI_TX_PIN);

    
    pio_set_irq0_source_mask_enabled(return_spi_pio(), 3840, true); // setting all 4 at once
    irq_set_exclusive_handler(get_pio_irq(), my_pio_isr);
    irq_set_enabled(PIO_IRQ, true); // Clear any pending interrupts FIRST
}

PUBLIC void testIRQPIO(uint pioNum) {
    PIO PIO_O = pioNum ? pio1 : pio0; //Selects the pio instance (0 or 1 for pioNUM)
    uint PIO_IRQ = pioNum ? PIO1_IRQ_0 : PIO0_IRQ_0;  // Selects the NVIC PIO_IRQ to use

    pio_spi.pio = PIO_O;
    pio_spi.pio_irq = PIO_IRQ;

		
    // Our assembled program needs to be loaded into this PIO's instruction
    // memory. This SDK function will find a location (offset) in the
    // instruction memory where there is enough space for our program. We need
    // to remember this location!
    uint offset = pio_add_program( pio_spi.pio, &clocked_input_program);

    // Find a free state machine on our chosen PIO (erroring if there are
    // none). Configure it to run our program, and start it, using the
    // helper function we included in our .pio file.
    uint SM = pio_claim_unused_sm(PIO_O, true);
    pio_spi.sm = SM;
    clocked_input_program_init(
        PIO_O,
        SM,
        offset,
        PICO_DEFAULT_SPI_RX_PIN);

//enables IRQ for the statemachine - setting IRQ0_INTE - interrupt enable register
    //pio_set_irq0_source_enabled(PIO_O, pis_interrupt0, true); // sets IRQ0
    //pio_set_irq0_source_enabled(PIO_O, pis_interrupt1, true); // sets IRQ1
	//pio_set_irq0_source_enabled(PIO_O, pis_interrupt2, true); // sets IRQ2
	//pio_set_irq0_source_enabled(PIO_O, pis_interrupt3, true); // sets IRQ3
  //*********or************	
    pio_sm_restart(PIO_O, SM);
	pio_set_irq0_source_mask_enabled(PIO_O, 3840, true); //setting all 4 at once
	
    irq_set_exclusive_handler(PIO_IRQ, my_pio_isr);  //Set the handler in the NVIC
    irq_set_enabled(PIO_IRQ, true);                    //enabling the PIO1_IRQ_0

}

// -----------------------------------------------------------------------------
// PIO + DMA SETUP
// ----------------------------------------------------------------------

PUBLIC void pio_dma_setup(void)
{
    pio_spi.dma_chan = dma_claim_unused_channel(true);
    pio_spi.dma_cfg = dma_channel_get_default_config(pio_spi.dma_chan);
    channel_config_set_transfer_data_size( &pio_spi.dma_cfg, DMA_SIZE_8);
    channel_config_set_read_increment( &pio_spi.dma_cfg, false);
    channel_config_set_write_increment( &pio_spi.dma_cfg, true);
    channel_config_set_dreq(
        &pio_spi.dma_cfg,
        pio_get_dreq(
            pio_spi.pio,
            pio_spi.sm,
            false // false for RX, true for TX. We are recieving data from the state machine, so we set this to false
        )
    );
    dma_channel_configure(
        pio_spi.dma_chan,
        &pio_spi.dma_cfg,
        give_array_address(),
        &pio_spi.pio->rxf[pio_spi.sm],
        BUF_LEN,
        false
    );
}




// -----------------------------------------------------------------------------
// ACCESSORS
// -----------------------------------------------------------------------------

PUBLIC PIO return_spi_pio(void)
{
    return pio_spi.pio;
}

PUBLIC uint return_spi_sm(void)
{
    return pio_spi.sm;
}

PUBLIC int return_channel(void)
{
    return pio_spi.dma_chan;
}

PUBLIC uint32_t return_size() 
{
    return pio_spi.size;
}

PUBLIC void set_size(uint32_t size) 
{
    pio_spi.size = size;
}

PUBLIC void set_pio_irq(uint pio_irq) 
{
     pio_spi.pio_irq = pio_irq;
}

PUBLIC uint get_pio_irq(void) 
{
    return pio_spi.pio_irq;
}
