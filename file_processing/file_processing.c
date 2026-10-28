
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "pico/stdlib.h"
#include "hardware/sync.h"
#include "hardware/gpio.h"
#include "ff.h"
#include "file_processing.h"
#include "queue.h"
#include "hardware_processing.h"     // error_processing() and DIAGNOSTICS_T

// -----------------------------------------------------------------------------
// SETTINGS
// -----------------------------------------------------------------------------

// Give up on one packet if it has not finished in this long (includes time spent
// waiting for the stick). After this the packet is dropped and the guard released.
#define FP_TIMEOUT_US     5000000u

// 1 = a stick with no file system is formatted automatically (this was the old
// no_filesystem() behaviour). It ERASES the stick. 0 = report the error instead.
#define FP_AUTO_FORMAT    1

#define FP_ROOT_DIR       "/TRTEST"
#define FP_CSV_HEADER     "Date,Time,Ohms(ohm),Voltage(V),Current(A),TestTime,Comment,State\n"

// -----------------------------------------------------------------------------
// DATA
// -----------------------------------------------------------------------------

typedef struct {
    char dates[15];
    char times[15];
    char date_directory[20];        // "/TRTEST/20-7-25"
} File_Info;

typedef struct {                    // the fields of one message, copied out of the DMA buffer
    char ohms[12];
    char voltage[12];
    char current[12];
    char test_time[12];
    char comment[32];
    char state[12];
} Row_Info;

typedef enum {
    FP_IDLE,                        // nothing in progress
    FP_ENSURE_ROOT,                 // /TRTEST exists (make it if not)
    FP_ENSURE_DATE_DIR,             // /TRTEST/<date> exists (make it if not)
    FP_APPEND_ROW,                  // open <time>.csv (made if new), header if new, write the row
    FP_FORMAT,                      // stick has no file system: make one
    FP_DONE,                        // finished OK
    FP_FAILED,                       // gave up
    FT_COUNT
} fp_state_t;

static File_Info file_info;
static Row_Info  row;
static char      csv_path[64];

// state of the machine, kept between calls
static fp_state_t fp_state     = FP_IDLE; // initial state is idle
static uint32_t   fp_started   = 0;  // intiial state of started is 0
static DIAGNOSTICS_T fp_fail_code = DIAG_USB_HARDWARE_ERROR;   // what to tell Gary if this packet fails
static bool       fp_formatted = false; // fp is not formatted(initial stage)

// A "session" is one continuous stay of the same stick. The first packet of a session
// creates <time>.csv; every later packet appends to that same file until the stick is
// pulled (msc_app.c bumps msc_mount_count on every successful mount).
volatile uint32_t msc_mount_count = 0;
static bool       session_valid = false;
static uint32_t   session_mount;                    // mount count the session belongs to
static uint32_t   pkt_mount;                        // mount count when this packet started
static char       session_dir[sizeof(file_info.date_directory)];
static char       session_path[sizeof(csv_path)];
static char       csv_dir[sizeof(file_info.date_directory)];   // folder for this packet's file

// big FatFs objects are static so they are not on the (small) stack.
// Safe because only one step runs at a time.
static FIL     s_fil;
static FILINFO s_fno;
#if FP_AUTO_FORMAT
static uint8_t s_mkfs_work[FF_MAX_SS];
#endif

// -----------------------------------------------------------------------------
// PRIVATE PROTOTYPES
// -----------------------------------------------------------------------------

PRIVATE void       post_event(event_type_t e);
PRIVATE bool       parse_message(uint8_t *buffer);
PRIVATE FRESULT    ensure_dir(const char *path);
PRIVATE fp_state_t fail_or_format(FRESULT fr, DIAGNOSTICS_T code);

PRIVATE void  extract_date_directory(const uint8_t *in, char *dates_directory, size_t size);
PRIVATE void  extract_date(const uint8_t *in, char *dates, size_t size);
PRIVATE char* extract_time(const uint8_t *in, char *times, size_t size);
PRIVATE char* extract_state(char *in, char *state, size_t size);
PRIVATE char* extract_comma_field(char *in, char *out, size_t size);

// -----------------------------------------------------------------------------
// SMALL HELPERS
// -----------------------------------------------------------------------------

