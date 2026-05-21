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

#include "edge_detector.pio.h"
// -----------------------------------------------------------------------------
// STRUCTURES
// -----------------------------------------------------------------------------

typedef struct {
    PIO pio;
    uint sm;
    int dma_chan;
    uint8_t size;
    dma_channel_config dma_cfg;
} pio_spi_t;

// -----------------------------------------------------------------------------
// GLOBALS
// -----------------------------------------------------------------------------

static pio_spi_t pio_spi;

static volatile bool spi_irq_disabled = false;

// -----------------------------------------------------------------------------
// GPIO IRQ CONTROL
// -----------------------------------------------------------------------------

PUBLIC void gpio_set_irq_active(uint gpio, uint32_t events, bool enabled) {
    io_bank0_irq_ctrl_hw_t *irq_ctrl_base = get_core_num() ?  
    &io_bank0_hw->proc1_irq_ctrl : &io_bank0_hw->proc0_irq_ctrl;
    io_rw_32 *en_reg = &irq_ctrl_base->inte[gpio / 8];
    events <<= 4 * (gpio % 8);
    if (enabled)
    {
        hw_set_bits(en_reg, events);
    }
    else
    {
        hw_clear_bits(en_reg, events);
    }
}

// -----------------------------------------------------------------------------
// GPIO ISR
// -----------------------------------------------------------------------------

PRIVATE void __not_in_flash_func(my_gpio_isr)(void) {
    uint32_t events =  gpio_get_irq_event_mask(PICO_DEFAULT_SPI_CSN_PIN);

    gpio_acknowledge_irq(PICO_DEFAULT_SPI_CSN_PIN,events);

    // -------------------------------------------------------------------------
    // CSn LOW -> START DMA AND CHECK STAGES 
    // -------------------------------------------------------------------------

    if (events & GPIO_IRQ_EDGE_FALL) {

        if(current_packet == PACKET_NONE) {
            current_packet = PACKET_START;
        }

        if(current_packet == PACKET_USB) {
            dma_start_channel_mask(1u << return_channel()); 
        }


    }

    // -------------------------------------------------------------------------
    // CSn HIGH -> PROCESS PACKET
    // -------------------------------------------------------------------------

    if (events & GPIO_IRQ_EDGE_RISE) {
        
        switch (current_packet) {
        case PACKET_USB:
            keyboard_check = false;
            break;

        case PACKET_KEYBOARD:
            usb_check = false;
            keyboard_check = true;
            break;

        case PACKET_NONE:
        default:
            break;
        }
    }
}


// -----------------------------------------------------------------------------
// GPIO SETUP
// -----------------------------------------------------------------------------

PUBLIC void set_gpio_pins(void)
{
    // CSN pin setup
    gpio_init(PICO_DEFAULT_SPI_CSN_PIN);
    gpio_set_dir(PICO_DEFAULT_SPI_CSN_PIN, GPIO_IN);
    gpio_pull_up(PICO_DEFAULT_SPI_CSN_PIN);

    // Map PIO pins
    pio_gpio_init(return_spi_pio(), PICO_DEFAULT_SPI_RX_PIN);
    pio_gpio_init(return_spi_pio(), PICO_DEFAULT_SPI_SCK_PIN);
    pio_gpio_init(return_spi_pio(), PICO_DEFAULT_SPI_TX_PIN);
    

    // Clear any pending interrupts FIRST
    gpio_acknowledge_irq(PICO_DEFAULT_SPI_CSN_PIN,
        GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE);

    // Set ISR
    irq_set_exclusive_handler(IO_IRQ_BANK0, my_gpio_isr);

    // Enable GPIO interrupt on CSN
    gpio_set_irq_enabled(PICO_DEFAULT_SPI_CSN_PIN,
        GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE,
        true);

    // Enable IRQ bank
    irq_set_enabled(IO_IRQ_BANK0, true);
}

// -----------------------------------------------------------------------------
// PIO + DMA SETUP
// -----------------------------------------------------------------------------

PUBLIC void pio_dma_setup(void)
{
    pio_spi.pio = pio0;

    pio_spi.sm = pio_claim_unused_sm( pio_spi.pio, true);

    uint offset = pio_add_program( pio_spi.pio, &clocked_input_program);


    clocked_input_program_init(
        pio_spi.pio,
        pio_spi.sm,
        offset,
        PICO_DEFAULT_SPI_RX_PIN);

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
            false
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

PUBLIC void read_first_byte()
{
    dma_start_channel_mask(1u << return_first_byte_channel()); 
}


// -----------------------------------------------------------------------------

// SYNC PIO MANAGEMENT

PUBLIC void pio_sync_setup(void)
{
    uint offset = pio_add_program(pio0, &csn_edge_program);
    uint sm = pio_claim_unused_sm(pio0, true);

    pio_sm_config c = csn_edge_program_get_default_config(offset);

    sm_config_set_in_pins(&c, PICO_DEFAULT_SPI_CSN_PIN );
    pio_gpio_init(pio0, PICO_DEFAULT_SPI_CSN_PIN   );
    
    gpio_set_dir(PICO_DEFAULT_SPI_CSN_PIN, GPIO_IN);

    pio_sm_init(pio0, sm, offset, &c);

// Start SM
    pio_sm_set_enabled(pio0, sm, true);
}
// -----------------------------------------------------------------------------


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



PUBLIC void set_size(uint32_t size) {
    pio_spi.size = size;
}