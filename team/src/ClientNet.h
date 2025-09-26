//
// CClientNet
//
// Networking for the client end
//

#ifndef __CClientNet__
#define __CClientNet__

#include "Network.h"

class CClientNet : public CNetwork {
 private:
 public:
  CClientNet(char* hostname, int port, int maxqueuelen = 204800);
  ~CClientNet();
};

#endif
