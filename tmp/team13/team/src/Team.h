/* Team.h
 * Declaration of CTeam
 * Manages and keeps track of ships
 * For use with MechMania IV
 * Misha Voloshin 6/1/98
 */

#ifndef _TEAM_H_DSEJFKWEJFWEHF
#define _TEAM_H_DSEJFKWEJFWEHF

#define PI 3.14159

#include "stdafx.h"
#include "Coord.h"
#include "Sendable.h"

#include "Ship.h"
#include "Station.h"
#include "World.h"
#include "Brain.h"

#ifndef maxTeamNameLen
#define maxTeamNameLen  33
#endif

#ifndef maxTextLen
#define maxTextLen  512
#endif

class CTeam : public CSendable
{
 public:
  CTeam(UINT TNum=0, CWorld *pWrld=NULL);
  BOOL Create(UINT numSh, UINT uCrd);
  ~CTeam();

  UINT GetShipCount() const;
  UINT GetTeamNumber() const;
  CShip* GetShip(UINT n) const;
  CStation* GetStation() const;
  CWorld* GetWorld() const;
  double GetScore() const;
  UINT GetWorldIndex() const;
  char *GetName();

  CShip* SetShip(UINT n, CShip* pSh);  // Returns ptr to old ship
  CStation* SetStation(CStation *pSt); // Returns ptr to old station
  CWorld* SetWorld(CWorld *pworld);
  UINT SetWorldIndex(UINT newInd);   // Returns old index
  UINT SetTeamNumber(UINT newTN);       // Returns old
  char *SetName(char *strname);      // Returns ptr to Name
  void Reset();                // Resets orders and text
  
  CBrain* GetBrain();             // Returns current CBrain object
  CBrain* SetBrain(CBrain* pBr);  // Returns old CBrain object

  virtual void Init() = 0;
  virtual void Turn() = 0;
  static CTeam *CreateTeam( void );

  unsigned GetSerInitSize() const;
  unsigned GetSerialSize() const;
  unsigned SerPackInitData (char *buf, unsigned len) const;  // returns packed len
  unsigned SerUnpackInitData (char *buf, unsigned len); 
  unsigned SerialPack (char *buf, unsigned len) const;
  unsigned SerialUnpack (char *buf, unsigned len);

  char MsgText[maxTextLen];
  double GetWallClock();    // Returns # of realtime seconds team's been thinking

  UINT uImgSet;           // For internal use only, specifies graphic

 protected:
  UINT TeamNum,uWorldIndex;
  UINT numShips;

  CBrain *pBrain;
  CShip **apShips;
  CStation *pStation;
  CWorld *pmyWorld;
  char Name[maxTeamNameLen];
};

#endif // ! _TEAM_H_DSEJFKWEJFWEHF
