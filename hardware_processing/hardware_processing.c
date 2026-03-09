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
// Example of an SPI bus slave using the PL022 SPI interface

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
#include "hardware/dma.h"

#define spi_default                         spi0
#define PICO_DEFAULT_SPI_SCK_PIN            3   // SPI clock, same as master
#define PICO_DEFAULT_SPI_RX_PIN             2
#define PICO_DEFAULT_SPI_TX_PIN             5
#define PICO_DEFAULT_SPI_CSN_PIN            4    
#define DEBUG_PIN                           6


#define PIO_SERIAL_CLKDIV                   10.f

//#define PICO_DEFAULT_SPI_SCK_PIN            2   // SPI clock, same as master
//#define PICO_DEFAULT_SPI_RX_PIN             0   // MOSI from master → receive on slave
//#define PICO_DEFAULT_SPI_TX_PIN             3   // MISO to master → send from slave
//#define PICO_DEFAULT_SPI_CSN_PIN            1   // Chip select
//   // GPIO pin for SPI interrupt line from master

typedef struct {
    PIO pio;
    uint sm;
    int data_dma_chan; // DMA channel for PIO transfers
    int control_dma_chan; // DMA channel for control transfers to reset the data DMA channel
    dma_channel_config pio_dma_chan_data_config; // DMA channel configuration for PIO transfers
    dma_channel_config pio_dma_chan_control_config; // DMA channel configuration for control transfers to reset the data DMA channel
} pio_spi_t;

static pio_spi_t pio_spi; 

PRIVATE void myIRQHandler(uint gpio, uint32_t events); 
PRIVATE void dma_init(void);

PRIVATE void gpio_clear_events(uint gpio, uint32_t events) {
    gpio_acknowledge_irq(gpio,events);
}


PRIVATE void myIRQHandler(uint gpio, uint32_t events) 
{
     int count = 2;
     BYTE *check;

    if(events & GPIO_IRQ_EDGE_FALL)
    {
        dma_start_channel_mask(1u << pio_spi.control_dma_chan); // Set the ping pong in motion by starting the control DMA channel, which will trigger the data DMA channel to start the first SPI data transfer
        uint32_t status = save_and_disable_interrupts(); // Disable interrupts to ensure atomic access to shared resources
        csn_high = true; // Set flag to indicate that CSN is active (low)
        restore_interrupts(status); // Restore previous interrupt state
    }

    if(events & GPIO_IRQ_EDGE_RISE)
    {
        dma_hw->ints0 = 1u << pio_spi.data_dma_chan; // Clear the interrupt request for the data DMA channel to acknowledge that the SPI transaction is complete and allow for future transactions to be detected
        uint32_t status = save_and_disable_interrupts();
        csn_high = false; // Set flag to indicate that CSN is high (inactive) 
        restore_interrupts(status); // Restore previous interrupt state; // Update the queue size based on the number of bytes received, which may be used to manage the data buffer and ensure that it does not overflow or to trigger processing of the received data once a complete message has been received.
        
        // dma_channel_unclaim(pio_spi.dma_chan); // Unclaim the DMA channel to free it up for future use when CSN goes high, indicating the end of an SPI transaction    
    }
}




