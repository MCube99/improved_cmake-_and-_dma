/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

/* Example to show how to navigate mass storage device with built-in command line.
 * Type help for list of supported commands and syntax (mostly linux commands)

 > help
 * help
        Print list of commands
 * cat
        Usage: cat [FILE]...
        Concatenate FILE(s) to standard output..
 * cd
        Usage: cd [DIR]...
        Change the current directory to DIR.
 * cp
        Usage: cp SOURCE DEST
        Copy SOURCE to DEST.
 * ls
        Usage: ls [DIR]...
        List information about the FILEs (the current directory by default).
 * pwd
        Usage: pwd
        Print the name of the current working directory.
 * mkdir
        Usage: mkdir DIR...
        Create the DIRECTORY(ies), if they do not already exist..
 * mv
        Usage: mv SOURCE DEST...
        Rename SOURCE to DEST.
 * rm
        Usage: rm [FILE]...
        Remove (unlink) the FILE(s).
 */

#include <stdio.h>
#include <stdbool.h>
#include "bsp/board_api.h"
#include "tusb.h"


#include "msc_app.h"
#include "file_processing.h"
#include "hardware_processing.h"
#include "queue.h"
#include "pico/stdlib.h"
#include "hid.h"
 

#define BSIZE 64
#define ESC 27
#define ENTER 10
#define END_OF_TEXT 3   //Ctrl+C
#define CANCEL 24       //Cancel



//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+
void led_blinking_task(void);
static uint8_t const keycode2ascii[128][2] =  { HID_KEYCODE_TO_ASCII }; //was uint8_t originally

static void process_kbd_report(hid_keyboard_report_t const *report);
static char* convert_to_string(const volatile uint8_t *ch);
PRIVATE uint8_t reverse_bits(uint8_t value);
void clear_array(char* message);


volatile bool usb_check = false; 
volatile bool keyboard_check = false;


/*------------- MAIN -------------*/
int main(void)
{
  stdio_init_all();   // USB CDC (hardware USB → PC)
 
  timer_hw->dbgpause = 0;
  board_init();
  
   // init host stack on configured roothub port
   
  tusb_rhport_init_t host_init = {
    .role = TUSB_ROLE_HOST,
    .speed = TUSB_SPEED_AUTO
  };
  tusb_init(BOARD_TUH_RHPORT, &host_init);

  board_init_after_tusb();
  queue_init();
  pio_dma_setup();
  pio_keyboard_setup();
  set_gpio_pins();

  msc_app_init();

  while (1)
  {
    tuh_task();
    msc_app_task();
    led_blinking_task();


    // This section is atomic, in the sense that it cannot be interrupted by the GPIO interrupt handler, which is important to ensure that we don't accidentally trigger an interrupt while we are in the middle of processing the SPI or keyboard data, which could lead to data corruption or other issues. By disabling interrupts during this section, we can ensure that the program functions correctly and efficiently without any issues related to interrupt handling.
    // Need to prevent flag_info from being modified by the interrupt handler.


    
    
    if (usb_check) // This means that the SPI transaction is complete and the data in the buffer is from the SPI, so we can start processing the SPI data and writing it to the USB. This is necessary because we need to wait until the SPI transaction is complete before we can start processing the SPI data, which could lead to data corruption or other issues if we start processing it too early.
    {
      file_processing_main();
    }
  }

  sleep_ms(1); 

  return 0;
}


//--------------------------------------------------------------------+
// Blinking Task
//--------------------------------------------------------------------+
void led_blinking_task(void) {
  const uint32_t interval_ms = 1000;
  static uint32_t start_ms = 0;

  static bool led_state = false;

  // Blink every interval ms
  if (board_millis() - start_ms < interval_ms) return; // not enough time
  start_ms += interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

// called after all tuh_hid_mount_cb
void tuh_mount_cb(uint8_t dev_addr)
{
  // application set-up
  printf("A device with address %d is mounted\r\n", dev_addr);
}

// called before all tuh_hid_unmount_cb
void tuh_umount_cb(uint8_t dev_addr)
{
  // application tear-down
  printf("A device with address %d is unmounted \r\n", dev_addr);
}


uint32_t tusb_time_millis_api(void) {
    return board_millis(); 
}

void tusb_time_delay_ms_api(uint32_t ms)
{
    // For the RP2040, the Pico SDK provides this:
    sleep_ms(ms);
}

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use. tuh_hid_parse_report_descriptor()
// can be used to parse common/simple enough descriptor.
// Note: if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE, it will be skipped
// therefore report_desc = NULL, desc_len = 0
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
  printf("HID device address = %d, instance = %d is mounted\r\n", dev_addr, instance);

  if(tuh_hid_interface_protocol(dev_addr, instance) == HID_ITF_PROTOCOL_KEYBOARD) {
    if ( !tuh_hid_receive_report(dev_addr, instance) )
    {
      printf("Error: cannot request to receive report\r\n");
    }
  }
}

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
  printf("HID device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
}



