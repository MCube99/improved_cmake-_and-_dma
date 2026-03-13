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
    int dma_chan; // DMA channel for PIO transfers
    dma_channel_config pio_dma_chan_config; // DMA channel configuration for PIO transfers
} pio_spi_t;

static pio_spi_t pio_spi; 

PRIVATE void myIRQHandler(uint gpio, uint32_t events); 
PRIVATE void dma_init(void);

PRIVATE void gpio_clear_events(uint gpio, uint32_t events) {
    gpio_acknowledge_irq(gpio,events);
}


PRIVATE void myIRQHandler(uint gpio, uint32_t events) 
{

    if(events & GPIO_IRQ_EDGE_FALL)
    {
        dma_init(); // Set up DMA to transfer data from PIO to memory when CSN goes low, indicating the start of an SPI transaction
        uint32_t status = save_and_disable_interrupts(); // Disable interrupts to ensure atomic access to shared resources
        csn_high = true; // Set flag to indicate that CSN is active (low)
        restore_interrupts(status); // Restore previous interrupt state
    }

    if(events & GPIO_IRQ_EDGE_RISE)
    {
        uint32_t status = save_and_disable_interrupts();
        csn_high = false; // Set flag to indicate that CSN is high (inactive) 
        restore_interrupts(status); // Restore previous interrupt state
         dma_channel_abort(pio_spi.dma_chan); // Unclaim the DMA channel to free it up for future use when CSN goes high, indicating the end of an SPI transactio
    }
}


PUBLIC void pio_dma_setup(void)
{
    pio_spi.pio = pio0;

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

    dma_channel_config c = dma_channel_get_default_config(pio_spi.dma_chan);

    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);

    channel_config_set_dreq(
        &c,
        pio_get_dreq(pio_spi.pio, pio_spi.sm, false)
    );

    pio_spi.pio_dma_chan_config = c;
}


PRIVATE void dma_init(void)
{   
    dma_channel_configure(
        pio_spi.dma_chan, 
        &pio_spi.pio_dma_chan_config,
        give_array_address(), // Destination address where data is written to memory
        &pio_spi.pio->rxf[pio_spi.sm], // PIO RX FIFO, // Destination address in memory where data is read from the PIO's RX FIFO
        BUF_LEN, // Number of transfers (bytes) to perform
        true); //start immediately



        dma_channel_wait_for_finish_blocking(pio_spi.dma_chan); // Waits for the DMA transfer to complete before proceeding. This ensures that all data has been transferred from the PIO's RX FIFO to the data buffer in memory before any further processing is done.
}


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

PUBLIC bool is_dma_done(void)
{
    return(!dma_channel_is_busy(pio_spi.dma_chan));
}

PUBLIC void wait_for_dma_to_be_done(void)
{
    dma_channel_wait_for_finish_blocking(pio_spi.dma_chan);
}
