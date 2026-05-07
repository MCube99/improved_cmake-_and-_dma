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

//#define PICO_DEFAULT_SPI_SCK_PIN            2   // SPI clock, same as master
//#define PICO_DEFAULT_SPI_RX_PIN             0   // MOSI from master → receive on slave
//#define PICO_DEFAULT_SPI_TX_PIN             3   // MISO to master → send from slave
//#define PICO_DEFAULT_SPI_CSN_PIN            1   // Chip select                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      
//   // GPIO pin for SPI interrupt line from master

typedef struct {
    PIO pio;
    uint sm;
    int dma_chan; // DMA channel for PIO transfers
    dma_channel_config pio_dma_chan_config; // DMA channel configuration for PIO transfers
} pio_spi_t;

typedef struct {
    PIO pio;
    uint sm;
} pio_keyboard_t;

static pio_spi_t pio_spi; 
static pio_keyboard_t pio_keyboard;

PRIVATE void myIRQHandler(uint gpio, uint32_t events); 
PRIVATE void pioIRQ();

PRIVATE void gpio_clear_events(uint gpio, uint32_t events) {
    gpio_acknowledge_irq(gpio,events);
}

PRIVATE void pioIRQ(){ 

    if(pio_interrupt_get(pio_keyboard.pio,3))
    {
        keyboard_check = false;
        pio_interrupt_clear(pio_keyboard.pio,3);
    }

    if(pio_interrupt_get(pio_keyboard.pio, 1))
    {
        keyboard_check = true;
        if(usb_check)
        {
            usb_check = false;
        }
        pio_interrupt_clear(pio_keyboard.pio,1);
    }
}



PRIVATE void myIRQHandler(uint gpio, uint32_t events) {
    if(events & GPIO_IRQ_EDGE_FALL) //simulate csn falling. This is becaue spi in pico is fubared so an alternative has to be set up. 
    {
        dma_start_channel_mask(1u << pio_spi.dma_chan); // Set up DMA to transfer data from PIO to memory when CSN goes low, indicating the start of an SPI transaction
    }

    if(events & GPIO_IRQ_EDGE_RISE) //this should be when it finishes writing  
    {
    //    gpio_set_irq_active(PICO_DEFAULT_SPI_CSN_PIN, GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE, false); //i will be continually blasted with this interrupt as long as the CSN pin is high, so I need to disable it until I am done processing the data from the SPI transaction. 
        check_data(); // Check if the SPI transaction is complete and the data in the buffer is ready to be processed. This function will set the appropriate flags in the flag_info variable, which will be checked in the main loop to determine when to read from the SPI and when to read from the keyboard.
   //     gpio_set_irq_active(PICO_DEFAULT_SPI_CSN_PIN, GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE, true); // Re-enable GPIO interrupts after we are done processing the data from the SPI transaction. This is important to ensure that we can continue to receive interrupts for future SPI transactions and keyboard inputs, allowing the program to function correctly and efficiently without data corruption or other issues.
    }
}


PUBLIC void pio_dma_setup(void) {
    pio_spi.pio = pio1;

    uint offset = pio_add_program(pio_spi.pio, &clocked_input_program);

    pio_spi.sm = pio_claim_unused_sm(pio_spi.pio, true);

    clocked_input_program_init(
        pio_spi.pio,
        pio_spi.sm,
        offset,
        PICO_DEFAULT_SPI_RX_PIN,
        PICO_DEFAULT_SPI_CSN_PIN
    );

    // Claim DMA channel
    pio_spi.dma_chan = dma_claim_unused_channel(true);
    pio_spi.pio_dma_chan_config = dma_channel_get_default_config(pio_spi.dma_chan);
    //Tranfers 8-bits at a time
    channel_config_set_transfer_data_size(&pio_spi.pio_dma_chan_config, DMA_SIZE_8); //sets the size of each DMA transfer to 32 bits
    channel_config_set_read_increment(&pio_spi.pio_dma_chan_config, false); //Disabled when reading from peripheral, as the source address is fixed
    channel_config_set_write_increment(&pio_spi.pio_dma_chan_config, true); //Writing into array, so set to true. 
   // channel_config_set_dreq(&pio_spi.pio_dma_chan_config, DREQ_PIO1_RX0); //Configures the DMA channel to be triggered by the PIO's RX FIFO for the specific state machine. This means that a DMA transfer will occur whenever there is data in the RX FIFO of the PIO state machine, allowing for efficient data handling without CPU intervention.
//    channel_config_set_dreq( &pio_spi.pio_dma_chan_config, DREQ_PIO1_RX0 + pio_spi.sm); //Configures the DMA channel to be triggered by the PIO's RX FIFO for the specific state machine. This means that a DMA transfer will occur whenever there is data in the RX FIFO of the PIO state machine, allowing for efficient data handling without CPU intervention.
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
    PIO pio;
    uint sm;
    uint offset;

    offset = pio_add_program(pio_keyboard.pio, &keyboard_input_program);

    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&keyboard_input_program, &pio, &sm, &offset, PICO_DEFAULT_SPI_TX_PIN, 1, true);
    pio_keyboard.pio = pio;
    pio_keyboard.sm = sm;
    hard_assert(success);
    
     keyboard_input_program_init(
        pio_keyboard.pio,
        pio_keyboard.sm,
        offset,
        PICO_DEFAULT_SPI_TX_PIN,
        PICO_DEFAULT_SPI_SCK_PIN,
        PICO_DEFAULT_SPI_CSN_PIN
    );

}

PUBLIC PIO return_pio() {
    return pio_keyboard.pio;
}

PUBLIC uint return_sm() {
    return pio_keyboard.sm;
}




PUBLIC void set_gpio_pins(){
    static uint PIO_IRQ;    // NVIC ARM CPU interrupt number SS

    gpio_init(PICO_DEFAULT_SPI_CSN_PIN );
    gpio_set_dir(PICO_DEFAULT_SPI_CSN_PIN , GPIO_IN);
    gpio_pull_up(PICO_DEFAULT_SPI_CSN_PIN); // Pull-up to ensure a defined state when not driven
    PIO_IRQ = pio_keyboard.pio ? PIO1_IRQ_0 : PIO0_IRQ_0;  // Selects the NVIC PIO_IRQ to us
    gpio_set_irq_enabled_with_callback(PICO_DEFAULT_SPI_CSN_PIN, GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE, true, &myIRQHandler);
    pio_set_irq0_source_enabled(pio_keyboard.pio, pis_interrupt0, true);
    pio_set_irq0_source_enabled(pio_keyboard.pio, pis_interrupt3, true);
    
    irq_set_exclusive_handler(PIO_IRQ, pioIRQ);
    irq_set_enabled(PIO_IRQ, true);                    //enabling the PIO1_IRQ_0
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


PUBLIC int return_channel(){
    return pio_spi.dma_chan; // Return the DMA channel number used for PIO transfers
}


