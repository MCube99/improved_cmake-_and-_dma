#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

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
    //uint PIO_IRQc
    int dma_chan;
    dma_channel_config dma_cfg;
} pio_spi_t;

typedef struct {
    PIO pio;
    uint sm;
    uint8_t ch;
} pio_keyboard_t;

struct usb_payload {
    uint32_t size;
    uint32_t difference;
    BYTE buffer[BUF_LEN];
};

// -----------------------------------------------------------------------------
// GLOBALS
// -----------------------------------------------------------------------------

static pio_spi_t pio_spi;
static pio_keyboard_t pio_keyboard;
static struct usb_payload usbPayload;

PRIVATE uint8_t return_keyboard_characters();
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
    event_type_t current_event;
    
    // -------------------------------------------------------------------------
    // CSn LOW -> START DMA AND CHECK STAGES 

    if (events & GPIO_IRQ_EDGE_FALL) {
        dequeue_interrupts(&current_event);
        
        
        switch(current_event) {
            case EVENT_USB_DETECTED:
                break;
            case EVENT_KEYBOARD_DETECTED:
                if(keyboard_check){
                    uint32_t status = save_and_disable_interrupts();
                    pio_sm_put(return_keyboard_pio(),return_keyboard_sm(),(uint32_t)return_keyboard_characters());
                    restore_interrupts_from_disabled(status);
                }
                break;
            default:
                uint32_t status = save_and_disable_interrupts();
                current_event = EVENT_SIZE_PACKET_RECIEVED;
                enqueue_interrupts(current_event);
                restore_interrupts_from_disabled(status);
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
    gpio_acknowledge_irq(PICO_DEFAULT_SPI_CSN_PIN, GPIO_IRQ_EDGE_FALL);

    // Set ISR
    irq_set_exclusive_handler(IO_IRQ_BANK0, my_gpio_isr);

    // Enable GPIO interrupt on CSN
    gpio_set_irq_enabled(PICO_DEFAULT_SPI_CSN_PIN, GPIO_IRQ_EDGE_FALL, true);

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

    memset(usbPayload.buffer, 0, sizeof(usbPayload.buffer));

    clocked_input_program_init(
        pio_spi.pio,
        pio_spi.sm,
        offset,
        PICO_DEFAULT_SPI_RX_PIN,
        PICO_DEFAULT_SPI_CSN_PIN);

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
        usbPayload.buffer,
        &pio_spi.pio->rxf[pio_spi.sm],
        BUF_LEN,
        false
    );

}
PUBLIC void pio_keyboard_setup(void){
    PIO pio = pio0;
    uint sm = pio_claim_unused_sm(pio, true);
    pio_keyboard.pio = pio;
    pio_keyboard.sm = sm;
    uint offset = pio_add_program( pio, &keyboard_input_program);
    keyboard_input_program_init(pio,
        sm,
        offset,
        PICO_DEFAULT_SPI_TX_PIN,
        PICO_DEFAULT_SPI_CSN_PIN);
}


// -----------------------------------------------------------------------------
// BUFFER ACCESSORS
// -----------------------------------------------------------------------------

PUBLIC uint8_t *give_array_address(void) {
    return usbPayload.buffer;
}

PUBLIC uint8_t *give_array_address_for_file_writing(void) {
    return &usbPayload.buffer[1];
}

PUBLIC int get_buffer_size(void) {
    return usbPayload.buffer[0];
}

// -----------------------------------------------------------------------------
// PROCESSING FOR MAIN
// -----------------------------------------------------------------------------
PUBLIC void usb_processing_main(void) {
    dma_start_channel_mask(1u << return_channel());
}

PUBLIC void keyboard_processing_main(void) {
    uint8_t ch;
    event_type_t classify_event;
    if(dequeue_keyboard(&ch)){
        if(ch == '/r') {
            pio_keyboard.ch = 0;
            classify_event = EVENT_PROCESSED;
        }else{
            pio_keyboard.ch = ch;
            classify_event = EVENT_KEYBOARD_DETECTED;
            keyboard_check = true;
        } 
        enqueue_interrupts(classify_event);
    }else{
        keyboard_check = false;
    }
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

PUBLIC PIO return_keyboard_pio(void)
{
    return pio_keyboard.pio;
}

PUBLIC uint return_keyboard_sm(void)
{
    return pio_keyboard.sm;
}

PRIVATE uint8_t return_keyboard_characters(void)
{
    return pio_keyboard.ch;
}

PUBLIC void set_size(uint32_t size) {
    usbPayload.size = size;
}

PUBLIC uint32_t return_size(void) {
    return usbPayload.size;
}

PUBLIC int return_channel(void)
{
    return pio_spi.dma_chan;
}
