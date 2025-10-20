/* TestTeam.C
 * Implementation of scripted test team
 * Reads commands from test_moves.txt and executes them on schedule
 */

// Include order matters - need OrderKind enum from Ship.h
#include "TestTeam.h"
#include "Ship.h"
#include "ParserModern.h"
#include <fstream>
#include <sstream>
#include <cstdio>
#include <iostream>
#include <unistd.h>  // for isatty() and STDIN_FILENO
#include <map>
#include <string>

// Global map from order string names to OrderKind enum values
static const std::map<std::string, OrderKind> ORDER_STRING_TO_ENUM = {
    {"O_SHIELD", O_SHIELD},
    {"O_LASER", O_LASER},
    {"O_THRUST", O_THRUST},
    {"O_TURN", O_TURN},
    {"O_JETTISON", O_JETTISON}
};

// Reverse map from OrderKind enum to string name (for logging)
static const std::map<OrderKind, std::string> ORDER_ENUM_TO_STRING = {
    {O_SHIELD, "O_SHIELD"},
    {O_LASER, "O_LASER"},
    {O_THRUST, "O_THRUST"},
    {O_TURN, "O_TURN"},
    {O_JETTISON, "O_JETTISON"}
};

// Factory function - tells the game to use our team class
CTeam* CTeam::CreateTeam() {
  return new TestTeam;
}

TestTeam::TestTeam() : CTeam(), current_turn(0) {
  // Constructor
}

TestTeam::~TestTeam() {
  // Nothing to clean up
}

void TestTeam::Init() {
  // Set team and ship names
  SetName("TestTeam");
  GetStation()->SetName("Test Station");
  GetShip(0)->SetName("Test-1");
  GetShip(1)->SetName("Test-2");
  GetShip(2)->SetName("Test-3");
  GetShip(3)->SetName("Test-4");

  // Initialize ships with default configuration
  // All ships get equal fuel/cargo split
  for (unsigned int i = 0; i < GetShipCount(); ++i) {
    GetShip(i)->SetCapacity(S_FUEL, 30.0);   // 30 tons fuel
    GetShip(i)->SetCapacity(S_CARGO, 30.0);  // 30 tons cargo
  }

  // Check for test file from command line
  extern CParser* g_pParser;
  const char* test_file = nullptr;

  if (g_pParser) {
    const std::string& test_moves_file = g_pParser->GetModernParser().testMovesFile;
    if (!test_moves_file.empty()) {
      test_file = test_moves_file.c_str();
    }
  }

  // Load test moves from specified file, stdin, or default
  if (test_file && std::string(test_file) == "-") {
    // Read from stdin (explicit flag)
    printf("[TestTeam] Reading from stdin (explicit --test-file -)\n");
    LoadTestMovesFromStream(std::cin, "stdin");
  } else if (test_file) {
    // Read from specified file
    printf("[TestTeam] Reading from file: %s\n", test_file);
    LoadTestMoves(test_file);
  } else if (!isatty(STDIN_FILENO)) {
    // No test file specified, but stdin is piped (not a terminal)
    // Automatically read from stdin
    printf("[TestTeam] Auto-detected piped input on stdin\n");
    LoadTestMovesFromStream(std::cin, "stdin (auto-detected)");
  } else {
    // Try default file
    printf("[TestTeam] Trying default file: test_moves.txt\n");
    LoadTestMoves("test_moves.txt");
  }

  printf("[TestTeam] Initialized with %zu scripted moves\n", moves.size());
}

