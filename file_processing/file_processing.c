#include "ff.h"
#include "file_processing.h"
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "queue.h"







typedef struct 
{

    bool path_exists;
    bool file_exists;
}Exists_check;

static char folder_buffer[ BUF_LEN ]; // initialised on its own since its static

static char dates[8];
static char times[8];


static FATFS fatfs;
FATFS *fs = &fatfs;


// Forward declarations
FRESULT start(void);
FRESULT ok(FRESULT fr);
FRESULT no_file(FRESULT fr);
FRESULT no_path(FRESULT fr);
FRESULT invalid_name(FRESULT fr);
FRESULT denied(FRESULT fr);
FRESULT exist(FRESULT fr);
FRESULT invalid_object(FRESULT fr);
FRESULT not_enabled(FRESULT fr);
FRESULT no_filesystem(FRESULT fr);
FRESULT mkfs_aborted(FRESULT fr);
FRESULT timeout(FRESULT fr);
FRESULT locked(FRESULT fr);
FRESULT too_many_open_files(FRESULT fr);
FRESULT start_error(FRESULT fr);




FRESULT (*handle_error[])(FRESULT fr) = {
    ok,       // FR_OK = 0 → no error handler
    no_file,    // FR_NO_FILE = 1
    no_path,    // FR_NO_PATH = 2
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
    start_error // FR_START = 13
   
};



Exists_check exists_check = {0};

PRIVATE void get_date(const char *in, char *date, size_t date_size);
PRIVATE void get_time(const char *in, char *time, size_t time_size);

// the function below exists to work on the results and errors

PUBLIC void file_processing_main( ) {
    FRESULT fr;
   

    fr = start();
    

     // Check for hardware/system errors
    
// This state machine is mainly for error handling. The ones in the if statement are hardware issues and can't be fixed by me. The ones in the state machine hopefully can.
    while(1){
        if( fr == FR_DISK_ERR || fr == FR_NOT_READY ||fr == FR_WRITE_PROTECTED || fr == FR_INT_ERR  ) {
             break; //idk what to do if there is an hardware issue
        }
         fr = handle_error[fr](fr);
    }
    
}



///////////FRESULT functions/////////////////////////

FRESULT ok(FRESULT fr) {// This is the function to check what needs to be done. 

    if(!exists_check.path_exists)
    {
         fr = FR_NO_PATH;
         return(fr);
    }
    if(!exists_check.file_exists)
    {
         fr = FR_NO_FILE;
         return(fr);
    }

    // else
    // {
    //     fr = FR_BREAK;
    // }





}

FRESULT start() { //This is the kick off function where the pico tries to mount onto the USB stick.

    FRESULT fr;
    fr = f_mount(fs, "", 1);
    return(fr); //sets off whole reaction
}

FRESULT no_path(FRESULT fr)
{
    FILINFO fno;
    uint8_t *buffer = give_array_address();

    memset(dates, 0, sizeof(dates));
    get_date(buffer, dates, sizeof(dates));   // dates used as folder/directory name
    

    fr = f_stat(dates, &fno);

    if (fr == FR_NO_FILE) {
        // directory does not exist so create it. Do not be foolled by the name, it is a directory not a file.
        fr = f_mkdir(dates);
    }
    else if (fr == FR_OK) {
        if (fno.fattrib & AM_DIR) {
            exists_check.path_exists = true;
        }
    }

    return fr;
}



FRESULT no_file(FRESULT fr) 
{
    FIL fil;
    UINT br, bw; 
    FILINFO fno;
    DIR dir;

    uint8_t *buffer = give_array_address();

    memset(times, 0, sizeof(times));

    get_time(buffer, times, sizeof(times));   // times used as file name
    f_opendir(&dir, "/");

    do {
        f_readdir(&dir, &fno);

        if (fno.fname[0] != 0) {

            if (fno.fattrib & AM_DIR) {
                if(strncmp(fno.fname, dates, strlen(dates)) == 0) // Check if the directory name matches the date
                {
                    if(f_stat(times, &fno) == FR_OK) // Check if the file already exists in the directory
                    {
                        fr = f_open(&fil, times, FA_OPEN_APPEND | FA_OPEN_EXISTING); // Open the directory for writing
                        f_puts(give_array_address(), &fil);
                        exists_check.file_exists = true; // Set flag to indicate that the path now exists
                    }

                }
                else
                {
                    fr = f_open(&fil, times,  FA_WRITE | FA_CREATE_ALWAYS);	/* Create a file */
                    f_puts(give_array_address(), &fil);
                    exists_check.file_exists = true; // Set flag to indicate that the path now exists
                }
            } 
            else {
                exists_check.file_exists = false; // Set flag to indicate that the padoes not exist
            }

        }

    } while (fno.fname[0] != 0);

    f_closedir(&dir);
    return(fr);
}

FRESULT invalid_name(FRESULT fr)
{
    ;
}

FRESULT denied( FRESULT fr)
{
    ;
}

FRESULT exist(FRESULT fr)
{
    fr = FR_OK;
    return(fr);
}

FRESULT invalid_object(FRESULT fr)
{
    ;
}

FRESULT not_enabled(FRESULT fr)
{
    ;
}

FRESULT no_filesystem(FRESULT fr)
{
   
    uint8_t work[FF_MAX_SS];
    f_mkfs("", NULL, work, sizeof(work));   /* makes file system here*/
    return(f_mount(&fatfs, "0:", 1));                 /*makes another attempt at mounting it*/
}

