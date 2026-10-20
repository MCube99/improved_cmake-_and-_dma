#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"

#include "queue.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "hardware/dma.h"
#include "hardware/sync.h"
#include "hardware/structs/iobank0.h"
#include "hardware/structs/pio.h"
#include "hardware_processing.h"
#include "clocked_input.pio.h"
#include "spi_miso.pio.h"
#include "board_api.h"
#include "CException.h"
#include "CExceptionConfig.h"

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
#define DMA_TIMEOUT_US  30000u
// -----------------------------------------------------------------------------
// GLOBALS
// -----------------------------------------------------------------------------

PRIVATE void __time_critical_func(pio_spi_write8_blocking)(const pio_collection_t *pio_collection, const uint8_t *src, size_t len);
PRIVATE PIO return_spi_mosi_pio(void);
PRIVATE uint return_spi_mosi_sm(void);
PRIVATE PIO return_spi_miso_pio(void);
PRIVATE uint return_spi_miso_sm(void);
PRIVATE uint return_dma_channel(void);
PRIVATE void dma_arm(void);
static volatile bool dma_armed = false;
volatile bool packet_recieved = false;
static uint32_t dma_armed_at;
// -----------------------------------------------------------------------------
// GPIO & DMA ISR
// -----------------------------------------------------------------------------

PRIVATE void __not_in_flash_func(my_gpio_isr)(void) {
    uint32_t events = gpio_get_irq_event_mask(PICO_SPI_CSN_PIN);
    gpio_acknowledge_irq(PICO_SPI_CSN_PIN,events);
    if (events & GPIO_IRQ_EDGE_FALL) {
        if(packet_recieved){
            return; // if the keyboard is being processed, then dont enqueue the event since it will be processed in the main loop.
        }
	    enqueue_events(EVENT_SIZE_PACKET_RECIEVED); // only one job, then it gets reenabled only at the end. 
    }
}

