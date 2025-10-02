/* KobayashiMaru.C
 * Implementation of the KobayashiMaru exploit class
 * Demonstrates memory layout exploitation in MechMania IV
 */

#include "KobayashiMaru.h"

// Constructor implementation
KobayashiMaru::KobayashiMaru(CCoord StPos, CTeam* pteam, unsigned int ShNum)
    : CShip(StPos, pteam, ShNum) {
    // Standard ship initialization handled by parent
}

// The exploit implementations are in the header as inline functions
// This file exists primarily to satisfy linker requirements