FRESULT mkfs_aborted(FRESULT fr)
{
    ;
}

FRESULT timeout(FRESULT fr)
{
    ;
}

FRESULT locked(FRESULT fr)
{
    ;
}

FRESULT too_many_open_files(FRESULT fr)
{
    ;
}

FRESULT start_error(FRESULT fr)
{
    ;
}

///////////Helper functions/////////////////////////


static void get_file_info()
{
    FRESULT fr;
    FILINFO fno;
    const char *fname = "TRTEST";


    printf("Test for \"%s\"...\n", fname);

    fr = f_stat(fname, &fno);
    switch (fr) {

    case FR_OK:
        printf("Size: %lu\n", fno.fsize);
        printf("Timestamp: %u-%02u-%02u, %02u:%02u\n",
               (fno.fdate >> 9) + 1980, fno.fdate >> 5 & 15, fno.fdate & 31,
               fno.ftime >> 11, fno.ftime >> 5 & 63);
        printf("Attributes: %c%c%c%c%c\n",
               (fno.fattrib & AM_DIR) ? 'D' : '-',
               (fno.fattrib & AM_RDO) ? 'R' : '-',
               (fno.fattrib & AM_HID) ? 'H' : '-',
               (fno.fattrib & AM_SYS) ? 'S' : '-',
               (fno.fattrib & AM_ARC) ? 'A' : '-');
        break;

    case FR_NO_FILE:
    case FR_NO_PATH:
        printf("\"%s\" is not exist.\n", fname);
        break;

    default:
        printf("An error occured. (%d)\n", fr);


    // Need to get file name from keyboard or soemthing
    }   
}


static void add_subdirectory() //need to get date time stamp and stuff and use that 
{
    FRESULT fr;
    FILINFO fno;
    const char *fname = "TRTEST";


    printf("Test for \"%s\"...\n", fname);

    fr = f_stat(fname, &fno);
    switch (fr) {

    case FR_OK:
    ;
        break;
        
        
    }

}


 /// Helper function /////





 PUBLIC void convert_ascii_to_string(uint8_t *buffer)
{

    
     int i = 0;
     
     for (i = 0; i < BUF_LEN - 1 && *buffer != '\n'; i++)
      {
        folder_buffer[i] = (char)buffer[i]; // convert each byte to a char and store it in the folder_buffer
      }

        folder_buffer[ i ] = '\0'; //already i++ so this is after the last one

    
}



PUBLIC unsigned int append_char(uint32_t byte)
{
    static unsigned int i = 0;

    folder_buffer[i++] = (char)byte; //adds char then appends 1 to the next
    folder_buffer[i] = '\0'; //this is for next level

    if (i == BUF_LEN - 1)
    {
        folder_buffer[++i] = '\0'; //adds 0 at the end. 
        i = 0;
    }
    return i;   // return current count
}


PUBLIC char* return_buffer()
{
    return folder_buffer; // this is just for testing, will need to change when we want to use the buffer
}


// PRIVATE void extract_date(const char *in_buf, char *out)
// {

    
//     unsigned d, m, y;
//     int n = 0;

//     while (*in_buf) {
//     if (sscanf(out, "%2u/%2u/%2u%n", &d, &m, &y, &n) == 3 &&
//         n == 8 &&
//         d <= 31 &&
//         m <= 12)
//     {
//         break;
//     }
//     in_buf++;
// }

//     sscanf(in_buf, "%8s", info);

// }

PRIVATE void get_date(const char *in, char *date, size_t date_size)
{
    
    memset(date,0,sizeof(date_size)); // initialize the date buffer to 0
    const char *date_ptr = in;
    while(*date_ptr) {
        if (*date_ptr == '/') 
        {
            date[0] = *( date_ptr - 2); // Get the first digit of the day 2     2
            date[1] = *(date_ptr - 1);  // Get the second digit of the day 0    0
            date[2] = '-'; // Get the first seperator                           -
            date[3] = *( date_ptr + 1); // Get the first digit of the month     7
            date[4] = '-'; // Get the second digit of the month                  -
            date[5]  = *( date_ptr + 3); // Get the first digit of the year       2
            date[6] = *( date_ptr + 4); // Get the second digit of the year      5
            date[7] = '\0'; // Get the third digit of the year
            break; // Exit the loop after extracting the date

             // Validate the date format (DD/MM/YY)
        }

            date_ptr++;
            // Found the first '/', now look for the second '/'
    }
}

PRIVATE void get_time(const char *in, char *time, size_t time_size)
{
    
    memset(time,0,sizeof(time_size)); // initialize the time buffer to 0
    const char *time_ptr = in;
    while(*time_ptr) {
        if (*time_ptr == ':') 
        {
            
            time[0] = *(time_ptr - 1);  // Get the digit of the hour 0              5
            time[1] = ':'; // Get the first seperator                               :
            time[2] = *( time_ptr + 1); // Get the first digit of the minute        4
            time[3] = *( time_ptr + 2); // Get the second digit of the minute       0
            time[4] = *( time_ptr + 3); // Get the third digit of the minute        :
            time[5] = *( time_ptr + 4); // Get the first digit of the second        4
            time[6] = *( time_ptr + 5); // Get the second digit of the second       6
            time[7] = '\0'; // Get the third digit of the year
            break; // Exit the loop after extracting the date

             // Validate the date format (HH:MM)
        }

            time_ptr++;
            // Found the first ':', now look for the second ':'
    }
}
        
  

