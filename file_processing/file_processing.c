
#include "ff.h"
#include "file_processing.h"
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "queue.h"
#include <ctype.h>
#include "common_header.h"




typedef struct 
{
    unsigned int path_exists:1;
    unsigned int file_exists:1;

}Exists_check; //can pack a lot in in one 


typedef struct
{
    char dates[15];
    char times[15];
    char times_header[15];
    char date_directory[15];
    char *starting_pointer;

}File_Info;

static File_Info file_info;
Exists_check exists_check = {0};



static FATFS fatfs;
FATFS *fs = &fatfs;


// Forward declarations of state machine functions
PRIVATE FRESULT start(void);
PRIVATE FRESULT ok(FRESULT fr);
PRIVATE FRESULT no_file(FRESULT fr);
PRIVATE FRESULT no_path(FRESULT fr);
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
PRIVATE FRESULT check_if_date_folder_already_exists(FRESULT fr);
PRIVATE FRESULT check_if_time_folder_already_exists(FRESULT fr);




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
    start_error, // FR_START = 13
    check_if_date_folder_already_exists,  // FR_CHECK_IF_DATE_FOLDER_ALREADY_EXISTS== 14
    check_if_time_folder_already_exists
};


PRIVATE void extract_date_directory(const char *in, char *dates, size_t size);
PRIVATE char* extract_time(const char *in, char *times, size_t size );
PRIVATE void extract_date(char *in, char *dates, size_t size);
PRIVATE char* extract_ohm(char *in, char *ohms, size_t size);
PRIVATE char* extract_voltage(char *in, char *voltage, size_t size);
PRIVATE char* extract_current(char *in, char *current, size_t size);
PRIVATE char* extract_test_time(char *in, char *test_time, size_t size);
PRIVATE char* extract_comment(char *in, char *comment, size_t size);
PRIVATE char* extract_state(char *in, char *state, size_t size);


PRIVATE bool check_if_folder_exists_in_date_directory(File_Info file_info);

// PRIVATE FRESULT read_root_directories();
 
// the function below exists to work on the results and errors

PUBLIC bool file_processing_main( ) { //called file_processing_main because this function goes in the main.c file
    FRESULT fr;
   // Initialise all date and time stuff early 
   // fr = f_getcwd(file_info.date_directory, strlen(file_info.date_directory)); //gets current directory and drive 
    // this is drive 0 and root directory
    memset(file_info.dates, 0, sizeof(file_info.dates));
    memset(file_info.times, 0, sizeof(file_info.times));
    memset(file_info.date_directory, 0, sizeof(file_info.date_directory));


    uint8_t *buffer = give_array_address(); //give_array_address();
    // int n = sizeof(file_info.dates)/sizeof(file_info.dates[0]);

    extract_date_directory(buffer, file_info.date_directory,sizeof(file_info.dates));   // dates used as folder/directory name
    extract_date(buffer, file_info.dates,sizeof(file_info.dates)); // extract time file name
    file_info.starting_pointer = extract_time(buffer, file_info.times,sizeof(file_info.times)); // extract the time part
    strcpy(file_info.times_header, file_info.times); //turn it into the time.csv and remove it from the time proper
    strcat(file_info.times_header,".csv");

    //strcat(file_info.date_directory,file_info.dates);
  //  snprintf(file_info.date_directory, sizeof(file_info.date_directory), "/%s", file_info.dates); //formats it so that 
  
     // Check for hardware/system errors
    fr = start();
// This state machine is mainly for error handling. The ones in the if statement are hardware issues and can't be fixed by me. The ones in the state machine hopefully can.
    while(1){
        if( fr == FR_DISK_ERR || fr == FR_NOT_READY ||fr == FR_WRITE_PROTECTED || fr == FR_INT_ERR || fr == FR_ALL_DONE ) {
             break; //idk what to do if there is an hardware issue
        }
         fr = handle_error[fr](fr);
    }

    if(fr == FR_ALL_DONE){
        event_type_t current_event = EVENT_DONE;
        enqueue_interrupts(current_event);
        return(true);
    }
    else{
        event_type_t current_event = EVENT_NONE;
        enqueue_interrupts(current_event);
        return(false);
    }
    
    
}

///////////FRESULT functions/////////////////////////

PRIVATE FRESULT ok(FRESULT fr) {// This is the function to check what needs to be done. 
    if(!exists_check.path_exists)
    {
         fr = FR_CHECK_IF_DATE_FOLDER_ALREADY_EXISTS;
         return(fr);
    }
    if(!exists_check.file_exists)
    {
         fr = FR_CHECK_IF_TIME_FILE_ALREADY_EXISTS;
         return(fr);
    }

    if(exists_check.file_exists && exists_check.path_exists)
    {
        fr = FR_ALL_DONE;
        return(fr);

    }
}

