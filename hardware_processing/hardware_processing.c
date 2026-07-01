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
#include "hardware/spi.h"
#include "queue.h"
#include "pico/binary_info.h"

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
    uint8_t num;
} pio_keyboard_t;


// -----------------------------------------------------------------------------
// GLOBALS
// -----------------------------------------------------------------------------

static pio_spi_t pio_spi;
static pio_keyboard_t pio_keyboard;


// -----------------------------------------------------------------------------
// GPIO ISR
// -----------------------------------------------------------------------------

PRIVATE void __not_in_flash_func(my_gpio_isr)(void) {    

    uint32_t events =  gpio_get_irq_event_mask(PICO_SPI_CSN_PIN);
    gpio_acknowledge_irq(PICO_SPI_CSN_PIN,events); 

    // -------------------------------------------------------------------------

    if (events & GPIO_IRQ_EDGE_FALL) {
        if(first_check){ // If hitting IRQ first time
            enqueue_interrupts(EVENT_SIZE_PACKET_RECIEVED);
            first_check = false; // No longer first check so false
        }

    main_check = true; // set main check to true so that the main loop will run. This is to prevent the main loop from running when there is no event to process.
}
}


// -----------------------------------------------------------------------------
// GPIO SETUP
// -----------------------------------------------------------------------------

PUBLIC void set_gpio_pins(void) {
    // CSN pin setup
    gpio_init(PICO_SPI_CSN_PIN);
    gpio_set_dir(PICO_SPI_CSN_PIN, GPIO_IN);
    gpio_pull_up(PICO_SPI_CSN_PIN);

    gpio_init(PICO_SPI_KEYBOARD_PIN); 
    gpio_set_dir(PICO_SPI_KEYBOARD_PIN, true);
    gpio_set_function(PICO_SPI_KEYBOARD_PIN, GPIO_FUNC_SIO); 
    gpio_put(PICO_SPI_KEYBOARD_PIN,0);


    pio_sm_clear_fifos(return_spi_pio(), return_spi_sm()); // Cleans out any FIFOS
    pio_sm_clear_fifos(return_keyboard_pio(), return_keyboard_sm());
    pio_sm_drain_tx_fifo(return_keyboard_pio(), return_keyboard_sm());
    // Clear any pending interrupts FIRST
    gpio_acknowledge_irq(PICO_SPI_CSN_PIN, GPIO_IRQ_EDGE_FALL);

    // Set ISR
    irq_set_exclusive_handler(IO_IRQ_BANK0, my_gpio_isr);

    // Enable GPIO interrupt on CSN
    gpio_set_irq_enabled(PICO_SPI_CSN_PIN, GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE, true);

    // Enable IRQ bank
    irq_set_enabled(IO_IRQ_BANK0, true);
}
#ifdef SPI_DEBUG
PUBLIC void spi_write(){
    spi_init(spi_default, 1000 * 1000);
    spi_set_slave(spi_default, true);
    gpio_set_function(PICO_SPI_RX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_SPI_TX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_SPI_CSN_PIN, GPIO_FUNC_SPI);
    // Make the SPI pins available to picotool
    bi_decl(bi_4pins_with_func(PICO_SPI_RX_PIN, PICO_SPI_TX_PIN, PICO_SPI_SCK_PIN, PICO_SPI_CSN_PIN, GPIO_FUNC_SPI));

    uint8_t out_buf[BUF_LEN], in_buf[BUF_LEN];

    // Initialize output buffer
    for (size_t i = 0; i < BUF_LEN; ++i) {
        // bit-inverted from i. The values should be: {0xff, 0xfe, 0xfd...}
        out_buf[i] = ~i;}

    uint8_t ch = 0x1F; 
    uint32_t num = 0;
    event_type_t classify_event;
    if(dequeue_keyboard(&ch)){
        spi_write_blocking(spi_default, &ch, 1);
    }

    classify_event = EVENT_KEYBOARD_DETECTED; 
    enqueue_interrupts(classify_event);
}
#endif 
// -----------------------------------------------------------------------------
// PIO + DMA SETUP
// -----------------------------------------------------------------------------

PUBLIC void pio_dma_setup(void) {
    PIO pio = pio0;
    uint sm = pio_claim_unused_sm(pio, true);
    pio_spi.pio = pio;
    pio_spi.sm = sm;

    uint offset = pio_add_program( pio_spi.pio, &clocked_input_program);

    memset(give_array_address(), 0, BUF_LEN);
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
        pio_get_dreq( // get data request
            pio_spi.pio,
            pio_spi.sm,
            false
        )
    );
    dma_channel_configure(
        pio_spi.dma_chan,
        &pio_spi.dma_cfg,
        give_array_address(), // buffer to write to 
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
    keyboard_input_program_init(pio,
        sm,
        offset,
        PICO_SPI_SCK_PIN,
        PICO_SPI_TX_PIN,
        PICO_SPI_CSN_PIN);
        
}

