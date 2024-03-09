#include <iostream>
#include <cstring>

#include "rpc_client.h"


using namespace std;



/*
    Constructor
*/
RpcClient::RpcClient
(
    LogManager*     aLogManager,
    SockManager*    aSockManager,
    SocketDomain    aDomain,
    SocketType      aType,
    string          aIp,
    int             aPort
):
SockRpc
(
    aLogManager,
    aSockManager,
    aDomain,
    aType,
    aIp,
    aPort
)
{
    request = ParamList::create();
    answer = ParamList::create();
}



/*
    Destructor
*/
RpcClient::~RpcClient()
{
    if( ownerRequest )
    {
        request -> destroy();
    }
    if( ownerAnswer )
    {
        answer -> destroy();
    }
}



/*
    Create client
*/
RpcClient* RpcClient::create
(
    LogManager*     aLogManager,
    SockManager*    aSockManager,
    SocketDomain    aDomain,
    SocketType      aType,
    string          aIp,
    int             aPort

)
{
    return new RpcClient
    (
        aLogManager,
        aSockManager,
        aDomain,
        aType,
        aIp,
        aPort
    );
}



/*
    Create net client
*/
RpcClient* RpcClient::create
(
    LogManager*     aLogManager,
    SockManager*    aSockManager,
    string          aIp,
    unsigned int    aPort
)
{
    auto result = new RpcClient
    (
        aLogManager,
        aSockManager,
        SD_INET,
        ST_TCP,
        aIp,
        aPort
    );
    return result;
}



/*
    Call method
*/
RpcClient* RpcClient::call()
{
    if( isOk() )
    {
        connect();
        if( isOk() )
        {
            if( request != NULL )
            {
                /* Send buffer to server */
                write( request );
                /* Read answer from server */
                clientRead();
            }
            if( !isOk() )
            {
                disconnect();
            }
        };
    }
    return this;
}



RpcClient* RpcClient::call
(
    string aMethod
)
{
    if( isOk() )
    {
        request -> setString( "method", aMethod );
        call();
        if( isOk() )
        {
            /* Result code processing */
            auto code = getAnswer() -> getString( Path{ "result", "code" });
            if( code != "ok" )
            {
                /* Protocol error */
                setCode( code );
            }
        }
        else
        {
            /* System error */
            getLog()
            -> warning( "RPC call error" )
            -> prm( "method", aMethod )
            -> prm( "code", getCode() );
        }
    }
    return this;
}



RpcClient* RpcClient::call
(
    int aMethod
)
{
    if( isOk() )
    {
        request -> setInt( "method", aMethod );
        call();
        if( isOk() )
        {
            /* Result code processing */
            auto code = getAnswer() -> getString( vector<string>{ "result", "code" });
            if( code != "ok" )
            {
                setCode( code );
            }
        }
        else
        {
            getLog()
            -> warning( "Rpc call error" )
            -> prm( "method", aMethod )
            -> prm( "code", getCode() );
        }
    }
    return this;
}



ParamList* RpcClient::getRequest()
{
    return request;
}



ParamList* RpcClient::getAnswer()
{
    return answer;
}




RpcClient* RpcClient::setRequest
(
    ParamList* aRequest
)
{
    if( ownerRequest )
    {
        ownerRequest = false;
        request -> destroy();
    }
    request = aRequest;
    return this;
}



RpcClient* RpcClient::setAnswer
(
    ParamList* aAnswer
)
{
    if( ownerAnswer )
    {
        ownerAnswer = false;
        answer -> destroy();
    }
    answer = aAnswer;
    return this;
}





/******************************************************************************
    Events
*/


/*
    On call event
    Method may be ovverided
*/
RpcClient* RpcClient::onCallBefore()
{
//    getLog() -> trace( "Client RPC on call before" ) -> lineEnd();
    return this;
}



/*
    Servers On call after event
    Method may be ovverided
*/
RpcClient* RpcClient::onCallAfter()
{
//    getLog() -> trace( "Client RPC on call after" ) -> lineEnd();
    return this;
}



/*
    On before read
    Method may be overrided
*/
bool RpcClient::onReadBefore
(
    string a /* ip income but not use */
)
{
    SockRpc::onReadBefore( a );
    return onCallBefore();
}



/*
    On after read
    Method may be overrided
*/
bool RpcClient::onReadAfter
(
    SockBuffer* aBuffer, /* buffer */
    int
)
{
    SockRpc::onReadAfter( aBuffer, 0 );

    auto header = SockRpcHeader::create( aBuffer );

    if( header.isValid() )
    {
        auto buffer = aBuffer -> getBuffer();
        void* pointer = &buffer[ sizeof( SockRpcHeader ) ];

        /* Create parms from buffer */
        answer -> clear() -> fromBuffer( pointer, header.argumentsSize );
        onCallAfter();
    }
    else
    {
        getLog() -> warning( "RPC server sent invalid header for RPC client" );
    }
    return true;
}
