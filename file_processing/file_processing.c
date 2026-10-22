
#include "ff.h"
#include "file_processing.h"
#include "hardware_processing.h"
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "queue.h"
#include <ctype.h>


//#################################################################### Another state machine implementation ######################################################################### 


/* File function return code (FRESULT) */

typedef enum {
    /* Special system/hardware errors — custom numbering */
    FR_DISK_ERR = 80,
    FR_INT_ERR = 81,
    FR_NOT_READY = 82,
    FR_WRITE_PROTECTED = 83,
    FR_INVALID_DRIVE = 84,
    FR_NOT_ENOUGH_CORE = 85,
    FR_INVALID_PARAMETER = 86,
    FR_ALL_DONE		= 87, //WHEN ITS ALL DONE

    /* Restart normal error sequence */
    FR_CHECK_IF_TRTEST_DIRECTORY_EXISTS = 0,
    FR_CHECK_IF_DATE_FOLDER_ALREADY_EXISTS= 1,
    FR_CHECK_IF_TIME_FILE_ALREADY_EXISTS = 2
    FR_NO_FILE = 3          /* normal errors */ 
    FR_NO_DATE_FOLDER_PATH = 4,
    FR_INVALID_NAME = 5,
    FR_DENIED = 6,
    FR_EXIST = 7,
    FR_INVALID_OBJECT = 8,
    FR_NOT_ENABLED = 9,
    FR_NO_FILESYSTEM = 10,
    FR_MKFS_ABORTED = 11,
    FR_TIMEOUT = 12,
    FR_LOCKED = 13,
    FR_TOO_MANY_OPEN_FILES = 14,
/* These are checK states*/
} FRESULT;



typedef struct
{
    char dates[15]; // these are storing dates and times
    char times[15];
    char times_header[15 + 4]; // times + 4 // this is the csv filr
    char date_directory[20]; // this is the overall directory including TRTEST an the date folder
    char *starting_pointer; // this is for parsing thru the commas and getting data for the csv file

}File_Info;

static File_Info file_info;
Exists_check exists_check = {0};
static FATFS fatfs;
FATFS *fs = &fatfs;


// Forward declarations of state machine functions
PRIVATE FRESULT no_file(FRESULT fr);
PRIVATE FRESULT no_date_folder_path(FRESULT fr);
PRIVATE FRESULT invalid_name(FRESULT fr);
PRIVATE FRESULT denied(FRESULT fr);
PRIVATE FRESULT exist(FRESULT fr);
PRIVATE FRESULT invalid_object(FRESULT fr);
PRIVATE FRESULT not_enabled(FRESULT fr);
PRIVATE FRESULT no_filesystem(FRESULT fr);
PRIVATE FRESULT mkfs_aborted(FRESULT fr);
PRIVATE FRESULT timeout(FRESULT fr);
PRIVATE FRESULT locked(FRESULT fr);
PRIVATE FRESULT too_many_open_files(FRESULT fr);
PRIVATE FRESULT start_error(FRESULT fr);
PRIVATE bool check_if_trtest_directory_exists(void);
PRIVATE FRESULT check_if_date_folder_already_exists(FRESULT fr);
PRIVATE FRESULT check_if_time_csv_already_exists(FRESULT fr);

FRESULT (*handle_error[])(FRESULT fr) = {
    check_if_trtest_directory_exists, // logical beginning, check if the trtest directory even exists at all 
    check_if_date_folder_already_exists,  // FR_CHECK_IF_DATE_FOLDER_ALREADY_EXISTS== 14
    check_if_time_csv_already_exists
    no_file,    // FR_NO_FILE = 1
    no_date_folder_path,    // FR_NO_DATE_FOLDER_PATH = 2. This is idk what to do with this
    invalid_name, // FR_INVALID_NAME = 3
    denied,     // FR_DENIED = 4
    exist,      // FR_EXIST = 5
    invalid_object, // FR_INVALID_OBJECT = 6
    not_enabled, // FR_NOT_ENABLED = 7
    no_filesystem, // FR_NO_FILESYSTEM = 8
    mkfs_aborted, // FR_MKFS_ABORTED = 9
    timeout,    // FR_TIMEOUT = 10
    locked,     // FR_LOCKED = 11
    too_many_open_files, // FR_TOO_MANY_OPEN_FILES = 12
    start_error, // FR_START = 13
};

