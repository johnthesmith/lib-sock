#include <thread>
#include <iostream>
#include <cstring>
#include <sstream>

#include "rpc_server.h"
#include "../core/buffer_to_hex.h"



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
        -> warning( "Server error" )
        -> prm( "code", getCode())
        -> prm( "message", getMessage())
        -> dump( getDetails());
    }
    return this;
}



/*
    Down server
*/
RpcServer* RpcServer::down()
{
    getLog() -> trace( "RPC server downing" ) -> lineEnd();
    stopListen();
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
    getLog()
    -> trapOn()
    -> begin( "RPC Server onReadAfter" )
    -> prm( "size", aBuffer -> getBufferSize() )
    -> lineEnd();

    auto result = true;
    auto header = SockRpcHeader::create( aBuffer );
    if( header.isValid() )
    {

        auto buffer = aBuffer -> getBuffer();

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
        onCallAfter( arguments, answer );
        getLog()
        -> dump( arguments, "arguments" )
        -> dump( answer, "result" );

        /* Send answer to client */
        write( answer, aHandle );

        arguments -> destroy();
        answer -> destroy();
    }
    else
    {
        getLog() -> warning( "Header is not valid" ) -> lineEnd();
        result = false;
    }

    getLog()
    -> end()
    -> lineEnd()
    -> trapOff();
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




/*
    Servers On error event
*/
bool RpcServer::onError
(
    Result* aResult
)
{
    getLog()
    -> warning( "Server error" )
    -> prm( "code", aResult -> getCode() )
    -> prm( "message", aResult -> getMessage() )
    -> dump( aResult -> getDetails() )
    ;

    return true;
}

