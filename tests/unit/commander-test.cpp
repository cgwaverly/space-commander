/******************************************************************************
*
* AUTHOR : Joseph
*
* FILE : commander-test.cpp
*
* PURPOSE : Test the space-commander using the Net2Com interface.
*
* CREATION DATE : 06-06-2014
*
* LAST MODIFIED : Sat 05 Jul 2014 09:05:27 PM EDT
*
******************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>  
#include <sys/wait.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/MemoryLeakDetectorMallocMacros.h"

#include "SpaceDecl.h"
#include "common/command-factory.h"
#include "common/icommand.h"
#include "fileIO.h"
#include "SpaceString.h"
#include "space-commander/Net2Com.h"
#include "dirUtl.h"
#include "UTestUtls.h"

#define RESULT_BUF_SIZE 50
#define CMD_BUF_SIZE 25
static char command_buf[CMD_BUF_SIZE] = {'\0'};
#define SPACE_COMMANDER_BIN  "bin/space-commander/space-commander" // use local bin, not the one unser CS1_APPS

#define UTEST_SIZE_OF_TEST_FILES 6


TEST_GROUP(CommanderTestGroup)
{
    Net2Com* netman;

    void setup()
    {
        mkdir(CS1_TGZ, S_IRWXU);
        mkdir(CS1_LOGS, S_IRWXU);

        pid_t pid = fork();

        if (pid == 0) {
            if(execl("./"SPACE_COMMANDER_BIN, SPACE_COMMANDER_BIN, NULL) == -1){
                fprintf(stderr, "[ERROR] %s:%s:%d ", __FILE__, __func__, __LINE__);
                exit(EXIT_FAILURE);
            }
        }

        // Make sure the commander is running
        while (system("ps aux | grep bin/space-commander/space-commander 1>/dev/null") != 0){
           usleep(1000); 
        }

        netman = Net2Com::create_netman();
        memset(command_buf, '\0', CMD_BUF_SIZE);
    }
    
    void teardown()
    {
#ifndef PRESERVE
        DeleteDirectoryContent(CS1_TGZ);
        rmdir(CS1_TGZ);

        DeleteDirectoryContent(CS1_LOGS);
        rmdir(CS1_LOGS);
#endif

        if (system("pidof space-commander | xargs  kill -15") != 0) {
            fprintf(stderr, "[ERROR] pidof space-commander | xargs -15 kill");
        }

        DeleteDirectoryContent(CS1_PIPES);

        if (netman) {
            delete netman;
            netman = NULL;
        }
    }
};

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * GROUP : CommanderTestGroup
 *
 * NAME : GetLog_Oldest_Success 
 *
 *-----------------------------------------------------------------------------*/