// Create folder and date directory
PRIVATE void extract_date_directory(const uint8_t *in, char *dates, size_t size);
PRIVATE void extract_date(const uint8_t *in, char *dates, size_t size);

// To fill our csv form
PRIVATE char* extract_time(const uint8_t *in, char *times, size_t size );
PRIVATE char* extract_state(char *in, char *state, size_t size);
PRIVATE char* extract_comma_field(char *in, char *out, size_t size);

PRIVATE bool check_if_folder_exists_in_date_directory(const File_Info *file_info);
// PRIVATE FRESULT read_root_directories();
 
// the function below exists to work on the results and errors


PUBLIC void file_parsing() {
    memset(file_info.dates, 0, sizeof(file_info.dates));
    memset(file_info.times, 0, sizeof(file_info.times));
    memset(file_info.date_directory, 0, sizeof(file_info.date_directory));
    memset(file_info.times_title, 0, sizeof(file_info.times_title)); // will hv to have a 0: at the beginning
    if(!msc_ready){
        error_processing(DIAG_USB_MOUNT_FAIL);
        enqueue_events(EVENT_FILE_PARSING); // re-enqueue the event to try again later
        return;
    }
    uint8_t *buffer = give_array_address(); //give_array_address();
    buffer[return_size()] = '\0';


    extract_date(buffer, file_info.dates,sizeof(file_info.dates)); // extract time file name
    file_info.starting_pointer = extract_time(buffer, file_info.times,sizeof(file_info.times)); // extract the time par
	    snprintf(file_info.times_title,sizeof(file_info.times_title),"%s:%s","0",file_info.date_directory)											//
	if (file_info.starting_pointer == NULL) {
		// malformed input — nothing to parse further, bail out safely
		error_processing(DIAG_USB_PARSE_FAIL);
		return;
	    }
    event_type_t current_event = EVENT_FILE_PROCESSING;
    enqueue_events(current_event);
}

PUBLIC void file_processing_main() { //called file_processing_main because this function goes in the main.c file
 static FRESULT fr = FR_CHECK_IF_TRTEST_DIRECTORY_EXISTS; // first run through
 static bool first_run = false; // this is when we first hit it. Main purpose is to hit the main.c as much as possible
 if( !first_run ){
   first_run=true;
   exists_check = (Exists_check){0};   // reset both flags on every call
 }
 else {
  if(fr == FR_ALL_DONE){
    first_run = false;
    enqueue_events(EVENT_DONE);
    return;
  }

  if( fr == FR_ALL_DONE || fr == FR_DISK_ERR || fr == FR_NOT_READY ||fr == FR_WRITE_PROTECTED || fr == FR_INT_ERR) {
    first_run = false;
     error_processing(DIAG_USB_HARDWARE_ERROR);
     break; //idk what to do if there is an hardware issue
      }
  fr = handle_error[fr](fr); // kick start it off.
 }
 enqueue_events(EVENT_FILE_PROCESSING); // must hit tuh_task() as much as possible.
}

/////////////////////////////////////////FRESULT functions/////////////////////////////////////////


PRIVATE FRESULT no_date_folder_path(FRESULT fr) {

    const char *fname = file_info.date_directory;
    fr = f_mkdir(fname);
    return fr;
}

PRIVATE FRESULT check_if_trtest_directory_exists(void) {
    DIR dir;
    FILINFO fno;
    FRESULT fr;

    fr = f_findfirst(&dir, &fno, "/", "TRTEST"); // looks for TRTEST file in the root directory

	while (fr == FR_OK && fno.fname[0]!=0)
	    {
		if ((fno.fattrib & AM_DIR) && (strcmp(fno.fname, "TRTEST") == 0)) // AM_DIR is a flag for it being a directory, and strncmp just checks if the name matches the name of the directory!!
		{
		    f_closedir(&dir);
		    fr = FR_CHECK_IF_DATE_FOLDER_ALREADY_EXISTS;
		    return(fr); 
		}

		fr = f_findnext(&dir, &fno);
	    }

	    f_closedir(&dir);

	    if (fr != FR_OK) {
		    return(fr);
	    }
}

