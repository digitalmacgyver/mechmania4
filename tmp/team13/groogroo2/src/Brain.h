/* Brain.h
 * Interface for hollow CBrain class
 * Inherit your AI's off of this class
 * by Misha Voloshin 9/13/98
 * For use with MechMania IV
 */

#ifndef _BRAIN_H_DLFKHDSLFKJSDLFJSLDJFLSDJFLSD
#define _BRAIN_H_DLFKHDSLFKJSDLFJSLDJFLSDJFLSD

class CShip;
class CTeam;

class CBrain
{
 public:
  CBrain() { };
  virtual ~CBrain() { };

  virtual void Decide() { };

  CTeam *pTeam;
  CShip *pShip;
};

#endif // _BRAIN_H_DLFKHDSLFKJSDLFJSLDJFLSDJFLSD
