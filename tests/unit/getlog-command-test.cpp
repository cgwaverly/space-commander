/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*
* AUTHORS : Space Concordia 2014, Joseph
*
* TITLE : getlog-command-test.cpp 
*
* DESCRIPTION : Tests the GetLogCommand class
*
*----------------------------------------------------------------------------*/
#include <time.h>
#include <unistd.h>
#include <dirent.h>     // DIR
#include <sys/types.h>
#include <sys/stat.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/MemoryLeakDetectorMallocMacros.h"

#include "SpaceDecl.h"

#include "SpaceString.h"
#include "common/command-factory.h"
#include "common/getlog-command.h"
#include "common/icommand.h"
#include "fileIO.h"
#include "common/commands.h"
#include "common/subsystems.h"
#include "dirUtl.h"

static char command_buf[GETLOG_CMD_SIZE] = {'\0'};

static void create_file(const char* path, const char* msg);

#define UTEST_SIZE_OF_TEST_FILES 6
static const char* data_6_bytes = "123456";

TEST_GROUP(GetLogTestGroup) {
    void setup(){
        mkdir(CS1_TGZ, S_IRWXU);
        memset(command_buf, 0, GETLOG_CMD_SIZE);

    }
    void teardown(){
        DeleteDirectoryContent(CS1_TGZ);
        rmdir(CS1_TGZ);
    }
};

