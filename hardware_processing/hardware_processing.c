#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "pico/stdlib.h"

#include "hardware/gpio.h"
#include "hardware/irq.h"
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
    uint num;
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

    uint32_t events =  gpio_get_irq_event_mask(PICO_SPI_CSN_PIN);
    gpio_acknowledge_irq(PICO_SPI_CSN_PIN,events); 

    if(keyboard_check){
        main_check = true;
        return;  // return early for keyboard since only want to crap out data. No need to dequeue interrupts
    }
    
    event_type_t event;
    dequeue_interrupts(&event);
    // -------------------------------------------------------------------------
    // CSn LOW -> START DMA AND CHECK STAGES 

    if (events & GPIO_IRQ_EDGE_FALL) {
        switch(event) {
            case EVENT_USB_PROCESSING:
                enqueue_interrupts(EVENT_USB_PROCESSING);
                break;
            case EVENT_KEYBOARD_DETECTED:
                enqueue_interrupts(EVENT_KEYBOARD_DETECTED);
                break;
            case EVENT_FILE_PROCESSING:
                enqueue_interrupts(EVENT_FILE_PROCESSING);
                break; 
            default:
                enqueue_interrupts(EVENT_SIZE_PACKET_RECIEVED);
                break;
        }
        
    }

    main_check = true;
}


// -----------------------------------------------------------------------------
// GPIO SETUP
// -----------------------------------------------------------------------------

PUBLIC void set_gpio_pins(void)
{
    // CSN pin setup
    gpio_init(PICO_SPI_CSN_PIN);
    gpio_set_dir(PICO_SPI_CSN_PIN, GPIO_IN);
    gpio_pull_up(PICO_SPI_CSN_PIN);

    gpio_init(PICO_DEFAULT_SPI_KEYBOARD_PIN); 
    gpio_set_dir(PICO_DEFAULT_SPI_KEYBOARD_PIN, true);
    gpio_set_function(PICO_DEFAULT_SPI_KEYBOARD_PIN, GPIO_FUNC_SIO); 
    gpio_pull_down(PICO_DEFAULT_SPI_KEYBOARD_PIN);

    // Map PIO pins
    pio_gpio_init(return_spi_pio(), PICO_SPI_RX_PIN );
    pio_gpio_init(return_spi_pio(), PICO_SPI_SCK_PIN);
    pio_gpio_init(return_spi_pio(), PICO_SPI_TX_PIN);

    pio_sm_clear_fifos(return_spi_pio(), return_spi_sm());
    pio_sm_clear_fifos(return_keyboard_pio(), return_keyboard_sm());
    // Clear any pending interrupts FIRST
    gpio_acknowledge_irq(PICO_SPI_CSN_PIN, GPIO_IRQ_EDGE_FALL);

    // Set ISR
    irq_set_exclusive_handler(IO_IRQ_BANK0, my_gpio_isr);

    // Enable GPIO interrupt on CSN
    gpio_set_irq_enabled(PICO_SPI_CSN_PIN, GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE, true);

    // Enable IRQ bank
    irq_set_enabled(IO_IRQ_BANK0, true);
}




// -----------------------------------------------------------------------------
// PIO + DMA SETUP
// -----------------------------------------------------------------------------

PUBLIC void pio_dma_setup(void)
{
    PIO pio = pio0;
    uint sm = pio_claim_unused_sm(pio, true);
    pio_spi.pio = pio;
    pio_spi.sm = sm;

    uint offset = pio_add_program( pio_spi.pio, &clocked_input_program);

    memset(usbPayload.buffer, 0, sizeof(usbPayload.buffer));
    pio_sm_clear_fifos(pio_spi.pio, pio_spi.sm);
    pio_sm_restart(pio_spi.pio, pio_spi.sm);
    clocked_input_program_init(
        pio_spi.pio,
        pio_spi.sm,
        offset,
        PICO_SPI_RX_PIN ,
        PICO_SPI_CSN_PIN);


}

PUBLIC inline void dma_setup(uint32_t size){
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
        dma_encode_transfer_count(size), // get this every time when classify packet is run.
        false
    );
    
}

