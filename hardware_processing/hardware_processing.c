// Copyright (c) 2021 Michael Stoops. All rights reserved.
// Portions copyright (c) 2021 Raspberry Pi (Trading) Ltd.
// 
// Redistribution and use in source and binary forms, with or without modification, are permitted provided that the 
// following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
//    disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
//    following disclaimer in the documentation and/or other materials provided with the distribution.
// 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote
//    products derived from this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE 
// USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// SPDX-License-Identifier: BSD-3-Clause
//


#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware_processing.h"
#include "hardware/clocks.h"
#include "file_processing.h"
#include "queue.h"          
#include "hardware/sync.h"
#include "pico/time.h"
#include "hardware/structs/iobank0.h"
#include "clocked_input.pio.h"
#include "keyboard_input.pio.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"


#define PIO_SERIAL_CLKDIV                   10.f

typedef struct {
    PIO pio;
    uint sm;
    int dma_chan; // DMA channel for PIO transfers
    dma_channel_config pio_dma_chan_config; // DMA channel configuration for PIO transfers
    uint PIO_IRQ; // PIO interrupt number for SPI
} pio_spi_t;

typedef struct {
    PIO pio;
    uint sm;
    uint PIO_IRQ; // PIO interrupt number for keyboard
} pio_keyboard_t;

static pio_spi_t pio_spi; 
static pio_keyboard_t pio_keyboard;

PRIVATE void myIRQHandler(uint gpio, uint32_t events); 
PRIVATE void pioIRQ();
PRIVATE PIO return_pio_for_spi();
PUBLIC void set_gpio_pins();


PRIVATE void gpio_clear_events(uint gpio, uint32_t events) {
    gpio_acknowledge_irq(gpio,events);
}

PRIVATE void pioIRQ() {
    // Clear the PIO interrupt to ensure that we can receive future interrupts from the PIO, which is important for efficient handling of the data and better overall performance of the program. By clearing the interrupt, we can ensure that we can continue to receive interrupts for future SPI transactions and keyboard inputs without any issues related to interrupt handling.
    if(pio_interrupt_get(pio_spi.pio, 0)) {
  //      dma_start_channel_mask(1u << pio_spi.dma_chan);
        pio_interrupt_clear(pio_spi.pio, 0);
        if(gpio_get(PICO_DEFAULT_KEYBOARD_PIN)==1){
            pio_interrupt_clear(pio_keyboard.pio, 1);
        }
    }

    else if(pio_interrupt_get(pio_spi.pio, 1)) {
        // Clear the PIO interrupt to ensure that we can receive future interrupts from the PIO, which is important for efficient handling of the data and better overall performance of the program. By clearing the interrupt, we can ensure that we can continue to receive interrupts for future SPI transactions and keyboard inputs without any issues related to interrupt handling.
        check_data(); // Check the data in the buffer to see if we have received new data from the SPI or keyboard, which is important to ensure that we can start processing the data and writing it to the USB as soon as possible without any issues related to timing or data corruption. By checking the data, we can ensure that we can efficiently handle the data and provide a better overall performance of the program.
    }

    else if(pio_interrupt_get(pio_keyboard.pio, 0)) {
        // Clear the PIO interrupt to ensure that we can receive future interrupts from the PIO, which is important for efficient handling of the data and better overall performance of the program. By clearing the interrupt, we can ensure that we can continue to receive interrupts for future SPI transactions and keyboard inputs without any issues related to interrupt handling.
        if(keyboard_check){
            pio_interrupt_clear(pio_keyboard.pio, 1);
        } 
    }
   
    
    pio_interrupt_clear(return_pio_for_spi(), 0);
}


//PRIVATE void irq_handler(void)
//{
    //uint pin = PICO_DEFAULT_SPI_CSN_PIN;
    //uint shift = 4 * (pin & 7);
    //uint32_t mask = (GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL) << shift;

    //// Read raw interrupt status
    //uint32_t status = iobank0_hw->intr[pin >> 3];

    //// Check FALLING edge (HIGH → LOW)
    //if (status & (GPIO_IRQ_EDGE_FALL << shift)) {

        //bool level = gpio_get(pin);  // read actual level (should be 0)

        //if (!level) {
            //// CONFIRMED LOW
            //// CSN asserted → start transaction
        //}
    //}

    //// Check RISING edge (LOW → HIGH)
    //if (status & (GPIO_IRQ_EDGE_RISE << shift)) {

        //bool level = gpio_get(pin);  // should be 1

        //if (level) {
            //// CONFIRMED HIGH
            //// CSN deasserted → end transaction
        //}
    //}

    //// Clear interrupt (VERY IMPORTANT)
    //iobank0_hw->intr[pin >> 3] = mask;