// Put an event on the queue from main-loop context (interrupts masked while doing it).
PRIVATE void post_event(event_type_t e) {
    uint32_t s = save_and_disable_interrupts();
    enqueue_events(e);
    restore_interrupts_from_disabled(s);
}

// Make sure a directory exists. FR_OK = it is there (or was just made).
PRIVATE FRESULT ensure_dir(const char *path) {
    FRESULT fr = f_stat(path, &s_fno);

    if (fr == FR_OK) {
        return (s_fno.fattrib & AM_DIR) ? FR_OK : FR_DENIED;   // a file with that name is in the way
    }
    if (fr == FR_NO_FILE || fr == FR_NO_PATH) {
        fr = f_mkdir(path);
        if (fr == FR_EXIST) fr = FR_OK;
    }
    return fr;
}

// A FatFs call failed: either format the stick (once), or give up and remember why.
PRIVATE fp_state_t fail_or_format(FRESULT fr, DIAGNOSTICS_T code) {
#if FP_AUTO_FORMAT
    if (fr == FR_NO_FILESYSTEM && !fp_formatted) {
        return FP_FORMAT;
    }
#else
    (void)fr;
#endif
    fp_fail_code = code;
    return FP_FAILED;
}

// -----------------------------------------------------------------------------
// STEP 1: PARSE THE MESSAGE (runs once per packet, on EVENT_FILE_PARSING)
// -----------------------------------------------------------------------------

// Pulls every field out of the message. Returns false if anything is missing,
// so a bad packet is rejected before the stick is touched.
PRIVATE bool parse_message(uint8_t *buffer) {
    char *p;

    memset(&file_info, 0, sizeof(file_info));
    memset(&row, 0, sizeof(row));

    extract_date_directory(buffer, file_info.date_directory, sizeof(file_info.date_directory));
    extract_date(buffer, file_info.dates, sizeof(file_info.dates));

    // both must have worked, or the folder name is just "/TRTEST/"
    if (file_info.dates[0] == '\0') return false;
    if (strlen(file_info.date_directory) <= strlen(FP_ROOT_DIR "/")) return false;

    p = extract_time(buffer, file_info.times, sizeof(file_info.times));
    if (!p || file_info.times[0] == '\0') return false;

    p = extract_comma_field(p, row.ohms,      sizeof(row.ohms));      if (!p) return false;
    p = extract_comma_field(p, row.voltage,   sizeof(row.voltage));   if (!p) return false;
    p = extract_comma_field(p, row.current,   sizeof(row.current));   if (!p) return false;
    p = extract_comma_field(p, row.test_time, sizeof(row.test_time)); if (!p) return false;
    p = extract_comma_field(p, row.comment,   sizeof(row.comment));   if (!p) return false;
    p = extract_state(p, row.state, sizeof(row.state));               if (!p) return false;

    // full path of the CSV, built once: "/TRTEST/20-7-25/12-30-45.csv"
    int n = snprintf(csv_path, sizeof(csv_path), "%s/%s.csv",
                     file_info.date_directory, file_info.times);
    if (n < 0 || (size_t)n >= sizeof(csv_path)) return false;

    snprintf(csv_dir, sizeof(csv_dir), "%s", file_info.date_directory);

    return true;
}

PUBLIC void file_parsing(void) {
    uint8_t *buffer = give_array_address();

    // End the message with a zero so strchr()/strlen() can never run past it.
    // The size byte is 8 bits, so size <= 255 and the buffer must be at least 256 bytes.
    uint32_t n = return_size();
    if (n > 255u) n = 255u;
    buffer[n] = '\0';

    if (parse_message(buffer)) {
	    error_processing(DIAG_ALL_OK);
        post_event(EVENT_FILE_PROCESSING);      // start writing
    } else {
        error_processing(DIAG_USB_PARSE_FAIL);  // bad message: tell Gary, then release the guard
        post_event(EVENT_DONE);
    }
}

// -----------------------------------------------------------------------------
// STEP 2: WRITE IT TO THE STICK (one small step per call, on EVENT_FILE_PROCESSING)
// -----------------------------------------------------------------------------

