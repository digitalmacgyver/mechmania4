//
// CServerNet
//
// Derived server networking flass
//
// connection numbers start at 1

#ifndef __CServerNet__
#define __CServerNet__

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "Network.h"

class CServerNet : public CNetwork {
 private:
  int port;
  int main_socket;
  struct sockaddr_in serv_addr;

 public:
  CServerNet(int themaxconn, int port, int maxqueuelen = 2048);

  int WaitForConn(void);
};

#endif
