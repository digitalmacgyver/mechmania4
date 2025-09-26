//
// CClientNet
//
// Networking for the client end
//

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <cstdio>
#include <cstring>

#include "ClientNet.h"

CClientNet::CClientNet(char *hostname, int port, int maxqueuelen)
    : CNetwork(1, maxqueuelen) {
  struct sockaddr_in serv_addr;
  struct hostent *hp;
  int fd;

  memset((char *)&serv_addr, 0, sizeof(serv_addr));

  if (!(hp = gethostbyname(hostname))) {
    //	printf("gethostbyname( \"%s\"): %s\n", hostname, hstrerror(h_errno) );
    printf("Error: gethostbyname failure\n");
  }

  memcpy((char *)&serv_addr.sin_addr, (char *)hp->h_addr, hp->h_length);

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);

  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    // perror( "socket" );
  }

  if (connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    close(fd);
    // perror( "connect" );
  } else
    NewConn(fd);
}

CClientNet::~CClientNet() {
  if (IsOpen(1))
    CloseConn(1);
}
