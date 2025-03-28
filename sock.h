#pragma once

/*
    Socket work
    https://rsdn.org/article/unix/sockets.xml
*/

#include <vector>
#include <sys/types.h>
#include <sys/socket.h>

#include "../core/result.h"

#include "sock_buffer.h"
#include "sock_manager.h"


/* Predeclaration sock for events definitions */
class Sock;



#define PACKET_WAITING_TIMEOUT_MCS 2000
#define READ_WAITING_TIMEOUT_MCS 500000


enum SocketDomain
{
    SD_INET     = AF_INET,  /* TCP/IP V4 */
    SD_UNIX     = AF_UNIX,  /* Local host socket */
    SD_IPX      = AF_IPX,   /* IPX protocol */
    SD_INTE6    = AF_INET6  /* TCP/IP V6 */
};



enum SocketType
{
    ST_TCP      = SOCK_STREAM,
    SD_UDP      = SOCK_DGRAM,
    SD_RAW      = SOCK_RAW
};



/*
    Cliients connections for server
*/
struct Сonnections
{
    int     handle = 0;     /* client connection handle after accept */
    string  address = "";   /* client ip address */
};



/*
    Socket class definition
*/
class Sock : public Result
{
    private:

        vector <Сonnections> connections;                  /* list of handles */

        /*
            State
        */
        bool                privateSockManager  = false;
        int                 handle              = -1;       /* Handle after openHandle method */
        SockManager*        handles             = NULL;     /* handles */
        unsigned int        queueSize           = 50;       /* Resuest queue size */
        unsigned int        packetSize          = 1024;     /* Data packet size */
        char*               resultBuffer        = NULL;
        unsigned int        resultBufferSize    = 0;
        string              remoteAddress       = "";
        string              id                  = "";       /* Socket id for handles */

        bool                listening           = false;

        /*
            Arguments
        */
        SocketDomain        domain                  = SD_INET;
        SocketType          type                    = ST_TCP;
        string              ip                      = "127.0.0.1";
        unsigned long long  readWaitingTimeoutMcs   = READ_WAITING_TIMEOUT_MCS;
        int                 port                    = 42;

        /*
            Create socket handle
        */
        Sock* openHandle();


        /*
            Read beffer
        */
        bool readInternal
        (
            int,        /* request handle */
            string = "" /* remote ip address */
        );


    public:


    /*
        Constructor
    */
    Sock
    (
        SockManager*    = NULL,
        SocketDomain    = SD_INET,
        SocketType      = ST_TCP,
        string          = "127.0.0.1",
        int             = 42
    );



    /*
        Destructor
    */
    ~Sock();



    /*
        Create socket for server
    */
    static Sock* create
    (
        SockManager*,   /* Sock manager */
        int             /* Port */
    );



    /*
        Create socket for client
    */
    Sock* create
    (
        SockManager*,   /* Sock manager */
        string,         /* Ip address */
        int             /* Port */
    );




    /*
        Destroy socket
    */
    void destroy();



    /*
        Socket connected status
        Return true when socket connected
    */
    bool isConnected();



    /*
        Set connected false and stop server lisener
    */
    Sock* stopListen();



    /*
        Socket role server
    */
    Sock* listen();



    /*
        Socket connect to server like client
    */
    Sock* connect();



    /*
        Disconnect
    */
    Sock* disconnect();



    /*
        Write buffer to socket
    */
    Sock* write
    (
        const void*,    /* Buffer */
        const size_t,   /* Size of buffer */
        int = -1        /* Handle for writing */
    );



    /*
        Write string to socket
    */
    Sock* write
    (
        string
    );



    /*
        Read buffer from socket for client
        Result will return to events
            onReadBefore
            onRead
            onReadAfter
    */
    Sock* clientRead();



    /*
        Convert string to numeric IP address
    */
    static unsigned int stringToIp
    (
        string
    );




    static string ipToString
    (
        unsigned int
    );



    /*
        Set packet size
    */
    Sock* setPacketSize
    (
        unsigned int
    );



    /*
        Return packet size
    */
    unsigned int getPacketSize();


    Sock* deleteBuffer();



    /*
        Return remote address
    */
    string getRemoteAddress();



    /*
        Return IP address
    */
    string getIp();



    /*
        Return port
    */
    int getPort();



    /*
        Return id for port and ip
    */
    string getId();



    /*
        Set read timeout
    */
    Sock* setReadWaitingTimeoutMcs
    (
        unsigned long long /* Value */
    );



    /*
        Return read timeout
    */
    unsigned long long getReadWaitingTimeoutMcs();



    /******************************************************************************
        Events
    */


    /*
        On before read
        Method may be overrided
    */
    virtual Sock* onConnectBefore();



    /*
        On error up
        Method may be overrided
    */
    virtual Sock* onConnectError();



    /*
        On success up
        Method may be overrided
    */
    virtual Sock* onConnectSuccess();



    /*
        On after up
        Method may be overrided
    */
    virtual Sock* onConnectAfter();



    /*
        On before read
        Method may be overrided
    */
    virtual bool onReadBefore
    (
        string  /* ip */
    );



    /*
        On before read
        Method may be overrided
    */
    virtual bool onRead
    (
        SockBuffer* /* buffer */
    );



    /*
        On after read
        Method may be overrided
    */
    virtual bool onReadAfter
    (
        SockBuffer*,    /* Buffers */
        int             /* Handle for remote connection */
    );



    /*
        On Lisen before
        Method may be overrided
    */
    virtual Sock* onListenBefore
    (
        unsigned short int  /* Port */
    );



    /*
        On Lisen after
        Method may be overrided
    */
    virtual Sock* onListenAfter
    (
        unsigned short int /* Port */
    );



    /*
        On read error event
    */
    virtual bool onReadError
    (
        Result*,
        SockBuffer*
    );



    /*
        On write error event
    */
    virtual bool onWriteError
    (
        Result*
    );
};
