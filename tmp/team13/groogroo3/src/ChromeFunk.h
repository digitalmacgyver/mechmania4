/* ChromeFunk.h
 * Header for the Chrome Funkadelic
 * Sample team
 */

#ifndef _CHROME_FUNKADELIC_
#define _CHROME_FUNKADELIC_

#include "Team.h"
#include "Brain.h"

//////////////////////////////////////
// Main class: Chrome Funkadelic team

class ChromeFunk : public CTeam
{
 public:
  ChromeFunk();
  ~ChromeFunk();

  void Init();
  void Turn();
};

/////////////////////////////////////
// Ship AI classes

//-----------------------------

class Voyager : public CBrain
// A short-lived class to depart from base
{
 public:
  CBrain *pLastBrain;

  Voyager(CBrain* pLB=NULL);
  ~Voyager();

  void Decide();
};

//-----------------------------

class Stalker : public CBrain
{
 public:
  CThing *pTarget;

  Stalker() { pTarget=NULL; }
  ~Stalker() { }

  void Decide();
};

//----------------------------

class Shooter : public Stalker
// Shooter will exhibit ability to Stalk
{
 public:
  Shooter() { }
  ~Shooter() { }

  void Decide();
};

//-----------------------------

class Gatherer : public Shooter
// Gatherer will exhibit both Stalker 
// and Shooter personalities
{
 public:
  Gatherer();
  ~Gatherer();

  void Decide();

  UINT SelectTarget();
  void AvoidCollide();
};

#endif  // _CHROME_FUNKADELIC_
