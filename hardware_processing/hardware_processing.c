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
#include "spi_cs_loop.pio.h"
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
    uint offset;
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
    uint8_t num;
    uint offset;
} pio_csn_t;


#define PICO_START          2
#define PICO_SPI_RX_PIN   ((PICO_START)      + 0)   // 2 
#define PICO_SPI_SCK_PIN  ((PICO_SPI_RX_PIN) + 1)   //3            // GPIO pin for SPI clock, same as master
#define PICO_SPI_CSN_PIN   ((PICO_SPI_RX_PIN) + 2)   //4             // GPIO pin for SPI chip select
#define PICO_SPI_TX_PIN  ((PICO_SPI_RX_PIN) + 3)   //5             // GPIO pin for SPI data to master → slave
#define DEBUG_PROBE_PIN  7   // pick any free GPIO
#define PICO_SPI_SIDESET_PIN 8
#define PICO_SPI_DEBUG_PROBE_PIN 9 // this is for the main c file type code
// -----------------------------------------------------------------------------
// GLOBALS
// -----------------------------------------------------------------------------

static pio_spi_t pio_spi;
static pio_keyboard_t pio_keyboard;
static pio_csn_t pio_csn;

PRIVATE inline void WaitCsnToFall(uint gpio);
PRIVATE uint read_register(PIO pio, uint sm, enum pio_src_dest reg);
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
            gpio_put(7,1);
            enqueue_interrupts(EVENT_SIZE_PACKET_RECIEVED); // this edge is the real one
            already_fired = true; // stay silent until event_processing_main() re-arms us
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

    gpio_init(DEBUG_PROBE_PIN);
    gpio_set_dir(DEBUG_PROBE_PIN,1);
// once, in setup:
   /// gpio_init(DEBUG_PROBE_PIN);
   /// gpio_set_dir(DEBUG_PROBE_PIN, GPIO_FUNC_PIO0);
   /// pio_csn.pio = pio;
   /// pio_csn.sm = sm;
   /// pio_csn.offset = offset;
   /// gpio_put(DEBUG_PROBE_PIN, 0);

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
        PICO_SPI_RX_PIN,
        PICO_SPI_SIDESET_PIN);

}

PUBLIC void pio_csn_setup(void){
	PIO pio = return_keyboard_pio();
	uint sm;
	uint offset;

	bool success = pio_claim_free_sm_and_add_program_for_gpio_range(
			&spi_cs_loop_program, 
			&pio,
			&sm,
			&offset,
			PICO_SPI_CSN_PIN,
			1,
			true);

    gpio_put(DEBUG_PROBE_PIN,1);
	hard_assert(success);
    hard_assert(pio == return_keyboard_pio());   // fail loudly if it landed on a different PIO block
	pio_csn.pio = pio;
	pio_csn.sm = sm;
	pio_csn.offset = offset;

    spi_cs_loop_init(
        pio,
        sm,
        offset,
        PICO_SPI_CSN_PIN,
        PICO_SPI_SIDESET_PIN);
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
        pio_interrupt_clear(return_keyboard_pio(),1);
        gpio_set_irq_enabled(PICO_SPI_CSN_PIN, GPIO_IRQ_EDGE_FALL, false); // disable the interrupt so that it does not fire again until the event is processed. This is to prevent the main loop from running when there is no event to process.
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
    event_type_t classify_event = EVENT_KEYBOARD_DETECTED;
    static int gary_code_mismatch_count = 0;

  //  WaitFallingEdge(PICO_SPI_CSN_PIN);
    //WaitCsnToFall(PICO_SPI_CSN_PIN);
    pio_interrupt_clear(return_keyboard_pio(),1);
    pio_sm_put(return_keyboard_pio(), return_keyboard_sm(), (uint32_t)ch << 24);
    gpio_put(DEBUG_PROBE_PIN, 1);

    
    // --- diagnostic: confirm what X actually holds right now ---
    uint32_t x_check = read_register(return_keyboard_pio(), return_keyboard_sm(), pio_x);
    gpio_put(PICO_SPI_SIDESET_PIN , (x_check == ((uint32_t)0xFF << 24)) ? 1 : 0);
    sleep_ms(500);
    gpio_put(PICO_SPI_SIDESET_PIN , 0);
    // -------------------------------------------------------------

    uint32_t status = save_and_disable_interrupts();
    if (!pio_sm_is_rx_fifo_empty(return_keyboard_pio(), return_keyboard_sm())) {
        uint32_t sampled = pio_sm_get(return_keyboard_pio(), return_keyboard_sm());
        uint8_t sampled_byte = sampled >> 24; // adjust shift based on your in_shift config/justification
        if (sampled_byte != GARY_CODE) {
            gary_code_mismatch_count++; // same diagnostic counter idea as before
        }
    }
    enqueue_interrupts(classify_event);
    restore_interrupts_from_disabled(status);
    return(false);
}

///PUBLIC bool keyboard_processing_main() {
///    uint8_t ch = 0;
///    event_type_t classify_event = EVENT_KEYBOARD_DETECTED;
///    static int gary_code_mismatch_count = 0;
///
///    if(dequeue_keyboard(&ch)){
/// //start transaction only when character is detected
///            pio_interrupt_clear(return_keyboard_pio(),1);
///            WaitFallingEdge(PICO_SPI_CSN_PIN); // wait for csn to fall before starting the transaction
///            pio_sm_put(return_keyboard_pio(), return_keyboard_sm(), (uint32_t)ch << 24);
///
///            if(ch == '\r'){
///            // Force the SM's PC back to the very start (irq wait 1),
///            // ending the keyboard-shifting loop and re-arming the gate.
///                gpio_set_irq_enabled(PICO_SPI_CSN_PIN, GPIO_IRQ_EDGE_FALL, true);
///                keyboard_check = false;
///                pio_sm_exec_wait_blocking(return_keyboard_pio(), return_keyboard_sm(), pio_encode_jmp(pio_keyboard.offset)); // offset 0 = keyboard_initial_processing
///                hw_set_bits(&(return_keyboard_pio())->irq, 1u << 1); 
///                classify_event = EVENT_DONE; // queing up the event
///                uint32_t status = save_and_disable_interrupts();
///                enqueue_interrupts(classify_event);
///                restore_interrupts_from_disabled(status);
///                return(true); // Simply return early
///            }
///    }
///    if (!pio_sm_is_rx_fifo_empty(return_keyboard_pio(), return_keyboard_sm())) {
///        uint32_t sampled = pio_sm_get(return_keyboard_pio(), return_keyboard_sm());
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

    uint32_t status = save_and_disable_interrupts();
    skip_next = true;
    already_fired = false;
    enqueue_interrupts(EVENT_NONE);
    restore_interrupts_from_disabled(status);
}

PRIVATE uint read_register(PIO pio, uint sm, enum pio_src_dest reg) {
    uint move_isr = pio_encode_mov(pio_isr, reg);
    pio_sm_exec_wait_blocking(pio, sm, move_isr);
    uint push = pio_encode_push(false, false);
    pio_sm_exec_wait_blocking(pio, sm, push);
    return pio_sm_get(pio, sm);
}

PRIVATE inline void WaitCsnToFall(uint gpio) {
    while (gpio_get(gpio) != 0) {
        tight_loop_contents();
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


PUBLIC int const return_channel(void)
{
    return pio_spi.dma_chan;
}