PRIVATE FRESULT check_if_date_folder_already_exists(FRESULT fr) {
    DIR dir;
    FILINFO fno = {0};
    fr = f_findfirst(&dir,&fno,"/TRTEST/",file_info.dates);
    while(fr == FR_OK && fno.fname[0]!=0){
	    if((fno.fattrib & AM_DIR) && (strncmp(fno.fname, file_info.dates))){
		 f_closedir(&dir);   
                 fr = FR_OK;
		 return(fr);
	    }
	     fr = f_findnext(&dir,&fno);
    }
 //   if (fno.fname[0] == 0)

    fr = f_mkdir(file_info.date_directory);
    f_closedir(&dir);
    fr = FR_NO_DATE_FOLDER_PATH;
    return fr;
}

PRIVATE FRESULT check_if_time_csv_already_exists(FRESULT fr) {
   DIR dir;
   FILINFO fno;
   fr = f_findfirst(&dir,&fno,file_info.date_directory, "*.csv"); //may amend this regex expression to more accurately match the csv expected
   while(fr == FR_OK && fno.fname[0]!=0){
	   if(strcmp(fno.fname, file_info.dates)){
	    f_closedir(&dir);
            fr = FR_ALL_DONE; //if csv already exists 
	    return(fr);
	   }
	   fr = f_findnext(&dir,&fno);
   }
   fr = FR_NO_FILE;
   return(FR_NO_FILE);
}

PRIVATE FRESULT append_data_to_file(FRESULT fr) { // no time csv
    DIR dir;
    FIL fp;
    TCHAR temp[64];
    TCHAR fullpath[64];
  // Declaring local values so that they get destroyed once function is done
    char ohms[5];
    char voltage[5];
    char current[5];
    char test_time[5];
    char comment[30];
    char state[6];
      // sets value for arrays to all 0
    memset(fullpath, 0, sizeof(fullpath));
    memset(temp, 0, sizeof(temp));
    memset(ohms,0,sizeof(ohms));
    memset(voltage, 0, sizeof(voltage));
    memset(current, 0, sizeof(current));
    memset(test_time, 0, sizeof(test_time));
    memset(comment, 0, sizeof(comment));
    memset(state, 0, sizeof(state));

    char *buffer = file_info.starting_pointer;
    buffer = extract_comma_field(buffer, ohms, sizeof(ohms)); 
	if (!buffer) return FR_INVALID_NAME;
    buffer = extract_comma_field(buffer, voltage, sizeof(voltage));
	if (!buffer) return FR_INVALID_NAME;
    buffer = extract_comma_field(buffer, current, sizeof(current));
	if (!buffer) return FR_INVALID_NAME;
    buffer = extract_comma_field(buffer, test_time, sizeof(test_time));
	if (!buffer) return FR_INVALID_NAME;
    buffer = extract_comma_field(buffer, comment, sizeof(comment));
	if (!buffer) return FR_INVALID_NAME;

    buffer = extract_comma_field(buffer, state, sizeof(state));
	if (!buffer) return FR_INVALID_NAME;
    
    //extract_voltage(buffer, voltage, size_t size); // this is for the next value, and so on and so fort

    fr = f_chdir(file_info.date_directory);

    if (fr != FR_OK) {
        return fr;
    }
	snprintf(temp, sizeof(temp), "%s/%s", fullpath, file_info.times_title);
    snprintf(file_info.times_title, sizeof(file_info.times_title), "%s.csv", file_info.times);
	snprintf(fullpath, sizeof(fullpath), "%s", temp);

        fr = f_open(&fp, fullpath, FA_WRITE | FA_OPEN_APPEND);

        if (fr == FR_OK) {

            
         //   fr = f_write(&fp,give_array_address_for_file_writing(),get_queue_size(),&byte_count_to_write);
            fr = f_puts(file_info.dates,&fp);
             f_puts(",\t",&fp);
            fr = f_puts(file_info.times,&fp);
            fr = f_puts(",\t",&fp);
            fr = f_puts(ohms, &fp); //ensures the file starts with a new line so it writes here instead of 
            f_puts(",\t", &fp); 
            fr = f_puts(voltage,&fp);
            f_puts(",\t",&fp);
            fr = f_puts(current,&fp);
            f_puts(",\t",&fp);
            fr = f_puts(test_time, &fp);
            f_puts(",\t", &fp);
            fr = f_puts(comment, &fp);
            f_puts("\t", &fp);
            fr = f_puts(state, &fp);
            f_puts("\n", &fp);
            exists_check.file_exists = true;
            fr = f_close(&fp);
        }

        return fr;
    }
}

