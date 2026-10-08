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
#include "hardware/watchdog.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+
void led_blinking_task(void);
static uint8_t const keycode2ascii[128][2] =  { HID_KEYCODE_TO_ASCII }; //was uint8_t originally
static void process_kbd_report(hid_keyboard_report_t const *report);
volatile bool packet_recieved = false; // This is for when a size packet is recieved and is used to return early from an ISR
bool not_space_check = false; // This is a guard condition for the keyboard. If the keyboard is being processed, then this will be set to true, and it will be set to false when the keyboard processing is done. This is to prevent the keyboard from being processed when the SPI is being processed.
/*------------- MAIN -------------*/

int main(void) {

  stdio_init_all();   // USB CDC (hardware USB → PC)
 //timer_hw->dbgpause = 0;
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
  pio_miso_setup();
  pio_mosi_setup();
  dma_channel_init_once();
  restore_interrupts_from_disabled(status);
  int counter = 0;
  msc_app_init();    // Check whether the previous boot was caused by watchdog
  bool watchdog_reboot = watchdog_enable_caused_reboot();
    if (watchdog_reboot) {
      ++counter;
    }

    // Enable watchdog once
    watchdog_enable(600, false); // 600ms timeout, no pause on sleep


while (1)
{
    tuh_task();
    //msc_app_task();
    led_blinking_task();
    watchdog_update();
    event_type_t event;  
    bool isitkeyboard = false;

////////////////////////////////////////////////////////// STATE MACHINE LOOP /////////////////////////////////////////////////////////////////////////////////////

  while ((dequeue_events(&event))) // this is mainly used for usb and processing events. The main loop will not run until the interrupt triggers it.
    {
      switch(event)
      {
          case EVENT_SIZE_PACKET_RECIEVED:{ 
              uint32_t status_packet = save_and_disable_interrupts();
              isitkeyboard = classify_packet(); // enque for keyboard. TD tomorrow!!
              restore_interrupts_from_disabled(status_packet);
              if(isitkeyboard){
                goto keyboard_processing;
              }
              break;
          }

          case EVENT_USB_PROCESSING:{ 
                usb_processing_main(); // 
                break;
          }
                
          case EVENT_FILE_PARSING: 
                file_parsing(); // 
                break;
          

          case EVENT_FILE_PROCESSING: 
                 file_processing_main(); 
                 break;   
          
          keyboard_processing:
          case EVENT_KEYBOARD_PROCESSING: 
                keyboard_processing_main();
                break;
          

          case EVENT_DONE:
                event_processing_main();
                break;

          case EVENT_NONE:
                event_processing_main();
                __attribute__((fallthrough));

          default:
              break;
      }
      watchdog_update();
      break; // need to hit tuh_task() as much as possible, so break out of the while loop to do that.
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
          if(not_space_check){
            uint8_t ch = keycode2ascii[keycode][is_shift ? 1 : 0];
            enqueue_keyboard(ch);
        }
    }

        // STOP condition (highest priority)

      prev_report = *report;
  }