PRIVATE void dma_handler(void) {
    dma_armed = false;

    uintptr_t base  = (uintptr_t)give_array_address();
    uintptr_t write = dma_hw->ch[ch].write_addr;
    TRY{
        if ((uint32_t)(write - base) == (uint32_t)return_size()) {
            enqueue_events(EVENT_FILE_PARSING);
        } else {
            uint32_t status = save_and_disable_interrupts();
            throw(DIAG_DMA_SHORT); // this is for the case where the size is less than 31, which is an error.
            restore_interrupts_from_disabled(status);
        }
    }
// Clear the interrupt request.
    dma_hw->ints0 = 1u << ch;
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
   // Direct, exclusive IRQ registration — no generic dispatch/callback overhead,
    // correct for time-critical CSN edge detection.
    irq_set_exclusive_handler(IO_IRQ_BANK0, my_gpio_isr);
    irq_set_enabled(IO_IRQ_BANK0, true);
    gpio_set_irq_enabled(PICO_SPI_CSN_PIN, GPIO_IRQ_EDGE_FALL, true);
}
//--------------------------------------------------------------------
// Blinking Task
//--------------------------------------------------------------------
PUBLIC void led_blinking_task(void) {
  const uint32_t interval_ms = 1000;
  static uint32_t start_ms = 0;

  static bool led_state = false;

  // Blink every interval ms
  if (board_millis() - start_ms < interval_ms) return; // not enough time
  start_ms += interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
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
    channel_config_set_read_increment( &pio_collection.spi_mosi.dma_cfg, false); //dma is not reading from this so false
    channel_config_set_write_increment( &pio_collection.spi_mosi.dma_cfg, true); // dma is writing to this so true
    channel_config_set_dreq(
        &pio_collection.spi_mosi.dma_cfg,
        pio_get_dreq( // get data request
            pio_collection.spi_mosi.pio,
            pio_collection.spi_mosi.sm,
            false
        )
    );

    dma_channel_set_irq0_enabled(pio_collection.spi_mosi.channel, true);
    // Configure the processor to run dma_handler() when DMA IRQ 0 is asserted
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
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

PUBLIC void dma_timeout_check(void) {            // designed to timeout any packet which havent come after a substantial period of time.
    //uint32_t status = save_and_disable_interrupts();
    if (dma_armed && (time_us_32() - dma_armed_at) > DMA_TIMEOUT_US) {
        dma_armed = false;
        throw(DIAG_DMA_TIMEOUT); // this is for the case where the size is less than 31, which is an error.
    }
   //     uint ch = return_dma_channel();
   //     dma_channel_abort(ch);
   //     PIO  pio = pio_collection.spi_mosi.pio;
   //     uint sm  = pio_collection.spi_mosi.sm;
   //     pio_sm_clear_fifos(pio, sm);
   //     pio_sm_restart(pio, sm);
   //     pio_sm_exec(pio, sm, pio_encode_jmp(pio_collection.spi_mosi.offset));
   //     dma_channel_acknowledge_irq0(ch);        // discard any IRQ left pending by the abort
   // }
 //   restore_interrupts_from_disabled(status);
}

PRIVATE void dma_arm(void) {
    dma_armed_at = time_us_32();
    dma_armed = true;                                  // set before starting, so the IRQ can't clear it first
    dma_start_channel_mask(1u << return_dma_channel());
}

// -----------------------------------------------------------------------------
// PROCESSING PLACE
// -----------------------------------------------------------------------------

PUBLIC void __time_critical_func(classify_packet)(void) {
    uint32_t size;
    event_type_t classify_event = 0;
    size = pio_sm_get_blocking(return_spi_mosi_pio(), return_spi_mosi_sm()); // get size byte
    set_size(size);
    packet_recieved = true; // should hit true at gpio isr so it leaves early. 

        if (size == GARY_CODE ) { // edge cases where due to data transmission there could be wrong things
            not_space_check = true;
            keyboard_processing_main(); // call function from here
        }
        else {
            dma_setup_fast(size);
            dma_arm();
        }
}

// -----------------------------------------------------------------------------
// PROCESSING FOR MAIN
//
// 1) USB PROCESSING FOR USB, AND DOES DMA STUFF
// 2) KEYBOARD PROCESSING IS REALLY JUST SPI MASTER SLAVE FULL DUPLEX COMMUNICATION
// 3) EVENT PROCESSING IS WHEN ITS ALL DONE REALLY
// -----------------------------------------------------------------------------



PUBLIC void keyboard_processing_main() {
    uint8_t ch = 0;
    if(dequeue_keyboard(&ch)){
 //start transaction only when character is detected
            pio_spi_write8_blocking(&pio_collection, &ch, 1); 
            if(ch == '\r' || ch == '\n'){
                not_space_check = false; // reset the not space check so that the next character can be processed.
            }
    }

    uint32_t status = save_and_disable_interrupts();
    enqueue_events(EVENT_DONE); // enqueue the next character to be processed    
    restore_interrupts_from_disabled(status);
}

PUBLIC void event_processing_main() { // will add more functionality to this later, but for now it just resets the packet recieved flag so that the next packet can be processed.
    packet_recieved = false; // reset the packet recieved flag so that the next packet can be processed.
    pio_sm_clear_fifos(return_spi_mosi_pio(), return_spi_mosi_sm()); // clear the fifos so that the next packet can be processed.
    pio_sm_clear_fifos(return_spi_miso_pio(), return_spi_miso_sm()); // clear the fifos so that the next packet can be processed.
    pio_sm_encode_jmp(return_spi_mosi_pio(), return_spi_mosi_sm(), pio_collection.spi_mosi.offset); // reset the state machine so that the next packet can be processed.
    pio_sm_encode_jmp(return_spi_miso_pio(), return_spi_miso_sm(), pio_collection.spi_miso.offset); // reset the state machine so that the next packet can be processed.
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
