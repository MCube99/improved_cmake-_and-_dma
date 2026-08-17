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
#include "keyboard_output.pio.h"
//#include "read_master.pio.h"

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
    uint8_t cs_pin;
    uint offset;
} pio_keyboard_t;

typedef struct {
    PIO pio;
    uint sm;
    uint8_t ch;
} pio_read_master_t;

#define PICO_START          2
#define PICO_SPI_RX_PIN   ((PICO_START)      + 0)   // 2 
#define PICO_SPI_SCK_PIN  ((PICO_SPI_RX_PIN) + 1)   //3            // GPIO pin for SPI clock, same as master
#define PICO_SPI_CSN_PIN   ((PICO_SPI_RX_PIN) + 2)   //4             // GPIO pin for SPI chip select
#define PICO_SPI_TX_PIN  ((PICO_SPI_RX_PIN) + 3)   //5             // GPIO pin for SPI data to master → slave
#define PICO_SPI_JMP_PIN ((PICO_SPI_RX_PIN) + 5)   // 7 

// -----------------------------------------------------------------------------
// GLOBALS
// -----------------------------------------------------------------------------

static pio_spi_t pio_spi;
static pio_keyboard_t pio_keyboard = {.cs_pin = PICO_SPI_CSN_PIN};
static pio_keyboard_t const *spi = &pio_keyboard; // pointer to the pio_keyboard struct, so that it can be used in the pio_spi_read8_blocking function
static pio_read_master_t pio_read_master;



void __time_critical_func(pio_spi_read8_blocking)(const pio_keyboard_t *spi, uint8_t *dst, size_t len);
// -----------------------------------------------------------------------------
// GPIO ISR
// -----------------------------------------------------------------------------

PRIVATE void __not_in_flash_func(my_gpio_isr)(void) {    
    static uint8_t count = 0; // This is the static variable that holds memory, and will be used to determine ISR tiggering times etc
    uint32_t events =  gpio_get_irq_event_mask(PICO_SPI_CSN_PIN);
    gpio_acknowledge_irq(PICO_SPI_CSN_PIN,events); 
    main_check = true; // set main check to true so that the main loop will run. This is to prevent the main loop from running when there is no event to process.

    // -------------------------------------------------------------------------
    if (events & GPIO_IRQ_EDGE_FALL ) {

        if(keyboard_check ){
            return; // early return if its keyboard mode or the first official csn for theu sb, since the first proper one is to do with the adc and not the usb.
        }

        if(count == 1 && first_check ){ // If hitting IRQ second time. Should only happen once, so this is a guard condition to prevent it from happening again. Only exception is if its set so by the code
            count = 0; // reset count to 0 so that the next time the ISR triggers, it will enqueue the EVENT SIZE PACKET RECIEVED event. This is to prevent the system from getting stuck in an invalid state due to invalid sizes.
            enqueue_interrupts(EVENT_SIZE_PACKET_RECIEVED); // starts the whole thing off. 
            first_check = false; // set to false so that the ISR will not enqueue the event again. This is to prevent the ISR from enqueing the event when the csn is meant for the DAC. We share the same csn pin for both the DAC and the SPI, so when the DAC is being used, the csn will be low, and this ISR will trigger. This is to prevent that from happening.
        }
        if(count > 1){
            count = 0; // reset count to 0 so that the next time the ISR triggers, it will enqueue the EVENT SIZE PACKET RECIEVED event. This is to prevent the system from getting stuck in an invalid state due to invalid sizes.
        }
    }
    ++count; // goes from 0 to 1, showing first round through the isr
}

// -----------------------------------------------------------------------------
// GPIO SETUP
// -----------------------------------------------------------------------------

