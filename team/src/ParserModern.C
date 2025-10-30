/* ParserModern.C
   Backward-compatible wrapper implementation
*/

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ParserModern.h"
#include "GameConstants.h"

CParser::CParser(int argc, char** argv) {
  SetDefaults();

  // Use the modern parser
  parser.Parse(argc, argv);

  // Copy values to maintain backward compatibility
  strncpy(hostname, parser.hostname.c_str(), 127);
  hostname[127] = '\0';

  strncpy(gfxreg, parser.gfxreg.c_str(), 127);
  gfxreg[127] = '\0';

  port = parser.port;
  numteams = parser.numteams;
  gfxflag = parser.gfxflag ? 1 : 0;
  needhelp = parser.needhelp ? 1 : 0;
  retry = parser.retry ? 1 : 0;
  reconnect = parser.reconnect ? 1 : 0;
  verbose = parser.verbose ? 1 : 0;
  enableAudioTestPing = parser.enableAudioTestPing ? 1 : 0;
  startAudioMuted = parser.startAudioMuted ? 1 : 0;

  // Initialize global game constants from the parsed values
  InitializeGameConstants(&parser);
}

CParser::~CParser() {}

void CParser::SetDefaults() {
  memset(hostname, 0, 128);
  memset(gfxreg, 0, 128);

  snprintf(hostname, sizeof(hostname), "localhost");
  snprintf(gfxreg, sizeof(gfxreg), "graphics.reg");
  port = 2323;
  numteams = 2;
  gfxflag = 0;
  needhelp = 0;
  retry = 0;
  reconnect = 0;
  verbose = 0;
  enableAudioTestPing = 0;
  startAudioMuted = 0;
}
