/*********************************************************************************
 *
 * AUTHORS : Space Concordia 2014, Joseph 
 *
 * TITLE : icommand.cpp
 *
 *********************************************************************************/


#include "common/icommand.h"

ICommand::ICommand() {
    this->cuid = -1;
    this->log_buffer = new char[CS1_MAX_LOG_ENTRY];
    memset(this->log_buffer, 0, CS1_MAX_LOG_ENTRY);
}

ICommand::ICommand(unsigned short cuid) : ICommand::ICommand() {
    this->cuid = cuid;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 *                             GETTERS / SETTERS
 *
 *--------------------------------------------------------------------------------*/
unsigned short ICommand::getCuid() {
    return this->cuid;
}

void ICommand::setCuid(unsigned short cuid) {
    this->cuid = cuid;
}