PUBLIC void set_gpio_pins(void) {
    // CSN pin setup
    gpio_init(PICO_SPI_CSN_PIN);
    gpio_set_dir(PICO_SPI_CSN_PIN, GPIO_IN);
    gpio_pull_up(PICO_SPI_CSN_PIN);
// The jmp pin in PIO will be controlled by the PIO and will go high when a keyboard enter is detected. Befire that, itll just be 0
    gpio_init(PICO_SPI_JMP_PIN); 
    gpio_set_dir(PICO_SPI_JMP_PIN, true);
    gpio_set_function(PICO_SPI_JMP_PIN, GPIO_FUNC_SIO); 
    gpio_put(PICO_SPI_JMP_PIN,0);

// inputs are always available to PIO and SIO, regardless of the direction, or function, unless you explicitly disable the input (enabled by default).
// so RX pin and SCK pin are always available to the PIO, regardless of the direction or function.

    gpio_init(PICO_SPI_TX_PIN);
    gpio_set_dir(PICO_SPI_TX_PIN, 1);
    gpio_set_function(PICO_SPI_TX_PIN, GPIO_FUNC_PIO0); 
    

    gpio_init(PICO_SPI_RX_PIN);
    gpio_set_dir(PICO_SPI_RX_PIN, 0);
    gpio_set_function(PICO_SPI_RX_PIN, GPIO_FUNC_PIO0); 

    gpio_init(PICO_SPI_SCK_PIN);
    gpio_set_dir(PICO_SPI_SCK_PIN, 0);
    gpio_set_function(PICO_SPI_SCK_PIN, GPIO_FUNC_PIO0); 

    // Clear any pending interrupts FIRST
    gpio_acknowledge_irq(PICO_SPI_CSN_PIN, GPIO_IRQ_EDGE_FALL);

    // Set ISR
    irq_set_exclusive_handler(IO_IRQ_BANK0, my_gpio_isr);
    irq_set_priority(IO_IRQ_BANK0, 0xFF); // lowest priority. The keyboard will be higher so that 

    // Enable GPIO interrupt on CSN
    gpio_set_irq_enabled(PICO_SPI_CSN_PIN, GPIO_IRQ_EDGE_FALL, true);


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

    pio_sm_clear_fifos(pio, sm); // Cleans out any FIFOS
    clocked_input_program_init(
        pio_spi.pio,
        pio_spi.sm,
        offset,
        PICO_SPI_RX_PIN ,
        PICO_SPI_CSN_PIN);
}

//PUBLIC void pio_read_master_setup(void){
//    PIO pio = pio0;
//    uint sm = pio_claim_unused_sm(pio, true);
//    pio_read_master.pio = pio;
//    pio_read_master.sm = sm;
//    uint offset = pio_add_program( pio_read_master.pio, &read_master_program);
//    pio_sm_clear_fifos(pio, sm);
//
//    read_master_program_init(
//        pio,
//        sm,
//        offset,
//        PICO_SPI_RX_PIN,
//        PICO_SPI_CSN_PIN);
//}