// Need to set up two DMA cahnnels. One shiffles data off to the  buffer while the other resets the first one.
PUBLIC void pio_dma_setup(void)
{
    pio_spi.pio = pio0;
    uint offset = pio_add_program(pio_spi.pio, &clocked_input_program);
    pio_spi.sm = pio_claim_unused_sm(pio_spi.pio, true);
    clocked_input_program_init(pio_spi.pio, pio_spi.sm, offset,PICO_DEFAULT_SPI_RX_PIN,PICO_DEFAULT_SPI_CSN_PIN  );

    // Select DMA channels 
    pio_spi.control_dma_chan = dma_claim_unused_channel(true);
    pio_spi.data_dma_chan = dma_claim_unused_channel(true);

    //Setup the control channel 
    pio_spi.pio_dma_chan_control_config = dma_channel_get_default_config(pio_spi.control_dma_chan);
    channel_config_set_transfer_data_size(&pio_spi.pio_dma_chan_control_config, DMA_SIZE_32); //sets the size of each DMA transfer to 32 bits
    channel_config_set_read_increment(&pio_spi.pio_dma_chan_control_config, false); //Disabled when reading from peripheral, as the source address is fixed
    channel_config_set_write_increment(&pio_spi.pio_dma_chan_control_config, false); // We are writing to a fixed address (the control register of the data DMA channel), so we don't want the write address to increment after each transfer
    channel_config_set_chain_to(&pio_spi.pio_dma_chan_control_config, pio_spi.data_dma_chan); // Chain the control DMA channel to the data DMA channel, so that when the control transfer is complete, the data channel will automatically trigger to start the next data transfer
    channel_config_set_ring(&pio_spi.pio_dma_chan_control_config, true, 3); // 1 << 3 byte boundary on write ptr
     // For control transfer to reset the data DMA channel    
     dma_channel_configure(
        pio_spi.data_dma_chan, // Channel to configure
        &pio_spi.pio_dma_chan_control_config, // Channel configuration for PIO data transfers
        give_reset_array_address(), // Write to data channel read address
        &dma_hw->ch[pio_spi.data_dma_chan].read_addr, // Not important where to read from
        1, // Number of transfers (bytes) to perform
        false); // Don't start immediately, will be triggered by control channel when CSN goes low  
       
    //Setup the data channel
    pio_spi.pio_dma_chan_data_config = dma_channel_get_default_config(pio_spi.data_dma_chan);
    //Tranfers 8-bits at a time
    channel_config_set_transfer_data_size(&pio_spi.pio_dma_chan_data_config, DMA_SIZE_8); //sets the size of each DMA transfer to 32 bits
    channel_config_set_read_increment(&pio_spi.pio_dma_chan_data_config, false); //Disabled when reading from peripheral, as the source address is fixed
    channel_config_set_write_increment(&pio_spi.pio_dma_chan_data_config, true); // We are writing to a buffer in memory, so we want the write address to increment after each transfer to go to the next position in the buffer
    channel_config_set_dreq(&pio_spi.pio_dma_chan_data_config, 0x3b); // DREQ paced by time0
    // chain to the controller DMA channel 
    channel_config_set_chain_to(&pio_spi.pio_dma_chan_data_config, pio_spi.control_dma_chan); // Chain the data DMA channel to the control DMA channel, so that when the data transfer is complete, the control channel will automatically trigger to reset the data DMA channel for the next transfer
 

     dma_channel_configure(
        pio_spi.control_dma_chan, // Channel to configure
        &pio_spi.pio_dma_chan_data_config, // Channel configuration for control transfers to reset
        give_array_address(), // Reserring it back to the beginning of the buffer in memory where data is written to by the data DMA channel
        &(pio_spi.pio->rxf[0]), // Destination address in memory where data is read from the PIO's RX FIFO
        BUF_LEN, // Number of transfers (bytes) to perform
        false); //start immediatel
}


/* PRIVATE void dma_init(void)
{   

    // For data transfer
    dma_channel_configure(
        pio_spi.control_dma_chan, // Channel to configure
        &pio_spi.pio_dma_chan_control_config, // Channel configuration for control transfers to reset
        give_array_address(), // Destination address where data is written to memory
        &(pio_spi.pio->rxf[0]), // Destination address in memory where data is read from the PIO's RX FIFO
        BUF_LEN, // Number of transfers (bytes) to perform
        false); //start immediately

     // For control transfer to reset the data DMA channel    
     dma_channel_configure(
        pio_spi.data_dma_chan, // Channel to configure
        &pio_spi.pio_dma_chan_config, // Channel configuration for PIO data transfers
        &dma_hw->ch[pio_spi.data_dma_chan].read_addr, // Write address data channel read address
        &(pio_spi.pio->rxf[0]), // Source address in memory where data is read from the PIO's RX FIFO
        1, // Number of transfers (bytes) to perform
        false); // Don't start immediately, will be triggered by control channel when CSN goes low  
       

   //     dma_channel_wait_for_finish_blocking(pio_spi.dma_chan); // Waits for the DMA transfer to complete before proceeding. This ensures that all data has been transferred from the PIO's RX FIFO to the data buffer in memory before any further processing is done.
}
 */

PUBLIC void set_gpio_pins(){
    gpio_init(PICO_DEFAULT_SPI_CSN_PIN );
    gpio_set_dir(PICO_DEFAULT_SPI_CSN_PIN , GPIO_IN);
    gpio_pull_up(PICO_DEFAULT_SPI_CSN_PIN); // Pull-up to ensure a defined state when not driven
    gpio_set_irq_enabled_with_callback(PICO_DEFAULT_SPI_CSN_PIN, GPIO_IRQ_EDGE_FALL|GPIO_IRQ_EDGE_RISE, true, &myIRQHandler);
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