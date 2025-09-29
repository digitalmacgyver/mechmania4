/* Team.h
 * Declaration of CTeam
 * Manages and keeps track of ships
 * For use with MechMania IV
 * Misha Voloshin 6/1/98
 */

#ifndef _TEAM_H_DSEJFKWEJFWEHF
#define _TEAM_H_DSEJFKWEJFWEHF

#include "Brain.h"
#include "Coord.h"
#include "Sendable.h"
#include "Ship.h"
#include "Station.h"
#include "World.h"
#include "stdafx.h"

#ifndef maxTeamNameLen
#define maxTeamNameLen 33
#endif

#ifndef maxTextLen
#define maxTextLen 512
#endif

class CTeam : public CSendable {
 public:
  CTeam(unsigned int TNum = 0, CWorld* pWrld = NULL);
  bool Create(unsigned int numSh, unsigned int uCrd);
  ~CTeam();

  unsigned int GetShipCount() const;
  unsigned int GetTeamNumber() const;
  CShip* GetShip(unsigned int n) const;
  CStation* GetStation() const;
  CWorld* GetWorld() const;
  double GetScore() const;
  unsigned int GetWorldIndex() const;
  char* GetName();

  CShip* SetShip(unsigned int n, CShip* pSh);   // Returns ptr to old ship
  CStation* SetStation(CStation* pSt);  // Returns ptr to old station
  CWorld* SetWorld(CWorld* pworld);
  unsigned int SetWorldIndex(unsigned int newInd);     // Returns old index
  unsigned int SetTeamNumber(unsigned int newTN);      // Returns old
  char* SetName(const char* strname);  // Returns ptr to Name
  void Reset();                        // Resets orders and text

  // Brain management for strategic context switching
  // Teams use these to assign appropriate tactical behaviors to ships
  CBrain* GetBrain();             // Returns current CBrain object
  CBrain* SetBrain(CBrain* pBr);  // Returns old CBrain object

  // Strategic AI methods
  virtual void Init() = 0;  // Team initialization and setup
  virtual void Turn() = 0;  // Strategic decision making and brain assignment
  static CTeam* CreateTeam(void);

  unsigned GetSerInitSize() const;
  unsigned GetSerialSize() const;
  unsigned SerPackInitData(char* buf,
                           unsigned len) const;  // returns packed len
  unsigned SerUnpackInitData(char* buf, unsigned len);
  unsigned SerialPack(char* buf, unsigned len) const;
  unsigned SerialUnpack(char* buf, unsigned len);

  char MsgText[maxTextLen];
  double GetWallClock();  // Returns # of realtime seconds team's been thinking

  unsigned int uImgSet;  // For internal use only, specifies graphic

 protected:
  unsigned int TeamNum, uWorldIndex;
  unsigned int numShips;

  CBrain* pBrain;
  CShip** apShips;
  CStation* pStation;
  CWorld* pmyWorld;
  char Name[maxTeamNameLen];
};

#endif  // ! _TEAM_H_DSEJFKWEJFWEHF