PRIVATE FRESULT start() { //This is the kick off function where the pico tries to mount onto the USB stick.

    FRESULT fr;
    fr = f_mount(fs, "0:", 0);
    return(fr); //sets off whole reaction
}

PRIVATE FRESULT no_path(FRESULT fr) {

    const char *fname = file_info.dates;
    fr = f_mkdir(fname);
    return fr;
}

PRIVATE FRESULT check_if_date_folder_already_exists(FRESULT fr) {
    DIR dir;
    FILINFO fno;

    exists_check.path_exists = false;

    fr = f_opendir(&dir, "/");
    if (fr != FR_OK)
        return fr;

    while (1)
    {
        fr = f_readdir(&dir, &fno);

        if (fr != FR_OK)
            break;

        if (fno.fname[0] == 0)
            break;  // end of directory

        // skip . and ..
        if (strcmp(fno.fname, ".") == 0 || strcmp(fno.fname, "..") == 0)
            continue;

        // skip hidden/system
        if (fno.fattrib & (AM_HID | AM_SYS))
            continue;

        // only directories
        if (fno.fattrib & AM_DIR)
        {
            if (strcmp(fno.fname, file_info.dates) == 0)
            {
                exists_check.path_exists = true;
                break;  
            }
        }
    }

    if (fno.fname[0] == 0)
    {
        fr = FR_NO_PATH;
    }

    f_closedir(&dir);

    return fr;
}

