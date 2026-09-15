#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"

#include "queue.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/dma.h"
#include "hardware/sync.h"
#include "hardware/structs/iobank0.h"
#include "hardware/structs/pio.h"
#include "hardware_processing.h"
#include "hardware/spi.h"

#include "pico/binary_info.h"
#include "clocked_input.pio.h"
#include "spi_mosi_loop.pio.h"
#include "spi_miso_loop.pio.h"

// -----------------------------------------------------------------------------
// STRUCTURES
// -----------------------------------------------------------------------------

typedef struct {
    PIO pio;
    uint sm;
    //uint PIO_IRQc
    int dma_chan;
    dma_channel_config dma_cfg;
    uint offset;
} pio_spi_t; // this is for the usb


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
    uint8_t num;
    uint offset;
} pio_recieve_t; // this is for the rxf from the master


typedef struct { // acts as an accumulation point for PIOf for easy managment
    pio_keyboard_t pio_miso; // called this since this is for transmit via spi
    pio_recieve_t pio_mosi;      // called this since this is for recieve via spi
} pio_collection_t;

static pio_collection_t pio_collection;
static pio_spi_t pio_spi;

#define PICO_START          2
#define PICO_SPI_RX_PIN   ((PICO_START)      + 0)   // 2 
#define PICO_SPI_SCK_PIN  ((PICO_SPI_RX_PIN) + 1)   //3            // GPIO pin for SPI clock, same as master
#define PICO_SPI_CSN_PIN   ((PICO_SPI_RX_PIN) + 2)   //4             // GPIO pin for SPI chip select
#define PICO_SPI_TX_PIN  ((PICO_SPI_RX_PIN) + 3)   //5             // GPIO pin for SPI data to master → slave
#define PICO_MOSI_DEBUG_PROBE_PIN  7   // pick any free GPIO. This is for the pio input
#define PICO_MISO_DEBUG_PROBE_PIN  8   // pick any free GPIO. This is for the pio output
#define PICO_CODE_DEBUG_PROBE_PIN 9 // this is for the main c file type code
#define PICO_CODE_DEBUG_ERROR_PIN 10 // this is for the main c file type code and is for error programs. 
// -----------------------------------------------------------------------------
// GLOBALS
// -----------------------------------------------------------------------------

PRIVATE void __time_critical_func(pio_spi_write8_read8_blocking)(const pio_collection_t *pio_collection,const uint8_t *src,uint8_t *dst, size_t len);
PRIVATE uint8_t __time_critical_func(pio_spi_write8_blocking)(const pio_collection_t *pio_collection, const uint8_t src, uint32_t *dest, size_t len);
PRIVATE uint read_register(const pio_collection_t *pio_collection, const enum pio_src_dest reg);
//PRIVATE uint32_t ReadRxValue(PIO pio, uint sm);
//PRIVATE uint32_t ReadTxValue(PIO pio, uint sm);
// -----------------------------------------------------------------------------
// GPIO ISR
// -----------------------------------------------------------------------------
static volatile bool skip_next = true; // // this is for DAC. First csn for usb side is for  DAC 
static volatile bool already_fired = false; //

PRIVATE void __not_in_flash_func(my_gpio_isr)(uint gpio, uint32_t events) {
    //uint32_t events = gpio_get_irq_event_mask(PICO_SPI_CSN_PIN);
    // gpio_acknowledge_irq(PICO_SPI_CSN_PIN, events);
    main_check = true; // set main check to true so that the main loop will run.
    if (events & GPIO_IRQ_EDGE_FALL) {
        if (keyboard_check || already_fired) {
            return; // not for me (keyboard mode active), or already fired this cycle
        }

        if (skip_next) { 
            skip_next = false; // this edge presumed not-for-me (e.g. DAC), skip it
        } else {
            enqueue_interrupts(EVENT_SIZE_PACKET_RECIEVED); // this edge is the real one
            already_fired = true; // stay silent until event_processing_main() re-arms us. For USB, this should be the second fire which happens
        }
    }
}
// -----------------------------------------------------------------------------
// GPIO SETUP
// -----------------------------------------------------------------------------

