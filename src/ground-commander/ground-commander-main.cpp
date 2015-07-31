//TODO:
//- TEST TEST TEST
//- Receive command
//- Check if it's OK or failure
//- If it's OK then life is good
//- If It's not OK then log the failure
//- log status of all commands in a file

#include "ground-commander-main.h"

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * NAME : main 
 *
 * DESCRIPTION : ground-commander main 
 * - the ground-commander main should read Dnet_w_com_r and Inet_w_com_r pipes
 *   for incoming data (result buffers) from the satellite
 * - it should also read the Command Input File (CMD_INPUT_PIPE) for commands
 *   to be sent to the satellite (command buffers)
 *
 *-----------------------------------------------------------------------------*/
int main() 
{
    create_pipes();
#ifdef GROUND_MOCK_SAT
    fprintf(stderr,"Mock configuration\n");
	mkdir(GND_PIPES, S_IRWXU);
#endif

    fprintf(stderr,"Follow log files in /home/logs for output\n");

    // TODO for mock satellite simulation, the ground and satellite commanders need to be
    // reading from a different set of named pipes    
    // TODO this is causing segfault
     commander = new Net2Com(GDcom_w_net_r, GDnet_w_com_r, GIcom_w_net_r, GInet_w_com_r);

    Shakespeare::log(Shakespeare::NOTICE, GC_LOGNAME, "Waiting for commands to send or satellite data");
     	
    while (true)
    {
        int result_bytes = read_results();
	
        if (result_bytes > 0) {
            //get result buffers
            memset(gc_log_buffer,0,CS1_MAX_LOG_ENTRY);
            snprintf(gc_log_buffer,CS1_MAX_LOG_ENTRY,"Got bytes from Ground Netman: %d", result_bytes);
            Shakespeare::log(Shakespeare::NOTICE, GC_LOGNAME, gc_log_buffer );

            perform(result_bytes);
        }
        // if no result buffers, proceed to check for commands to send
        read_command();
        sleep(COMMANER_SLEEP_TIME);
    }        
    
    if (commander) {
        delete commander;
        commander = 0;
    }

    return CS1_SUCCESS;
}

/**
 * create_pipes will ensure that all pipes required for IPC are present
 **/
int create_pipes() 
{
  if(!cmd_input.Exist()) {
        Shakespeare::log(Shakespeare::NOTICE,GC_LOGNAME,"Creating "COMMAND_INPUT_PIPE);
        cmd_input.CreatePipe();
    };
    return CS1_SUCCESS; // TODO when will this ever return CS1_FAILURE?
}

/**
 * Read for responses from ground station (result buffers)
 */
int read_results() 
{
	memset(info_buffer, 0, sizeof(char) * 255);
	
	int bytes = commander->ReadFromInfoPipe(info_buffer, 255);
	return bytes;
}

/**
 * read_command will parse a file containing commands, and write them to the
 * info and data pipes as necessary to deliver them to the netman, and 
 * subsequently to the satellite
 */
int read_command()
{
    memset(cmd_buffer,0,MAX_COMMAND_SIZE);
    //
    // the command input pipe contains command buffers that are ready to be passed
    // through the pipes to the satellite commander
    int input_bytes_read = cmd_input.ReadFromPipe(cmd_buffer, MAX_COMMAND_SIZE);

    if (input_bytes_read > 0) // if we have read a command from the command_input_pipe
    {
        snprintf(gc_log_buffer,CS1_MAX_LOG_ENTRY,"Read from command input file: %s", stored_command.c_str());
        Shakespeare::log(Shakespeare::NOTICE,GC_LOGNAME,gc_log_buffer);
        // TODO: write to normal pipes
        int data_bytes_written = commander->WriteToDataPipe( stored_command.c_str() );
        // TODO implement passing size // int data_bytes_written = commander->WriteToDataPipe(result, size);

        if (data_bytes_written > 0) 
        {
            return data_bytes_written;
            delete_command(); // delete_command is obsolete now, IIRC reading from pipe removes the line automatically
            // TODO perhaps rewrite the data back to the pipe if it failed to be passed on correctly
        }
        else 
        {
            return CS1_FAILURE;
        }
    }

    return input_bytes_read; // TODO so far this is never checked
}

/**
 * delete_command will remove the command from the command input file
 * if writing to the pipes was successful
 */
