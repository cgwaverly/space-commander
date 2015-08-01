#ifndef _NET2COM_H_
#define _NET2COM_H_
#include "space-commander/NamedPipe.h"

#ifdef GROUND_MOCK_SAT
    typedef enum {
        Dnet_w_com_r = 0,
        Dcom_w_net_r = 1,
        Inet_w_com_r = 2,
        Icom_w_net_r = 3,
        GDnet_w_com_r = 4,
        GDcom_w_net_r = 5,
        GInet_w_com_r = 6,
        GIcom_w_net_r = 7
    } pipe_num_t;
#define GND_PIPES "/home/pipes/ground"
#else
    typedef enum {
        Dnet_w_com_r = 0,
        Dcom_w_net_r = 1,
        Inet_w_com_r = 2,
        Icom_w_net_r = 3
    } pipe_num_t;
#endif

class Net2Com{
    private :
        static const int NULL_CHAR_LENGTH = 1;
        static const int NUMBER_OF_PIPES = 4;
	static const int NUMBER_OF_PIPES_MOCK_SAT = 8;
        static const char* pipe_str[];

#ifdef GROUND_MOCK_SAT         
        NamedPipe* pipe[NUMBER_OF_PIPES_MOCK_SAT];
#else
	NamedPipe* pipe[NUMBER_OF_PIPES];
#endif

        NamedPipe* dataPipe_w;
        NamedPipe* dataPipe_r;
        NamedPipe* infoPipe_w;
        NamedPipe* infoPipe_r;
    
    public :
        // ORDER : DATA_WRITE, DATA_READ, INFO_WRITE, INFO_READ
        Net2Com(pipe_num_t dataw, pipe_num_t datar, pipe_num_t infow, pipe_num_t infor);
        static Net2Com* create_netman();
        static Net2Com* create_commander();
        
        ~Net2Com();
        int WriteToDataPipe(const char* str);
        int WriteToDataPipe(unsigned char);
        int WriteToDataPipe(const void*, int);
        int ReadFromDataPipe(char* buffer, int buf_size);
        int WriteToInfoPipe(const char* str);
        int WriteToInfoPipe(const void*, int);
        int WriteToInfoPipe(unsigned char);
        int ReadFromInfoPipe(char* buffer, int buf_size);

        void OpenReadPipesPersistently();                                               // If you are using this mode, you have to 
        void OpenWritePipesPersistently();                                              // persistently open BOTH sides, otherwise it blocks.

    private :
        bool Initialize();
        bool CreatePipes();
};
#endif
