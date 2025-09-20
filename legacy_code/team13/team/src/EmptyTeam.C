/* EmptyTeam.C
 * Implementation for CEmptyTeam
 * Example of how to write teams
 * For use with MechMania IV
 * 10/2/98 by Misha Voloshin
 */

#include "EmptyTeam.h"

// Specify our team as the 
// team to use for the game
CTeam* CTeam::CreateTeam()
{
  return new CEmptyTeam;
}

//////////////////////////////
// Construction/Destruction

CEmptyTeam::CEmptyTeam()
{
}

CEmptyTeam::~CEmptyTeam()
{
}

/////////////////////////////
// Inherited methods

void CEmptyTeam::Init()
{
}

void CEmptyTeam::Turn()
{
}