PUBLIC void pio_keyboard_setup(void){
    PIO pio = pio0;
    uint sm = pio_claim_unused_sm(pio, true);
    pio_keyboard.pio = pio;
    pio_keyboard.sm = sm;
    uint offset = pio_add_program(pio,&keyboard_output_program);
    pio_keyboard.offset = offset;
    pio_sm_clear_fifos(return_keyboard_pio(), return_keyboard_sm());
    
    keyboard_output_program_init(
        pio,
        sm,
        offset,
        PICO_SPI_RX_PIN);
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


// -----------------------------------------------------------------------------
// PROCESSING PLACE
// -----------------------------------------------------------------------------

PUBLIC void classify_packet(void) {

    uint32_t size;
    event_type_t classify_event = 0;
    size = pio_sm_get_blocking(return_spi_pio(), return_spi_sm());
    set_size(size);

    if (size == GARY_CODE ) { // edge cases where due to data transmission there could be wrong things
        keyboard_check = true; // need to save and disable interrupts so that the write i not interrupted.
        classify_event = EVENT_KEYBOARD_DETECTED;
    }

    else if(size > GARY_CODE ) {
        pio_interrupt_clear(return_spi_pio(),0);
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
//
// 1) USB PROCESSING FOR USB, AND DOES DMA STUFF
// 2) KEYBOARD PROCESSING IS REALLY JUST SPI MASTER SLAVE FULL DUPLEX COMMUNICATION
// 3) EVENT PROCESSING IS WHEN ITS ALL DONE REALLY
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
    static uint32_t num = 0;
    event_type_t classify_event = EVENT_KEYBOARD_DETECTED;
    static uint8_t size = 0;

    if(dequeue_keyboard(&ch)){ //start transaction only when character is detected
        pio_sm_exec_wait_blocking(return_keyboard_pio(), return_keyboard_sm(), pio_encode_wait_gpio(1, PICO_SPI_CSN_PIN )); // Wait for csn to go high
        pio_sm_exec_wait_blocking(return_keyboard_pio(), return_keyboard_sm(), pio_encode_wait_gpio(0, PICO_SPI_CSN_PIN )); // Wait for csn to go low
        if(pio_sm_is_tx_fifo_empty(return_keyboard_pio(), return_keyboard_sm())){
            pio_sm_put(return_keyboard_pio(), return_keyboard_sm(), (uint32_t)ch<<24);
        }else{
            size = pio_sm_get_tx_fifo_level(return_keyboard_pio(),return_keyboard_sm());
        }
        if(pio_interrupt_get(return_keyboard_pio(), 1 )){
            pio_interrupt_clear(return_keyboard_pio(), 1); // clear the interrupt for the keyboard output, so it can run
        }
        num = pio_sm_get(return_keyboard_pio(), return_keyboard_sm()); // read from the fifo 
    }


    if(ch == '\r'){
        classify_event = EVENT_DONE;
        enqueue_interrupts(classify_event);
        return(true); // Simply return early
    }
    enqueue_interrupts(classify_event);
    return(false);
}

PUBLIC bool event_processing_main() {
    if(pio_interrupt_get(return_keyboard_pio(),1)){
        gpio_put(PICO_SPI_JMP_PIN,0); // set the keyboard pin low to indicate that the keyboard is done. This is to prevent the system from crashing due to invalid sizes.
        if(pio_interrupt_get(return_spi_pio(), 0)){
            pio_interrupt_clear(return_spi_pio(), 0);
            pio_sm_put(return_spi_pio(),return_spi_sm(), 0); // to reset it
        }
        keyboard_check = false;
    }

    if(pio_interrupt_get(return_spi_pio(), 2)){
        pio_interrupt_clear(return_spi_pio(), 2);
    }
    enqueue_interrupts(EVENT_NONE);

}


void __time_critical_func(pio_spi_read8_blocking)(const pio_keyboard_t *spi, uint8_t *dst, size_t len) {
    size_t tx_remain = len, rx_remain = len;
    io_rw_8 *txfifo = (io_rw_8 *) &spi->pio->txf[spi->sm];
    io_rw_8 *rxfifo = (io_rw_8 *) &spi->pio->rxf[spi->sm];
    while (tx_remain || rx_remain) {
        if (tx_remain && !pio_sm_is_tx_fifo_full(spi->pio, spi->sm)) {
            *txfifo = 0;
            --tx_remain;
        }
        if (rx_remain && !pio_sm_is_rx_fifo_empty(spi->pio, spi->sm)) {
            *dst++ = *rxfifo;
            --rx_remain;
        }
    }
}


void __time_critical_func(pio_spi_write8_read8_blocking)(const pio_keyboard_t *spi, uint8_t *src, uint8_t *dst,
                                                         size_t len) {
    size_t tx_remain = len, rx_remain = len;
    io_rw_8 *txfifo = (io_rw_8 *) &spi->pio->txf[spi->sm];
    io_rw_8 *rxfifo = (io_rw_8 *) &spi->pio->rxf[spi->sm];
    while (tx_remain || rx_remain) {
        if (tx_remain && !pio_sm_is_tx_fifo_full(spi->pio, spi->sm)) {
            *txfifo = *src++;
            --tx_remain;
        }
        if (rx_remain && !pio_sm_is_rx_fifo_empty(spi->pio, spi->sm)) {
            *dst++ = *rxfifo;
            --rx_remain;
        }
    }
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

PUBLIC PIO const return_master_pio(void)
{
    return pio_read_master.pio;
}

PUBLIC uint const return_master_sm(void)
{
    return pio_read_master.sm;
}

PUBLIC int const return_channel(void)
{
    return pio_spi.dma_chan;
}