int delete_command()
{
    string read_command;
    ifstream cmd_input_file(CMD_INPUT_PIPE);
    
    if( !cmd_input_file.is_open()) {
        Shakespeare::log(Shakespeare::ERROR,GC_LOGNAME,"Command input file failed to open");
        return CS1_FAILURE;
    }
    
    // TODO again, what is this for?
    ofstream out(CMD_TEMP_FILE);

    if( !out.is_open()) {
        Shakespeare::log(Shakespeare::ERROR,GC_LOGNAME,"Temp file failed to open. Check directory permmissions");
        return CS1_FAILURE;
    }

    while( getline(cmd_input_file,read_command) ){
        if(read_command.compare(stored_command) != 0){
            out << read_command << "\n";
        }
    }

    cmd_input_file.close();
    out.close();
    remove(CMD_INPUT_PIPE);
    rename(CMD_TEMP_FILE,CMD_INPUT_PIPE);

    // TODO validate number of bytes removed
    return CS1_SUCCESS;
}

/**
 * The perform function will parse incoming bytes from the Dnet_w_com_r pipe 
 * and attempt to detect result buffers. 
 **/
int perform(int bytes)
{
    // TODO there is supposed to be a master log of outstanding command requests and
    // the result of the command execution. Except for getlog, there is a one-to-one
    // relationship between commands and responses, but we don't want the system to 
    // hang waiting for the response. The system should be able to handle any command 
    // responses coming from the satellite in any order
#ifdef GROUND_MOCK_SAT // using the single pipe method, built from Olivier's hack of gnd_main (netman)
	if (bytes) 
	{ // success
    #ifdef CS1_DEBUG
        InfoBytes* result_object;
        ICommand* command = CommandFactory::CreateCommand(info_buffer);
		switch ( (uint8_t)info_buffer[0] )
		{
			case GETLOG_CMD:
				Shakespeare::log(Shakespeare::NOTICE, GC_LOGNAME, "Decoding GETLOG_CMD...");
                
				// TODO: log to proper system (get log), e.g. log to database
                
                result_object = ((GetLogCommand* )command)->ParseResult(info_buffer,"/home/logs/cheese");

                break;
			case GETTIME_CMD:
				Shakespeare::log(Shakespeare::NOTICE, GC_LOGNAME, "Decoding GETTIME_CMD...");
                break;
			default:
				Shakespeare::log(Shakespeare::NOTICE, GC_LOGNAME, "Not sure what we got...");
		}
    #endif

        //InfoBytes * parsed_result = ParseResultData(info_buffer);

        // TODO

        //
        // TODO

	}
#else // This code is intended for the info and data pipe operation

    char* buffer = NULL; // this buffer will be filled in the default case, see below!
    // TODO change this, we prefer the code to read better sequentially
    int read_total = 0;
    unsigned char read = 0;

    for(int i = 0; i != bytes; i++)
    {
        read = (unsigned char)info_buffer[i];
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
                        if (data_bytes != read_total) {
                            Shakespeare::log(Shakespeare::ERROR, GC_LOGNAME, "Something went wrong !!");
                            read_total = 0;
                            break;
                        }

                        string* obtainedSpaceData = (ParseResultData(buffer))->ToString();
                        // TODO this is returning not null 
                        if (obtainedSpaceData != NULL) { // success
                            if (buffer[(uint8_t)MAGIC_BYTE] == GETLOG_CMD) {
                                // TODO: log to proper system (get log)
                            } else {
                                Shakespeare::log(Shakespeare::NOTICE, "GROUND_COMMANDER", obtainedSpaceData->c_str());
                            }

                            delete obtainedSpaceData;
                            obtainedSpaceData = NULL;

                             // TODO - are we deleting a command from the input file because we received its result?
                            // this is assuming the result buffer corresponds to the last executed command. Not necessarily true.
                        }

                        free(buffer);
                        buffer = NULL;
                    }
                    
                    sleep(COMMANER_SLEEP_TIME); //We have to sleep because we got data from the info pipe. Wait for the data pipe to be ready
                    
                }

                read_total = 0;
                break;

            }
            default:
                read_total += read;
                buffer = (char* )realloc(buffer, read_total * sizeof(char));
                memset(buffer, 0, sizeof(char) * read_total);
            break;
        }
    }
#endif
    
    return CS1_SUCCESS; 
}

/** 
 * ParseResultData will take the incoming result buffers, create commands using the factory,
 * and call the command ParseResult function to populate the InfoBytes object for 
 * further processing
 * 
 * TODO verify this method is working correctly. TEST
 **/
InfoBytes* ParseResultData(char* result_buffer)
{
            InfoBytes* result_object;
            ICommand* command = CommandFactory::CreateCommand(result_buffer);
            result_object = (InfoBytes* )command->ParseResult(result_buffer);
            return result_object;
}

