/* ParserModern.h
   Backward-compatible wrapper for ArgumentParser
   Drop-in replacement for Parser.h with enhanced functionality
*/

#ifndef _PARSER_MODERN_H_MM4
#define _PARSER_MODERN_H_MM4

#include "ArgumentParser.h"

// CParser wrapper for backward compatibility
// This allows existing code to work without modification
class CParser {
public:
    CParser(int argc, char** argv);
    ~CParser();

    // Original public interface maintained
    char hostname[128], gfxreg[128];
    int port, numteams, gfxflag, needhelp, retry, reconnect;

    // New functionality - access to modern features
    bool UseNewFeature(const std::string& feature) const {
        return parser.UseNewFeature(feature);
    }

    // Direct access to the modern parser if needed
    ArgumentParser& GetModernParser() { return parser; }
    const ArgumentParser& GetModernParser() const { return parser; }

protected:
    void SetDefaults();

private:
    ArgumentParser parser;
};

#endif // _PARSER_MODERN_H_MM4