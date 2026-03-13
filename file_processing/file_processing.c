
#include "ff.h"
#include "file_processing.h"
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "queue.h"
#include <ctype.h>



typedef struct 
{
    bool path_exists;
    bool file_exists;
}Exists_check;


typedef struct
{
    char dates[8];
    char times[8];
    uint8_t* store;
    int diff;
}File_Info;

static File_Info file_info;




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
PRIVATE void extract_date(const char *in, char *dates, size_t size);
PRIVATE void extract_time(const char *in, char *times, size_t size );
 
// the function below exists to work on the results and errors

PUBLIC void file_processing_main( ) {
    FRESULT fr;
   

    fr = start();

    memset(file_info.dates, 0, sizeof(file_info.dates));
    memset(file_info.times, 0, sizeof(file_info.times));

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

FRESULT start() 
{ //This is the kick off function where the pico tries to mount onto the USB stick.

    FRESULT fr;
    fr = f_mount(fs, "", 1);
    return(fr); //sets off whole reaction
}

FRESULT no_path(FRESULT fr)
{
    FILINFO fno;
    uint8_t *buffer = give_array_address();

    extract_date(buffer, file_info.dates,sizeof(file_info.dates));   // dates used as folder/directory name

    fr = f_stat(file_info.dates, &fno);

    if (fr == FR_NO_FILE) {
        // directory does not exist so create it. Do not be foolled by the name, it is a directory not a file.
        fr = f_mkdir(file_info.dates);
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

   extract_time(buffer, file_info.times, sizeof(file_info.times));   // times used as file name

    f_opendir(&dir, "/");

    do {
        f_readdir(&dir, &fno);

        if (fno.fname[0] != 0) {

            if (fno.fattrib & AM_DIR) {
                if(strncmp(fno.fname, file_info.dates, strlen(file_info.dates)) == 0) // Check if the directory name matches the date
                {
                    if(f_stat(file_info.times, &fno) == FR_OK) // Check if the file already exists in the directory
                    {
                        fr = f_open(&fil, file_info.times, FA_OPEN_APPEND | FA_OPEN_EXISTING); // Open the directory for writing
                        f_puts(give_array_address(), &fil);
                        exists_check.file_exists = true; // Set flag to indicate that the path now exists
                    }

                }
                else
                {
                    fr = f_open(&fil, file_info.times,  FA_WRITE | FA_CREATE_ALWAYS);	/* Create a file */
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



PRIVATE void extract_date(const char *in, char *dates, size_t size)
{
    memset(dates,0,size);
 
    *(dates + size - 1) = '\0'; // ensure null termination
    char *date_ptr = strchr(in,'/');

    if(date_ptr == NULL)
    {
        return;
    }
    
    int ptr_diff = date_ptr - in; //get difference from startin point to where the '/' is

    if(ptr_diff < 0)
    {
        return;
    }
    int i = 0;
    int j = 0;
    int k = 0;

    for(; i < ptr_diff; )
    {
        if(ptr_diff >= 2)
        {
            dates[i++] = *( date_ptr - 2 );  //1st digit to 0
            dates[i++] = *(date_ptr - 1);    //2nd digit to 1
            dates[i++] = '-'; //actual / to 2nd

        }
        else if(ptr_diff == 1)
        {
            dates[i++] = *(date_ptr - 1); //only one digit needed
            dates[i++] = '-';     // for the actual /.
        }
        else
        {
            return; //should never be 3
        }
    }
 
   char *date_ptr2 = strchr(date_ptr + 1, '/'); // look for the second '/' starting from the character after the first '/'

    if(date_ptr2 == NULL)
    {
        return;
    }

    int ptr_diff2 = date_ptr2 - date_ptr;
    --ptr_diff2; //get difference from 1st / to the second /, minus 1 to not include the first /

    for(; j < ptr_diff2; )
    {
        if(ptr_diff2 >= 2)
        {
            dates[(j++)+i] = *( date_ptr - 2 ); 
            dates[(j++)+i] = *(date_ptr2 - 1);
            dates[(j++)+i] = '-';
        }
        else if(ptr_diff2 == 1)
        {
            dates[(j++)+i] = *(date_ptr2 - 1);
            dates[(j++)+i] = '-';
        }

        else
        {
            return;
        }
    }

    char *date_ptr3 = strchr(date_ptr2+1, ',');

    int ptr_diff3 = date_ptr3 - date_ptr2; //get difference from 2nd / to the end of the string
    --ptr_diff3;
    for(; k < ptr_diff3; )
    {
        if(ptr_diff3 == 2)
        {
            dates[(k++)+i+j] = *(date_ptr2 + 1);
            dates[(k++)+i+j] = *(date_ptr2 + 2);

        }
        else if(ptr_diff3 == 1)
        {
            dates[(k++)+i+j] = *(date_ptr2 + 1);
            dates[(k++)+i+j] = *(date_ptr2 + 2);
        }
    }
    
}

PRIVATE void extract_time(const char *in, char *time, size_t size)
{
    memset(time,0,size);
    time[size-1] = '\0';
    char *start = strchr(in, ',');
    char *end   = strchr(start + 1, ',');

    int i = 0;

    for(char *p = start + 1; p < end && i < size-1; p++)
    {
        if(isspace(*p))
            continue;

        time[i++] = *p;
    }



}








  
