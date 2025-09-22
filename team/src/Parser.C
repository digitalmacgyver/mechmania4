/* Parser.C
   Inefficient command-line parser :)
   For use with MechMania IV
   Misha Voloshin, 9/17/98
*/

#include "Parser.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>

////////////////////////
// Construction/Destruction

CParser::CParser(int argc, char **argv)
{
  SetDefaults();
  
  int i;
  for (i=1; i<argc; i++) {
    if (argv[i][0]!='-') {
      needhelp=1;
      continue;
    }

    switch (argv[i][1]) {
      case 'h': memccpy(hostname,argv[i]+2,0,128); break;
      case 'g': memccpy(gfxreg,argv[i]+2,0,128); break;
      case 'G': gfxflag=1; break;
      case 'R': retry=1; reconnect=1; break;
      case 'p': port=atoi(argv[i]+2); break;
      case 'T': numteams=atoi(argv[i]+2); break;
      default: needhelp=1;
    }
  }
}

CParser::~CParser()
{

}

/////////////////////////
// Protected stuff

void CParser::SetDefaults()
{
  memset(hostname, 0, 128);
  memset(gfxreg, 0, 128);

  snprintf(hostname, sizeof(hostname), "localhost");
  snprintf(gfxreg, sizeof(gfxreg), "graphics.reg");
  port=2323;
  numteams=2;
  gfxflag=0;
  needhelp=0;
  retry=0;
  reconnect=0;
}
