#include <string>
#include <iostream>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <vector>
#include <cstring>

#include "sock.h"
#include "../core/heap.h"
#include "../core/utils.h"
#include "../core/buffer_to_hex.h"
#include "../json/param_list.h"



/*
    Constructor
*/
Sock::Sock
(
    SockManager*    aSockManager,
    SocketDomain    aDomain,
    SocketType      aType,
    string          aIp,
    int             aPort
)
{
    /* Set arguments */
    domain  = aDomain;
    type    = aType;
    ip      = aIp;
    port    = aPort;

    /* Creaate id */
    id = getId();

    /* Set or create handles */
    if( aSockManager == NULL )
    {
        handles = SockManager::create();
        privateSockManager = true;
    }
    else
    {
        handles = aSockManager;
    }
}



/*
    Destructor
*/
Sock::~Sock()
{
    if( privateSockManager )
    {
        handles -> destroy();
    }
    deleteBuffer();
}



/*
    Create socket for server
*/
Sock* Sock::create
(
    SockManager*    aSockManager,
    int             aPort    /* Port */
)
{
    return new Sock
    (
        aSockManager,
        SD_INET,
        ST_TCP,
        "127.0.0.1",
        aPort
    );
}



/*
    Create socket for client
*/
Sock* Sock::create
(
    SockManager*    aSockManager,
    string          aIp,     /* Ip address */
    int             aPort    /* Port */
)
{
    return new Sock
    (
        aSockManager,
        SD_INET,
        ST_TCP,
        aIp,
        aPort
    );
}



/*
    Destroy socket
*/
void Sock::destroy()
{
    delete this;
}



/*
    Open socket handle
    Or return exists handle
*/
Sock* Sock::openHandle()
{
    handle = -1;

    if( isOk())
    {
        handle = handles -> getHandle( id );

        if( handle == -1 )
        {
            handle = socket( domain, type, 0 );
            if( handle > -1 )
            {
                handles -> addHandle( id, handle );
            }
            else
            {
                setCode( "SocketCreateError" );
            }
        }
    }

    return this;
}



/*
    Socket role server
*/
Sock* Sock::listen()
{
    if( isOk() )
    {
        /* Search handle */
        handle = handles -> getHandle( id );
        if( handle == -1 )
        {
            /* Create handle */
            handle = socket( domain, type, 0 );
            if( handle == -1 )
            {
                setCode( "ErrorOpenHandleForListen" );
            }

            if( isOk())
            {
                /* Nonblock socket enabled */
                fcntl( handle, F_SETFL, O_NONBLOCK);

                struct sockaddr_in addr;
                addr.sin_family = domain;
                addr.sin_port = htons( port );
                addr.sin_addr.s_addr = htonl( INADDR_ANY );

                /* Set reuse option for socket */
                const int enabled = 1;
                setsockopt( handle, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));

                /* bind socket */
                if
                (
                    bind
                    (
                        handle,
                        ( struct sockaddr* )&addr,
                        sizeof( addr )
                    ) < 0
                )
                {
                    setResult
                    (
                        "BindSocketError",
                         std::strerror(errno)
                    );
                }
            }

            /* Listen socket */
            if( isOk() )
            {
                onListenBefore( port );
                if( ::listen( handle, queueSize ) < 0 )
                {
                    setCode( "ServerListenError" );
                }
                else
                {
                    handles -> addHandle( id, handle );
                }
            }
        }

        listening = true;
        while( isOk() && listening )
        {
            /* create FD_SET - list of events */
            fd_set readset;             /* Define the structure */
            FD_ZERO( &readset );        /* Clear structure */
            FD_SET( handle, &readset ); /* Add listener handle to structure */

            /* Define max handle */
            int maxHandle = handle;

            /* Add clients handles to structure */
            for( auto connection : connections )
            {
                FD_SET( connection.handle, &readset );
                maxHandle = max( maxHandle, connection.handle );
            }

            /* Define exception timout */
            timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;

            /* Select events for handles */
            auto selectResult = select
            (
                maxHandle + 1, &readset, NULL, NULL, &timeout
            );

            /* Check selected results */
            switch( selectResult )
            {
                case -1:
                    setCode( "ConnectionWaitingError" );
                break;
                case 0:
//                    setCode( "ConnectionTimeout" );
                break;
            }

            /* Check servers handle in structure */
            if( isOk() && FD_ISSET( handle, &readset ))
            {
                /* New connection */
                /* Define address structiure and his size */
                struct sockaddr remoteAddressStruct;
                unsigned int remoteSize = sizeof( remoteAddressStruct );

                /* The socket waiting request */
                int request = accept
                (
                    handle,
                    &remoteAddressStruct,
                    &remoteSize
                );

                if( request > 0 )
                {
                    /* Make request socket unblocked */
                    fcntl( request, F_SETFL, O_NONBLOCK);
                    /* Registrate new client connection */
                    connections.push_back
                    (
                        Сonnections
                        {
                            request,
                            ipToString
                            (
                                (( sockaddr_in* ) &remoteAddressStruct )
                                -> sin_addr.s_addr
                            )
                        }
                    );
                }
            }

            /* Read clients data */
            if( isOk() )
            {
                for( long unsigned int i = 0; i < connections.size(); i++ )
                {
                    auto connection = connections[ i ];
                    if( FD_ISSET( connection.handle, &readset ))
                    {
                        /* Client data read */
                        if
                        (
                            !readInternal( connection.handle, connection.address )
                        )
                        {
                            close( connection.handle );
                            connections.erase( connections.begin() + i );
                        }
                    }
                }
            }
        }
        listening = false;

        handles -> closeHandlesByThread( id );

        onListenAfter( port );
    }

    return this;
}