PRIVATE FRESULT no_file(FRESULT fr) {
    FIL fp;

    TCHAR fullpath[64];
    memset(fullpath, 0, sizeof(fullpath));

    fr = f_chdir(file_info.date_directory);
    if (fr != FR_OK) {
        return fr; // can't open directory
    }

	snprintf(fullpath, sizeof(fullpath), "%s", file_info.times_title);

        fr = f_open(&fp,fullpath, FA_WRITE | FA_CREATE_ALWAYS);
        if (fr == FR_OK) {
            f_puts("Date,\tTime,\tOhms(ohm)),\tVoltage(V),\tCurrent(A),\tTestTime,\tComment,\tState\n", &fp); //adds a tab in the middle
            f_close(&fp);
        }
	fr = FR_APPEND_DATA_TO_FILE;
        return(fr); //this is to create a new file


}

PRIVATE bool check_if_folder_exists_in_date_directory(const File_Info *file_info) {
    DIR dir;
    FILINFO fno;
    FRESULT fr;

    fr = f_opendir(&dir, file_info->date_directory);
    if (fr != FR_OK) {
        return false;
    }

    while (1)
    {
        fr = f_readdir(&dir, &fno);

        if (fr != FR_OK || fno.fname[0] == 0) {
            break;   // error or end of directory
        }

        if (strcmp(fno.fname, file_info->times_header) == 0) {
            f_closedir(&dir);
            return true;
        }
    }

    f_closedir(&dir);
    return false;
}


PRIVATE FRESULT invalid_name(FRESULT fr) {
    (void)fr;
    return(fr);
}

PRIVATE FRESULT denied( FRESULT fr) {
    (void)fr;
    return(fr);
}

PRIVATE FRESULT exist(FRESULT fr) {
    fr = FR_OK;
    return(fr);
}

PRIVATE FRESULT invalid_object(FRESULT fr) {
    (void)fr;
    return(fr);
}

PRIVATE FRESULT not_enabled(FRESULT fr) {
    (void)fr;
    return(fr);
}

PRIVATE FRESULT no_filesystem(FRESULT fr) {
    uint8_t work[FF_MAX_SS];
    f_mkfs("", NULL, work, sizeof(work));   /* makes file system here*/
    return(f_mount(&fatfs, "0:", 1));                 /*makes another attempt at mounting it*/
}

PRIVATE FRESULT mkfs_aborted(FRESULT fr) {
    (void)fr;
    return(fr);
}

PRIVATE FRESULT timeout(FRESULT fr) {
    (void)fr;
    return(fr);
}

PRIVATE FRESULT locked(FRESULT fr) {
    (void)fr;
    return(fr);
}

PRIVATE FRESULT too_many_open_files(FRESULT fr) {
    (void)fr;
    return(fr);
}

PRIVATE FRESULT start_error(FRESULT fr) {
    (void)fr;
    return(fr);
}




/////////////////////////////////////////////// Extraction Function ////////////////////////////////////////////////////// /
// Builds the absolute path of the date folder, e.g. "/TRTEST/20-7-25",
// from the start of the incoming data (e.g. "20/7/25,12:30:45,...").
// Folder names can't contain '/', so the slashes in the date become '-'.
PRIVATE void extract_date_directory(const uint8_t *in, char *dates_directory, size_t size) {
   memset(dates_directory,0,size);// start with an empty, zeroed buffer

   // Fixed parent folder. Starting with '/' makes the whole path absolute,
   // so f_mkdir/f_chdir behave the same wherever the working directory is.
   const char *prefix = "/TRTEST/";
   size_t prefix_len = strlen(prefix);
   strncpy(dates_directory, prefix, size - 1);// bounded copy, can't overflow
   dates_directory[size-1]='\0';// make sure it's always terminated
    // Find the two '/' separators in the date (day/month/year).
   const char *start = strchr((const char *)in,'/');   // first '/'
   if(!start) return;
   char *end = strchr(start+1,'/');
   if(!end) return;
   const char *p =(const char*) in;

   int i = (int)prefix_len; // write after "/TRTEST/", not over it

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