PUBLIC void set_gpio_pins(void) {
    // CSN pin setup
    gpio_init(PICO_SPI_CSN_PIN);
    gpio_set_function(PICO_SPI_CSN_PIN, GPIO_FUNC_SIO);
    gpio_set_dir(PICO_SPI_CSN_PIN, GPIO_IN);
    gpio_pull_up(PICO_SPI_CSN_PIN);

// inputs are always available to PIO and SIO, regardless of the direction, or function, unless you explicitly disable the input (enabled by default).
// so RX pin and SCK pin are always available to the PIO, regardless of the direction or function.

    gpio_init(PICO_SPI_TX_PIN);
    gpio_set_dir(PICO_SPI_TX_PIN, 1);
    gpio_set_function(PICO_SPI_TX_PIN, GPIO_FUNC_PIO0); //set it as output
    
    gpio_init(PICO_SPI_RX_PIN);
    gpio_set_dir(PICO_SPI_RX_PIN, 0); // sets them as input

    gpio_init(PICO_SPI_SCK_PIN);
    gpio_set_dir(PICO_SPI_SCK_PIN, 0);

///    gpio_init(PICO_CODE_DEBUG_PROBE_PIN);
///   gpio_set_dir(PICO_CODE_DEBUG_PROBE_PIN,1);
///   gpio_put(PICO_CODE_DEBUG_PROBE_PIN,0);

 ///   gpio_init(PICO_CODE_DEBUG_ERROR_PIN );
 ///   gpio_set_dir(PICO_CODE_DEBUG_ERROR_PIN,1);
 ///   gpio_put(PICO_CODE_DEBUG_ERROR_PIN,0);// once, in setup:
   /// gpio_init(PICO_MOSI_DEBUG_PROBE_PIN);
   /// gpio_set_dir(PICO_MOSI_DEBUG_PROBE_PIN, GPIO_FUNC_PIO0);
   /// pio_csn.pio = pio;
   /// pio_csn.sm = sm;
   /// pio_csn.offset = offset;
   /// gpio_put(PICO_MOSI_DEBUG_PROBE_PIN, 0);

    gpio_set_irq_enabled_with_callback(PICO_SPI_CSN_PIN, GPIO_IRQ_EDGE_FALL, true, &my_gpio_isr);
    // Clear any pending interrupts FIRST
//   gpio_acknowledge_irq(PICO_SPI_CSN_PIN, GPIO_IRQ_EDGE_FALL);
//   // Set ISR
//   irq_set_exclusive_handler(IO_IRQ_BANK0, my_gpio_isr);
//   // Enable GPIO interrupt on CSN
//   gpio_set_irq_enabled(PICO_SPI_CSN_PIN, GPIO_IRQ_EDGE_FALL, true);
//   // Enable IRQ bank
//   irq_set_enabled(IO_IRQ_BANK0, true);
}

// -----------------------------------------------------------------------------
// PIO + DMA SETUP
// -----------------------------------------------------------------------------