//}


PUBLIC void set_gpio_pins() {
    gpio_init(PICO_DEFAULT_SPI_CSN_PIN );
    gpio_set_dir(PICO_DEFAULT_SPI_CSN_PIN , GPIO_IN);
    gpio_pull_up(PICO_DEFAULT_SPI_CSN_PIN); // Pull-up to ensure a defined state when not driven
    
    gpio_init(PICO_DEFAULT_KEYBOARD_PIN );
    gpio_set_dir(PICO_DEFAULT_KEYBOARD_PIN , GPIO_IN);
    gpio_pull_down(PICO_DEFAULT_KEYBOARD_PIN); // Pull-down to ensure a defined state when not driven
} 

    // This is to setup the GPIO interrupts. It is initially disabled. No clue as to how it works however LOL.
    // Reference: https://forums.raspberrypi.com/viewtopic.php?t=325355
   // irq_set_enabled(IO_IRQ_BANK0, false);
    //uint pin =PICO_DEFAULT_SPI_CSN_PIN;
    //iobank0_hw->intr[pin >> 3] =
    //(GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL)
    //<< (4 * (pin & 7));
    //irq_set_priority(IO_IRQ_BANK0, PICO_HIGHEST_IRQ_PRIORITY);
    //hw_set_bits(&iobank0_hw->proc1_irq_ctrl.inte[pin>>3], GPIO_IRQ_EDGE_RISE|GPIO_IRQ_EDGE_FALL << 4*(pin&7));
    //irq_set_exclusive_handler(IO_IRQ_BANK0, irq_handler);
    //irq_set_enabled(IO_IRQ_BANK0, true);
   //// gpio_set_irq_enabled_with_callback(PICO_DEFAULT_SPI_CSN_PIN, GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE, true, &myIRQHandler);

PUBLIC void pio_dma_setup(void) {
    pio_spi.pio = pio1; // Use PIO 1 for SPI
    pio_spi.PIO_IRQ = pio_get_irq_num(pio_spi.pio, 0); // Get the appropriate IRQ number for the PIO instance and interrupt index 0, which is important to ensure that we can set the correct interrupt handler for the PIO interrupts. By getting the correct IRQ number, we can ensure that we can efficiently handle the data and provide a better overall performance of the program.
    uint offset = pio_add_program(pio_spi.pio, &clocked_input_program);

    pio_spi.sm = pio_claim_unused_sm(pio_spi.pio, true);

    clocked_input_program_init(
        pio_spi.pio,
        pio_spi.sm,
        offset,
        PICO_DEFAULT_SPI_RX_PIN,
        PICO_DEFAULT_SPI_CSN_PIN,
		PICO_DEFAULT_KEYBOARD_PIN
    );

   pio_set_irq0_source_mask_enabled(pio_spi.pio, pis_interrupt0|pis_interrupt1, true); // Enable both interrupt sources for the PIO, which is important to ensure that we can receive interrupts for both SPI transactions and keyboard inputs without any issues related to interrupt handling. By enabling both interrupt sources, we can efficiently handle the data and provide a better overall performance of the program.
   irq_set_exclusive_handler(pio_spi.PIO_IRQ, pioIRQ); // Set the interrupt handler for the PIO interrupts to the pioIRQ function, which is important to ensure that we can efficiently handle the data and provide a better overall performance of the program. By setting the exclusive handler, we can ensure that the pioIRQ function is called whenever there is an interrupt from the PIO, allowing us to efficiently process the data and write it to the USB without any issues related to timing or data corruption.
   irq_set_enabled(pio_spi.PIO_IRQ, true); // Enable the PIO interrupts, which is important to ensure that we can receive interrupts for both SPI transactions and keyboard inputs without any issues related to interrupt handling. By enabling the interrupts, we can efficiently handle the data and provide a better overall performance of the program. 
    // Claim DMA channel
    pio_spi.dma_chan = dma_claim_unused_channel(true);
    pio_spi.pio_dma_chan_config = dma_channel_get_default_config(pio_spi.dma_chan);
    //Tranfers 8-bits at a time
    channel_config_set_transfer_data_size(&pio_spi.pio_dma_chan_config, DMA_SIZE_8); //sets the size of each DMA transfer to 32 bits
    channel_config_set_read_increment(&pio_spi.pio_dma_chan_config, false); //Disabled when reading from peripheral, as the source address is fixed
    channel_config_set_write_increment(&pio_spi.pio_dma_chan_config, true); //Writing into array, so set to true.  // channel_config_set_dreq(&pio_spi.pio_dma_chan_config, DREQ_PIO1_RX0); //Configures the DMA channel to be triggered by the PIO's RX FIFO for the specific state machine. This means that a DMA transfer will occur whenever there is data in the RX FIFO of the PIO state machine, allowing for efficient data handling without CPU intervention.  //    channel_config_set_dreq( &pio_spi.pio_dma_chan_config, DREQ_PIO1_RX0 + pio_spi.sm); //Configures the DMA channel to be triggered by the PIO's RX FIFO for the specific state machine. This means that a DMA transfer will occur whenever there is data in the RX FIFO of the PIO state machine, allowing for efficient data handling without CPU intervention.
    channel_config_set_dreq(&pio_spi.pio_dma_chan_config, pio_get_dreq(pio_spi.pio, pio_spi.sm, false)); //Configures the DMA channel to be triggered by the PIO's RX FIFO for the specific state machine. This means that a DMA transfer will occur whenever there is data in the RX FIFO of the PIO state machine, allowing for efficient data handling without CPU intervention.
    dma_channel_configure(
        pio_spi.dma_chan, 
        &pio_spi.pio_dma_chan_config,
        give_array_address(), // Destination address where data is written to memory
        &pio_spi.pio->rxf[pio_spi.sm], // PIO RX FIFO, // Destination address in memory where data is read from the PIO's RX FIFO
        BUF_LEN, // Number of transfers (bytes) to perform
        false); //start later

}