PUBLIC void file_processing_main(void) {      // called from main.c
    uint32_t now = time_us_32();
    FRESULT  fr;

    if (fp_state == FP_IDLE) {                 // first call for this packet
        fp_started   = now;
        fp_formatted = false;
        pkt_mount    = msc_mount_count; // stick just got put in.

        if (session_valid && session_mount == msc_mount_count && (strcmp(csv_dir,session_dir)==0)) { // stick hasnt moved. So not incremented
            snprintf(csv_path, sizeof(csv_path), "%s", session_path);
            snprintf(csv_dir,  sizeof(csv_dir),  "%s", session_dir);
            fp_state = FP_APPEND_ROW;
        } else {
            session_valid = false;             // new stick (or first packet): start a session
            fp_state      = FP_ENSURE_ROOT;
        }
    }

    if ((now - fp_started) > FP_TIMEOUT_US) {  // took too long: give up
        fp_fail_code = msc_ready ? DIAG_USB_HARDWARE_ERROR : DIAG_USB_MOUNT_FAIL;
        fp_state     = FP_FAILED;
    }
    else if (!msc_ready) {                     // no stick yet: keep the packet, try again next pass
        post_event(EVENT_FILE_PROCESSING);
        return;
    }

    switch (fp_state) {

    case FP_ENSURE_ROOT:
        fr = ensure_dir(FP_ROOT_DIR);
        fp_state = (fr == FR_OK) ? FP_ENSURE_DATE_DIR : fail_or_format(fr, DIAG_USB_MOUNT_FAIL);
        break;

    case FP_ENSURE_DATE_DIR:
        fr = ensure_dir(csv_dir);
        fp_state = (fr == FR_OK) ? FP_APPEND_ROW : fail_or_format(fr, DIAG_USB_HARDWARE_ERROR);
        break;

    case FP_APPEND_ROW:
        fr = f_open(&s_fil, csv_path, FA_WRITE | FA_OPEN_APPEND);   // creates the file if missing
        if (fr == FR_OK) {
            int w = 0;
            if (f_size(&s_fil) == 0) {                              // brand new file: header first
                w = f_puts(FP_CSV_HEADER, &s_fil);
            }
            if (w >= 0) {
                w = f_printf(&s_fil, "%s,%s,%s,%s,%s,%s,%s,%s\n",
                             file_info.dates, file_info.times,
                             row.ohms, row.voltage, row.current,
                             row.test_time, row.comment, row.state);
            }
            if (w < 0) fr = FR_DISK_ERR;
            FRESULT fc = f_close(&s_fil);
            if (fr == FR_OK) fr = fc;
        }
        fp_state = (fr == FR_OK) ? FP_DONE : fail_or_format(fr, DIAG_USB_HARDWARE_ERROR);
        break;

#if FP_AUTO_FORMAT
    case FP_FORMAT:
        fp_formatted = true;                                         // only ever once per packet
        fr = f_mkfs("", NULL, s_mkfs_work, sizeof(s_mkfs_work));     // formats the current drive
        if (fr == FR_OK) {
            fp_state = FP_ENSURE_ROOT;                               // FatFs re-mounts on next access
        } else {
            fp_fail_code = DIAG_USB_MOUNT_FAIL;
            fp_state     = FP_FAILED;
        }
        break;
#endif

    case FP_DONE:
        if (!session_valid && msc_mount_count == pkt_mount) {        // first good write of this stick
            snprintf(session_path, sizeof(session_path), "%s", csv_path);
            snprintf(session_dir,  sizeof(session_dir),  "%s", csv_dir);
            session_mount = pkt_mount;
            session_valid = true;
        }
        fp_state = FP_IDLE;
	gpio_put(PICO_CODE_DEBUG_PROBE_PIN ,1);
        post_event(EVENT_DONE);                                      // releases the packet guard
        return;

    case FP_FAILED:
    default:
        session_valid = false;                                       // next packet starts a fresh session
        fp_state = FP_IDLE;
        error_processing(fp_fail_code);                              // tell Gary why; he resends
        post_event(EVENT_DONE);                                      // releases the packet guard
        return;
    }

    post_event(EVENT_FILE_PROCESSING);                               // come back for the next step
}

// -----------------------------------------------------------------------------
// EXTRACTION FUNCTIONS (same as before, except extract_state, see note)
// -----------------------------------------------------------------------------

