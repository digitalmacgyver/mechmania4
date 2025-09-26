/* mm4obs.C
 * MechMania IV Observer executable
 * 9/3/1998 Misha Voloshin
 */

#include <unistd.h>   // for sleep

#include "Client.h"
#include "Observer.h"
#include "ParserModern.h"

// Global parser instance for feature flag access
CParser* g_pParser = nullptr;

int main(int argc, char *argv[])
{
  CParser PCmdLn(argc,argv);
  g_pParser = &PCmdLn;  // Set global parser instance
  if (PCmdLn.needhelp==1) {
    printf ("mm4obs [-R] [-G] [-pport] [-hhostname] [-ggfxreg]\n");
    printf ("  -R:  Attempt reconnect after server disconnect\n");
    printf ("  -G:  Activate full graphics mode\n"); 
    printf ("  port defaults to 2323\n  hostname defaults to localhost\n");
    printf ("  gfxreg defaults to graphics.reg\n");
    printf ("MechMania IV: The Vinyl Frontier   10/2/98\n");
    exit(1);
  }

  printf ("Initializing graphics...\n");
  Observer myObs(PCmdLn.gfxreg, PCmdLn.gfxflag);

  printf ("Graphics initialized\n");
  myObs.setAttractor(0);

  CClient *pmyClient = new CClient(PCmdLn.port,PCmdLn.hostname, true);  
  CClient *pSpareCl;
  // true for observer

  time_t tstamp=0, tnow=tstamp;
  while (1) {
    if (pmyClient->IsOpen()==0
	&& tstamp<(time(&tnow)-5)) {
      if (PCmdLn.retry==0) {
	printf ("Server disconnected, terminating application\n");
	break;
      }

      tstamp=tnow;
      myObs.setAttractor(1);
      pSpareCl = new CClient(PCmdLn.port,PCmdLn.hostname, true);
      if (pSpareCl->IsOpen()) {
	delete pmyClient;
	pmyClient = pSpareCl;
	myObs.setAttractor(0);
      }
      else {
	delete pSpareCl;
      }
    }

    pmyClient->ReceiveWorld();
    myObs.getWorld(pmyClient->GetWorld());
    myObs.plotWorld();
    // Check for keyboard toggles and whatnot
    myObs.getKeystroke();

    pmyClient->SendAck();
  }

  delete pmyClient;
  return 0;
}
