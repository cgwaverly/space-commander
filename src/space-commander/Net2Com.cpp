#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "SpaceDecl.h"
#include "space-commander/Net2Com.h"
#include "space-commander/NamedPipe.h"

// CS1_PIPES is defined in space-lib/include/SpaceDecl.h 
#ifdef GROUND_MOCK_SAT
       const char* Net2Com::pipe_str[] = { CS1_PIPES"/Dnet-w-com-r", 
                                        CS1_PIPES"/Dcom-w-net-r", 
                                        CS1_PIPES"/Inet-w-com-r", 
                                        CS1_PIPES"/Icom-w-net-r",
                                        GND_PIPES"/Dnet-w-com-r", 
                                        GND_PIPES"/Dcom-w-net-r", 
                                        GND_PIPES"/Inet-w-com-r", 
                                        GND_PIPES"/Icom-w-net-r"};
#else 
    const char* Net2Com::pipe_str[] = { CS1_PIPES"/Dnet-w-com-r", 
                                        CS1_PIPES"/Dcom-w-net-r", 
                                        CS1_PIPES"/Inet-w-com-r", 
                                        CS1_PIPES"/Icom-w-net-r"};
#endif

//----------------------------------------------
//  Constructor
//----------------------------------------------
Net2Com::Net2Com(pipe_num_t dataw, pipe_num_t datar, pipe_num_t infow, pipe_num_t infor)
{
    Initialize();
    CreatePipes();

    dataPipe_w = pipe[dataw];
    dataPipe_r = pipe[datar];
    infoPipe_w = pipe[infow];
    infoPipe_r = pipe[infor];

    /* A process can open a FIFO in non-blocking mode.
    * In this case, opening for read only will succeed EVEN if no-one has 
    * opened on the write side yet, opening for write only will FAIL with 
    * ENXIO (no such device or address) unless the other end has already been opened.
    */
    dataPipe_r->Open('r');  // Hence, we open read end of pipes! WARNING : 
                            // Both processes (Netman and Commander) need to have there Net2Com created before starting using the pipes.
    infoPipe_r->Open('r');
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*
* NAME : create_commander
*
*-----------------------------------------------------------------------------*/
Net2Com* Net2Com::create_netman()
{
    return new Net2Com(Dnet_w_com_r, Dcom_w_net_r,  Inet_w_com_r, Icom_w_net_r);
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*
* NAME : create_commander
*
*-----------------------------------------------------------------------------*/
Net2Com* Net2Com::create_commander()
{
    return new Net2Com(Dcom_w_net_r, Dnet_w_com_r, Icom_w_net_r, Inet_w_com_r);
}

//----------------------------------------------
//  Destructor
//----------------------------------------------
Net2Com::~Net2Com()
{
#ifdef GROUND_MOCK_SAT

    for (int i=0; i<NUMBER_OF_PIPES_MOCK_SAT; i++){
	delete pipe[i];
    }
#else
    for (int i=0; i<NUMBER_OF_PIPES; i++){
        delete pipe[i];
    }

#endif
}

//----------------------------------------------
//  Initialize
//----------------------------------------------
bool Net2Com::Initialize()
{

#ifdef GROUND_MOCK_SAT

    for (int i=0; i < NUMBER_OF_PIPES_MOCK_SAT; i++)
	{
	 pipe[i] = new NamedPipe(pipe_str[i]);
	}

#else
    for (int i=0; i<NUMBER_OF_PIPES; i++){
        pipe[i] = new NamedPipe(pipe_str[i]);
    }
    
#endif

  return true;
}
//----------------------------------------------
//  CreatePipes
//----------------------------------------------
bool Net2Com::CreatePipes()
{
#ifdef GROUND_MOCK_SAT
    for(int i=0; i<NUMBER_OF_PIPES_MOCK_SAT; i++)
	{
	  if(pipe[i]->Exist() == false){
		pipe[i]->CreatePipe();
	   }
	}
#else
    for (int i=0; i<NUMBER_OF_PIPES; i++){
        if (pipe[i]->Exist() == false){
            pipe[i]->CreatePipe();
        }
    }

#endif

    return true;
}


//----------------------------------------------
// WriteToDataPipe
//----------------------------------------------
int Net2Com::WriteToDataPipe(const char* str)
{
    int result = dataPipe_w->WriteToPipe(str, strlen(str) + NULL_CHAR_LENGTH);
    return result;
}

int Net2Com::WriteToDataPipe(const void* data, int size)
{
    int result = dataPipe_w->WriteToPipe(data, size);
    return result;
}

int Net2Com::WriteToDataPipe(unsigned char number)
{
    unsigned char byte = number;
    return dataPipe_w->WriteToPipe(&byte, sizeof(unsigned char));
}
//----------------------------------------------
// ReadFromDataPipe
//----------------------------------------------
int Net2Com::ReadFromDataPipe(char* buffer, int buf_size)
{
    return dataPipe_r->ReadFromPipe(buffer, buf_size);
}

//----------------------------------------------
// WriteToInfoPipe
//----------------------------------------------
int Net2Com::WriteToInfoPipe(const char* str)
{
    int result = infoPipe_w->WriteToPipe(str, strlen(str) + NULL_CHAR_LENGTH);
    return result;
}

int Net2Com::WriteToInfoPipe(const void* data, int size)
{
    int result = infoPipe_w->WriteToPipe(data, size);
    return result;
}

int Net2Com::WriteToInfoPipe(unsigned char number){
    unsigned char byte = number;
    return infoPipe_w->WriteToPipe(&byte, sizeof(unsigned char));
}

//----------------------------------------------
//  ReadFromInfoPipe
//----------------------------------------------
int Net2Com::ReadFromInfoPipe(char* buffer, int buf_size)
{
    return infoPipe_r->ReadFromPipe(buffer, buf_size);
}