void TestTeam::Turn() {
  current_turn++;

  // Log ship 0's position and orientation at the start of each turn
  if (GetShipCount() > 0) {
    CShip* ship0 = GetShip(0);
    if (ship0 != nullptr) {
      CCoord pos = ship0->GetPos();
      double orient = ship0->GetOrient();
      double orient_deg = orient * 180.0 / 3.14159265359;
      bool docked = ship0->IsDocked();
      CTraj vel = ship0->GetVelocity();

      printf("[SHIP0-STATE] Turn %d: pos=(%.2f, %.2f) orient=%.6f rad (%.2f deg) vel=(%.2f @ %.2f deg) docked=%d\n",
             current_turn, pos.fX, pos.fY, orient, orient_deg, vel.rho, vel.theta * 180.0 / 3.14159265359, docked ? 1 : 0);
    }
  }

  // Execute any moves scheduled for this turn
  for (const TestMove& move : moves) {
    if (move.turn == current_turn) {
      // Handle message commands
      if (move.is_message) {
        ExecuteMessageCommand(move);
        continue;
      }

      // Validate ship number
      if (move.shipnum < 0 || move.shipnum >= (int)GetShipCount()) {
        printf("[TestTeam] Turn %d: Invalid ship number %d (skipping)\n",
               current_turn, move.shipnum);
        continue;
      }

      CShip* ship = GetShip(move.shipnum);

      // Check if ship has been destroyed (null pointer)
      if (ship == nullptr) {
        // Get order name from the reverse map for logging
        auto it = ORDER_ENUM_TO_STRING.find(move.order);
        const char* order_name = (it != ORDER_ENUM_TO_STRING.end()) ?
                                 it->second.c_str() : "UNKNOWN";

        printf("TEST_WARNING: Was scheduled to issue order %s %.2f to ship %d but that ship has been destroyed.\n",
               order_name, move.magnitude, move.shipnum);
        continue;
      }

      // Get order name from the reverse map for logging
      auto it = ORDER_ENUM_TO_STRING.find(move.order);
      const char* order_name = (it != ORDER_ENUM_TO_STRING.end()) ?
                               it->second.c_str() : "UNKNOWN";

      double fuel_cost = ship->SetOrder(move.order, move.magnitude);

      // For turn orders, log detailed information
      if (move.order == O_TURN) {
        double actual_order = ship->GetOrder(O_TURN);
        printf("[TestTeam] Turn %d: Ship %d (%s) executing %s: requested=%.6f -> fuel_cost=%.6f, stored_order=%.6f (%.2f%% of requested)\n",
               current_turn, move.shipnum, ship->GetName(), order_name,
               move.magnitude, fuel_cost, actual_order, (actual_order / move.magnitude) * 100.0);
      } else {
        printf("[TestTeam] Turn %d: Ship %d (%s) executing %s %.2f -> fuel_cost=%.4f\n",
               current_turn, move.shipnum, ship->GetName(), order_name, move.magnitude, fuel_cost);
      }
    }
  }
}

void TestTeam::LoadTestMoves(const char* filename) {
  std::ifstream file(filename);

  if (!file.is_open()) {
    printf("[TestTeam] Warning: Could not open %s (no moves will be executed)\n", filename);
    printf("[TestTeam] Ships will remain idle unless test file is provided\n");
    return;
  }

  LoadTestMovesFromStream(file, filename);
  file.close();
}

void TestTeam::LoadTestMovesFromStream(std::istream& stream, const char* source_name) {
  std::string line;
  int line_num = 0;

  while (std::getline(stream, line)) {
    line_num++;

    // Skip empty lines and comments
    if (line.empty() || line[0] == '#') {
      continue;
    }

    // Parse: shipnum,turn,ORDER_KIND,magnitude
    std::istringstream iss(line);
    std::string token;
    std::vector<std::string> tokens;

    while (std::getline(iss, token, ',')) {
      // Trim whitespace
      size_t start = token.find_first_not_of(" \t\r\n");
      size_t end = token.find_last_not_of(" \t\r\n");
      if (start != std::string::npos) {
        tokens.push_back(token.substr(start, end - start + 1));
      } else {
        tokens.push_back("");
      }
    }

    // Check if this is a message command (format: turn,MSG_OP,test_name,message_text)
    if (tokens.size() >= 3 && (tokens[1] == "MSG_SET" || tokens[1] == "MSG_APPEND" || tokens[1] == "MSG_CLEAR")) {
      try {
        TestMove move;
        move.is_message = true;
        move.turn = std::stoi(tokens[0]);

        if (tokens[1] == "MSG_SET") {
          move.msg_op = MSG_OP_SET;
        } else if (tokens[1] == "MSG_APPEND") {
          move.msg_op = MSG_OP_APPEND;
        } else {
          move.msg_op = MSG_OP_CLEAR;
        }

        move.msg_test_name = (tokens.size() >= 3) ? tokens[2] : "";
        move.msg_text = (tokens.size() >= 4) ? tokens[3] : "";

        moves.push_back(move);
      } catch (const std::exception& e) {
        printf("[TestTeam] Warning: Line %d message parse error: %s, skipping: %s\n",
               line_num, e.what(), line.c_str());
      }
      continue;
    }

    if (tokens.size() != 4) {
      printf("[TestTeam] Warning: Line %d has %zu fields (expected 4), skipping: %s\n",
             line_num, tokens.size(), line.c_str());
      continue;
    }

    try {
      TestMove move;
      move.is_message = false;
      move.shipnum = std::stoi(tokens[0]);
      move.turn = std::stoi(tokens[1]);
      move.order = ParseOrderKind(tokens[2]);
      move.magnitude = std::stod(tokens[3]);

      moves.push_back(move);

    } catch (const std::exception& e) {
      printf("[TestTeam] Warning: Line %d parse error: %s, skipping: %s\n",
             line_num, e.what(), line.c_str());
    }
  }

  printf("[TestTeam] Loaded %zu test moves from %s\n", moves.size(), source_name);
}

