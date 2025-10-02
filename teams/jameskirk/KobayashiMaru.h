/* KobayashiMaru.h
 * Exploit demonstration class - derived from CShip
 * Shows how teams could have exploited the engine's architecture
 * in the original MechMania IV framework (circa 1998)
 *
 * PURPOSE: Demonstrates the laser power exploit by exposing protected
 * members of CShip through inheritance and C-style casting.
 * This allows direct manipulation of the orders array to bypass
 * engine validation checks.
 *
 * NOTE: This exploit leverages a Time-Of-Check-Time-Of-Use (TOCTOU)
 * vulnerability in the World::LaserModel() function where the engine
 * reads the laser value before validating it, allowing manipulation
 * between these two operations.
 */

#ifndef _KOBAYASHIMARU_H_
#define _KOBAYASHIMARU_H_

#include "Ship.h"

class KobayashiMaru : public CShip {
public:
    // Standard constructor
    KobayashiMaru(CCoord StPos, CTeam* pteam = NULL, unsigned int ShNum = 0);

    // Copy constructor from CShip instance
    KobayashiMaru(const CShip& ship) : CShip(ship) {};

    // The REAL cheat: expose protected array addresses (1998-style exploit)
    // Can manipulate ANY CShip by reinterpreting memory layout!
    static double* GetOrdersArray(CShip* ship) {
        // Exploit: CShip and KobayashiMaru have identical memory layouts
        // Use C-style cast to access protected members (pre-C++98 style)
        return ((KobayashiMaru*)ship)->adOrders;
    }

    static double* GetStatsArray(CShip* ship) {
        // Same exploit for ship statistics
        return ((KobayashiMaru*)ship)->adStatCur;
    }
};

#endif // _KOBAYASHIMARU_H_