/* TestTeam.h
 * Scripted test team that reads moves from a file
 * Used for testing specific game scenarios and behaviors
 */

#ifndef _TESTTEAM_H_
#define _TESTTEAM_H_

#include "Team.h"
#include <vector>
#include <string>

// Message operation types for testing the messaging interface
enum MessageOp {
  MSG_OP_SET,      // SetMessage
  MSG_OP_APPEND,   // AppendMessage
  MSG_OP_CLEAR     // ClearMessage
};

// Represents a single scheduled move from the test script
struct TestMove {
  int shipnum;       // Which ship (0-3)
  int turn;          // Which turn to execute (1-based)
  OrderKind order;   // Order type (O_THRUST, O_TURN, etc.)
  double magnitude;  // Order magnitude

  // Message testing fields
  bool is_message;           // True if this is a message command, not an order
  MessageOp msg_op;          // Message operation type
  std::string msg_text;      // Message text to send
  std::string msg_test_name; // Name of the test (for verbose output)
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
  void ExecuteMessageCommand(const TestMove& move);

  std::vector<TestMove> moves;   // All scripted moves
  int current_turn;              // Current turn number (1-based)
};

#endif