PRIVATE FRESULT check_if_time_folder_already_exists(FRESULT fr) {
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
    buffer = extract_ohm(buffer, ohms, sizeof(ohms)); //want to pass pointer and then modify pointer to pooubt ti bextl
    buffer = extract_voltage(buffer, voltage, sizeof(voltage));
    buffer = extract_current(buffer, current, sizeof(current));
    buffer = extract_test_time(buffer, test_time, sizeof(test_time));
    buffer = extract_comment(buffer, comment, sizeof(comment));
    buffer = extract_state(buffer, state, sizeof(state));
    
    //extract_voltage(buffer, voltage, size_t size); // this is for the next value, and so on and so fort

    fr = f_chdir(file_info.date_directory);

    if (fr != FR_OK) {
        return fr;
    }

    // Implement a function here to check for files in the directory and se

    if (check_if_folder_exists_in_date_directory(file_info)) { //this is for if the folder just created exists
        fr = f_getcwd(fullpath, sizeof(fullpath));

        if (fr != FR_OK) {
            return fr;
        }

        sprintf(temp, "%s/%s", fullpath, file_info.times_header);
        strcpy(fullpath, temp);

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
    else
    {
        fr = FR_NO_FILE;
        return(fr);
    }

    return FR_NO_PATH;
}
 



PRIVATE FRESULT no_file(FRESULT fr) {
    FIL fp;

    TCHAR temp[64];
    TCHAR fullpath[64];

    memset(fullpath, 0, sizeof(fullpath));
    memset(temp, 0, sizeof(temp));

    fr = f_chdir(file_info.date_directory);
    if (fr != FR_OK) {
        return fr; // can't open directory
    }
    fr = f_getcwd(fullpath, sizeof(fullpath));

     if (fr != FR_OK) {
          return fr;
         }

        sprintf(temp, "%s", file_info.times_header);
        strcpy(fullpath, temp);

        fr = f_open(&fp,temp, FA_WRITE | FA_CREATE_ALWAYS);
        if (fr == FR_OK) {
            f_puts("Date,\tTime,\tOhms(ohm)),\tVoltage(V),\tCurrent(A),\tTestTime,\tComment,\tState\n", &fp); //adds a tab in the middle
            f_close(&fp);
        }
        return(fr); //this is to create a new file


}

PRIVATE bool check_if_folder_exists_in_date_directory(File_Info file_info) {
    DIR dir;
    FILINFO fno;
    FRESULT fr;

    fr = f_opendir(&dir, file_info.date_directory);
    if (fr != FR_OK) {
        return false;
    }

    while (1)
    {
        fr = f_readdir(&dir, &fno);

        if (fr != FR_OK || fno.fname[0] == 0) {
            break;   // error or end of directory
        }

        if (strcmp(fno.fname, file_info.times_header) == 0) {
            f_closedir(&dir);
            return true;
        }
    }

    f_closedir(&dir);
    return false;
}


PRIVATE FRESULT invalid_name(FRESULT fr) {
    (void)fr;
}

PRIVATE FRESULT denied( FRESULT fr) {
    (void)fr;
}

PRIVATE FRESULT exist(FRESULT fr) {
    fr = FR_OK;
    return(fr);
}

PRIVATE FRESULT invalid_object(FRESULT fr) {
    (void)fr;
}

PRIVATE FRESULT not_enabled(FRESULT fr) {
    (void)fr;
}

PRIVATE FRESULT no_filesystem(FRESULT fr) {
    uint8_t work[FF_MAX_SS];
    f_mkfs("", NULL, work, sizeof(work));   /* makes file system here*/
    return(f_mount(&fatfs, "0:", 1));                 /*makes another attempt at mounting it*/
}

PRIVATE FRESULT mkfs_aborted(FRESULT fr) {
    (void)fr;
}

PRIVATE FRESULT timeout(FRESULT fr) {
    (void)fr;
}

PRIVATE FRESULT locked(FRESULT fr) {
    (void)fr;
}

PRIVATE FRESULT too_many_open_files(FRESULT fr) {
    (void)fr;
}

PRIVATE FRESULT start_error(FRESULT fr) {
    (void)fr;
}

///////////Helper functions/////////////////////////


static void get_file_info() {
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




 /// Helper function /////



// PRIVATE void extract_date_directory(const char *in_buf, char *out)
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

//// Extraction Function /////////////

PRIVATE void extract_date_directory(const uint8_t *in, char *dates_directory, size_t size) {
   memset(dates_directory,0,size);
   dates_directory[0] = '/';
   dates_directory[size-1]='\0';

   char *start = strchr(in,'/');  
   char *end = strchr(start+1,'/');
   char* const beginning = in; //immutable pointer to first 
   char *p = in+1;

   int i = 1;
   for(; p<end && i<size-1;p++){
        if (*p == '/'){
            *p = '-';
        }
        dates_directory[i++] = *p;

   }
   dates_directory[i++] = '-';
   ++p;

   char *end_final = strchr(end+1,',');
   for(; p<end_final && i<size-1; p++)
   {
        dates_directory[i++] = *p;
   }

   p = beginning;

}

PRIVATE void extract_date( char *in, char *dates, size_t size) {
    memset(dates,0,size);
    dates[size-1]='\0';
    char *end = strchr(in,'/');  
    char *p = in+1;

    int i = 0;
     for(; p<end && i<size-1;p++){
        if (*p == '/'){ // / can be mistaken for directory
            *p = '-';
        }
        dates[i++] = *p;
    }
//    dates[i++] = '-';
//    ++p;

   char *end_final = strchr(end+1,',');
   for(; p<end_final && i<size-1; p++)
   {
    if (*p == '/'){ // / can be mistaken for directory
            *p = '-';
        }
        dates[i++] = *p;
   }
}

PRIVATE char* extract_time(const char *in, char *time, size_t size) {
    memset(time,0,size);
    time[size-1] = '\0';
    char *start = strchr(in, ',');
    char *end   = strchr(start + 1, ',');
    char *const check = (char *)give_array_address();
    int i = 0;
    char *p = start + 1;

    for(; p < end && i < size-1; p++)
    {
        if(isspace(*p))
            continue;

         if(*p == ':'){
            *p = '-';
        } 

        time[i++] = *p;
    }

    return(p);



}

PRIVATE char* extract_ohm(char *in, char *ohms, size_t size) {
    memset(ohms, 0, size);

    

     char *ptr  = strchr(in , ',');
    if (!ptr) return NULL;

    

    char *ptr_end = strchr(ptr + 1, ','); // 
    if (!ptr_end) return NULL;

    ptr++;   // move past comma

    size_t j = 0;

    while (ptr < ptr_end && j < size - 1)
    {
        if (!isspace((unsigned char)*ptr))
        {
            ohms[j++] = *ptr;
        }
        ptr++;
    }

    ohms[j] = '\0';

    return ptr;
}

PRIVATE char* extract_voltage(char *in, char *voltage, size_t size) {
    memset(voltage, 0, size);

    char *ptr = strchr(in, ',');    //1st comma
    if (!ptr) return NULL;


    char *ptr_end = strchr(ptr + 1, ',');
    if (!ptr_end) return NULL;

    ptr++;   // move past comma

    size_t j = 0;

    while (ptr < ptr_end && j < size - 1)
    {
        if (!isspace((unsigned char)*ptr))
        {
           voltage[j++] = *ptr;
        }
        ptr++;
    }

    voltage[j] = '\0';

    return ptr;

}

PRIVATE char* extract_current(char *in, char *current, size_t size) {
    memset(current, 0, size);

    char *ptr = strchr(in, ',');    //1st comma
    if (!ptr) return NULL;

    char *ptr_end = strchr(ptr + 1, ',');
    if (!ptr_end) return NULL;

     ptr++;   // move past comma

    size_t j = 0;

    while (ptr < ptr_end && j < size - 1)
    {
        if (!isspace((unsigned char)*ptr))
        {
           current[j++] = *ptr;
        }
        ptr++;
    }

    current[j] = '\0';

    return ptr;

}

PRIVATE char* extract_test_time(char *in, char *test_time, size_t size) {
    memset(test_time, 0, size);

    char *ptr = strchr(in, ',');    //1st comma
    if (!ptr) return NULL;



    char *ptr_end = strchr(ptr + 1, ',');
    if (!ptr_end) return NULL;

     ptr++;   // move past comma

    size_t j = 0;

    while (ptr < ptr_end && j < size - 1)
    {
        if (!isspace((unsigned char)*ptr))
        {
           test_time[j++] = *ptr;
        }
        ptr++;
    }

    test_time[j] = '\0';

    return ptr;
}


PRIVATE char* extract_comment(char *in, char *comment, size_t size) {
    memset(comment, 0, size);

    char *ptr = strchr(in, ',');    //1st comma
    if (!ptr) return NULL;

    char *ptr_end = strchr(ptr + 1, ',');
    if (!ptr_end) return NULL;

     ptr++;   // move past comma

    size_t j = 0;

    while (ptr < ptr_end && j < size - 1)
    {
        if (!isspace((unsigned char)*ptr))
        {
           comment[j++] = *ptr;
        }
        ptr++;
    }

    comment[j] = '\0';

    return ptr;
}

PRIVATE char* extract_state(char *in, char *state, size_t size) {
    memset(state, 0, size);

    char *ptr = strchr(in, ',');    //1st comma
    if (!ptr) return NULL;

    char *ptr_end = strchr(ptr + 1, '\n');
    if (!ptr_end) return NULL;

     ptr++;   // move past comma

    size_t j = 0;

    while (ptr < ptr_end)
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







/* memset(dates,0,size);
 
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
    }*/



/* PRIVATE FRESULT read_root_directory() //should work in any directory
{
    DIR dir;                // directory object (not a pointer)
    FILINFO filinfo;        // file information structure
    char *filename;
    FRESULT fr;

    // open current directory
    fr = f_opendir(&dir, "/");

    if (fr != FR_OK)
    {
        return FR_NO_PATH;
    }

    // read directory entries one by one
    while (1)
    {
        fr = f_readdir(&dir, &filinfo);

        // stop if error or end of directory
        if (fr != FR_OK || filinfo.fname[0] == 0)
        {
            break;
        }

        filename = filinfo.fname;

        // get file metadata
        fr = f_stat(filename, &filinfo);

        switch (fr)
        {
            case FR_NO_FILE:
            case FR_NO_PATH:
                fr = FR_NO_PATH;
                break;

            case FR_OK:

                // check if entry is a directory
                if (filinfo.fattrib & AM_DIR)
                {
                     if(strncmp(file_info.fname, file_info.dates,10) == 0) // Check if the directory name matches the date.  Since that will be the one files will be based on
                {
                    if(f_stat(file_info.times, &fno) == FR_OK) //  Checks if time exists in the file
                    {
                        fr = f_open(&fil, file_info.times, FA_OPEN_APPEND | FA_OPEN_EXISTING); // Open the directory for writing
                        f_puts(give_array_address(), &fil);
                        exists_check.file_exists = true; // Set flag to indicate that the path now exists
                    }
                    else
                    {
                         fr = f_open(&fil, file_info.times, FA_WRITE | FA_CREATE_ALWAYS);
                         f_puts(give_array_address(), &fil);
                         exists_check.file_exists = true; // Set flag to indicate that the path now exists
                    }

                }
                }

                break;

            default:
                break;
        }
    }

    // close directory when finished
    f_closedir(&dir);

    return fr;
} */


// void dir(const char *dirname)
// {
//     FRESULT fr;
// 	DIR *dp;
// 	FILINFO file_info;
// 	struct stat fs;
// 	char *filename;
// 	char directory[BUFSIZ];
//     fr = f_chdir(dirname)
// 	if( fr != FR_OK )
// 	{
// 		fprintf(stderr,"Unable to change to %s\n",dirname);
// 		exit(1);
// 	}

// 	fr = f_getcwd(directory, BUFSIZE); 


// 	dp = opendir(directory);
// 	if( dp==NULL )
// 	{
// 		fprintf(stderr,"Unable to read directory '%s'\n",
// 				directory
// 			   );
// 		exit(1);
// 	}

// 	printf("%s\n",directory);
// 	while( (entry=readdir(dp)) != NULL )
// 	{
// 		filename = entry->d_name;
// 		if( strncmp( filename,".",1)==0 )
// 			continue;

// 		stat(filename,&fs);
// 		if( S_ISDIR(fs.st_mode) )
// 			dir(filename);
// 	}

// 	closedir(dp);
// }

// PRIVATE void getcwd(char *buff, uint len)
// {

//     fr = f_getcwd(cwd, BUFSIZE); 
// }