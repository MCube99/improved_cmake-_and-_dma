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
#include "keyboard_input.pio.h"

// -----------------------------------------------------------------------------
// STRUCTURES
// -----------------------------------------------------------------------------

typedef struct {
    PIO pio;
    uint sm;
    int dma_chan;
    dma_channel_config dma_cfg;

} pio_spi_t;

typedef struct {
    PIO pio;
    uint sm;

} pio_keyboard_t;

// -----------------------------------------------------------------------------
// GLOBALS
// -----------------------------------------------------------------------------

static pio_spi_t pio_spi;
static pio_keyboard_t pio_keyboard;

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
    // CSn LOW -> START DMA
    // -------------------------------------------------------------------------

    if (events & GPIO_IRQ_EDGE_FALL) {
        dma_start_channel_mask(1u << pio_spi.dma_chan);
        }
    

    // -------------------------------------------------------------------------
    // CSn HIGH -> PROCESS PACKET
    // -------------------------------------------------------------------------

    if (events & GPIO_IRQ_EDGE_RISE) {
        packet_type_t type = classify_packet();
        
        switch (type) {
        case PACKET_USB:
            usb_check = true;
            keyboard_check = false;
            if (spi_irq_disabled) 
            { 
                gpio_set_irq_active(PICO_DEFAULT_SPI_CSN_PIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
                spi_irq_disabled = false;
            }    
                break;

        case PACKET_KEYBOARD:


            usb_check = false;
            keyboard_check = true;

            gpio_set_irq_active(PICO_DEFAULT_SPI_CSN_PIN,GPIO_IRQ_EDGE_FALL |GPIO_IRQ_EDGE_RISE,false);
            spi_irq_disabled = true;
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
    gpio_init(PICO_DEFAULT_SPI_CSN_PIN);

    gpio_set_dir( PICO_DEFAULT_SPI_CSN_PIN, GPIO_IN);

    gpio_pull_up(PICO_DEFAULT_SPI_CSN_PIN);

    gpio_acknowledge_irq( PICO_DEFAULT_SPI_CSN_PIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE);

    gpio_set_irq_enabled( PICO_DEFAULT_SPI_CSN_PIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);

    irq_set_exclusive_handler( IO_IRQ_BANK0, my_gpio_isr);

    irq_set_enabled(IO_IRQ_BANK0, true);
}

// -----------------------------------------------------------------------------
// PIO + DMA SETUP
// -----------------------------------------------------------------------------

PUBLIC void pio_dma_setup(void)
{
    pio_spi.pio = pio0;

    uint offset = pio_add_program( pio_spi.pio, &clocked_input_program);

    pio_spi.sm = pio_claim_unused_sm( pio_spi.pio, true);

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

// -----------------------------------------------------------------------------
// KEYBOARD PIO
// -----------------------------------------------------------------------------

PUBLIC void pio_keyboard_setup(void)
{
    pio_keyboard.pio = pio0;

    pio_keyboard.sm =
        pio_claim_unused_sm(
            pio_keyboard.pio,
            true
        );

    uint offset = pio_add_program(
        pio_keyboard.pio,
        &keyboard_input_program
    );

    keyboard_input_program_init(
        pio_keyboard.pio,
        pio_keyboard.sm,
        offset,
        PICO_DEFAULT_SPI_SCK_PIN
    );
}

// -----------------------------------------------------------------------------
// ACCESSORS
// -----------------------------------------------------------------------------

PUBLIC PIO return_keyboard_pio(void)
{
    return pio_keyboard.pio;
}

PUBLIC uint return_keyboard_sm(void)
{
    return pio_keyboard.sm;
}

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