void create_file(const char* path, const char* msg) {
    FILE* file = fopen(path, "w+");
    fprintf(file, "%s", msg);
    fclose(file);
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * GROUP : GetLogTestGroup
 *
 * NAME : Execute_OPT_NOOPT_NOFILES
 * 
 *-----------------------------------------------------------------------------*/
TEST(GetLogTestGroup, Execute_OPT_NOOPT_NOFILES) {
    // This is the Command to create on the ground.
    size_t result_size;
    GetLogCommand ground_cmd(OPT_NOOPT, 0, 0, 0);
    ground_cmd.GetCmdStr(command_buf);

    ICommand *command = CommandFactory::CreateCommand(command_buf);
    char* result = (char*)command->Execute(&result_size);

    CHECK(result_size == 4 );
    CHECK(result[1] == CS1_FAILURE);
    GetLogInfoBytes *getlog_info = (GetLogInfoBytes*)command->ParseResult(result);

    CHECK(getlog_info->getlog_status == CS1_FAILURE);
    // Cleanup
    if (command){
        delete command;
        command = NULL;
    }

    if (result) {
        free(result);
        result = 0;
    }
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * GROUP : GetLogTestGroup
 *
 * NAME : GetInfoBytes_returnsCorrectInfoBytes
 * 
 *-----------------------------------------------------------------------------*/
TEST(GetLogTestGroup, GetInfoBytes_returnsCorrectInfoBytes) {
    const char *filepath = CS1_TGZ"/Watch-Puppy20140101.tgz";
    char buffer[GETLOG_INFO_SIZE] = {0};

    create_file(filepath, data_6_bytes);

    struct stat attr;
    stat(filepath, &attr);

    GetLogCommand::GetInfoBytes(buffer, filepath);
    ino_t inode = SpaceString::getUInt(buffer);     // for now, there is only the inode (4 bytes) to check

    CHECK_EQUAL((unsigned int)inode, (unsigned int)attr.st_ino);
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * GROUP : GetLogTestGroup
 *
 * NAME : Execute_OPT_DATE_OPT_SUB_getTgz_returnsCorrectFile
 * 
 *-----------------------------------------------------------------------------*/
TEST(GetLogTestGroup, Execute_OPT_DATE_OPT_SUB_getTgz_returnsCorrectFile) {
    const char* path = CS1_TGZ"/Updater20140102.txt";  
    size_t result_size;

    create_file(CS1_TGZ"/Watch-Puppy20140101.txt", "file a");
    usleep(1000000);
    create_file(CS1_TGZ"/Updater20140102.txt", "file b");
    usleep(1000000);
    create_file(CS1_TGZ"/Updater20140103.txt", "file c");
    usleep(5000);

    char* result = 0;
    const char* dest = CS1_TGZ"/Watch-Puppy20140102.txt-copy";

    Date date(2014, 1, 2); 

    // This is the Command to create on the ground.
    GetLogCommand ground_cmd(OPT_DATE|OPT_SUB, UPDATER, 0, date.GetTimeT());
    ground_cmd.GetCmdStr(command_buf);

    // This is the Command that the space-commander will create
    GetLogCommand *command = (GetLogCommand*)CommandFactory::CreateCommand(command_buf);
    result = (char*)command->Execute(&result_size);

    CHECK(result_size == 16);

    GetLogInfoBytes *getlog_info = (GetLogInfoBytes*)command->ParseResult(result, dest);

    #ifdef CS1_DEBUG
    std::cerr << "[DEBUG] " << __FILE__ << " indoe is "  << getlog_info->inode << endl;
    #endif

    CHECK_EQUAL(0, getlog_info->next_file_in_result_buffer);
    CHECK(*(result + CMD_RES_HEAD_SIZE + GETLOG_INFO_SIZE + UTEST_SIZE_OF_TEST_FILES) == EOF);
    CHECK(*(result + CMD_RES_HEAD_SIZE + GETLOG_INFO_SIZE + UTEST_SIZE_OF_TEST_FILES + 1) == EOF);
    CHECK(diff(dest, path));     

    // Check inode
    struct stat attr;
    stat(path, &attr);
    CHECK_EQUAL(attr.st_ino, getlog_info->inode);


    // Cleanup
    if (command){
        delete command;
        command = NULL;
    }

    if (result) {
        free(result);
        result = 0;
    }
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * GROUP : GetLogTestGroup
 *
 * NAME : Execute_OPT_NOOPT_get2TGZ_returns2OldestTgz 
 * 
 *-----------------------------------------------------------------------------*/
TEST(GetLogTestGroup, Execute_OPT_NOOPT_get2TGZ_returns2OldestTgz) {
    const char* path = CS1_TGZ"/Watch-Puppy20140101.txt";  
    const char* path2 = CS1_TGZ"/Updater20140102.txt";  
    size_t result_size;
    
    create_file(CS1_TGZ"/Watch-Puppy20140101.txt", "file a");
    usleep(1000000);
    create_file(CS1_TGZ"/Updater20140102.txt", "file b");
    usleep(1000000);
    create_file(CS1_TGZ"/Updater20140103.txt", "file c");
    usleep(5000);

    char* result = 0;
    const char* dest = CS1_TGZ"/Watch-Puppy20140101.txt-copy";
    const char* dest2 = CS1_TGZ"/Updater20140102.txt-copy";

    // This is the Command to create on the ground.
    GetLogCommand ground_cmd(OPT_SIZE, 0, (size_t)CS1_MAX_FRAME_SIZE * 2, 0);
    ground_cmd.GetCmdStr(command_buf);

    ICommand *command = CommandFactory::CreateCommand(command_buf);
    result = (char*)command->Execute(&result_size);

    FILE *pFile = fopen(dest, "wb");

    if (pFile) {
        fwrite(result + CMD_RES_HEAD_SIZE + GETLOG_INFO_SIZE, 1, 6, pFile);        // TODO fix this to read until EOF, or add the size to the result buffer
        fclose(pFile);
    }

    pFile = fopen(dest2, "wb");

    if (pFile) {
        fwrite(result + CMD_RES_HEAD_SIZE + 6 + 2 * GETLOG_INFO_SIZE + GETLOG_ENDBYTES_SIZE, 1, 6, pFile); // TODO fix this to read until EOF, or add the size to the result buffer
        fclose(pFile);
    }

    CHECK(*(result + GETLOG_INFO_SIZE + CMD_RES_HEAD_SIZE + 6) == EOF);
    CHECK(*(result + GETLOG_INFO_SIZE + CMD_RES_HEAD_SIZE + 7) == EOF);
    CHECK(diff(dest, path));     
    CHECK(diff(dest2, path2));     

    // Cleanup
    if (command){
        delete command;
        command = NULL;
    }

    if (result) {
        free(result);
        result = 0;
    }

}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * GROUP : GetLogTestGroup
 *
 * NAME : Execute_OPT_NOOPT_returnsOldestTgz 
 * 
 *-----------------------------------------------------------------------------*/
TEST(GetLogTestGroup, Execute_OPT_NOOPT_returnsOldestTgz) {
    const char* path = CS1_TGZ"/Watch-Puppy20140101.txt";  
    size_t result_size;

    create_file(CS1_TGZ"/Watch-Puppy20140101.txt", "file a");
    usleep(1000000);
    create_file(CS1_TGZ"/Updater20140102.txt", "file b");
    usleep(1000000);
    create_file(CS1_TGZ"/Updater20140103.txt", "file c");
    usleep(5000);

    char* result = 0;
    const char* dest = CS1_TGZ"/Watch-Puppy20140101.txt-copy";

    // This is the Command to create on the ground.
    GetLogCommand ground_cmd(OPT_NOOPT, 0, 0, 0);
    ground_cmd.GetCmdStr(command_buf);

    ICommand *command = CommandFactory::CreateCommand(command_buf);
    result = (char*)command->Execute(&result_size);

    FILE *pFile = fopen(dest, "wb");

    if (pFile) {
        fwrite(result + CMD_RES_HEAD_SIZE + GETLOG_INFO_SIZE, 1, 6, pFile);        // TODO fix this to read until EOF, or add the size to the result buffer
        fclose(pFile);
    }

    CHECK(diff(dest, path));     

    // Cleanup
    if (command){
        delete command;
        command = NULL;
    }

    if (result) {
        free(result);
        result = 0;
    }
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * GROUP : GetLogTestGroup
 *
 * NAME : ReadFile_FromStartToEnd_success 
 * 
 *-----------------------------------------------------------------------------*/
TEST(GetLogTestGroup, ReadFile_FromStartToEnd_success) {
    char buffer[CS1_MAX_FRAME_SIZE + 50] = {0};
    const char* path = CS1_TGZ"/Updater20140101.txt";
    const char* dest = CS1_TGZ"/Updater20140101.txt-copy";
    create_file(path, "data");

    size_t bytes = GetLogCommand::ReadFile_FromStartToEnd(buffer, path, 0, CS1_MAX_FRAME_SIZE);

    FILE *pFile = fopen(dest, "wb");

    if (pFile) {
        fwrite(buffer, 1, bytes, pFile);
        fclose(pFile);
    }

   CHECK(diff(dest, path));     
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * GROUP : GetLogTestGroup
 *
 * NAME : GetNextFile_SUB_returnsOldestFilenameThatBelongsToSub 
 * 
 *-----------------------------------------------------------------------------*/
TEST(GetLogTestGroup, GetNextFile_SUB_returnsOldestFilenameThatBelongsToSub) {
    create_file(CS1_TGZ"/Watch-Puppy20140101.txt", "file a");
    usleep(1000000);
    create_file(CS1_TGZ"/Updater20140102.txt", "file b");
    usleep(1000000);
    create_file(CS1_TGZ"/Updater20140103.txt", "file c");
    usleep(5000);

    command_buf[0] = GETLOG_CMD;
    command_buf[1] = OPT_SUB;
    command_buf[2] = UPDATER;
    GetLogCommand *command = (GetLogCommand*)CommandFactory::CreateCommand(command_buf);

    char* oldest_file = command->GetNextFile();
    STRCMP_EQUAL("Updater20140102.txt", oldest_file);

    if (command != NULL){
        delete command;
        command = NULL;
    }

}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * GROUP : GetLogTestGroup
 *
 * NAME : GetNextFile_NOOPT_returnsOldestFilename 
 * 
 *-----------------------------------------------------------------------------*/
TEST(GetLogTestGroup, GetNextFile_NOOPT_returnsOldestFilename) {
    create_file(CS1_TGZ"/a.txt", "file a");
    usleep(1000000);
    create_file(CS1_TGZ"/b.txt", "file b");
    usleep(1000000);
    create_file(CS1_TGZ"/c.txt", "file c");
    usleep(5000);

    command_buf[0] = GETLOG_CMD;

    GetLogCommand *command = (GetLogCommand*)CommandFactory::CreateCommand(command_buf);

    char* oldest_file = command->GetNextFile();
    STRCMP_EQUAL("a.txt", oldest_file);

    if (command != NULL){
        delete command;
        command = NULL;
    }
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * GROUP : GetLogTestGroup
 *
 * NAME : FindOldestFile_OPT_SUB_returnsCorrectFilename 
 * 
 *-----------------------------------------------------------------------------*/
TEST(GetLogTestGroup, FindOldestFile_OPT_SUB_returnsCorrectFilename) {
    GetLogCommand command;

    create_file(CS1_TGZ"/SubA20140101.txt", "file a");
    usleep(1000000);
    create_file(CS1_TGZ"/SubA20140102.txt", "file b");
    usleep(1000000);
    create_file(CS1_TGZ"/SubC20140101.txt", "file c");
    usleep(5000);

    char* oldestFile = command.FindOldestFile(CS1_TGZ, "SubA");

    STRCMP_EQUAL("SubA20140101.txt", oldestFile);

    if (oldestFile != 0){
        free(oldestFile);
        oldestFile = 0;
    }
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*
* GROUP : GetLogTestGroup
*
* NAME : FindOldestFile_returnsTheCorrectFilename 
*
*-----------------------------------------------------------------------------*/
TEST(GetLogTestGroup, FindOldestFile_returnsTheCorrectFilename) {
    GetLogCommand command;

    create_file(CS1_TGZ"/a.txt", "file a");
    usleep(1000000);
    create_file(CS1_TGZ"/b.txt", "file b");
    usleep(1000000);
    create_file(CS1_TGZ"/c.txt", "file c");
    usleep(5000);

    char* oldestFile = command.FindOldestFile(CS1_TGZ, NULL);

    STRCMP_EQUAL("a.txt", oldestFile);

    if (oldestFile != 0){
        free(oldestFile);
        oldestFile = 0;
    }
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*
* NAME : GetLogTestGroup  :: GetFileLastModifTimeT_returnsCorrectTimeT
* 
*-----------------------------------------------------------------------------*/
TEST(GetLogTestGroup, GetFileLastModifTimeT_returnsCorrectTimeT)
{
    const char* path = "aFile.txt";
    FILE* file = fopen(path, "w+");

    if (file != NULL){
        fclose(file);
        time_t fileCreationDate = GetLogCommand::GetFileLastModifTimeT(path);
        struct tm* timeinfo = localtime(&fileCreationDate);

        time_t currentTime = time(NULL);
        struct tm* current_tm = localtime(&currentTime);

        CHECK_EQUAL(current_tm->tm_year, timeinfo->tm_year);
        CHECK_EQUAL(current_tm->tm_mon, timeinfo->tm_mon);
        CHECK_EQUAL(current_tm->tm_mday, timeinfo->tm_mday);
        CHECK_EQUAL(current_tm->tm_hour, timeinfo->tm_hour);
        CHECK_EQUAL(current_tm->tm_min, timeinfo->tm_min);

        remove(path);
    } else {
        FAIL("Couldn't open the file");
    }
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * GROUP : GetLogTestGroup
 *
 * NAME : BuildPath_returnsCorrectPath
 * 
 *-----------------------------------------------------------------------------*/
TEST(GetLogTestGroup, BuildPath_returnsCorrectPath) {
    const char* dir = "/my/directory";
    const char* file = "the_file.file";
    const char* expected = "/my/directory/the_file.file";

    char buffer[CS1_PATH_MAX];

    STRCMP_EQUAL(expected, SpaceString::BuildPath(buffer, dir, file));
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * GROUP : GetLogTestGroup
 *
 * NAME : prefixMatches_returnsTrue
 * 
 *-----------------------------------------------------------------------------*/
TEST(GetLogTestGroup, prefixMatches_returnsTrue) {
    const char* haystack = "StarMoonStorm";
    const char* needle1 = "";
    const char* needle2 = "Star";
    const char* needle3 = "NotThere";
    
    CHECK(GetLogCommand::prefixMatches(haystack, needle1));
    CHECK(GetLogCommand::prefixMatches(haystack, needle2));
    CHECK(GetLogCommand::prefixMatches(haystack, needle3) == 0);
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * GROUP : GetLogTestGroup
 *
 * NAME : GetCmdStr_returnsCorrectCmd 
 * 
 *-----------------------------------------------------------------------------*/
TEST(GetLogTestGroup, GetCmdStr_returnsCorrectCmd) {
    char expected[GETLOG_CMD_SIZE] = {0};
    expected[0] = GETLOG_CMD;
    expected[1] = OPT_SUB | OPT_SIZE | OPT_DATE;
    expected[2] = UPDATER; 
    SpaceString::get4Char(expected + 3, 666);
    SpaceString::get4Char(expected + 7, 666);
    
    ICommand *cmd = new GetLogCommand(OPT_SUB | OPT_SIZE | OPT_DATE, UPDATER, 666, 666);
    cmd->GetCmdStr(command_buf);

    #ifdef CS1_DEBUG
        fprintf(stderr, "[INFO] command_buf : %x %x %x %zd %zd\n", command_buf[0], 
                                                           command_buf[1], 
                                                           command_buf[2],
                                                           SpaceString::getUInt(&command_buf[3]),
                                                           SpaceString::getUInt(&command_buf[7]));
    #endif

    CHECK_EQUAL(memcmp(expected, command_buf, GETLOG_CMD_SIZE), 0);

    if (cmd) {
        delete cmd;
        cmd = 0;
    }
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * GROUP : GetLogTestGroup
 *
 * NAME : HasNextFile_returnsPointerToNextData 
 * 
 *-----------------------------------------------------------------------------*/
TEST(GetLogTestGroup, HasNextFile_returnsPointerToNextData) {
    const char result[] = { EOF, EOF, 'I',
                            EOF, EOF, 'B' };

    const char* next_data = GetLogCommand::HasNextFile(result);
    if (next_data) {
        CHECK_EQUAL('I', *next_data);
    } else {
        FAIL("null pointer");
    }

    next_data = GetLogCommand::HasNextFile(next_data + 1);

    if (next_data) {
        CHECK_EQUAL('B', *next_data);
    } else {
        FAIL("null pointer");
    }
}