PUBLIC void pio_keyboard_setup(void) {
    pio_keyboard.pio = pio1; // Use PIO 1 for SPI
    pio_keyboard.PIO_IRQ = pio_get_irq_num(pio_keyboard.pio, 0); // Get the appropriate IRQ number for the PIO instance and interrupt index 0, which is important to ensure that we can set the correct interrupt handler for the PIO interrupts. By getting the correct IRQ number, we can ensure that we can efficiently handle the data and provide a better overall performance of the program.
    uint offset = pio_add_program(pio_keyboard.pio, &keyboard_input_program);
    pio_keyboard.sm = pio_claim_unused_sm(pio_keyboard.pio, true);
    keyboard_input_program_init(
        pio_keyboard.pio,
        pio_keyboard.sm,
        offset,
        PICO_DEFAULT_SPI_TX_PIN,
        PICO_DEFAULT_SPI_SCK_PIN);
    pio_set_irq1_source_mask_enabled(pio_keyboard.pio, pis_interrupt0|pis_interrupt1, true); // Enable both interrupt sources for the PIO, which is important to ensure that we can receive interrupts for both SPI transactions and keyboard inputs without any issues related to interrupt handling. By enabling both interrupt sources, we can efficiently handle the data and provide a better overall performance of the program.
   irq_set_exclusive_handler(pio_keyboard.PIO_IRQ, pioIRQ); // Set the interrupt handler for the PIO interrupts to the pioIRQ function, which is important to ensure that we can efficiently handle the data and provide a better overall performance of the program. By setting the exclusive handler, we can ensure that the pioIRQ function is called whenever there is an interrupt from the PIO, allowing us to efficiently process the data and write it to the USB without any issues related to timing or data corruption.
   irq_set_enabled(pio_keyboard.PIO_IRQ, true); // Enable the PIO interrupts, which is important to ensure that we can receive interrupts for both SPI transactions and keyboard inputs without any issues related to interrupt handling. By enabling the interrupts, we can efficiently handle the data and provide a better overall performance of the program. 
   
}

PUBLIC PIO return_pio() {
    return pio_keyboard.pio;
}
PRIVATE PIO return_pio_for_spi() {
    return pio_spi.pio;
}

PUBLIC uint return_sm() {
    return pio_keyboard.sm;
}
PUBLIC int return_channel(){
    return pio_spi.dma_chan; // Return the DMA channel number used for PIO transfers
}

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




