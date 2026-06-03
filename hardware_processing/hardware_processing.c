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
//#include "keyboard_input.pio.h"

// -----------------------------------------------------------------------------
// STRUCTURES
// -----------------------------------------------------------------------------

typedef struct {
    PIO pio;
    uint sm;
    int dma_chan;
    uint32_t size;
    dma_channel_config dma_cfg;
    uint8_t letter;
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
PRIVATE void my_gpio_isr(uint gpio, uint32_t events);
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
//  __not_in_flash_func
PRIVATE void my_gpio_isr(uint gpio, uint32_t events) {
  //  uint32_t events =  gpio_get_irq_event_mask(PICO_DEFAULT_SPI_CSN_PIN);
static uint8_t count = 0;
  //  gpio_acknowledge_irq(PICO_DEFAULT_SPI_CSN_PIN,events);

    // -------------------------------------------------------------------------
    // CSn LOW -> START DMA AND CHECK STAGES 

    if (events & GPIO_IRQ_EDGE_FALL) {

        if(current_packet == PACKET_NONE) {
            uint32_t status = save_and_disable_interrupts(); // disable interrupts to ensure that the packet classification and processing is not interrupted, which could lead to data corruption or other issues
            current_packet = PACKET_START;
            size_byte_set = true;
            restore_interrupts(status); // re-enable interrupts
        }

        if(current_packet == PACKET_USB) {
            dma_start_channel_mask(1u << return_channel()); 
            uint32_t status = save_and_disable_interrupts(); // disable interrupts to ensure that the packet classification and processing is not interrupted, which could lead to data corruption or other issues
            usb_check = true;
            keyboard_check = false; // ensure that the keyboard processing is not active while processing the USB packet, which could lead to data corruption or other issues if both are active at the same time
            restore_interrupts(status); // re-enable interrupts
        }

        if(current_packet == PACKET_KEYBOARD) {
            uint32_t status = save_and_disable_interrupts(); // disable interrupts to ensure that the packet classification and processing is not interrupted, which could lead to data corruption or other issues
            keyboard_check = true;
            usb_check = false; // ensure that the USB processing is not active while processing the keyboard packet, which could lead to data corruption or other issues if both are active at the same time
            restore_interrupts(status); // re-enable interrupts
        }

    }

    if (events & GPIO_IRQ_EDGE_RISE) {

        if(current_packet == PACKET_START) {
            pio_spi.size = pio_sm_get( return_spi_pio(), return_spi_sm() ); // read the first byte of the packet, which is the size byte, and store it in the pio_spi struct for later use
        }


        if(current_packet == PACKET_NONE) {
            pio_sm_restart(return_spi_pio(), return_spi_sm()); // reset the PIO state machine to be ready for the next packet
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
    
    // RX pin setup

        // Map PIO pins
    pio_gpio_init(return_spi_pio(), PICO_DEFAULT_SPI_RX_PIN);
    pio_gpio_init(return_spi_pio(), PICO_DEFAULT_SPI_SCK_PIN);
    pio_gpio_init(return_spi_pio(), PICO_DEFAULT_SPI_TX_PIN);

    pio_sm_restart(return_spi_pio(), return_spi_sm()); // reset the PIO state machine to ensure that it is in a known state before starting to process packets

    // Set up GPIO IRQ for CSN pin
    gpio_set_irq_enabled_with_callback(
        PICO_DEFAULT_SPI_CSN_PIN,
        GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE,
        true,
        &my_gpio_isr
    );
}

// -----------------------------------------------------------------------------
// PIO + DMA SETUP
// -----------------------------------------------------------------------------
//  PUBLIC void keyboard_pio_setup(void) {
//     PIO pio = pio0;
//     uint offset = pio_add_program(pio, &keyboard_input_program);
//     uint sm = pio_claim_unused_sm(pio, true);
//     pio_keyboard.pio = pio;
//     pio_keyboard.sm = sm;   
//     keyboard_input_program_init(pio, sm, offset, PICO_DEFAULT_SPI_TX_PIN);

//  }


PUBLIC void pio_dma_setup(void) {
    PIO pio = pio0;
    pio_spi.pio = pio;

    pio_spi.sm = pio_claim_unused_sm( pio_spi.pio, true);

    uint offset = pio_add_program( pio_spi.pio, &clocked_input_program);


    clocked_input_program_init(
        pio_spi.pio,
        pio_spi.sm,
        offset,
        PICO_DEFAULT_SPI_RX_PIN,
        PICO_DEFAULT_SPI_CSN_PIN
    );

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

//PUBLIC void pio_keyboard_setup(void)
//
//   pio_keyboard.pio = pio1;
//
//   pio_keyboard.sm =
//       pio_claim_unused_sm(
//           pio_keyboard.pio,
//           true
//       );
//
//   uint offset = pio_add_program(
//       pio_keyboard.pio,
//       &keyboard_input_program
//   );
//
//   keyboard_input_program_init(
//       pio_keyboard.pio,
//       pio_keyboard.sm,
//       offset,
//       PICO_DEFAULT_SPI_RX_PIN
//   );


//PUBLIC void spi_slave_writing(void)
//{
     //spi_init(spi_default, 1000 * 1000);
    //spi_set_slave(spi_default, true);
    //gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
    //gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
    //gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);
    //gpio_set_function(PICO_DEFAULT_SPI_CSN_PIN, GPIO_FUNC_SPI);
    //// Make the SPI pins available to picotool
    //bi_decl(bi_4pins_with_func(PICO_DEFAULT_SPI_RX_PIN, PICO_DEFAULT_SPI_TX_PIN, PICO_DEFAULT_SPI_SCK_PIN, PICO_DEFAULT_SPI_CSN_PIN, GPIO_FUNC_SPI));

    
//}

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

//    PUBLIC PIO return_keyboard_pio(void)
//    {
//        return pio_keyboard.pio;
//    }
//
//    PUBLIC uint return_keyboard_sm(void)
//    {
//        return pio_keyboard.sm;
//    }
PUBLIC int return_channel(void)
{
    return pio_spi.dma_chan;
}


PUBLIC void set_size(uint32_t size) {
    pio_spi.size = size;
}

PUBLIC uint32_t get_size(void) {
    return pio_spi.size;
}

PUBLIC void set_char(uint8_t letter) {
    pio_spi.letter = letter;
}

PUBLIC uint8_t get_char(void) {
    return pio_spi.letter;
}