// -----------------------------------------------------------------------------
// PROCESSING PLACE
// -----------------------------------------------------------------------------

PUBLIC void classify_packet(void) {

    uint32_t size;
    event_type_t classify_event = 0;
    if(!pio_sm_is_rx_fifo_empty(return_spi_pio(), return_spi_sm())){
        size = pio_sm_get(return_spi_pio(), return_spi_sm());
    }
    else{
        size = 0;
    }
    set_size(size);

    if ((size == GARY_CODE || size == GARY_CODE - 1 || size == GARY_CODE + 1 || size == 0)) { // edge cases where due to data transmission there could be wrong things

        keyboard_check = true; // need to save and disable interrupts so that the write i not interrupted.
        classify_event = EVENT_KEYBOARD_DETECTED;
    }

    else if(size >32 && size < GARY_CODE) {

        if(pio_interrupt_get(return_spi_pio(), 0)){
            pio_interrupt_clear(return_spi_pio(),0);
        }
        pio_sm_put(return_spi_pio(),return_spi_sm(),size);
        dma_setup(size); //pass size to dma setup so it can set up transfer size
        classify_event = EVENT_USB_PROCESSING;
    } 
    else{
        classify_event = EVENT_NONE; //Invalid size, so just ignore it and do nothing. This is to prevent the system from crashing due to invalid sizes.
        first_check = true; // reset the first check so that the next time the ISR triggers, it will enqueue the EVENT SIZE PACKET RECIEVED event. This is to prevent the system from getting stuck in an invalid state due to invalid sizes.
    }

    

    enqueue_interrupts(classify_event);
}


// -----------------------------------------------------------------------------
// PROCESSING FOR MAIN
// -----------------------------------------------------------------------------
PUBLIC bool usb_processing_main(void) {
    dma_start_channel_mask(1u << return_channel()); // start the DMA transfer
    dma_channel_wait_for_finish_blocking(return_channel()); // Wait for the DMA transfer to complete

        // 1. Pause the DMA channel
    hw_clear_bits(&dma_hw->ch[return_channel()].ctrl_trig, DMA_CH0_CTRL_TRIG_EN_BITS);

        // 2. Now that the DMA channel is paused, we can safely read the write address

    uintptr_t base = (uintptr_t)give_array_address();
    uintptr_t write = dma_hw->ch[return_channel()].write_addr;
    uint32_t difference = write - base;

    if(difference ==  return_size())
    {
        enqueue_interrupts(EVENT_FILE_PROCESSING);
        dma_channel_cleanup(return_channel());
        dma_channel_unclaim(return_channel());
        return(true);
    }
    else
    {
        enqueue_interrupts(EVENT_NONE);
        return(false);
    }
}

PUBLIC bool keyboard_processing_main() {
    uint8_t ch = 0x1F; 
    event_type_t classify_event;

    if(pio_interrupt_get(return_keyboard_pio(),1)){
        pio_interrupt_clear(return_keyboard_pio(),1);
    }
    if(!pio_sm_is_tx_fifo_full(return_keyboard_pio(), return_keyboard_sm())){ // guarrd condition to check if the tx fifo is full or not. If it is full, then it will not be able to put any more data into it, so it will just return false and not do anything. This is to prevent the system from crashing due to invalid sizes.
        if(dequeue_keyboard(&ch)){ //this is the ebent that is triggered when the keyboard is pressed. It will dequeue the letter from the keyboard queue and put it into the tx fifo of the pio. This is to prevent the system from crashing due to invalid sizes.
            pio_sm_put(return_keyboard_pio(), return_keyboard_sm(), ((uint)ch<<24)); 
        }
    }
    if(ch == '\r'){
        classify_event = EVENT_DONE;
        enqueue_interrupts(classify_event);
        return(true); // Simply return early
    }
    classify_event = EVENT_KEYBOARD_DETECTED; 
    enqueue_interrupts(classify_event);
    return(false);
    }


        


PUBLIC bool event_processing_main() {
    if(pio_interrupt_get(return_keyboard_pio(),1)){
        if(pio_interrupt_get(return_spi_pio(), 0)){
            pio_interrupt_clear(return_spi_pio(), 0);
        }
        pio_sm_put(return_spi_pio(),return_spi_sm(), 0); // to reset it
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

PUBLIC int return_channel(void)
{
    return pio_spi.dma_chan;
}
