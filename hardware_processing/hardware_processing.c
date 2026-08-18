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
static pio_keyboard_t pio_keyboard;



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

    // Enable GPIO interrupt on CSN
    gpio_set_irq_enabled(PICO_SPI_CSN_PIN, GPIO_IRQ_EDGE_FALL, true);


    // Enable IRQ bank
    irq_set_enabled(IO_IRQ_BANK0, true);
}

// -----------------------------------------------------------------------------
// PIO + DMA SETUP
// -----------------------------------------------------------------------------

PUBLIC void pio_dma_setup(void)
{
    PIO pio;
    uint sm;
    uint offset;

    bool success =
        pio_claim_free_sm_and_add_program_for_gpio_range(
            &clocked_input_program,
            &pio,
            &sm,
            &offset,
            PICO_SPI_RX_PIN,
            1,
            true
        );

    hard_assert(success);

    // Store the resources selected by the SDK
    pio_spi.pio = pio;
    pio_spi.sm = sm;

    pio_sm_clear_fifos(pio, sm);

    clocked_input_program_init(
        pio,
        sm,
        offset,
        PICO_SPI_RX_PIN,
        PICO_SPI_CSN_PIN
    );
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

PUBLIC void pio_keyboard_setup(void)
{
    PIO pio;
    uint sm;
    uint offset;

    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(
            &keyboard_output_program,
            &pio,
            &sm,
            &offset,
            PICO_SPI_RX_PIN,
            1,
            true
        );

    hard_assert(success);

    pio_keyboard.pio = pio;
    pio_keyboard.sm = sm;
    pio_keyboard.offset = offset;

    pio_sm_clear_fifos(pio, sm);

    keyboard_output_program_init(
        pio,
        sm,
        offset,
        PICO_SPI_RX_PIN
    );
}
PUBLIC void dma_channel_init_once(void){
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
}

PUBLIC void __time_critical_func(dma_setup_fast)(uint32_t size){
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

PUBLIC void __time_critical_func(classify_packet)(void) {

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
        dma_setup_fast(size); //pass size to dma setup so it can set up transfer size
        classify_event = EVENT_USB_PROCESSING;
    } 
    else{
        classify_event = EVENT_NONE; //Invalid size, so just ignore it and do nothing. This is to prevent the system from crashing due to invalid sizes.
        first_check = true; // reset the first check so that the next time the ISR triggers, it will enqueue the EVENT SIZE PACKET RECIEVED event. This is to prevent the system from getting stuck in an invalid state due to invalid sizes.
    }
    uint32_t status = save_and_disable_interrupts();
    enqueue_interrupts(classify_event);
    restore_interrupts_from_disabled(status);
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
        uint32_t status = save_and_disable_interrupts();
        enqueue_interrupts(EVENT_FILE_PROCESSING);
        restore_interrupts_from_disabled(status);
        dma_channel_cleanup(return_channel());
        return(true);
    }
    else
    {
        uint32_t status = save_and_disable_interrupts();
        enqueue_interrupts(EVENT_NONE);
        restore_interrupts_from_disabled(status);
        return(false);
    }
}

PUBLIC bool keyboard_processing_main() {
    uint8_t ch;
    static uint32_t num = 0;
    event_type_t classify_event = EVENT_KEYBOARD_DETECTED;
    static uint8_t size = 0;
    static uint8_t num1 = 0;

    if(dequeue_keyboard(&ch)){ //start transaction only when character is detected
            pio_interrupt_clear(return_keyboard_pio(),1);
            size = pio_sm_get_pc(return_keyboard_pio(), return_keyboard_sm());
            pio_sm_put(return_keyboard_pio(), return_keyboard_sm(), (uint32_t)ch << 24);
           num1 =  pio_sm_get_tx_fifo_level(return_keyboard_pio(), return_keyboard_sm());
    }

    size = pio_sm_get_pc(return_keyboard_pio(), return_keyboard_sm());

    if(ch == '\r'){
        classify_event = EVENT_DONE;
        uint32_t status = save_and_disable_interrupts();
        enqueue_interrupts(classify_event);
        restore_interrupts_from_disabled(status);
        return(true); // Simply return early
    }
    uint32_t status = save_and_disable_interrupts();
    enqueue_interrupts(classify_event);
    restore_interrupts_from_disabled(status);
    return(false);
}

PUBLIC void event_processing_main() {
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
    uint32_t status = save_and_disable_interrupts();
    enqueue_interrupts(EVENT_NONE);
    restore_interrupts_from_disabled(status);

}

PRIVATE static uint read_register(PIO pio, uint sm, enum pio_src_dest reg) {
    uint move_isr = pio_encode_mov(pio_isr, reg);
    pio_sm_exec_wait_blocking(pio, sm, move_isr);
    uint push = pio_encode_push(false, false);
    pio_sm_exec_wait_blocking(pio, sm, push);
    return pio_sm_get(pio, sm);
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


PUBLIC int const return_channel(void)
{
    return pio_spi.dma_chan;
}