/*
    Connect socket
*/
Sock* Sock::connect()
{
    if( isOk() )
    {
        /* Search handle for current users parameters ip port etc */
        handle = handles -> getHandle( id );
        if( handle == -1 )
        {
            handle = socket( domain, type, 0 );
            if( handle == -1 )
            {
                setCode( "SocketCreateError" );
            }
            else
            {
                /* Nonblock socket enabled */
                fcntl( handle, F_SETFL, O_NONBLOCK );

                /* On before */
                onConnectBefore();

                /* Create address structure */
                struct sockaddr_in addr;
                addr.sin_family = domain;
                addr.sin_port = htons( port );
                addr.sin_addr.s_addr = Sock::stringToIp( ip );

                /* Connection begin */
                int c = ::connect
                (
                    handle,
                    ( struct sockaddr *)&addr, sizeof( addr )
                );

                if
                (
                    c != -1 || errno == EINPROGRESS
                )
                {
                    /* Connection waiting. black magic */
                    fd_set writeSet;
                    FD_ZERO( &writeSet );
                    FD_SET( handle, &writeSet );

                    /* Connection waiting timeout */
                    timeval timeout{};
                    timeout.tv_sec = 2;

                    /* Connection progress waiting */
                    int selectResult = select
                    (
                        handle + 1,
                        NULL,
                        &writeSet,
                        NULL,
                        &timeout
                    );

                    /* Check connection waiting results */
                    if( selectResult == -1 )
                    {
                        setCode( "ConnectionWaitingError" );
                    }
                    else if( selectResult == 0 )
                    {
                        setCode( "ConnectionTimeout" );
                    }
                }
                else
                {
                    setCode( "ConnectError" );
                }

                /* Conenction events */
                if( isOk() )
                {
                    /* Add handle to handles */
                    handles -> addHandle( id, handle );
                    onConnectSuccess();
                }

                /* On after */
                onConnectAfter();
            }

            if( !isOk() )
            {
                onConnectError();
            }
        }
    }

    return this;
}



/*
    Connect socket
*/
Sock* Sock::disconnect()
{
    handles -> closeHandlesByThread( id );
    return this;
}


