#include "ff.h"
#include "file_processing.h"
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "queue.h"
#include "hardware_processing.h"






typedef struct 
{
    bool folder_exists:1;
    bool path_exists:1;
    bool file_exists:1;
}Exists_check;

static char folder_buffer[ BUF_LEN ]; // initialised on its own since its static


static FATFS fatfs;
FATFS *fs = &fatfs;

// Forward declarations
FRESULT start(void);
FRESULT ok(FRESULT fr);
FRESULT no_file(FRESULT fr);
FRESULT no_path(FRESULT fr);
FRESULT no_folder(FRESULT fr);
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
    no_folder,
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



PRIVATE void extract_date(const char *in_buf, char *out,size_t size);
PRIVATE const char* nth_strchr(const char* s, int c, int n);
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

    if(!exists_check.folder_exists)
    {
        fr = FR_NO_FOLDER;
        return(fr);
    }

    // else
    // {
    //     fr = FR_BREAK;
    // }





}

FRESULT start() { //This is the kick off function where the pico tries to mount onto the USB stick.

    FRESULT fr;;
    fr = f_mount(fs, "", 1);
    return(fr); //sets off whole reaction
}

FRESULT no_path(FRESULT fr)
{
    FIL fil;
    UINT bw;
   char *buffer = give_array_address(); // get the current position in the buffer to use for file name
   wait_for_dma_to_be_done();
   
   char dates[9];
    memset(dates,0,sizeof(dates));
    extract_date(buffer, dates, sizeof(dates));
   // extract_date(folder_buffer, dates);


    char *dir  = dates;
    const char *file = "MAAZ/test.txt";

    fr = f_mkdir(dir);

    if (fr == FR_OK || fr == FR_EXIST)
    {
        fr = f_open(&fil, file, FA_WRITE | FA_CREATE_ALWAYS);
        if (fr == FR_OK)
        {
            
            f_puts(folder_buffer, &fil);
            fr = f_close(&fil);
        }
    }

    return fr;
}



FRESULT no_file(FRESULT fr) {
    FIL fil;
    UINT br, bw;  

    f_chdir("TRTEST");
    fr = f_open(&fil, "newfile.txt", FA_WRITE | FA_CREATE_ALWAYS);	/* Create a file */
	if (fr == FR_OK) {
		f_write(&fil, "It works!\r\n", 11, &bw);	/* Write data to the file */
        f_puts(folder_buffer, &fil);
		fr = f_close(&fil);	
    }
    return(fr);
}

FRESULT no_folder(FRESULT fr)
{
    FILINFO fno;

    const char *fname = "MAAZ";
    fr = f_stat(fname, &fno); //checks for the file name
    if(fr == FR_OK){
        if (fno.fattrib & AM_DIR) { //checks if directory was created
            exists_check.folder_exists = true;
        } 
        else {
            fr = f_mkdir(fname); 
            // will leave the flag still false so it can come and check its been done later
          }
    }

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


PRIVATE void extract_date(const char *in, char *dates, size_t size)
{
    memset(dates,1,size);
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

    for(; i < ptr_diff; )
    {
        if(ptr_diff >= 2)
        {
            dates[i++] = *( date_ptr - 2 );  //1st digit to 0
            dates[i++] = *(date_ptr - 1);    //2nd digit to 1
            dates[i++] = *(date_ptr); //actual / to 2nd

        }
        else if(ptr_diff == 1)
        {
            dates[i++] = *(date_ptr - 1); //only one digit needed
            dates[i++] = *(date_ptr);     // for the actual /.
        }
        else
        {
            return; //should never be 3
        }
    }
    // Works uptill here, dont amend this part just work around it 
   char *date_ptr2 = date_ptr + 1;

    while (*date_ptr2 != '\0')
    {
        if (*date_ptr2 == '/')
        {
            break;
        }

        date_ptr2++;
    }

    int ptr_diff2 = date_ptr2 - date_ptr;

    for(int j = 0; j < ptr_diff2; )
    {
        if(ptr_diff2 >= 2)
        {
            dates[(++j)+i] = *(date_ptr2 - 2);
            dates[(j++)+i] = *(date_ptr2 - 1);
            dates[(j++)+i] = *(date_ptr2);
        }
        else if(ptr_diff2 == 1)
        {
            dates[(j++)+i] = *(date_ptr2 - 1);
            dates[(j++)+i] = *(date_ptr2);
        }

        else
        {
            return;
        }
    }



   


   
}



PRIVATE const char* nth_strchr(const char* s, int c, int n)
{
    int c_count;
    char* nth_ptr;

    for (c_count=1,nth_ptr=strchr(s,c); 
         nth_ptr != NULL && c_count < n && c!=0; 
         c_count++) 
    {
         nth_ptr = strchr(nth_ptr+1, c);
    }

    return nth_ptr;
}
        
  

