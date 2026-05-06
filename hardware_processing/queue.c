#include <stdio.h>
#include <stdlib.h>
#include "queue.h"
#include <string.h>
#include "hardware/sync.h"
#include "hardware/dma.h"
#include "hardware_processing.h"



#define GARY_CODE                           0xFE // Arbitrary value to indicate that the data in the buffer is from the keyboard, not the SPI. This is necessary since both the keyboard and SPI write into the same buffer.

struct queue_type {
    BYTE buffer[ BUF_LEN ];   
}; 

static struct queue_type myQueue; // Static instance of the queue structure


PUBLIC void queue_init( ){
    memset(myQueue.buffer, 0, BUF_LEN); // initialize all buffer elements to 0
}

PUBLIC uint8_t* give_array_address(void){
    return (uint8_t*)&myQueue.buffer[0];
}

PUBLIC uint8_t*  give_array_address_for_file_writing() {
    return (uint8_t*)&myQueue.buffer[1];
}

PUBLIC int get_queue_size() {
    return(myQueue.buffer[0]);
}

PUBLIC inline void check_data(void) {
    uintptr_t base = (uintptr_t)&myQueue.buffer[0];
    uintptr_t write = dma_hw->ch[return_channel()].write_addr;

    uint32_t difference = write - base;
    uint8_t first = myQueue.buffer[0];

    if (difference == (first + 1)) {
        uint32_t status = save_and_disable_interrupts(); // Disable interrupts to ensure atomicity of the following operations, which is important to prevent data corruption or other issues that could arise from concurrent access to shared resources such as the buffer and flags. By disabling interrupts, we can ensure that the program functions correctly and efficiently without any issues related to interrupt handling.
        usb_check = true;
        keyboard_check = false;
        restore_interrupts(status); // Restore interrupts after we are done processing the SPI data, which is important to allow the program to continue functioning correctly and efficiently without any issues related to interrupt handling. By restoring interrupts, we can ensure that the program can continue to receive interrupts for future SPI transactions and keyboard inputs, allowing for efficient handling of the data and better overall performance of the program.
    }

    else if (first == GARY_CODE) {
        // need to clear the interrupt to make sure that the keyboard_input.pio can start
        uint32_t status = save_and_disable_interrupts(); // Disable interrupts to ensure atomicity of the following operations, which is important to prevent data corruption or other issues that could arise from concurrent access to shared resources such as the buffer and flags. By disabling interrupts, we can ensure that the program functions correctly and efficiently without any issues related to interrupt handling.
        usb_check = false; // Set the CSN_USB_EVENT flag to indicate that the SPI transaction is complete and the data in the buffer is from the SPI, so we can start processing the SPI data and writing it to the USB. This is necessary because we need to wait until the SPI transaction is complete before we can start processing the SPI data, which could lead to data corruption or other issues if we start processing it too early.
        keyboard_check = true; // Set the KEYBOARD_EVENT flag to indicate that the data in the buffer is from the keyboard, not the SPI, so we can start processing the keyboard data and writing it to the USB. This is necessary since both the keyboard and SPI write into the same buffer, so we need to differentiate between them to ensure that we process the data correctly and efficiently without any issues related to data corruption or other issues.
        restore_interrupts(status); // Restore interrupts after we are done processing the SPI data, which is important to allow the program to continue functioning correctly and efficiently without any issues related to interrupt handling. By restoring interrupts, we can ensure that the program can continue to receive interrupts for future SPI transactions and keyboard inputs, allowing for efficient handling of the data and better overall performance of the program.
    }
}// End of queue.c file:

        //pio_interrupt_clear(return_pio(), 0);