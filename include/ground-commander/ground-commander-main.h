/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*
* AUTHORS : Space Concordia 2014, Shawn, Robert, Mathias, Joseph 
*
* TITLE : ground-commander-main.h 
*
* DESCRIPTION : Header file for ground-commander-main.cpp
*
*----------------------------------------------------------------------------*/

#include <cstdio>
#include <cstdlib>
#include <time.h>
#include <cstring>
#include <signal.h>
#include <sstream>
#include <unistd.h>
#include <inttypes.h>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include "space-commander/Net2Com.h"
#include "common/command-factory.h"
#include "shakespeare.h"
#include "common/subsystems.h"
#include "SpaceDecl.h"

const char MAGIC_BYTE = 0;

// TODO these should go in a header file shared by ground and satellite commanders
const string LAST_COMMAND_FILENAME("last-command");
const int COMMAND_RESEND_INDEX = 0;
const char COMMAND_RESEND_CHAR = '!';
const int MAX_COMMAND_SIZE     = CS1_MAX_FRAME_SIZE-CS1_MAX_CMD_RESULT_HEADER_SIZE; // TODO define the this limit more clearly
const int MAX_BUFFER_SIZE      = MAX_COMMAND_SIZE;  

const char ERROR_CREATING_COMMAND  = '1';
const char ERROR_EXECUTING_COMMAND = '2';

// Declarations
static char info_buffer[MAX_BUFFER_SIZE] = {'\0'};
static char cmd_buffer[MAX_COMMAND_SIZE] = {'\0'};
static Net2Com* commander = 0; 
static string stored_command;
InfoBytes* ParseResultData(char* result_buffer);
int process_results(int bytes);
int read_results();
int read_command();
int delete_command();
int create_pipes();

const char* GC_LOGNAME = cs1_systems[CS1_GND_COMMANDER]; 
char gc_log_buffer[CS1_MAX_LOG_ENTRY] = {0};

// TODO What is this file for?
const char CMD_TEMP_FILE[] = "/home/groundCommanderTemp"; //###MAKE SURE TO HAVE WRITE PERMISSIONS###

#define COMMAND_INPUT_PIPE "/home/pipes/cmd-input"
#define MOCK_PIPE "/home/pipes/mock-pipe"

static NamedPipe cmd_input(COMMAND_INPUT_PIPE);
// The file where commands are added to be sent to the satellite
const char CMD_INPUT_PIPE[] = COMMAND_INPUT_PIPE;

