/* Parser.h
   Hack up an expedient class
   For use with MechMania IV
   Misha Voloshin, 9/17/98
*/

#ifndef _PARSER_H_SDLFJHSDLFJHSDLFHDJK
#define _PARSER_H_SDLFJHSDLFJHSDLFHDJK

class CParser 
{
 public:
  CParser(int argc, char **argv);
  ~CParser();

  char hostname[128],gfxreg[128];
  int port,numteams,gfxflag, needhelp, retry, reconnect;

 protected:
  void SetDefaults();
};

#endif // _PARSER_H_SDLFJHSDLFJHSDLFHDJK
