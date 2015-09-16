/*******************************************************************************
*
* AUTHORS : Space Concordia 2014, Joseph 
*
* TITLE : Net2Com-test.cpp
*
*******************************************************************************/
#include <cstring>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include "CppUTest/TestHarness.h"

#include "SpaceDecl.h"
#include "fileIO.h"
#include "space-commander/Net2Com.h"


//************************************************************
//************************************************************
//              Net2ComTestGroup
//************************************************************
//************************************************************
TEST_GROUP(Net2ComTestGroup)
{
    Net2Com* netman;
    Net2Com* commander;
    static const int BUFFER_SIZE = 50;
    static const int NULL_CHAR_LENGTH = 1;

    void setup()
    {
        mkdir(CS1_PIPES, S_IRWXU);
	mkdir(GND_PIPES, S_IRWXU);
        netman = new Net2Com(Dnet_w_com_r, Dcom_w_net_r,  Inet_w_com_r, Icom_w_net_r);
        commander = new Net2Com(Dcom_w_net_r, Dnet_w_com_r, Icom_w_net_r, Inet_w_com_r);
    }

    void teardown()
    {
        if (netman != NULL) {
            delete netman;
            netman = NULL;
        }

        if (commander != NULL) {
            delete commander;
            commander = NULL;
        }

        DeleteDirectoryContent(CS1_PIPES);
        rmdir(CS1_PIPES);
    }
};

TEST(Net2ComTestGroup, WriteToDataPipe_ReturnsCorrectNumberOfBytesWritten)
{
    char buffer[BUFFER_SIZE];
    const char* data = "SomeUsefullData";
    size_t result;

    result = netman->WriteToDataPipe(data);
    commander->ReadFromDataPipe(buffer, BUFFER_SIZE);

    CHECK_EQUAL(strlen(data) + NULL_CHAR_LENGTH, result);
    STRCMP_EQUAL(data, buffer);
}

TEST(Net2ComTestGroup, ReadFromDataPipe_ReturnsCorrectString)
{
    char buffer[BUFFER_SIZE];
    const char* data = "myCommand";
    size_t result;

    result = commander->WriteToDataPipe(data);
    netman->ReadFromDataPipe(buffer, BUFFER_SIZE);

    CHECK_EQUAL(strlen(data) + NULL_CHAR_LENGTH, result);
    STRCMP_EQUAL(data, buffer);
}

TEST(Net2ComTestGroup, WriteToInfoPipe_ReturnsCorrectNumberOfBytesWritten)
{
    char buffer[BUFFER_SIZE];
    const unsigned char byte = 252;
    int result;

    result = netman->WriteToInfoPipe(&byte, 1);
    CHECK_EQUAL(1, result);
    commander->ReadFromInfoPipe(buffer, BUFFER_SIZE);
    CHECK_EQUAL(byte, (unsigned char)buffer[0]);
}

TEST(Net2ComTestGroup, ReadFromInfoPipe_ReturnsCorrectByte)
{
    char buffer[BUFFER_SIZE];
    const unsigned char byte = 252;
    int result = 0;

    result = commander->WriteToInfoPipe(&byte, 1);
    CHECK_EQUAL(1, result);

    netman->ReadFromInfoPipe(buffer, BUFFER_SIZE);
    CHECK_EQUAL(byte, (unsigned char)buffer[0]);
    
}

TEST(Net2ComTestGroup, ReadFromInfoPipe_persist_open_ReturnsCorrectByte)
{
    char buffer[BUFFER_SIZE];
    const unsigned char byte = 252;
    int result = 0;

    result = commander->WriteToInfoPipe(&byte, 1);
    CHECK_EQUAL(1, result);

    int bytes_read = 0;
    bytes_read = netman->ReadFromInfoPipe(buffer, BUFFER_SIZE);

    CHECK_EQUAL(bytes_read, 1);
    CHECK_EQUAL(byte, (unsigned char)buffer[0]);
}

TEST(Net2ComTestGroup, ReadFromInfoPipe_TryOverFlowBuffer_ReadUntilBufferIsFull)
{
    const int BUF_SIZE = 10;
    const int TO_SMALL = 5;
    char buffer[BUF_SIZE] = {'\0'};
    const char* data = "myCommand";
    size_t result;

    result = commander->WriteToDataPipe(data);
    CHECK_EQUAL(strlen(data) + NULL_CHAR_LENGTH, result);

    int bytes_read = netman->ReadFromDataPipe(buffer, TO_SMALL);

    STRCMP_EQUAL("myCom", buffer);
    CHECK_EQUAL(TO_SMALL, bytes_read);

}
