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
#include "spi_miso.pio.h"

// -----------------------------------------------------------------------------
// STRUCTURES
// -----------------------------------------------------------------------------

typedef struct {
    PIO pio;
    uint sm;
    //uint PIO_IRQc
    int channel;
    dma_channel_config dma_cfg;
    uint offset;
} spi_mosi_t; // this is for the usb


typedef struct {
    PIO pio;
    uint sm;
    uint8_t num;
    uint offset;
} spi_miso_t; // this for the keyboard


typedef struct { // acts as an accumulation point for PIOf for easy managment
    spi_miso_t spi_miso; // called this since this is for transmit via spi
    spi_mosi_t spi_mosi; // this is for the usb
} pio_collection_t;

static pio_collection_t pio_collection;

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

PRIVATE void __time_critical_func(pio_spi_write8_blocking)(const pio_collection_t *pio_collection, const uint8_t *src, size_t len);
PRIVATE uint32_t read_register(const pio_collection_t *pio_collection, const enum pio_src_dest reg);
PRIVATE PIO return_spi_mosi_pio(void);
PRIVATE uint return_spi_mosi_sm(void);
PRIVATE PIO return_spi_miso_pio(void);
PRIVATE uint return_spi_miso_sm(void);
PRIVATE uint return_dma_channel(void);
PRIVATE uint8_t reverse_bits(uint8_t value);
//PRIVATE uint32_t ReadRxValue(PIO pio, uint sm);
//PRIVATE uint32_t ReadTxValue(PIO pio, uint sm);
// -----------------------------------------------------------------------------
// GPIO ISR
// -----------------------------------------------------------------------------
static volatile bool skip_next = true; // // this is for DAC. First csn for usb side is for  DAC 
static volatile bool already_fired = false; //

PRIVATE void __not_in_flash_func(my_gpio_isr)(uint gpio, uint32_t events) {
    //uint32_t events = gpio_get_irq_event_mask(PICO_spi_mosi_PIN);
    // gpio_acknowledge_irq(PICO_spi_mosi_PIN, events);
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
// PIO SETUP
// -----------------------------------------------------------------------------

PUBLIC void pio_mosi_setup(void)
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
            PICO_SPI_TX_PIN - PICO_SPI_RX_PIN ,
            true
        );

    hard_assert(success);

    // Store the resources selected by the SDK
    pio_collection.spi_mosi.pio = pio;
    pio_collection.spi_mosi.sm = sm;
    pio_collection.spi_mosi.offset = offset;

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
            &spi_miso_program,
            &pio,
            &sm,
            &offset,
            PICO_SPI_SCK_PIN,
            (PICO_SPI_TX_PIN - PICO_SPI_SCK_PIN)+1,
            true
        );

    hard_assert(success);

    pio_collection.spi_miso.pio = pio;
    pio_collection.spi_miso.sm = sm;
    pio_collection.spi_miso.offset = offset;

    pio_sm_clear_fifos(pio, sm);

    spi_miso_program_init(
        pio,
        sm,
        offset,
        PICO_SPI_CSN_PIN,
        PICO_SPI_TX_PIN);
}

    
// -----------------------------------------------------------------------------
//  DMA SETUP
// -----------------------------------------------------------------------------
PUBLIC void dma_channel_init_once(void){
    pio_collection.spi_mosi.channel = dma_claim_unused_channel(true);
    pio_collection.spi_mosi.dma_cfg = dma_channel_get_default_config(pio_collection.spi_mosi.channel);
    channel_config_set_transfer_data_size( &pio_collection.spi_mosi.dma_cfg, DMA_SIZE_8);
    channel_config_set_read_increment( &pio_collection.spi_mosi.dma_cfg, false);
    channel_config_set_write_increment( &pio_collection.spi_mosi.dma_cfg, true);
    channel_config_set_dreq(
        &pio_collection.spi_mosi.dma_cfg,
        pio_get_dreq( // get data request
            pio_collection.spi_mosi.pio,
            pio_collection.spi_mosi.sm,
            false
        )
    );
}

