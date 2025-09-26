/* Brain.h
 * Strategic Context Switching System for AI Behaviors
 *
 * The Brain system implements a strategic context switching pattern where:
 * - Teams analyze strategic situations and select appropriate tactical behaviors
 * - Brains execute focused, goal-oriented AI for specific contexts
 * - Ships can dynamically switch between different behavioral contexts
 *
 * Design Intent:
 * - Teams handle strategic decisions ("What should we be doing?")
 * - Brains handle tactical execution ("How do we do it?")
 * - Enables modular, reusable AI behaviors for different game contexts
 *
 * Example contexts: GetVinylEarlyGame, AttackEnemyShipLateGame,
 *                   GetVinylLowFuelEarlyGame, DefendStation, etc.
 *
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

  // Execute tactical AI for the current strategic context
  // Each brain implements focused behavior for a specific goal/situation
  virtual void Decide() { };

  // References to team and ship for context-aware decision making
  CTeam *pTeam;   // Strategic context and team resources
  CShip *pShip;   // Individual ship state and capabilities
};

#endif // _BRAIN_H_DLFKHDSLFKJSDLFJSLDJFLSDJFLSD