// Invoked when received report from device via interrupt endpoint (key down and key up)
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  printf("received report from HID device address = %d, instance = %d\r\n", dev_addr, instance);

  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

  switch (itf_protocol)
  {
    case HID_ITF_PROTOCOL_KEYBOARD:
      printf("HID receive boot keyboard report\r\n");
      process_kbd_report( (hid_keyboard_report_t const*) report );
    break;
  }

  // continue to request to receive report
  if ( !tuh_hid_receive_report(dev_addr, instance) )
  {
    printf("Error: cannot request to receive report\r\n");
  }
}


//--------------------------------------------------------------------+
// Keyboard
//--------------------------------------------------------------------+

// look up new key in previous keys
static inline bool find_key_in_report(hid_keyboard_report_t const *report, uint8_t keycode)
{
  for(uint8_t i=0; i<6; i++)
  {
    if (report->keycode[i] == keycode){
      return true;
    }  
  }

  return false;
}

static void process_kbd_report(hid_keyboard_report_t const *report)
{
    static hid_keyboard_report_t prev_report = { 0, 0, {0} };

    for (uint8_t i = 0; i < 6; i++)
    {
        uint8_t keycode = report->keycode[i];
        if (!keycode) continue;

        if (find_key_in_report(&prev_report, keycode))
            continue; //filter out key releases and held keys, only process new key presses

        bool const is_shift =
            report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT);

        uint8_t ch = keycode2ascii[keycode][is_shift ? 1 : 0];

        // STOP condition (highest priority)
        if (ch == ESC || ch == ENTER || ch == '\r' || ch == '\n')
        {
            break;
        }

        ch = reverse_bits(ch);

        // if (keyboard_reading && (ch >= 32 && ch <= 126)) for future

        // Only forward if active

        //  if ((flag_info & KEYBOARD_SEND_EVENT) && (ch >= 32 && ch <= 126))
        
        if (keyboard_check) // Only forward if we have received keyboard data, which is indicated by the keyboard_check flag being set to true. This is important to ensure that we only forward valid keyboard data and not random data that could be in the buffer, which could lead to data corruption or other issues.
        {
              pio_sm_put_blocking(return_pio(), return_sm(), ch); // Send the character to the PIO state machine for processing. This is necessary to ensure that the keyboard data is processed correctly and efficiently without any issues related to timing or data corruption. By using the PIO state machine, we can offload the processing of the keyboard data from the main CPU, allowing for more efficient handling of the data and better overall performance of the program. 
        }
    }

    prev_report = *report;
}
 

PRIVATE uint8_t reverse_bits(uint8_t value) {
    uint8_t result = 0;
    for (int i = 0; i < 8; i++) {
        result <<= 1; // Shift result left to make room for the next bit
        result |= (value & 1); // Add the least significant bit of value to result
        value >>= 1; // Shift value right to process the next bit
    }
    return result;
}

  
/* static char* convert_to_string(const volatile uint8_t *ch)
{

    static char read[40]; // persistent buffer
    
    read[39] = '\0';
    static uint8_t i = 0; //recalls how many time the function is calle and stores it. 
      // Stop adding if buffer is full or an escape key is received

    if (*ch == ESC || *ch == END_OF_TEXT || *ch == CANCEL || *ch == ENTER || i ==39 )
    {
      flag_check = FLAG_ESCAPE;
      i=0;
      return read;
    }

    if (i < 39 )  // ensure space for '\0'
     {
       read[i++] = (char)*ch; 
       flag_check = FLAG_NOT_ESCAPE;
     }
}

void clear_array(char* message)
{
    while(*message)
    {
        *message = '\0';
        message++;
    }
} */




/* static void process_kbd_report(hid_keyboard_report_t const *report)
{
  static hid_keyboard_report_t prev_report = { 0, 0, {0} }; // previous report to check key released
  FILE *fptr;
  char buffer[BSIZE];

  fptr = fopen("usbhost.txt", "w");
  //------------- example code ignore control (non-printable) key affects -------------//
  for(uint8_t i=0; i<6; i++)
  {
    if ( report->keycode[i] )
    {
      if ( find_key_in_report(&prev_report, report->keycode[i]) )
      {
        // exist in previous report means the current key is holding
      }else
      {
        // not existed in previous report means the current key is pressed
        bool const is_shift = report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT);
        char ch = keycode2ascii[report->keycode[i]][is_shift ? 1 : 0];
        //Maybe need to do something here to get it to output only characters 
        
        putchar(ch);
        putchar('\n');
        if ( ch == '\r' ) putchar('\n'); // added new line for enter key

        fflush(stdout); // flush right away, else nanolib will wait for newline
      }
    }
    // TODO example skips key released
  }

  prev_report = *report;
} */


////Miscallaneous/////

///
    /*      else if (ch == '\r' || ch == '\n')  // handle newline
                {
                    putchar('\n');
                    fprintf(fptr, '\n');
                } */
                // fflush(fptr);    // flush file output immediately
                // fflush(stdout);  // flush terminal output
//              }