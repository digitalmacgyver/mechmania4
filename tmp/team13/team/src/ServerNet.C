// 
// CServerNet
//
// Derived server networking flass
//
// connection numbers start at 1

#include <stdio.h>
#include <string.h>
#include <iostream.h>

#include "ServerNet.h"

CServerNet::CServerNet( int themaxconn, int port, int maxqueuelen ) 
  : CNetwork( themaxconn , maxqueuelen)
{
    int i;

    
    if( (main_socket = socket( AF_INET, SOCK_STREAM, 0 )) < 0 )
    {
        perror( "socket" );
    }

    i=2;
    if ( setsockopt(  main_socket, SOL_SOCKET, SO_REUSEADDR,
                      (char *) &i, sizeof(i)) < 0 )
    {
        perror( "socket" );
    }                            
    
    memset( (char *) &serv_addr, 0, sizeof( serv_addr ));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl( INADDR_ANY );
    serv_addr.sin_port = htons( port );

    if( bind(main_socket, (struct sockaddr *) &serv_addr,
             sizeof( serv_addr ) ) < 0 )
    {
        perror( "bind" );
    }

    if( listen ( main_socket,5) != 0 )
    {
	perror( "listen" );
        close(main_socket);
    }       
}

int CServerNet::WaitForConn( void )
{
    fd_set main_fds;
    fd_set e_fds;
    struct timeval timeout = {600,0}; // default to 10 minute timeout
    int new_fd;
    struct sockaddr_in cli_addr;
    int sin_len = sizeof( struct sockaddr_in ); 

    //debug
    socklen_t debugme = sin_len;
    
    FD_ZERO( &main_fds );
    FD_SET( main_socket, &main_fds );
  
    e_fds = main_fds;
    
    if( select( main_socket+1, &main_fds, NULL, &e_fds, &timeout ) == 1 )
    {
	if( !FD_ISSET( main_socket, &main_fds ) )
	{
	    
	    close( main_socket );
	    return -1;
	}
	new_fd = accept( main_socket, (struct sockaddr *) &cli_addr, 
			 &debugme );
	return NewConn( new_fd );
    }
    else
    {
	return 0;
	
    }
}
 