TEST(CommanderTestGroup, GetLog_Oldest_Success) 
{
    const char* path = CS1_TGZ"/Watch-Puppy20140101.txt";  
    size_t result_size;
    UTestUtls::CreateFile(CS1_TGZ"/Watch-Puppy20140101.txt", "file a");
    usleep(1000000);
    UTestUtls::CreateFile(CS1_TGZ"/Updater20140102.txt", "file b");
    usleep(1000000);
    UTestUtls::CreateFile(CS1_TGZ"/Updater20140103.txt", "file c");
    usleep(5000);

    char* result = 0;
    const char* dest = CS1_TGZ"/Watch-Puppy20140101.txt-copy";


    // This is the Command to create on the ground.
    GetLogCommand ground_cmd(OPT_NOOPT, 0, 0, 0);
    ground_cmd.GetCmdStr(command_buf);

    GetLogCommand *command = (GetLogCommand*)CommandFactory::CreateCommand(command_buf);
    result = (char*)command->Execute(&result_size);

    GetLogInfoBytes* getlog_info = (GetLogInfoBytes*)command->ParseResult(result, dest);

    #ifdef CS1_DEBUG
    std::cerr << "[DEBUG] " << __FILE__ << " indoe is "  << getlog_info->inode << endl;
    #endif

    CHECK_EQUAL(0, getlog_info->next_file_in_result_buffer);
    CHECK(*(result + 2 + GETLOG_INFO_SIZE + UTEST_SIZE_OF_TEST_FILES) == EOF);
    CHECK(*(result + GETLOG_INFO_SIZE + UTEST_SIZE_OF_TEST_FILES + 3) == EOF);
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
 * GROUP : CommanderTestGroup
 *
 * NAME : GetLog_Oldest_Pipe_Success 
 *
 *-----------------------------------------------------------------------------*/
TEST(CommanderTestGroup, GetLog_Oldest_Pipe_Success) 
{
    char result[RESULT_BUF_SIZE] = {0};
 
    UTestUtls::CreateFile(CS1_TGZ"/Watch-Puppy20140101.txt", "file a");
    usleep(1000000);
    UTestUtls::CreateFile(CS1_TGZ"/Updater20140102.txt", "file b");
    usleep(1000000);
    UTestUtls::CreateFile(CS1_TGZ"/Updater20140103.txt", "file c");
    usleep(5000);


    // This is the Command to create on the ground.
    GetLogCommand ground_cmd(OPT_NOOPT, 0, 0, 0);
    ground_cmd.GetCmdStr(command_buf);
    ICommand *command = CommandFactory::CreateCommand(command_buf);

  // use Netman Net2Com to send data to space-commander Net2Com
    netman->WriteToInfoPipe((unsigned char)CMD_BUF_SIZE);
    netman->WriteToDataPipe(command_buf, CMD_BUF_SIZE);
    netman->WriteToInfoPipe((unsigned char)0xFF);
    netman->WriteToInfoPipe((unsigned char)0x01);
    netman->WriteToDataPipe((unsigned char)0x21);
    netman->WriteToInfoPipe((unsigned char)0xFF);


    while (netman->ReadFromDataPipe(result, RESULT_BUF_SIZE) == 0) {
        // Give enough time to the commander to proceed!
        usleep(1000);
    }

    GetLogInfoBytes *getlog_info = (GetLogInfoBytes*)command->ParseResult(result);
    
    CHECK_EQUAL(0, getlog_info->next_file_in_result_buffer);
    CHECK(*(result + 2 + GETLOG_INFO_SIZE + UTEST_SIZE_OF_TEST_FILES) == EOF);
    CHECK(*(result + GETLOG_INFO_SIZE + UTEST_SIZE_OF_TEST_FILES + 3) == EOF);
    char temp[6];
    memcpy(temp,getlog_info->getlog_message,6);

    CHECK(strncmp(temp,"file a", 6) == 0);

    if (command) {
        delete command;
        command = NULL;
    }
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * GROUP : CommanderTestGroup
 *
 * NAME : DeleteLog_Success 
 *
 *-----------------------------------------------------------------------------*/
TEST(CommanderTestGroup, DeleteLog_Success) 
{
    char result[RESULT_BUF_SIZE] = {0};
    // Creates the file
    char inode_str[4] = {'\0'};
    const char* filetest_path = CS1_TGZ"/filetest.tgz";
    FILE* filetest = fopen(filetest_path, "w+");
    
    if (!filetest) {
        string msg("Can't create ");
        msg.append(filetest_path);
        msg.append(", make sure the directory exists on your system.");
        FAIL(msg.c_str());
    }

    fprintf(filetest, "some text to test");
    fclose(filetest);

    ino_t inode = GetLogCommand::GetInoT(filetest_path); 
    SpaceString::get4Char(inode_str, inode);

    // Check that the inode is good.
    ino_t inode_check = SpaceString::getUInt(inode_str);
    CHECK_EQUAL((unsigned int)inode, (unsigned int)inode_check);

    snprintf(command_buf, 3, "7I");  // 7 is the command number, I means inode
    memcpy(command_buf + 2, inode_str, 4);

    #ifdef CS1_DEBUG
        fprintf(stderr, "[DEBUG] %s:%s:%d filetest_path is : %s\n", __FILE__, __func__, __LINE__, filetest_path);
        fprintf(stderr, "[DEBUG] %s:%s:%d inode is : %d\n", __FILE__, __func__, __LINE__, (unsigned int)inode);
    #endif

    // use Netman Net2Com to send data to space-commander Net2Com
    netman->WriteToInfoPipe((unsigned char)CMD_BUF_SIZE);
    netman->WriteToDataPipe(command_buf, CMD_BUF_SIZE);
    netman->WriteToInfoPipe((unsigned char)0xFF);
    netman->WriteToInfoPipe((unsigned char)0x01);
    netman->WriteToDataPipe((unsigned char)0x21);
    netman->WriteToInfoPipe((unsigned char)0xFF);


    while (netman->ReadFromDataPipe(result, RESULT_BUF_SIZE) == 0) {
        // Give enough time to the commander to proceed!
        usleep(1000);
    }

    // Checks that the file has been deleted.
    CHECK_EQUAL(-1, access(filetest_path, F_OK));

    char status[2] = {'\0'};
    strncpy(status, result + 1, 1);
    CHECK_EQUAL(0, atoi(status));
}
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * GROUP : CommanderTestGroup
 *
 * NAME : SetTime_Success 
 *
 *-----------------------------------------------------------------------------*/
TEST(CommanderTestGroup, SetTime_Success) 
{
    char result[SETTIME_RTN_SIZE + CMD_RES_HEAD_SIZE] = {0};
    time_t rawtime;
    
    time(&rawtime);
    command_buf[0] = SETTIME_CMD;
    command_buf[SETTIME_CMD_SIZE - 1] = 0xFF;// turn rtc set-time off
    SpaceString::getTimetInChar(command_buf+1,rawtime);

    // use Netman Net2Com to send data to space-commander Net2Com
    netman->WriteToInfoPipe((unsigned char)SETTIME_CMD_SIZE);
    netman->WriteToDataPipe(command_buf, SETTIME_CMD_SIZE);
    netman->WriteToInfoPipe((unsigned char)0xFF);
    netman->WriteToInfoPipe((unsigned char)0x01);
    netman->WriteToDataPipe((unsigned char)0x21);
    netman->WriteToInfoPipe((unsigned char)0xFF);


    while (netman->ReadFromDataPipe(result, RESULT_BUF_SIZE) == 0) {
        // Give enough time to the commander to proceed!
        usleep(1000);
    }
    SetTimeCommand command(1000);
    InfoBytesSetTime* settime_info = (InfoBytesSetTime*)command.ParseResult(result);

    CHECK(result[0]==SETTIME_CMD);
    if ( getuid() == 0 ) //Some systems might need to be root user to set time succesfully
        CHECK(settime_info->time_status == CS1_SUCCESS); 
    CHECK(settime_info->time_set == rawtime);
}
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * GROUP : CommanderTestGroup
 *
 * NAME : GetTime_Success 
 *
 *-----------------------------------------------------------------------------*/
TEST(CommanderTestGroup, GetTime_Success) 
{
    char result[GETTIME_RTN_SIZE + CMD_RES_HEAD_SIZE] = {0};
    time_t rawtime;
    command_buf[0] = GETTIME_CMD;

    // use Netman Net2Com to send data to space-commander Net2Com
    netman->WriteToInfoPipe((unsigned char)GETTIME_CMD_SIZE);
    netman->WriteToDataPipe(command_buf, GETTIME_CMD_SIZE);
    netman->WriteToInfoPipe((unsigned char)0xFF);
    netman->WriteToInfoPipe((unsigned char)0x01);
    netman->WriteToDataPipe((unsigned char)0x21);
    netman->WriteToInfoPipe((unsigned char)0xFF);

    while (netman->ReadFromDataPipe(result, RESULT_BUF_SIZE) == 0) {
        // Give enough time to the commander to proceed!
        usleep(1000);
    }
    GetTimeCommand command;
    InfoBytesGetTime *gettime_info = (InfoBytesGetTime*)command.ParseResult(result);
    time(&rawtime);

    CHECK(result[0]==GETTIME_CMD);
    if ( getuid() == 0 ) //Some systems might need to be root user to set time succesfully
        CHECK(gettime_info->time_status == CS1_SUCCESS); 
    CHECK(gettime_info->time_set == rawtime);
}