OrderKind TestTeam::ParseOrderKind(const std::string& order_str) {
  // Use the global map to convert string to enum
  auto it = ORDER_STRING_TO_ENUM.find(order_str);
  if (it != ORDER_STRING_TO_ENUM.end()) {
    return it->second;
  } else {
    printf("[TestTeam] Warning: Unknown order type '%s', defaulting to O_THRUST\n",
           order_str.c_str());
    return O_THRUST;
  }
}

void TestTeam::ExecuteMessageCommand(const TestMove& move) {
  printf("\n[MSG-TEST] Testing %s\n", move.msg_test_name.c_str());

  size_t len_before = strlen(MsgText);

  if (move.msg_op == MSG_OP_SET) {
    printf("[MSG-TEST] Calling SetMessage(\"%s\")\n", move.msg_text.c_str());
    printf("[MSG-TEST] Buffer before: length=%zu\n", len_before);

    MessageResult result = SetMessage(move.msg_text.c_str());

    size_t len_after = strlen(MsgText);
    printf("[MSG-TEST] Buffer after: length=%zu\n", len_after);
    printf("[MSG-TEST] Result: ");
    switch (result) {
      case MSG_SUCCESS:
        printf("MSG_SUCCESS\n");
        break;
      case MSG_TRUNCATED:
        printf("MSG_TRUNCATED (expected for long messages)\n");
        break;
      case MSG_NO_SPACE:
        printf("MSG_NO_SPACE (unexpected for SetMessage)\n");
        break;
    }

  } else if (move.msg_op == MSG_OP_APPEND) {
    printf("[MSG-TEST] Calling AppendMessage(\"%s\")\n", move.msg_text.c_str());
    printf("[MSG-TEST] Buffer before: length=%zu content=\"%s\"\n", len_before, MsgText);

    MessageResult result = AppendMessage(move.msg_text.c_str());

    size_t len_after = strlen(MsgText);
    printf("[MSG-TEST] Buffer after: length=%zu content=\"%s\"\n", len_after, MsgText);
    printf("[MSG-TEST] Result: ");
    switch (result) {
      case MSG_SUCCESS:
        printf("MSG_SUCCESS\n");
        break;
      case MSG_TRUNCATED:
        printf("MSG_TRUNCATED (message was too long)\n");
        break;
      case MSG_NO_SPACE:
        printf("MSG_NO_SPACE (buffer is full)\n");
        break;
    }

  } else if (move.msg_op == MSG_OP_CLEAR) {
    printf("[MSG-TEST] Calling ClearMessage()\n");
    printf("[MSG-TEST] Buffer before: length=%zu\n", len_before);

    ClearMessage();

    size_t len_after = strlen(MsgText);
    printf("[MSG-TEST] Buffer after: length=%zu\n", len_after);
    printf("[MSG-TEST] Result: Buffer cleared successfully\n");
  }

  printf("[MSG-TEST] Test %s complete\n\n", move.msg_test_name.c_str());
}