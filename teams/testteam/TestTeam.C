/* TestTeam.C
 * Implementation of scripted test team
 * Reads commands from test_moves.txt and executes them on schedule
 */

#include "TestTeam.h"
#include "Ship.h"
#include <fstream>
#include <sstream>
#include <cstdio>

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
  // Initialize ships with default configuration
  // All ships get equal fuel/cargo split
  for (unsigned int i = 0; i < GetShipCount(); ++i) {
    GetShip(i)->SetCapacity(S_FUEL, 30.0);   // 30 tons fuel
    GetShip(i)->SetCapacity(S_CARGO, 30.0);  // 30 tons cargo
  }

  // Load test moves from file
  LoadTestMoves("test_moves.txt");

  printf("[TestTeam] Initialized with %zu scripted moves\n", moves.size());
}

void TestTeam::SelectTeamName() {
  SetName("TestTeam");
  GetStation()->SetName("Test Station");
}

void TestTeam::SelectShipNames() {
  GetShip(0)->SetName("Test-1");
  GetShip(1)->SetName("Test-2");
  GetShip(2)->SetName("Test-3");
  GetShip(3)->SetName("Test-4");
}

void TestTeam::Turn() {
  current_turn++;

  // Execute any moves scheduled for this turn
  for (const TestMove& move : moves) {
    if (move.turn == current_turn) {
      // Validate ship number
      if (move.shipnum < 0 || move.shipnum >= (int)GetShipCount()) {
        printf("[TestTeam] Turn %d: Invalid ship number %d (skipping)\n",
               current_turn, move.shipnum);
        continue;
      }

      CShip* ship = GetShip(move.shipnum);

      // Execute the order
      const char* order_names[] = {"THRUST", "TURN", "JETTISON", "LASER"};
      const char* order_name = (move.order >= 0 && move.order <= 3) ?
                               order_names[move.order] : "UNKNOWN";

      printf("[TestTeam] Turn %d: Ship %d (%s) executing %s %.2f\n",
             current_turn, move.shipnum, ship->GetName(), order_name, move.magnitude);

      ship->SetOrder(move.order, move.magnitude);
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

  std::string line;
  int line_num = 0;

  while (std::getline(file, line)) {
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

    if (tokens.size() != 4) {
      printf("[TestTeam] Warning: Line %d has %zu fields (expected 4), skipping: %s\n",
             line_num, tokens.size(), line.c_str());
      continue;
    }

    try {
      TestMove move;
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

  file.close();
  printf("[TestTeam] Loaded %zu test moves from %s\n", moves.size(), filename);
}

OrderKind TestTeam::ParseOrderKind(const std::string& order_str) {
  if (order_str == "THRUST" || order_str == "O_THRUST") {
    return O_THRUST;
  } else if (order_str == "TURN" || order_str == "O_TURN") {
    return O_TURN;
  } else if (order_str == "JETTISON" || order_str == "O_JETTISON") {
    return O_JETTISON;
  } else if (order_str == "LASER" || order_str == "O_LASER") {
    return O_LASER;
  } else {
    printf("[TestTeam] Warning: Unknown order type '%s', defaulting to O_THRUST\n",
           order_str.c_str());
    return O_THRUST;
  }
}