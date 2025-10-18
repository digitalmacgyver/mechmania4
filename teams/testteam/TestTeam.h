/* TestTeam.h
 * Scripted test team that reads moves from a file
 * Used for testing specific game scenarios and behaviors
 */

#ifndef _TESTTEAM_H_
#define _TESTTEAM_H_

#include "Team.h"
#include <vector>
#include <string>

// Represents a single scheduled move from the test script
struct TestMove {
  int shipnum;       // Which ship (0-3)
  int turn;          // Which turn to execute (1-based)
  OrderKind order;   // Order type (O_THRUST, O_TURN, etc.)
  double magnitude;  // Order magnitude
};

class TestTeam : public CTeam {
public:
  TestTeam();
  ~TestTeam();

  void Init();
  void Turn();

private:
  void LoadTestMoves(const char* filename);
  void LoadTestMovesFromStream(std::istream& stream, const char* source_name);
  OrderKind ParseOrderKind(const std::string& order_str);

  std::vector<TestMove> moves;   // All scripted moves
  int current_turn;              // Current turn number (1-based)
};

#endif