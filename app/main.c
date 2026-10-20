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
#include "hardware/watchdog.h"
#include "hid.h"
#include "CException.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+
/*------------- MAIN -------------*/

int main(void) {

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
  pio_miso_setup();
  pio_mosi_setup();
  dma_channel_init_once();
  restore_interrupts_from_disabled(status);

  msc_app_init();
  CEXCEPTION_T e;
    bool watchdog_reboot = watchdog_enable_caused_reboot();

    if (watchdog_reboot) {
        // The Pico rebooted because the watchdog expired.
        // Handle/log this if needed.
    }
    watchdog_enable(600, true); // Set watchdog timeout to 600 ms and enable pause on debug

while (1) {
    tuh_task();
    Try{
        dma_timeout_check();
    }
    Catch(e){
        error_processing_main(e); // Handle the exception, e.g., log it, reset the system, etc.
    }
    msc_app_init();
    led_blinking_task();
    event_type_t event;
    watchdog_update();

////////////////////////////////////////////////////////// STATE MACHINE LOOP /////////////////////////////////////////////////////////////////////////////////////

  while ((dequeue_events(&event))) // this is mainly used for usb and processing events. The main loop will not run until the interrupt triggers it.
  {
      switch(event)
      {
          case EVENT_SIZE_PACKET_RECIEVED: {
              uint32_t status_packet = save_and_disable_interrupts();
              classify_packet(); // This cannot be interrupted as critical
              restore_interrupts_from_disabled(status_packet);
              break;
          }

          case EVENT_FILE_PARSING: {
                Try{
                    file_parsing(); 
                }
                Catch(e){
                    error_processing_main(e);
                }
                 break;
          }
          case EVENT_FILE_PROCESSING: {
                Try{
                    file_processing_main(); 
                }
                Catch(e){
                    error_processing_main(e);
                }
                 break;
            }
          case EVENT_DONE:
                event_processing_main();
                break;
            
          case EVENT_DMA_ERROR: {
              // Handle DMA error
              error_processing_main(DIAG_DMA_TIMEOUT); // or appropriate error code
              break;
          }

          default:
              break;
      }
      watchdog_update(); // update the watchdog timer to prevent it from expiring
     break; // break out of the while loop since the event has been processed and the main loop can run again.
  }
  
  }
}



  

    
