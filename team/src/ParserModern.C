/* ParserModern.C
   Backward-compatible wrapper implementation
*/

#include "ParserModern.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>

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
}

CParser::~CParser() {
}

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
}