/*
    Write buffer to socket
*/
Sock* Sock::write
(
    const void*     aBuffer,
    const size_t    aSize,
    int             aHandle
)
{
    if( isOk() )
    {
        if( !isConnected() )
        {
            setCode( "SocketIsNotConnectedForWrite" );
        }
        else
        {
            auto sended = send
            (
                aHandle == -1 ? handle : aHandle,
                aBuffer,
                aSize,
                MSG_NOSIGNAL    /* Prevent SIGPIPE */
            );

            if( sended < 0 || (size_t) sended != aSize )
            {
                auto error = Result::create( "SocketWriteError" );
                error -> getDetails()
                -> setInt( "size", aSize )
                -> setInt( "sended", sended );
                onWriteError( error );
                error -> destroy();
            }
        }
    }

    return this;
}



/*
    Write string
*/
Sock* Sock::write
(
    string a
)
{
    write( a.c_str(), a.size() );
    return this;
}



/*
    Read buffer
*/
Sock* Sock::clientRead()
{
    /* create FD_SET - list of events */
    fd_set readset;             /* Define the structure */
    FD_ZERO( &readset );        /* Clear structure */
    FD_SET( handle, &readset ); /* Add listener handle to structure */

    /* Define exception timout */
    timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    /* Select events for handles */
    auto selectResult = select
    (
        handle + 1, &readset, NULL, NULL, &timeout
    );

    /* Check selected results */
    switch( selectResult )
    {
        case -1:
            setCode( "ConnectionWaitingError" );
        break;
        case 0:
            setCode( "ConnectionTimeout" );
        break;
    }

    /* Check servers handle in structure */
    if( isOk() && FD_ISSET( handle, &readset ))
    {
        readInternal( handle );
    }

    return this;
}



/*
    Read beffer
*/
bool Sock::readInternal
(
    int aHandle,    /* request handle */
    string aIp      /* remote ip address */
)
{
    bool result = true;

    if( isOk() )
    {
        /* Create error */
        auto error = Result::create();
        /* Create buffer */
        auto buffer = SockBuffer::create();

        if( onReadBefore( aIp ))
        {
            if( !isConnected() )
            {
                error -> setResult( "no_connected" );
            }
            else
            {
                /* Read loop */
                bool read = true;
                long long readMoment = now();

                while( read )
                {
                    auto item = buffer -> add( packetSize );
                    int bytesRead = 0;

                    bytesRead = recv
                    (
                        aHandle,
                        item -> getPointer(),
                        packetSize,
                        0
                    );

                    switch( bytesRead )
                    {
                        case -1:
                        {
                            /* Error */
                            read = errno == EAGAIN || errno == EINTR;

                            if( read )
                            {
                                usleep( PACKET_WAITING_TIMEOUT_MCS );
                                auto waitingTime = now() - readMoment;
                                read = waitingTime < readWaitingTimeoutMcs;
                                if( !read )
                                {
                                    error
                                    -> setCode( "socket_read_waiting_error" )
                                    -> getDetails()
                                    -> setInt( "packetSize", packetSize )
                                    -> setInt( "readWaitingTimeoutMcs", readWaitingTimeoutMcs )
                                    -> setInt( "waitingTimeMcs", waitingTime )
                                    ;
                                }
                            }
                            else
                            {
                                error -> setResult
                                (
                                    "socket_read_error",
                                    std::strerror(errno)
                                );
                            }

                            break;
                         }
                        case 0:
                        {
                            /* Read zero */
                            read = false;
                        }
                        default:
                        {
                            /* Read */
                            item -> setReadSize( bytesRead );
                            readMoment = now();
                            read = onRead( buffer );
                        }
                    }
                }

                if( error -> isOk() )
                {
                    /* Finall call onReadAfter */
                    result = onReadAfter( buffer, aHandle );
                }

                /* Destroy buffer */
            }
        }

        if( !error -> isOk() )
        {
            onReadError( error, buffer );
        }

        buffer -> destroy();
        error -> destroy();
    }

    return result;
}



