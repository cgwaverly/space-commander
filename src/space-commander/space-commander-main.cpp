#include <cstdio>
#include <cstdlib>
#include <time.h>
#include <cstring>
#include <signal.h>
#include <sstream>
#include <unistd.h>
#include <inttypes.h>

#include "space-commander/Net2Com.h"
#include "common/command-factory.h"
#include "shakespeare.h"
#include "common/subsystems.h"
#include "SpaceDecl.h"

// TODO these should go in a header file shared by ground and satellite commanders
const string LAST_COMMAND_FILENAME("last-command");
const int COMMAND_RESEND_INDEX = 0;
const char COMMAND_RESEND_CHAR = '!';
const int MAX_COMMAND_SIZE     = 255;
const int MAX_BUFFER_SIZE      = 255;

const char ERROR_CREATING_COMMAND  = '1';
const char ERROR_EXECUTING_COMMAND = '2';

static char log_buffer[255] = {0};
static char info_buffer[255] = {'\0'};
static Net2Com* commander = 0; 

const char* LOGNAME = cs1_systems[CS1_COMMANDER];

// Declarations
static void out_of_memory_handler();
static int execute_commands(int bytes);

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * NAME : main 
 *
 * DESCRIPTION : space-commander main 
 *
 *-----------------------------------------------------------------------------*/
int main() 
{
    set_new_handler(&out_of_memory_handler);

    commander = new Net2Com(Dcom_w_net_r, Dnet_w_com_r, 
                                                    Icom_w_net_r, Inet_w_com_r);

    if (!commander) {
        memset(log_buffer,0,MAX_BUFFER_SIZE);
        snprintf(log_buffer, MAX_BUFFER_SIZE, "[ERROR] %s:%s:%d Failed in Net2Com instantiation\n", 
                                                __FILE__, __func__, __LINE__);
        return EXIT_FAILURE; /* PCD will take care of 
                              * restarting space-commander
                              */
    }

    Shakespeare::log(Shakespeare::NOTICE, LOGNAME, 
                                            "Waiting commands from ground...");

    while (true) 
    {
        memset(info_buffer, 0, sizeof(char) * 255);
        int bytes = commander->ReadFromInfoPipe(info_buffer, 255);

        if (bytes > 0) {
            execute_commands(bytes);
        }

        sleep(COMMANER_SLEEP_TIME);
    }

    if (commander) {
        delete commander;
        commander = 0;
    }

    return 0;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * NAME : execute_commands
 *
 * DESCRIPTION : reads from the pipes and execute the command.
 *
 *-----------------------------------------------------------------------------*/
int execute_commands(int bytes)
{
#ifdef CS1_DEBUG
    char debug_buffer[255] = {0};
#endif
    char* buffer = NULL;    // TODO  This buffer scared me ! 
    int read_total = 0;
    ICommand* command  = NULL;
    char previous_command_buffer[MAX_COMMAND_SIZE] = {'\0'};
    unsigned char read = 0;

    for(int i = 0; i != bytes; i++) {
        read = (unsigned char)info_buffer[i];

#ifdef CS1_DEBUG
        std::ostringstream msg;
        msg << "Read from info pipe = " << (unsigned int)read << " bytes";
        Shakespeare::log(Shakespeare::NOTICE, LOGNAME, msg.str());
#endif

        switch (read) 
        {
            case 252: 
                break;
            case 253:
            case 254:
            case 255: 
            {
                int data_bytes = 0;
                while (data_bytes == 0) {
                    data_bytes = commander->ReadFromDataPipe(buffer, read_total);

                    if (data_bytes > 0) {
#ifdef CS1_DEBUG
                          std::ostringstream msg;
                          msg << "Read " << data_bytes << " bytes from ground station: ";
                          memset (debug_buffer,0,255);
                          for(uint8_t z = 0; z < data_bytes; ++z){
                              uint8_t c = buffer[z];
                              snprintf(debug_buffer,5, "0x%02X ", c);
                              msg << debug_buffer;
                          }
                          Shakespeare::log(Shakespeare::NOTICE, LOGNAME, msg.str());
#endif

                        if (data_bytes != read_total) {
                            Shakespeare::log(Shakespeare::ERROR, LOGNAME, "Something went wrong !!");
                            read_total = 0;
                            break;
                        }

                        FILE *fp_last_command = NULL;
                        unsigned int retry = 10000;

                        if (buffer[COMMAND_RESEND_INDEX] == COMMAND_RESEND_CHAR) 
                        {
                            while(retry > 0 && fp_last_command == NULL){
                                fp_last_command = fopen(LAST_COMMAND_FILENAME.c_str(), "r");
                                retry -=1;
                            }

                            if (fp_last_command != NULL) 
                            {
                                fread(previous_command_buffer, sizeof(char), MAX_COMMAND_SIZE, fp_last_command);
                                fclose(fp_last_command);

                                command = CommandFactory::CreateCommand(previous_command_buffer);

                                if (command != NULL) 
                                {
                                    Shakespeare::log(Shakespeare::NOTICE, 
                                                                    LOGNAME, 
                                                                            "Executing command");

                                    size_t size = 0;
                                    char* result  = (char* )command->Execute(&size);
                                    if (result != NULL) 
                                    {
                                        memset(log_buffer,0,MAX_BUFFER_SIZE);
                                        snprintf(log_buffer, MAX_BUFFER_SIZE, "Command output = %s\n", result);
                                        Shakespeare::log(Shakespeare::NOTICE,LOGNAME,log_buffer);

                                        if (size > 0) {
                                            commander->WriteToDataPipe(result, size);
                                        } else { // this branch will be remove as soon as ALL commands returns the SIZE ! TODO
                                                 // Because it is not safe to rely on a NULL terminator in the result buffer.
                                            commander->WriteToDataPipe(result);
                                        }

                                        free(result); // TODO allocate result buffer with new in all icommand subclasses and use delete
                                        result = NULL;
                                    } else {
                                        // ERROR_EXECUTING_COMMAND defined at top of this file
                                        commander->WriteToInfoPipe(ERROR_EXECUTING_COMMAND);
                                    }

                                    if (command) {
                                        delete command;
                                        command = NULL;
                                    }
                                } else {
                                    // ERROR_CREATING_COMMAND defined at top of this file
                                    commander->WriteToInfoPipe(ERROR_CREATING_COMMAND);
                                }

                                memset(previous_command_buffer, '\0', MAX_COMMAND_SIZE);
                            }
                        } else {

                            while (retry > 0 && fp_last_command == NULL) {
                                fp_last_command = fopen(LAST_COMMAND_FILENAME.c_str(), "w");
                                retry -= 1;
                            }

                            if (fp_last_command != NULL) {
                                fwrite(buffer, sizeof(char), data_bytes, fp_last_command);
                                fclose(fp_last_command);
                            }
                        }

                        free(buffer);
                        buffer = NULL;
                    }

                    sleep(COMMANER_SLEEP_TIME);
                } //end while

                read_total = 0;
                break;
            }
            default:
                    read_total += read;
                    buffer = (char* )realloc(buffer, read_total * sizeof(char)); // todo get rid of this realloc! use new instead
                    memset(buffer, 0, sizeof(char) * read_total);
                break;
        } // end switch
    } // end for

    return 0;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * NAME : out_of_memory_handler 
 *
 * DESCRIPTION : This function is called when memory allocation with new fails.
 *
 *-----------------------------------------------------------------------------*/
void out_of_memory_handler()
{
    std::cerr << "[ERROR] new failed\n";
    throw bad_alloc();
}

