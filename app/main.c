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
#include "board_api.h"
#include "tusb.h"
#include "msc_app.h"
#include "file_processing.h"
#include "hardware_processing.h"
#include "queue.h"
#include "pico/stdlib.h"
#include "hid.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+
void led_blinking_task(void);
static uint8_t const keycode2ascii[128][2] =  { HID_KEYCODE_TO_ASCII }; //was uint8_t originally

static void process_kbd_report(hid_keyboard_report_t const *report);
PRIVATE uint8_t reverse_bits(uint8_t value);

volatile bool main_check = false; // This is a signal event for the main loop. If this is false, then the main loop will not run. It is set to true when the SPI ISR triggers, and it is set to false when the event processing main function runs. This is to prevent the main loop from running when there is no event to process.
volatile bool keyboard_check = false; // This is guard condiiton for the kryboard. If the keyboard ISR will trigger, then if that isnt true the event wont happen, and the activity(enqueing it) will be skipped. This is to prevent the keyboard from being processed when the SPI is being processed.

/*------------- MAIN -------------*/
int main(void) {

  bool is_file_finished = false;
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

  uint32_t status = save_and_disable_interrupts();
  set_gpio_pins();
  pio_dma_setup();
  dma_channel_init_once();
  pio_keyboard_setup();
  pio_csn_setup();
  restore_interrupts_from_disabled(status);
  msc_app_init();


while (1)
{
    tuh_task();
    msc_app_task();
    led_blinking_task();
    event_type_t event;

////////////////////////////////////////////////////////// STATE MACHINE LOOP /////////////////////////////////////////////////////////////////////////////////////

  while ((main_check && dequeue_interrupts(&event))) // the main check acts as an signal event wheras the dequeueing interupt acts as a guard condition. It has to be true for the state machine to process. 
    {

      switch(event)
      {
          case EVENT_SIZE_PACKET_RECIEVED: {
              uint32_t status_packet = save_and_disable_interrupts();
              classify_packet(); // This cannot be interrupted as critical
              restore_interrupts_from_disabled(status_packet);
              break;
      }

          case EVENT_USB_PROCESSING: {
                bool is_usb_finished = false;
                is_usb_finished = usb_processing_main(); // the csn should not toggle after this, so it should fall straight down to file processing if its done correctly
                if(!is_usb_finished){ //if not correct size break, else fall through to file processing
                  break; }
                }
                __attribute__((fallthrough));

          case EVENT_FILE_PROCESSING: {
                 bool is_file_finished = false;
                 is_file_finished = file_processing_main(); 
                 if(!is_file_finished){ //if not correct size break, else fall through to keyboard processing
                 break; }
          }
                __attribute__((fallthrough));


          case EVENT_DONE:
                event_processing_main();

          default:
              break;
      }

      main_check = false; //reset to false so that the main loop will not run until the next interrupt triggers it. This is to prevent the main loop from running when there is no event to process.

  }

  if( keyboard_check ){ // if the keyboard is being processed, then the main loop will not run until the keyboard is done processing. This is to prevent the main loop from running when there is no event to process.
    bool is_keyboard_finished = false; // if a new char is entered, then it can only enter here if csn falls. 
    is_keyboard_finished = keyboard_processing_main(); //keyboard_processing_main();spi_slave_setup()
    if( is_keyboard_finished ){
      main_check = true; // set main check to true so that the main loop will run.
      keyboard_check = false; // set keyboard check to false so that keyboard wont have to be checked since its processed
    }else{
      main_check = false; // set main check to false so that the main loop will not run until the next interrupt triggers it. This is to prevent the main loop from running when there is no event to process.
      keyboard_check = true; // set keyboard check to true so that the keyboard processing will cxotinue until it escapes
    }
  }

  }
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

        bool const is_shift = report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT);

        if(keyboard_check){ // guard condition
          uint8_t ch = keycode2ascii[keycode][is_shift ? 1 : 0];
          enqueue_keyboard(ch);
        }
    }

        // STOP condition (highest priority)

      prev_report = *report;
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
    
