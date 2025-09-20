/* ServerTeam.h
 * Interface for CServerTeam
 * Team object with empty Init and Turn methods
 * For use on the server end
 * MechMania IV 10/2/98
 * by Misha Voloshin
 */

#ifndef _SERVERTEAM_H_EFSKJHEFEFJKWEHFJKLHEFK
#define _SERVERTEAM_H_EFSKJHEFEFJKWEHFJKLHEFK

#include "Team.h"

class CServerTeam : public CTeam
{
 public:
  CServerTeam();   // Empty constructor, default CTeam
  ~CServerTeam();  // Empty destructor

  void Init();     // Empty Init
  void Turn();     // Empty Turn
};

#endif  // _SERVERTEAM_H_EFSKJHEFEFJKWEHFJKLHEFK
