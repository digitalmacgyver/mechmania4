/* EmptyTeam.h
 * Header for CEmptyTeam
 * Example of how to write teams
 * For use with MechMania IV
 * 10/2/98 by Misha Voloshin
 */

#ifndef _EMPTY_TEAM_H_ESDJHSDLFJHSDKLFHSDLKH
#define _EMPTY_TEAM_H_ESDJHSDLFJHSDKLFHSDLKH

#include "Team.h"

class CEmptyTeam : public CTeam {
 public:
  CEmptyTeam();
  ~CEmptyTeam();

  void Init();
  void Turn();
};

#endif  // _EMPTY_TEAM_H_ESDJHSDLFJHSDKLFHSDLKH
