/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*
* AUTHORS : Space Concordia 2014, Joseph 
*           (inspired by the previous version : getlog-command-cpp-obsolete201404)
*
* TITLE : getlog-command.cpp
*
*----------------------------------------------------------------------------*/
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <iostream>
#include <limits.h>
#include <time.h>
#include <string.h>

#include <assert.h>

#include "shakespeare.h"
#include "SpaceString.h"
#include "common/subsystems.h"
#include "common/commands.h"
#include "common/getlog-command.h"

extern const char* s_cs1_subsystems[];  // defined in subsystems.cpp

static char log_buf[CS1_MAX_LOG_ENTRY] = {0};

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*
* NAME : GetLogCommand
*
*-----------------------------------------------------------------------------*/
GetLogCommand::GetLogCommand()
{
    this->opt_byte = 0x0;
    this->size = CS1_MAX_FRAME_SIZE;        // Max number of bytes to retreive.  size / tgz-part-size = number of frames
    this->date = Date();                    // Default to oldest possible log file.
    this->subsystem = 0x0;
    this->number_of_processed_files = 0;
}

GetLogCommand::GetLogCommand(char opt_byte, char subsystem, size_t size, time_t time)
{
    this->opt_byte = opt_byte;
    this->subsystem = subsystem;
    this->size = size;
    this->date = Date(time);
    this->number_of_processed_files = 0;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*
* NAME : ~GetLogCommand
* 
*-----------------------------------------------------------------------------*/
GetLogCommand::~GetLogCommand()
{

}
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*
* NAME : Execute 
* 
* PURPOSE : Executes the GetLogCommand.
*           - if no OPT_SIZE is not specified, retreives one tgz
*           - if OPT_SIZE is specified, retreives floor(SIZE / CS1_MAX_FRAME_SIZE) tgzs
*
*-----------------------------------------------------------------------------*/
void* GetLogCommand::Execute(size_t *pSize)
{
    /* TODO IN PROGRESS : add [INFO] and [END] bytes before and after EACH file read into result buffer.
    * result : [INFO] + [TGZ DATA] + [END]
    * INFO : use inode instead of filename to limit the size
    */
    char get_log_status = CS1_SUCCESS; 
    char *result = 0;

    char filepath[CS1_PATH_MAX] = {'\0'};
    char buffer[CS1_MAX_FRAME_SIZE] = {'\0'};
    char *file_to_retreive = 0;
    size_t bytes = 0;
    size_t number_of_files_to_retreive = 1;         // defaults to 1

    if (OPT_ISSIZE(this->opt_byte)) { 
        number_of_files_to_retreive = this->size / CS1_MAX_FRAME_SIZE;
    }

    while (number_of_files_to_retreive) { 
        file_to_retreive = this->GetNextFile();
#ifdef CS1_DEBUG
       memset(this->log_buffer, 0, CS1_MAX_LOG_ENTRY);
       snprintf(this->log_buffer,CS1_MAX_LOG_ENTRY, " %s() - file_to_retreive : %s\n", __func__, file_to_retreive);
       Shakespeare::log(Shakespeare::DEBUG, cs1_systems[CS1_COMMANDER], this->log_buffer);

#endif
        if ( file_to_retreive[0] != '\0' )
        {
            SpaceString::BuildPath(filepath, CS1_TGZ, file_to_retreive);

            // Prepares Info bytes 
            GetLogCommand::GetInfoBytes(buffer, filepath);
            bytes += GETLOG_INFO_SIZE; 

            // Reads the file in 'buffer'
            bytes += GetLogCommand::ReadFile(buffer + bytes, filepath); 

            // add END bytes 
            bytes += GetLogCommand::GetEndBytes(buffer + bytes);

            // Track
            number_of_files_to_retreive--; 
            this->MarkAsProcessed(filepath); /* 'filepath' is considered as processed for this instance of the GetLogCommand
                                         *  if you send a new GetLogCommand with the same parameters, 'filepath' will not
                                         *  be considered as processed. i.e. the processed_files array belongs to this 
                                         *  instance only
                                         */
        }
        else
        {
            get_log_status = CS1_FAILURE;
            number_of_files_to_retreive--;
        }    
    }
    // add END bytes
    bytes += GetLogCommand::GetEndBytes(buffer + bytes);

    // allocate the result buffer
    result = (char*)malloc(sizeof(char) * (bytes + CMD_RES_HEAD_SIZE));
    
    result[0] = GETLOG_CMD;
    result[1] = get_log_status;
    if (result) {
        // Saves the tgz data in th result buffer
        memcpy(result + CMD_RES_HEAD_SIZE, buffer, bytes);
    }

    *pSize = bytes + CMD_RES_HEAD_SIZE;
    return (void*)result;
}
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*
* NAME : ReadFile
* 
* PURPOSE : Reads the specefied file into the specified buffer.
*           N.B. max size is CS1_MAX_FRAME_SIZE
*
*-----------------------------------------------------------------------------*/
size_t GetLogCommand::ReadFile(char *buffer, const char *filename)
{
    return GetLogCommand::ReadFile_FromStartToEnd(buffer, filename, START, CS1_MAX_FRAME_SIZE);
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*
* NAME : ReadFile_FromStartToEnd
* 
* PURPOSE : Reads 'filename' starting from byte 'start' to byte 'start + size'
*           into 'buffer', use this function if the hability to retreive
*           partial files is needed. 
*
* RETURN : The number of bytes read into buffer.
*
*-----------------------------------------------------------------------------*/
size_t GetLogCommand::ReadFile_FromStartToEnd(char *buffer, const char *filename, size_t start, size_t size)
{
    FILE *pFile = fopen(filename, "rb");
    size_t bytes = 0;

    if (!buffer) {
	memset(log_buf, 0, CS1_MAX_LOG_ENTRY);
        snprintf(log_buf, CS1_MAX_LOG_ENTRY, " %s:%d - The output buffer is NULL.\n", __func__, __LINE__);
        Shakespeare::log(Shakespeare::ERROR, cs1_systems[CS1_COMMANDER], log_buf);
        return bytes;
    }

    if (!pFile) {
	memset(log_buf, 0, CS1_MAX_LOG_ENTRY);
        snprintf(log_buf,CS1_MAX_LOG_ENTRY, " %s:%d - Cannot fopen the file.\n", __func__, __LINE__);
	Shakespeare::log(Shakespeare::ERROR, cs1_systems[CS1_COMMANDER], log_buf);
        return bytes;
    }

    fseek(pFile, start, 0);
    bytes = fread(buffer, 1, size, pFile);

    if (feof(pFile)) {
        #ifdef CS1_DEBUG
	memset(log_buf, 0, CS1_MAX_LOG_ENTRY);
        snprintf(log_buf,CS1_MAX_LOG_ENTRY, "%s EOF reached \n", __func__);
	Shakespeare::log(Shakespeare::NOTICE, cs1_systems[CS1_COMMANDER], log_buf);
        #endif
    } else {
	memset(log_buf, 0, CS1_MAX_LOG_ENTRY);
        snprintf(log_buf,CS1_MAX_LOG_ENTRY, " %s:%s:%d - EOF has not been reached, the file will be incomplete", __FILE__, __func__, __LINE__);
	Shakespeare::log(Shakespeare::ERROR, cs1_systems[CS1_COMMANDER], log_buf);
    }

    // Cleanup
    if (pFile) {
        fclose(pFile);
        pFile = 0;
    }

    return bytes;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*
* NAME : GetNextFile 
* 
* PURPOSE : Returns the name of the next file to retreive according to the 
*           opt_byte
*           
* RETURN  : char* to STATIC memory! A second call to GetNextFile will overwrite
*           the buffer.
*
*-----------------------------------------------------------------------------*/
char* GetLogCommand::GetNextFile(void) 
{
    static char filename[CS1_NAME_MAX] = {'\0'};
    char* buf = 0;

    if (OPT_ISNOOPT(this->opt_byte)) 
    { 
        // 1. No Options are specified, retreive the oldest package.
	memset(this->log_buffer, 0, CS1_MAX_LOG_ENTRY);
        snprintf(this->log_buffer,CS1_MAX_LOG_ENTRY, " Execute GetLogCommand with OPT_NOOPT : Finding oldest tgz...\n");
	Shakespeare::log(Shakespeare::NOTICE, cs1_systems[CS1_COMMANDER], this->log_buffer);
        buf = GetLogCommand::FindOldestFile(CS1_TGZ, NULL);     // Pass NULL to match ANY Sub
    } 
    else if (OPT_ISSUB(this->opt_byte) && !OPT_ISDATE(this->opt_byte)) 
    {
        // 2. The Subsystem is defined, retreive the oldest package that belongs to that subsystem.
	memset(this->log_buffer, 0, CS1_MAX_LOG_ENTRY);
        snprintf(this->log_buffer,CS1_MAX_LOG_ENTRY, " Execute GetLogCommand with OPT_SUB : Finding oldest tgz that matches SUB...\n");
	Shakespeare::log(Shakespeare::NOTICE, cs1_systems[CS1_COMMANDER], this->log_buffer);
        buf = GetLogCommand::FindOldestFile(CS1_TGZ, s_cs1_subsystems[(size_t)this->subsystem]);
    } 
    else if (OPT_ISSUB(this->opt_byte) && OPT_ISDATE(this->opt_byte)) 
    {
        // Assuming there is only one file with this SUB and this DATE <- NOT TRUE!
	memset(this->log_buffer, 0, CS1_MAX_LOG_ENTRY);
        snprintf(this->log_buffer,CS1_MAX_LOG_ENTRY, " %s():%d OPT_SUB | OPT_DATE\n", __func__, __LINE__);
	Shakespeare::log(Shakespeare::DEBUG, cs1_systems[CS1_COMMANDER], this->log_buffer);
        char pattern[CS1_NAME_MAX];
        strcpy(pattern, s_cs1_subsystems[(size_t)this->subsystem]);
        strcat(pattern, this->date.GetString());
	memset(this->log_buffer, 0, CS1_MAX_LOG_ENTRY);
        snprintf(this->log_buffer,CS1_MAX_LOG_ENTRY, " %s():%d OPT_DATE | OPT_DATE : pattern is %s\n", __func__, __LINE__, pattern);
	Shakespeare::log(Shakespeare::DEBUG, cs1_systems[CS1_COMMANDER], this->log_buffer);

        buf = GetLogCommand::FindOldestFile(CS1_TGZ, pattern);
    }
    
    if (buf) { 
        assert(strlen(buf) < CS1_NAME_MAX);
        strcpy(filename, buf);

        free (buf);
        buf = 0;
    } else {
        memset(filename, '\0', CS1_NAME_MAX); // if but is null, clear the static char buffer!
    }

    return filename;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*
* NAME : FindOldestFile 
* 
* PURPOSE : Returns the name of the oldest file present in the specified 
*           directory and that matches 'pattern' (if not NULL)
*           N.B. returns a newly allocated char*  FREE IT
*
*-----------------------------------------------------------------------------*/
char* GetLogCommand::FindOldestFile(const char* directory_path, const char* pattern) 
{
    struct dirent* dir_entry = 0;
    DIR* dir = 0;
    time_t oldest_timeT = INT_MAX - 1;
    time_t current_timeT = 0;
    char buffer[CS1_PATH_MAX] = {'\0'};
    char* oldest_filename = (char*)malloc(sizeof(char) * CS1_NAME_MAX);

    if (!oldest_filename) {
        return 0;
    }

    memset(oldest_filename, '\0', CS1_NAME_MAX * sizeof(char));
    
    dir = opendir(directory_path);

    while ((dir_entry = readdir(dir))) { 
        if (dir_entry->d_type == DT_REG // is a regular file
                && !this->isFileProcessed(dir_entry->d_ino) // is NOT processed
                    && GetLogCommand::prefixMatches(dir_entry->d_name, pattern)) 
        { 
            SpaceString::BuildPath(buffer, directory_path, dir_entry->d_name);

            current_timeT = GetLogCommand::GetFileLastModifTimeT(buffer); 

            if (current_timeT < oldest_timeT) {
                oldest_timeT = current_timeT;
                strncpy(oldest_filename, dir_entry->d_name, strlen(dir_entry->d_name) + 1);
            }
        }
    }
    
    if (dir) {
        closedir(dir);
        dir = NULL;
    }

    return oldest_filename; 
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*
* NAME : MarkAsProcessed 
* 
* PURPOSE : Add the inode of a file to the processed_files array
*
*-----------------------------------------------------------------------------*/
void GetLogCommand::MarkAsProcessed(const char *filepath) 
{
    struct stat attr;
    stat(filepath, &attr);

#ifdef CS1_DEBUG
    memset(this->log_buffer, 0, CS1_MAX_LOG_ENTRY);
    snprintf(this->log_buffer,CS1_MAX_LOG_ENTRY, " %s() - inode %d\n", __func__, (unsigned int)attr.st_ino);
    Shakespeare::log(Shakespeare::DEBUG, cs1_systems[CS1_COMMANDER], this->log_buffer);
#endif
    this->processed_files[this->number_of_processed_files] = attr.st_ino;
    this->number_of_processed_files++;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*
* NAME : isFileProcessed 
* 
* PURPOSE : Checks if a file has been processed
*
*-----------------------------------------------------------------------------*/
bool GetLogCommand::isFileProcessed(unsigned long inode) 
{
    for (size_t i = 0; i < this->number_of_processed_files; i++) {
        if (inode == this->processed_files[i]) {
#ifdef CS1_DEBUG
	    memset(this->log_buffer, 0, CS1_MAX_LOG_ENTRY);
            snprintf(this->log_buffer,CS1_MAX_LOG_ENTRY, " %s() - inode %d\n", __func__, (unsigned int)inode);
	    Shakespeare::log(Shakespeare::DEBUG, cs1_systems[CS1_COMMANDER], this->log_buffer);
#endif
            return true;
        }
    }
    
    return false;
}

/* Jacket over the preceding function, to accept the file path */
bool GetLogCommand::isFileProcessed(const char *filepath)
{
    struct stat attr;
    stat(filepath, &attr);

    return this->isFileProcessed(attr.st_ino);
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*
* NAME : GetFileLastModifTimeT 
* 
* PURPOSE : return the time_t corresponding to the last modification date of 
*           a specified file.
*
*-----------------------------------------------------------------------------*/
time_t GetLogCommand::GetFileLastModifTimeT(const char *path) 
{
    struct stat attr;
    stat(path, &attr);
    return attr.st_mtime;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*
* NAME : prefixMatches 
* 
* PURPOSE : Returns true if the 'filename' constains 'pattern'
*
*-----------------------------------------------------------------------------*/
bool GetLogCommand::prefixMatches(const char* filename, const char* pattern) 
{
    if (filename == NULL || pattern == NULL) {
        return true;
    }

    if (strstr(filename, pattern)) {
	    return true;
    } 

    return false;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*
* NAME : Build_GetLogCommand 
* 
* PURPOSE : Builds a GetLogCommand and saves it into 'command_buf'
*
*-----------------------------------------------------------------------------*/
char* GetLogCommand::Build_GetLogCommand(char command_buf[GETLOG_CMD_SIZE], char opt_byte, char subsystem, size_t size, time_t date) 
{
   command_buf[0] = GETLOG_CMD;
   command_buf[1] = opt_byte;
   command_buf[2] = subsystem;
   SpaceString::get4Char(command_buf + 3, size);
   SpaceString::get4Char(command_buf + 7, date);

   return command_buf;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*
* NAME : GetInfoBytes 
* 
* PURPOSE : Builds and saves the info bytes for the file 'filepath' at the 
*           location pointed to by 'buffer' 
*
* DESCRIPTION : [ino_t] - inode off the file, to uniquely identify
*                          it and be able to call the DeleteLogCommand with it.
*               [checksum] - TODO
*
*-----------------------------------------------------------------------------*/
char* GetLogCommand::GetInfoBytes(char *buffer, const char *filepath) 
{
    char info_bytes[GETLOG_INFO_SIZE] = {'\0'};

    ino_t inode = GetLogCommand::GetInoT(filepath); // gets the inode of the file
    SpaceString::get4Char(info_bytes, inode);       // saves it in the buffer

    memcpy(buffer, info_bytes, GETLOG_INFO_SIZE);
    return buffer;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*
* NAME : GetEndBytes
* 
* PURPOSE : Used to separate files in the result buffer of the GetLogCommand
*
*-----------------------------------------------------------------------------*/
int GetLogCommand::GetEndBytes(char *buffer)
{
    buffer[0] = EOF;
    buffer[1] = EOF;
   
    return GETLOG_ENDBYTES_SIZE;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*
* NAME : BuildInfoBytesStruct
* 
* PURPOSE : Receives a buffer containing GETLOG_INFO_SIZE bytes and populates
*           the InfoBytes struct at *pInfo with those data.
*
*-----------------------------------------------------------------------------*/
InfoBytes* GetLogCommand::BuildInfoBytesStruct(GetLogInfoBytes* pInfo, const char *buffer)
{
    if (pInfo) {
        pInfo->inode = SpaceString::getUInt(buffer); 
    }

   return pInfo;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*
* NAME : GetInoT
* 
* PURPOSE : Returns the ino_t associated with the file at 'filepath'
*
*-----------------------------------------------------------------------------*/
ino_t GetLogCommand::GetInoT(const char *filepath)
{
    struct stat attr;
    stat(filepath, &attr);

    return attr.st_ino;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*
* NAME : GetCmdStr
* 
* PURPOSE : Builds a GetLogCommand and saves it into 'cmd_buf', meant to be
*           use by the GroundCommander. 
*
*-----------------------------------------------------------------------------*/
char* GetLogCommand::GetCmdStr(char* cmd_buf)
{
    GetLogCommand::Build_GetLogCommand(cmd_buf,
                                       this->opt_byte,
                                       this->subsystem,
                                       this->size,
                                       this->date.GetTimeT());
    
    return cmd_buf;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*
* NAME : ParseResult                                                        TODO UnitTest me
* 
* PURPOSE : Parses the result buffer returned by the execute function.
*
* ARGUMENTS : result    : pointer to the result buffer
*             filepath  : string of the form "path/file.log" (if the file exists,
*                         it will be overwritten)
*   
* RETURN : struct InfoBytes* to STATIC memory (Make a COPY!)
*
*-----------------------------------------------------------------------------*/
InfoBytes* GetLogCommand::ParseResult(char *result, const char *filename)
{
    static struct GetLogInfoBytes info_bytes;
    FILE* pFile = 0;

    if (!result || result[CMD_ID] != GETLOG_CMD) {
        Shakespeare::log(Shakespeare::ERROR,cs1_systems[CS1_COMMANDER],"GetLog failure: Can't parse result");
        info_bytes.getlog_status = CS1_FAILURE;
        return &info_bytes;
    }

    info_bytes.getlog_status = result[CMD_STS];
    if (info_bytes.getlog_status == CS1_SUCCESS)
    {
        result += CMD_RES_HEAD_SIZE;

        // 1. Get InfoBytes
        this->BuildInfoBytesStruct(&info_bytes, result);
        result += GETLOG_INFO_SIZE; 

        // 2. Save data as a file
        info_bytes.getlog_message = result; 

        if (filename) 
        {
            pFile = fopen(filename, "wb");

            if (!pFile) {
		memset(this->log_buffer, 0, CS1_MAX_LOG_ENTRY);
                snprintf(this->log_buffer,CS1_MAX_LOG_ENTRY, " %s:%s:%d cannot create the file %s\n", 
                                                __FILE__, __func__, __LINE__, filename);
		Shakespeare::log(Shakespeare::ERROR, cs1_systems[CS1_COMMANDER], this->log_buffer);
            }
        }

        int bytes = 0;
        while (*result != EOF) 
        {
            if (pFile) {
                fwrite(result , 1, 1, pFile);        // why one byte at a time ??? QUESTION 
            }

            result++;
            bytes++;
        }
	
	memset(this->log_buffer, 0, CS1_MAX_LOG_ENTRY);
        snprintf(this->log_buffer, CS1_MAX_LOG_ENTRY, 
                    "GetLog success with inode %lu and with message(%i bytes)", info_bytes.inode, bytes); // modified type %u to  %lu since  info_bytes.inode is of type ino_t which is long unsigned int
        Shakespeare::log(Shakespeare::NOTICE, cs1_systems[CS1_COMMANDER], this->log_buffer);

        info_bytes.message_bytes_size = bytes;
        info_bytes.next_file_in_result_buffer = this->HasNextFile(result);

        if (pFile) {
            fclose(pFile);
        }
    }
    else
    {
       Shakespeare::log(Shakespeare::ERROR,cs1_systems[CS1_COMMANDER], "GetLog failure: No files may exist");
    }

    return &info_bytes;    
}

InfoBytes* GetLogCommand::ParseResult(char* result)
{
    return this->ParseResult(result, 0);
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*
* NAME : HasNextFile 
* 
* PURPOSE : Check if END bytes (2x) are there (i.e. EOF EOF EOF EOF) 
*           This means that there is no more files in the result buffer.
*   
*-----------------------------------------------------------------------------*/
const char* GetLogCommand::HasNextFile(const char* result)
{
    int eof_count = 0;
    const char *current_char = result;

    while(*current_char == EOF) {
        eof_count++;    
        current_char++;
    }

    if (eof_count == 4) {
        return 0;
    }

    if (eof_count == 2) {
        return result + 2;
    }

    return 0;
}
