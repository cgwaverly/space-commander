/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * AUTHORS : Space Concordia 2015
 *
 * TITLE : icommand.h 
 *
 * NOTES :
 *           CMD_HEAD_SIZE => size of the common header for sent commands
 * CMD_HEAD:     |   [0] - CMD_ID
 *               |   [1] - CMD_CUID
 *               |   [2] - CMD_CUID
 *
 *           CMD_RES_HEAD_SIZE  => size of the common header for result buffers
 * CMD_RES_HEAD  |   [0] - CMD_ID
 *               |   [1] - CMD_CUID
 *               |   [2] - CMD_CUID
 *               |   [3] - CMD_STS (for result buffer only only)
 *
 *----------------------------------------------------------------------------*/
#ifndef ICOMMAND_H
#define ICOMMAND_H

/* OLD sizes TO BE REMOVED
#define CMD_HEAD_SIZE 1
#define CMD_RES_HEAD_SIZE 2
#define CMD_ID 0
#define CMD_STS 1
//*/

//*
#define CMD_HEAD_SIZE 3
#define CMD_RES_HEAD_SIZE 4
#define CMD_ID 0
#define CMD_CUID 1
#define CMD_STS 3
//*/

#include <string.h>
#include "SpaceDecl.h"
#include "infobytes.h"

class ICommand {
private: 
    unsigned short cuid;

protected:
    char* log_buffer;

public:
    ICommand();

    ICommand(unsigned short cuid);

    virtual ~ICommand() {
        if (this->log_buffer) {
            delete[] this->log_buffer;
            this->log_buffer = 0;
        }
    };

    virtual void* Execute(size_t* size){ return 0; } 

    // Intended to the GroundCommander
    // The GroundCommander can use the Command's contructor to build a Command and then
    // call GetCmdStr to build the command buffer to be sent to the satellite. The idea is that 
    // the Commands themselve should have the knowledge of how to build the command buffer.
    // Also, they allow to have a queue of ICommand waiting to be sent and to parse the result buffer returned.
    virtual char* GetCmdStr(char *cmd_buf) { return 0;}; // will be set to pure virtual when implemented in all cmds TODO
    virtual InfoBytes* ParseResult(char *result) = 0; // Idem



    unsigned short getCuid();
    void setCuid(unsigned short cuid);

};

#endif