PUBLIC void __time_critical_func(dma_setup_fast)(uint32_t size){
    dma_channel_configure(
        pio_collection.spi_mosi.channel,
        &pio_collection.spi_mosi.dma_cfg,
        give_array_address(), // buffer to write to 
        &pio_collection.spi_mosi.pio->rxf[pio_collection.spi_mosi.sm],
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
    size = pio_sm_get_blocking(return_spi_mosi_pio(), return_spi_mosi_sm()); // get size byte
    set_size(size);

    if (size == GARY_CODE ) { // edge cases where due to data transmission there could be wrong things
        gpio_set_irq_enabled(PICO_SPI_CSN_PIN, GPIO_IRQ_EDGE_FALL, false); // disable the interrupt so that it does not fire again until the event is processed. This is to prevent the main loop from running when there is no event to process.
        //gpio_put(PICO_CODE_DEBUG_PROBE_PIN,1);
        keyboard_check = true; // need to save and disable interrupts so that the write i not interrupted.
        return;
    }
    else if (size > GARY_CODE) {
        dma_setup_fast(size);
        classify_event = EVENT_USB_PROCESSING;
    }
    else {
        classify_event = EVENT_NONE;
        skip_next = true;
        already_fired = false; //Invalid size, so just ignore it and do nothing. This is to prevent the system from crashing due to invalid sizes.
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
    dma_start_channel_mask(1u << return_dma_channel()); // start the DMA transfer
    dma_channel_wait_for_finish_blocking(return_dma_channel()); // Wait for the DMA transfer to complete

        // 1. Pause the DMA channel
    hw_clear_bits(&dma_hw->ch[return_dma_channel()].ctrl_trig, DMA_CH0_CTRL_TRIG_EN_BITS);

        // 2. Now that the DMA channel is paused, we can safely read the write address

    uintptr_t base = (uintptr_t)give_array_address();
    uintptr_t write = dma_hw->ch[return_dma_channel()].write_addr;
    uint32_t difference = write - base;

    if(difference ==  return_size())
    {
        uint32_t status = save_and_disable_interrupts();
        enqueue_interrupts(EVENT_FILE_PROCESSING);
        restore_interrupts_from_disabled(status);
        dma_channel_cleanup(return_dma_channel());
        return(true);
    }
    else
    {
        uint32_t status = save_and_disable_interrupts();
        enqueue_interrupts(EVENT_NONE);
        restore_interrupts_from_disabled(status);
        dma_channel_cleanup(return_dma_channel());
        return(false);
    }
}

//PUBLIC bool keyboard_processing_main() {
//    //uint32_t letter = 0;
//    uint32_t size = 0; 
//    uint8_t ch = 'm';   // TEMPORARY: flood test
//   // letter = ch << 24; //just to make sure it is at the MSB 
//    static uint32_t result = 0;
//    static uint32_t read = 0;
//    pio_spi_write8_blocking(&pio_collection, &ch, 1);
//}

PUBLIC bool keyboard_processing_main() {
    uint8_t ch = 0;
    event_type_t classify_event = EVENT_KEYBOARD_DETECTED;

    if(dequeue_keyboard(&ch)){
 //start transaction only when character is detected
            pio_spi_write8_blocking(&pio_collection, &ch, 1);
            if(ch == '\r'){
            // Force the SM's PC back to the very start (irq wait 1),
            // ending the keyboard-shifting loop and re-arming the gate.
                keyboard_check = false;
                classify_event = EVENT_DONE; // queing up the event
                uint32_t status = save_and_disable_interrupts();
                enqueue_interrupts(classify_event);
                restore_interrupts_from_disabled(status);
                return(true); // Simply return early
            }
    }
        uint32_t status = save_and_disable_interrupts();
        enqueue_interrupts(classify_event);
        restore_interrupts_from_disabled(status);
        return(false);
}

PUBLIC void event_processing_main() {
    gpio_set_irq_enabled(PICO_SPI_CSN_PIN, GPIO_IRQ_EDGE_FALL, true); // disable the interrupt so that it does not fire again until the event is processed. This is to prevent the main loop from running when there is no event to process.
    uint32_t status = save_and_disable_interrupts();
    skip_next = true;
    enqueue_interrupts(EVENT_NONE);
    restore_interrupts_from_disabled(status);
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------

PRIVATE void __time_critical_func(pio_spi_write8_blocking)(const pio_collection_t *pio_collection, const uint8_t *src, size_t len) {
    size_t tx_remain = len;
    size_t rx_remain = len;

    io_rw_8 *tx_fifo = (io_rw_8 *)&pio_collection->spi_miso.pio->txf[pio_collection->spi_miso.sm];
    io_rw_8 *rx_fifo = (io_rw_8 *)&pio_collection->spi_mosi.pio->rxf[pio_collection->spi_mosi.sm];

    while(tx_remain || rx_remain) {
        if(tx_remain && !pio_sm_is_tx_fifo_full(pio_collection->spi_miso.pio, pio_collection->spi_miso.sm)) {
            *tx_fifo = *src++;
            --tx_remain;
        }

        if(rx_remain && !pio_sm_is_rx_fifo_empty(pio_collection->spi_mosi.pio, pio_collection->spi_mosi.sm)) {
            (void)*rx_fifo; // discard the received data
            --rx_remain;
        }
    }


}

PRIVATE uint32_t read_register(const pio_collection_t *pio_collection, const enum pio_src_dest reg) { // for debugging purposes.
    uint move_isr = pio_encode_mov(pio_isr, reg);
    pio_sm_exec_wait_blocking(pio_collection->spi_miso.pio, pio_collection->spi_miso.sm, move_isr);
    uint push = pio_encode_push(false, false);
    pio_sm_exec_wait_blocking(pio_collection->spi_miso.pio, pio_collection->spi_miso.sm, push);
    return(pio_sm_get(pio_collection->spi_miso.pio, pio_collection->spi_miso.sm));
}

  PRIVATE uint8_t reverse_bits(uint8_t value) {

    uint8_t result = 0;

      for (int i = 0; i < 8; i++) {
          result <<= 1;
          result |= (value & 1);
          value >>= 1;
      }
      return result;
  }
    

// -----------------------------------------------------------------------------
// ACCESSORS
// -----------------------------------------------------------------------------


PRIVATE uint return_dma_channel(void)
{
    return pio_collection.spi_mosi.channel;
}

PRIVATE PIO return_spi_miso_pio(void)
{
    return pio_collection.spi_miso.pio;
}

PRIVATE uint return_spi_miso_sm(void)
{
    return pio_collection.spi_miso.sm;
}

PRIVATE uint return_spi_mosi_sm(void)
{
    return pio_collection.spi_mosi.sm;
}

PRIVATE PIO return_spi_mosi_pio(void)
{
    return pio_collection.spi_mosi.pio;
}
