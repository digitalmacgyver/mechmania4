/* NoOp.h
 * Minimal do-nothing team for testing
 * All ships remain idle, no commands issued
 */

#ifndef _NOOP_H_
#define _NOOP_H_

#include "Team.h"

class NoOp : public CTeam {
public:
  NoOp();
  ~NoOp();

  void Init();
  void Turn();

private:
  // No state needed - this team does nothing
};

#endif