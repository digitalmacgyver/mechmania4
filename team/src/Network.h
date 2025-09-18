// 
// CNetwork
//
// base networking class.
//
// connection numbers start at 1

#ifndef __CNetwork__
#define __CNetwork__

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>

const char n_oback[]="ObReady!";  // Observer acknowledge string
const char n_servconack[]="Conn MM4 Serv";  // Connect ack
const char n_obcon[] = "Observer Conned";  // Connect observer
const char n_teamcon[]="Team  Connected";  // Connect team, same length as obcon

class CNetwork
{
private:
    int *fds; //connection file descriptors
    int *timeout;
    int maxconn;

    int maxqlen, *queuelen;
    char **queue, *queuebuf;
    
    int next_conn;
    int max_fd;

    fd_set sockets_fds;

    int fd2conn( int fd );

protected:
    int NewConn( int fd );
    void CloseConn( int conn );

    friend class CServer;

public:
	// CNetwork
	//    themaxconn - maximum number of connections
        //    thequeuelen - max size of queue for each connection
	//
	// Constructs the network object to handle maxconn connections
    CNetwork( int themaxconn, int thequeuelen);
    virtual ~CNetwork();

	// SendPkt
	//   data - characters to send
	//   len - lengh of the data to send
	// 
	// SendPkt send the data to the specified connectios
	// retuns 0 on success
    int SendPkt( int conn, const char *data, int len );
    
        // RecvPkt
        //   data - buffer to write packet into
        //   len - max size of data
        //
        // RecvPkt waits to recive a packet less then or equal in length to
        // len.  RecvPkt sets len to the amout of data it read and returns
        // the connection number from which it read.  If length is less than
        // one a timeout has occured.
    int RecvPkt( char *data, int & len );

    // Misha's crazy queue-related stuff
    int CatchPkt();  // Waits for pkt, adds to appropriate conn queue
    int GetQueueLength (int conn=1);  // Returns amt of data in queue #conn
    char *GetQueue (int conn=1);  // Returns ptr to bottom of queue #conn
    void FlushQueue (int conn=1);  // Empties data out of queue #conn

    void SetTimeout( int conn, int thetimeout );

    int IsOpen( int conn );
};

#endif 
