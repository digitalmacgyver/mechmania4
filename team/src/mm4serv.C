#include "Server.h"
#include "ParserModern.h"
#include "World.h"
#include "Team.h"
#include "Station.h"

#include <sys/types.h>
#include <ctime>

// Global parser instance for feature flag access
CParser* g_pParser = nullptr;

int main(int argc, char *argv[])
{
  srand(time(NULL));
  CParser PCmdLn(argc,argv);
  g_pParser = &PCmdLn;  // Set global parser instance

  if (PCmdLn.needhelp==1) {
    printf ("mm4serv [-pport] [-Tnumteams]\n");
    printf ("  port defaults to 2323\n  numteams defaults to 2\n");
    printf ("MechMania IV: The Vinyl Frontier   10/2/98\n");
    exit(1);
  }

  CServer myServ(PCmdLn.numteams,PCmdLn.port);

  myServ.ConnectClients();   // Sends ack & ID to clients
  myServ.MeetTeams();        // Clients send back team info

  while (myServ.GetTime()<300.0) {
     myServ.Simulation();

    myServ.BroadcastWorld();
    myServ.ReceiveTeamOrders();
  }

  // Log final scores
  printf("\n========================================\n");
  printf("           FINAL SCORES\n");
  printf("========================================\n");
  CWorld* pWorld = myServ.GetWorld();
  if (pWorld) {
    for (UINT i = 0; i < pWorld->GetNumTeams(); i++) {
      CTeam* pTeam = pWorld->GetTeam(i);
      if (pTeam && pTeam->GetStation()) {
        printf("%s: %.2f vinyl\n",
               pTeam->GetName(),
               pTeam->GetStation()->GetVinylStore());
      }
    }
  }
  printf("========================================\n\n");

  return 0;
}
