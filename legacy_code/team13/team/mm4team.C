/* mm4team.C
 * MechMania IV Observer executable
 * 9/15/1998 Misha Voloshin
 */

#include "Client.h"
#include "Parser.h"
 
int main(int argc, char *argv[])
{
  CParser PCmdLn(argc,argv);
  if (PCmdLn.needhelp==1) {
    printf ("mm4team -pport -hhostname\n");
    printf ("  port defaults to 2323\n  hostname defaults to localhost\n");
    printf ("MechMania IV: The Vinyl Frontier   10/2/98\n");
    exit(1);
  }

  CClient myClient(PCmdLn.port,PCmdLn.hostname, FALSE);

  while (1) {
    if (myClient.IsOpen()==0) {
      printf ("Disconnected from MechMania IV server\n");
      printf ("Terminating application\n");
      break;
    }
    
    myClient.ReceiveWorld();
    myClient.DoTurn();
  }

  return 0;
}
