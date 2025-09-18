#include "Server.h"
#include "Parser.h" 

#include <sys/types.h>
#include <ctime>

int main(int argc, char *argv[])
{
  srand(time(NULL));
  CParser PCmdLn(argc,argv);

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

  return 0;
}