PUBLIC void pio_keyboard_setup(void){
    PIO pio = pio0;
    uint sm = pio_claim_unused_sm(pio, true);
    pio_keyboard.pio = pio;
    pio_keyboard.sm = sm;
    uint offset = pio_add_program( pio, &keyboard_input_program);
    pio_sm_clear_fifos(pio_keyboard.pio, pio_keyboard.sm);
    pio_sm_restart(pio_keyboard.pio, pio_keyboard.sm);
    keyboard_input_program_init(pio,
        sm,
        offset,
        PICO_SPI_SCK_PIN,
        PICO_DEFAULT_SPI_KEYBOARD_PIN);
        
}

// -----------------------------------------------------------------------------
// BUFFER ACCESSORS
// -----------------------------------------------------------------------------

PUBLIC uint8_t *const give_array_address(void) {
    return usbPayload.buffer;
}

PUBLIC uint8_t *const give_array_address_for_file_writing(void) {
    return &usbPayload.buffer[1];
}

PUBLIC int get_buffer_size(void) {
    return usbPayload.buffer[0];
}

// -----------------------------------------------------------------------------
// PROCESSING FOR MAIN
// -----------------------------------------------------------------------------
PUBLIC bool usb_processing_main(void) {
    dma_start_channel_mask(1u << return_channel());
    while (!(dma_hw->intr & 1u << return_channel()))
        tight_loop_contents();
    dma_hw->ints0 = 1u << return_channel();
    
    uintptr_t base = (uintptr_t)usbPayload.buffer;
    uintptr_t write = dma_hw->ch[return_channel()].write_addr;
    uint32_t difference = write - base;

    if(difference ==  return_size())
    {
        enqueue_interrupts(EVENT_FILE_PROCESSING);
        return(true);
    }
    else
    {
        enqueue_interrupts(EVENT_NONE);
        return(false);
    }
}

PUBLIC void keyboard_processing_main() {
    uint8_t ch = 0; 
    event_type_t classify_event;
    if(pio_sm_is_tx_fifo_full(return_keyboard_pio(), return_keyboard_sm())){
        pio_sm_drain_tx_fifo(return_keyboard_pio(), return_keyboard_sm());
    }
    if(dequeue_keyboard(&ch)){
        pio_sm_put_blocking(return_keyboard_pio(), return_keyboard_sm(),ch);
        if(ch == '\r'){
            gpio_put(PICO_SPI_KEYBOARD_PIN, 1);
            classify_event = EVENT_DONE;
            enqueue_interrupts(classify_event);
            return; // Simply return early
        }
    }

    classify_event = EVENT_KEYBOARD_DETECTED; 
    pio_keyboard.num = pio_sm_get_tx_fifo_level(return_keyboard_pio(), return_keyboard_sm());
    enqueue_interrupts(classify_event);
}

        


PUBLIC void event_processing_main() {
    if(pio_interrupt_get(return_keyboard_pio(),1)){
        gpio_put(PICO_SPI_KEYBOARD_PIN, 0);
        keyboard_check = false;
    }

    if(pio_interrupt_get(return_spi_pio(), 2)){
        pio_interrupt_clear(return_spi_pio(), 2);
    }
    enqueue_interrupts(EVENT_NONE);

}
               
// -----------------------------------------------------------------------------
// ACCESSORS
// -----------------------------------------------------------------------------

PUBLIC PIO const return_spi_pio(void)
{
    return pio_spi.pio;
}

PUBLIC uint const return_spi_sm(void)
{
    return pio_spi.sm;
}

PUBLIC PIO const return_keyboard_pio(void)
{
    return pio_keyboard.pio;
}

PUBLIC uint const return_keyboard_sm(void)
{
    return pio_keyboard.sm;
}


PUBLIC void set_size(uint32_t size) 
{
    usbPayload.size = size;
}

PUBLIC uint32_t return_size(void) 
{
    return usbPayload.size;
}

PUBLIC int return_channel(void)
{
    return pio_spi.dma_chan;
}
