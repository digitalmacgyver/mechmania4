/* ServerTeam.C
 * Implementation of CServerTeam
 * Empty team class
 * MechMania IV 10/2/98
 * by Misha Voloshin
 */

#include "ServerTeam.h"

CTeam* CTeam::CreateTeam() { return new CServerTeam(); }

//////////////////////////////
// Construction/Destruction

CServerTeam::CServerTeam() {}

CServerTeam::~CServerTeam() {}

//////////////////////////////
// Methods

void CServerTeam::Init() {}

void CServerTeam::Turn() {}