/*
    Convert string to numeric IP address
*/
unsigned int Sock::stringToIp
(
    string aString
)
{
    int a, b, c, d;
    char arr[4];

    sscanf( aString.c_str(), "%d.%d.%d.%d", &a, &b, &c, &d );

    arr[0] = a;
    arr[1] = b;
    arr[2] = c;
    arr[3] = d;

    return *( unsigned int *) arr;
}



string Sock::ipToString
(
    unsigned int a
)
{
    auto ip = ( unsigned char* )&a;
    return
    to_string( ip[ 0 ] ) + "." +
    to_string( ip[ 1 ] ) + "." +
    to_string( ip[ 2 ] ) + "." +
    to_string( ip[ 3 ] );
}



Sock* Sock::deleteBuffer()
{
    if( resultBuffer != NULL )
    {
        delete [] resultBuffer;
        resultBuffer = NULL;
        resultBufferSize = 0;
    }
    return this;
}



/******************************************************************************
    Setters and getters
*/



/*
    Set packet size
*/
Sock* Sock::setPacketSize
(
    unsigned int a
)
{
    packetSize = a;
    return this;
}



/*
    Return packet size
*/
unsigned int Sock::getPacketSize()
{
    return packetSize;
}



/*
    Set read timeout
*/
Sock* Sock::setReadWaitingTimeoutMcs
(
    unsigned long long a /* Value */
)
{
    readWaitingTimeoutMcs = a;
    return this;
}



/*
    Return read timeout
*/
unsigned long long Sock::getReadWaitingTimeoutMcs()
{
    return readWaitingTimeoutMcs;
}



/*
    Set connected false and stop server lisener
*/
Sock* Sock::stopListen()
{
    listening = false;
    return this;
}



/*
    Return IP address
*/
string Sock::getIp()
{
    return ip;
}



/*
    Return port
*/
int Sock::getPort()
{
    return port;
}



bool Sock::isConnected()
{
    return handles -> getHandle( id ) != -1;
}


/******************************************************************************
    Events
*/


/*
    On before read
    Method may be overrided
*/
Sock* Sock::onConnectBefore()
{
    /* Empty */
    return this;
}



/*
    On error up
    Method may be overrided
*/
Sock* Sock::onConnectError()
{
    /* Empty */
    return this;
}



/*
    On success up
    Method may be overrided
*/
Sock* Sock::onConnectSuccess()
{
    /* Empty */
    return this;
}



/*
    On after up
    Method may be overrided
*/
Sock* Sock::onConnectAfter()
{
    /* Empty */
    return this;
}



/*
    On before read
    Method may be overrided
*/
bool Sock::onReadBefore
(
    string aIp
)
{
    return true;
}



/*
    On before read
    Method may be overrided
*/
bool Sock::onRead
(
    SockBuffer* /* buffer */
)
{
    return false;
}



/*
    On after read
    Method may be overrided
*/
bool Sock::onReadAfter
(
    SockBuffer*,    /* buffer */
    int             /* Handle for remote connection */
)
{
    /* Empty */
    return true;
}



/*
    On Listen before
    Method may be overrided
*/
Sock* Sock::onListenBefore
(
    unsigned short int aPort
)
{
    return this;
}



/*
    On Listen after
    Method may be overrided
*/
Sock* Sock::onListenAfter
(
    unsigned short int aPort
)
{
    return this;
}



/*
    On read error
*/
bool Sock::onReadError
(
    Result* aResult,
    SockBuffer* aSock
)
{
    resultFrom( aResult );
    return aResult -> isOk();
}



/*
    On write error
*/
bool Sock::onWriteError
(
    Result* aResult
)
{
    resultFrom( aResult );
    return aResult -> isOk();
}



string Sock::getId()
{
    return ip + ":" + to_string( port );
}
