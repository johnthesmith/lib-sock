#include <thread>
#include <iostream>
#include <cstring>
#include <sstream>

#include "rpc_server.h"
#include "../core/buffer_to_hex.h"
#include "../json/param_list_log.h"



using namespace std;



/*
    Constructor
*/
RpcServer::RpcServer
(
    LogManager*     aLogManager,
    SockManager*    aSockManager,
    SocketDomain    aDomain,
    SocketType      aType,
    int             aPort
):
SockRpc
(
    aLogManager,
    aSockManager,
    aDomain,
    aType,
    "127.0.0.1",
    aPort
)
{
}



/*
    Create socket
*/
RpcServer* RpcServer::create
(
    LogManager*     aLogManager,
    SockManager*    aSockManager,
    SocketDomain    aDomain,
    SocketType      aType,
    int             aPort

)
{
    return new RpcServer
    (
        aLogManager,
        aSockManager,
        aDomain,
        aType,
        aPort
    );
}



/*
    Up server
*/
RpcServer* RpcServer::up()
{
    listen();
    if( !isOk() )
    {
        getLog()
        -> warning( "Server up error" )
        -> prm( "code", getCode());
    }
    return this;
}



/*
    Down server
*/
RpcServer* RpcServer::down()
{
    getLog() -> trace( "RPC server downing" ) -> lineEnd();
    disconnect();
    return this;
}





/*
    On before read
    Method may be overrided
*/
bool RpcServer::onReadBefore
(
    string aIp
)
{
//    getLog() -> trace( "RPC Server onReadBefore" ) -> prm( "ip", aIp );
    return onCallBefore( aIp );
}



/*
    On after read
    Method may be overrided
*/
bool RpcServer::onReadAfter
(
    SockBuffer* aBuffer,    /* buffer */
    int aHandle             /* handle of the client socket */
)
{
    auto result = true;
    auto header = SockRpcHeader::create( aBuffer );
    if( header.isValid() )
    {
        getLog() -> begin( "RPC Server onReadAfter" ) -> lineEnd();

        auto buffer = aBuffer -> getBuffer();
        char* pointer = &buffer[ sizeof( SockRpcHeader ) ];

        /* Get method arguments */
        auto argumentsBuffer = (void*) ( &buffer[ sizeof( SockRpcHeader ) ] );

        auto answer = ParamList::create();
        auto arguments = ParamList::create()
        -> fromBuffer
        (
            argumentsBuffer,
            header.argumentsSize
        );

        /* Call onAfter method for server */
        ParamListLog::dump( getLog(), arguments, "arguments" );
        onCallAfter( arguments, answer );
        ParamListLog::dump( getLog(), answer, "answer" );

        /* Send answer to client */
        write( answer, aHandle );

        arguments -> destroy();
        answer -> destroy();

        getLog() -> end() -> lineEnd();
    }
    else
    {
        getLog() -> warning( "Header is not valid" ) -> lineEnd();
        result = false;
    }
    return result;
}



/*
    On call event
    Method may be ovverided
*/
bool RpcServer::onCallBefore
(
    string aIp /* client ip address */
)
{
//    getLog() -> trace( "RPC" ) -> prm( "ip", aIp );
    return true;
}



/*
    Servers On call after event
    Method may be ovverided
*/
RpcServer* RpcServer::onCallAfter
(
    ParamList* aArguments,
    ParamList* aResults

)
{
//    getLog() -> trace( "onCallAfter" );
    aResults -> setString( "Answer", "Hello world!" );
    return this;
}