// Builds the absolute path of the date folder, e.g. "/TRTEST/20-7-25",
// from the start of the incoming data (e.g. "20/7/25,12:30:45,...").
// Folder names can't contain '/', so the slashes in the date become '-'.
PRIVATE void extract_date_directory(const uint8_t *in, char *dates_directory, size_t size) {
   memset(dates_directory,0,size);// start with an empty, zeroed buffer

   // Fixed parent folder. Starting with '/' makes the whole path absolute,
   // so f_mkdir/f_stat behave the same wherever the working directory is.
   const char *prefix = FP_ROOT_DIR "/";
   size_t prefix_len = strlen(prefix);
   strncpy(dates_directory, prefix, size - 1);// bounded copy, can't overflow
   dates_directory[size-1]='\0';// make sure it's always terminated
    // Find the two '/' separators in the date (day/month/year).
   const char *start = strchr((const char *)in,'/');   // first '/'
   if(!start) return;
   char *end = strchr(start+1,'/');
   if(!end) return;
   const char *p =(const char*) in;

   size_t i = prefix_len; // write after "/TRTEST/", not over it

   // Part 1: copy "day/month" (everything up to the second '/'),
   // turning '/' into '-'. Stops early if the output buffer is nearly full
   for(; p<end && i<size-1;p++){
    char c = *p;
        if (c == '/'){
            c = '-';
        }
        dates_directory[i++] = c;

   }
// Part 2: copy the year (everything up to the first ',').
   if(i< size - 1){
    dates_directory[i++] = '-';
   }
   ++p;

   char *end_final = strchr(end+1,',');
   if(!end_final) return;

   for(; p<end_final && i<size-1; p++)
   {
        char c = *p;
        dates_directory[i++] = c;
   }
// Result, e.g. "/TRTEST/20-7-25", always null-terminated thanks to the memset.
}

PRIVATE void extract_date(const uint8_t *in, char *dates, size_t size) {
    memset(dates,0,size);
    dates[size-1]='\0';
    const char *end = strchr((const char *)in,'/');
    if(!end) return;
    const char *p = (const char *)in;

    size_t i = 0;
    for(; p<end && i<size-1;p++){
        char c = *p;
        if (c == '/') { c = '-'; }
        dates[i++] = c;
    }

    const char *end_final = strchr(end+1,',');
    if(!end_final) return;
    for(; p<end_final && i<size-1; p++)
    {
        char c = *p;
        if (c == '/') { c = '-'; }
        dates[i++] = c;
    }
}

PRIVATE char* extract_time(const uint8_t *in, char *time, size_t size) {
    memset(time,0,size);
    time[size-1] = '\0';
    char *start = strchr((const char*)in, ',');
    if(!start) return NULL;
    char *end   = strchr((const char*)start + 1, ',');
    if(!end) return NULL;
    size_t i = 0;
    const char *p = start + 1;

    for(; p < end && i < size-1; p++)
    {
        char c = *p;
        if(isspace((unsigned char)c))
            continue;

         if(c== ':'){
            c = '-';
        }

        time[i++] = c;
    }

    return (char *)p;
}

// The last field. It ends at '\n', or at the end of the message if there is no '\n'
// (the old version rejected the whole message when the '\n' was missing).
PRIVATE char* extract_state(char *in, char *state, size_t size) {
    memset(state, 0, size);

    char *ptr = strchr(in, ',');    //1st comma
    if (!ptr) return NULL;

    char *ptr_end = strchr(ptr + 1, '\n');
    if (!ptr_end) ptr_end = ptr + 1 + strlen(ptr + 1);   // no '\n': use the end of the message

     ptr++;   // move past comma

    size_t j = 0;

    while (ptr < ptr_end && j < size - 1)
    {
        if (!isspace((unsigned char)*ptr))
        {
           state[j++] = *ptr;
        }
        ptr++;
    }

    state[j] = '\0';

    return ptr;
}


PRIVATE char* extract_comma_field(char *in, char *out, size_t size) {
    memset(out, 0, size);

    char *ptr = strchr(in, ',');
    if (!ptr) return NULL;

    char *ptr_end = strchr(ptr + 1, ',');
    if (!ptr_end) return NULL;

    ptr++;
    size_t j = 0;

    while (ptr < ptr_end && j < size - 1) {
        if (!isspace((unsigned char)*ptr)) {
            out[j++] = *ptr;
        }
        ptr++;
    }

    out[j] = '\0';
    return ptr;
}