PUBLIC void pio_dma_setup(void)
{
    PIO pio;
    uint sm;
    uint offset;

    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(
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
    pio_spi.offset = offset;

    pio_sm_clear_fifos(pio, sm);

    clocked_input_program_init(
        pio,
        sm,
        offset,
        PICO_SPI_RX_PIN,
        PICO_SPI_CSN_PIN
    );
}

PUBLIC void pio_miso_setup(void){
    PIO pio;
    uint sm;
    uint offset;

    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(
            &spi_miso_loop_program,
            &pio,
            &sm,
            &offset,
            PICO_SPI_RX_PIN,
            1,
            true
        );

    hard_assert(success);

    pio_collection.pio_miso.pio = pio;
    pio_collection.pio_miso.sm = sm;
    pio_collection.pio_miso.offset = offset;

    pio_sm_clear_fifos(pio, sm);

    spi_miso_loop_program_init(
        pio,
        sm,
        offset,
        PICO_SPI_SCK_PIN,
        PICO_SPI_CSN_PIN,
        PICO_MISO_DEBUG_PROBE_PIN,
        PICO_SPI_TX_PIN
    );

}

PUBLIC void pio_mosi_setup(void){
    PIO pio = return_keyboard_mosi_pio();
    uint offset = pio_add_program(pio, &spi_mosi_loop_program);
    int sm = pio_claim_unused_sm(pio, false);
    // Check instruction memory before adding the program

    spi_mosi_loop_init(
        pio,
        sm,
        offset,
        PICO_SPI_CSN_PIN,
        PICO_SPI_RX_PIN);

    pio_collection.pio_mosi.pio = pio;
    pio_collection.pio_mosi.sm = sm;
    pio_collection.pio_mosi.offset = offset;
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
    size = pio_sm_get_blocking(return_spi_pio(), return_spi_sm()); // get size byte
    set_size(size);

    if (size == GARY_CODE ) { // edge cases where due to data transmission there could be wrong things
        pio_interrupt_clear(return_keyboard_miso_pio(),1);
        gpio_set_irq_enabled(PICO_SPI_CSN_PIN, GPIO_IRQ_EDGE_FALL, false); // disable the interrupt so that it does not fire again until the event is processed. This is to prevent the main loop from running when there is no event to process.
        gpio_put(PICO_CODE_DEBUG_PROBE_PIN,1);
        keyboard_check = true; // need to save and disable interrupts so that the write i not interrupted.
        return;
    }
    else if (size > GARY_CODE) {
        pio_interrupt_clear(return_spi_pio(),0);
        pio_sm_put(return_spi_pio(),return_spi_sm(),size);
        dma_setup_fast(size);
        classify_event = EVENT_USB_PROCESSING;
    }
    else {
        classify_event = EVENT_NONE;
        skip_next = true;
        already_fired = false; //Invalid size, so just ignore it and do nothing. This is to prevent the system from crashing due to invalid sizes.
        pio_interrupt_clear(return_spi_pio(), 0);
        pio_sm_exec_wait_blocking(return_spi_pio(), return_spi_sm(), pio_encode_jmp(pio_spi.offset)); // force PC back to "flush:" — full reset
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
        dma_channel_cleanup(return_channel());
        return(false);
    }
}

PUBLIC bool keyboard_processing_main() {
    uint8_t ch = 'm';   // TEMPORARY: flood test
    uint32_t size = 0; 
    static uint8_t result = 0;
    result = pio_spi_write8_blocking(&pio_collection, ch<<24,&size, 1);
  //  pio_sm_exec_wait_blocking(return_keyboard_miso_pio(),return_keyboard_miso_sm(),pio_encode_pull(false, true));
  //  pio_sm_exec_wait_blocking(return_keyboard_miso_pio(),return_keyboard_miso_sm(),pio_encode_mov(pio_x, pio_osr));
    static int gary_code_mismatch_count = 0;

    if(size == GARY_CODE){
        event_type_t classify_event = EVENT_KEYBOARD_DETECTED;
        uint32_t status = save_and_disable_interrupts();
        enqueue_interrupts(classify_event);
        restore_interrupts_from_disabled(status);
        gpio_put(PICO_CODE_DEBUG_PROBE_PIN,0);
        return(true);
    }
    else if(size != GARY_CODE){
        gary_code_mismatch_count++;
        pio_sm_exec_wait_blocking(return_keyboard_miso_pio(),return_keyboard_miso_sm(),pio_encode_jmp(pio_collection.pio_miso.offset + spi_miso_loop_offset_keyboard_miso_entry)); // force PC back to keyboard_setup 
        uint32_t status = save_and_disable_interrupts();
        enqueue_interrupts(EVENT_NONE);
        restore_interrupts_from_disabled(status);
        return(false);
    }
    // -------------------------------------------------------------
}

///PUBLIC bool keyboard_processing_main() {
///    uint8_t ch = 0;
///    event_type_t classify_event = EVENT_KEYBOARD_DETECTED;
///    static int gary_code_mismatch_count = 0;
///
///    if(dequeue_keyboard(&ch)){
/// //start transaction only when character is detected
///            pio_interrupt_clear(return_spi_pio(),1);
///            WaitFallingEdge(PICO_SPI_CSN_PIN); // wait for csn to fall before starting the transaction
///            pio_sm_put(return_spi_pio(), return_keyboard_sm(), (uint32_t)ch << 24);
///
///            if(ch == '\r'){
///            // Force the SM's PC back to the very start (irq wait 1),
///            // ending the keyboard-shifting loop and re-arming the gate.
///                gpio_set_irq_enabled(PICO_SPI_CSN_PIN, GPIO_IRQ_EDGE_FALL, true);
///                keyboard_check = false;
///                pio_sm_exec_wait_blocking(return_spi_pio(), return_keyboard_sm(), pio_encode_jmp(pio_keyboard.offset)); // offset 0 = keyboard_initial_processing
///                hw_set_bits(&(return_spi_pio())->irq, 1u << 1); 
///                classify_event = EVENT_DONE; // queing up the event
///                uint32_t status = save_and_disable_interrupts();
///                enqueue_interrupts(classify_event);
///                restore_interrupts_from_disabled(status);
///                return(true); // Simply return early
///            }
///    }
///    if (!pio_sm_is_rx_fifo_empty(return_spi_pio(), return_keyboard_sm())) {
///        uint32_t sampled = pio_sm_get(return_spi_pio(), return_keyboard_sm());
///        uint8_t sampled_byte = sampled >> 24; // adjust shift based on your in_shift config/justification
///        if (sampled_byte != GARY_CODE) {
///            gary_code_mismatch_count++; // same diagnostic counter idea as before
///        }
///    }
///        uint32_t status = save_and_disable_interrupts();
///        enqueue_interrupts(classify_event);
///        restore_interrupts_from_disabled(status);
///        return(false);
///}
///
PUBLIC void event_processing_main() {
    if(pio_interrupt_get(return_spi_pio(), 0)){
        pio_interrupt_clear(return_spi_pio(), 0);
        pio_sm_exec_wait_blocking(return_spi_pio(), return_spi_sm(), pio_encode_jmp(pio_spi.offset)); // force PC back to "flush:" — full reset
    }
    gpio_set_irq_enabled(PICO_SPI_CSN_PIN, GPIO_IRQ_EDGE_FALL, true); // disable the interrupt so that it does not fire again until the event is processed. This is to prevent the main loop from running when there is no event to process.
    uint32_t status = save_and_disable_interrupts();
    skip_next = true;
    
    enqueue_interrupts(EVENT_NONE);
    restore_interrupts_from_disabled(status);
}

PRIVATE uint read_register(const pio_collection_t *pio_collection, const enum pio_src_dest reg) { // for debugging purposes.
    uint move_isr = pio_encode_mov(pio_isr, reg);
    pio_sm_exec_wait_blocking(pio_collection->pio_miso.pio, pio_collection->pio_miso.sm, move_isr);
    uint push = pio_encode_push(false, false);
    pio_sm_exec_wait_blocking(pio_collection->pio_miso.pio, pio_collection->pio_miso.sm, push);
    return pio_sm_get(pio_collection->pio_miso.pio, pio_collection->pio_miso.sm);
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------
// FUNCTIONS TO READ AND WRITE TO PIO FIFOS
// ------------------------------------------------------------------------------------------------------------------------------------------------------
PRIVATE uint8_t __time_critical_func(pio_spi_write8_blocking)(const pio_collection_t *pio_collection, const uint8_t src, uint32_t *dest, size_t len)
{
    uint32_t debug = 0;
  //  size_t tx_remain = len;
  //  pio_sm_put(pio_collection->pio_miso.pio, pio_collection->pio_miso.sm, (uint32_t)src<< 24); // make sure the data is MSB first

      if (!pio_sm_is_tx_fifo_full(pio_collection->pio_miso.pio, pio_collection->pio_miso.sm)) {
          pio_sm_put(pio_collection->pio_miso.pio, pio_collection->pio_miso.sm, (uint32_t)src<<24); // make sure the data is MSB first
          debug = read_register(pio_collection, pio_x);

      }
        if (!pio_sm_is_rx_fifo_empty(pio_collection->pio_mosi.pio, pio_collection->pio_mosi.sm)) {
            *dest = pio_sm_get_blocking(pio_collection->pio_mosi.pio, pio_collection->pio_mosi.sm); // read the data from the RX FIFO to clear it
        }

        debug = (debug>>24)&0xFF; // get the MSB of the debug value
        return(debug);

}


///PRIVATE void __time_critical_func(pio_spi_write8_read8_blocking)(const pio_collection_t *pio_collection,const uint8_t *src, uint8_t *dst, size_t len) 
///{
///    size_t tx_remain = len;
///    size_t rx_remain = len;
///
///    io_rw_8 *tx_fifo = (io_rw_8 *)&pio_collection->pio_miso.pio->txf[ pio_collection->pio_miso.sm ];
///    io_ro_8 *rx_fifo = (io_ro_8 *)&pio_collection->pio_mosi.pio->rxf[ pio_collection->pio_mosi.sm ];
///
///    while (tx_remain || rx_remain) {
///
///        if (tx_remain && !pio_sm_is_tx_fifo_full( pio_collection->pio_miso.pio, pio_collection->pio_miso.sm))
///        {
///            *tx_fifo = *src++;
///            --tx_remain;
///        }
///
///        if (rx_remain && !pio_sm_is_rx_fifo_empty(pio_collection->pio_mosi.pio, pio_collection->pio_mosi.sm))
///        {
///            *dst++ = *rx_fifo;
///            --rx_remain;
///        }
///    }
///}

// ----------------------------------------------------------------------------
// CODE TO DEBUG PIO FROM WHAT I CAN GATHER 
// ----------------------------------------------------------------------------

///PRIVATE uint32_t ReadRxValue(PIO pio, uint sm) {
///    uint32_t rx_value;
///    if(sm == 0){
///        rx_value = pio->rxf[0];
///    } else if(sm == 1){
///        rx_value = pio->rxf[1];
///    } else if(sm == 2){
///        rx_value = pio->rxf[2];
///    } else if(sm == 3){
///        rx_value = pio->rxf[3];
///    }
///    return rx_value;
///}
///
///PRIVATE uint32_t ReadTxValue(PIO pio, uint sm) {
///    uint32_t tx_value;
///    if(sm == 0){
///        tx_value = pio->txf[0];
///    } else if(sm == 1){
///        tx_value = pio->txf[1];
///    } else if(sm == 2){
///        tx_value = pio->txf[2];
///    } else if(sm == 3){
///        tx_value = pio->txf[3];
///    }
///    return tx_value;
///}
///}


// -----------------------------------------------------------------------------

// Probably dont need this function, but just in case.
   


               
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

PUBLIC PIO const return_keyboard_miso_pio(void)
{
    return pio_collection.pio_miso.pio;
}

PUBLIC uint const return_keyboard_miso_sm(void)
{
    return pio_collection.pio_miso.sm;
}


PUBLIC int const return_channel(void)
{
    return pio_spi.dma_chan;
}

PUBLIC int const return_keyboard_mosi_sm(void)
{
    return pio_collection.pio_mosi.sm;
}

PUBLIC PIO const return_keyboard_mosi_pio(void)
{
    return pio_collection.pio_mosi.pio;
}
