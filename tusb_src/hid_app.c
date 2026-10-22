#include "tusb.h"
#include "board_api.h"
#include "queue.h"


static uint8_t const keycode2ascii[128][2] =  { HID_KEYCODE_TO_ASCII }; //was uint8_t originally
static void process_kbd_report(hid_keyboard_report_t const *report);
volatile bool not_space_check = false; // This is guard condiiton for the kryboard. If the keyboard ISR will trigger, then if that isnt true the event wont happen, and the activity(enqueing it) will be skipped. This is to prevent the keyboard from being processed when the SPI is being processed.
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
  (void) dev_addr;
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

  if(not_space_check){ // if the keyboard is being processed, then dont enqueue the event since it will be processed in the main loop.
    not_space_check = false; // reset the not space check so that the next character can be processed.
  }
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

        if(not_space_check){ // guard condition. Volatile because this is asynchronous
          uint8_t ch = keycode2ascii[keycode][is_shift ? 1 : 0];
          enqueue_keyboard(ch);
        }
    }

        // STOP condition (highest priority)

      prev_report = *report;
  }


