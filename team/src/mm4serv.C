#include <sys/types.h>

#include <cmath>
#include <ctime>
#include <limits>
#include <string>
#include <vector>

#include "ParserModern.h"
#include "Server.h"
#include "Station.h"
#include "Team.h"
#include "World.h"

// Global parser instance for feature flag access
CParser* g_pParser = nullptr;

int main(int argc, char* argv[]) {
  srand(time(NULL));
  CParser PCmdLn(argc, argv);
  g_pParser = &PCmdLn;  // Set global parser instance

  if (PCmdLn.needhelp == 1) {
    printf("mm4serv [-pport] [-Tnumteams] [--announcer-velocity-clamping]\n");
    printf("  port defaults to 2323\n  numteams defaults to 2\n");
    printf("  --announcer-velocity-clamping enables velocity clamping announcements\n");
    printf("MechMania IV: The Vinyl Frontier   10/2/98\n");
    exit(1);
  }

  // Log enabled feature flags if verbose mode is on
  if (PCmdLn.verbose) {
    printf("\n========================================\n");
    printf("MM4SERV FEATURE FLAGS\n");
    printf("========================================\n");
    printf("verbose: %s\n", PCmdLn.verbose ? "ON" : "OFF");

    const auto& features = PCmdLn.GetModernParser().features;
    printf("\nFeature flags (true = legacy/buggy behavior):\n");
    for (const auto& pair : features) {
      printf("  %s: %s\n", pair.first.c_str(), pair.second ? "ENABLED" : "disabled");
    }
    printf("========================================\n\n");
  }

  CServer myServ(PCmdLn.numteams, PCmdLn.port);

  myServ.ConnectClients();  // Sends ack & ID to clients
  myServ.MeetTeams();       // Clients send back team info

  // Game loop - run until max turns reached
  CWorld* pWorld = myServ.GetWorld();
  while (pWorld && pWorld->GetCurrentTurn() < g_game_max_turns) {
    myServ.Simulation();

    myServ.BroadcastWorld();
    myServ.ReceiveTeamOrders();
  }

  if (pWorld) {
    const unsigned int numTeams = pWorld->GetNumTeams();
    std::vector<double> teamScores(numTeams, 0.0);
    double bestScore = std::numeric_limits<double>::lowest();

    for (unsigned int i = 0; i < numTeams; ++i) {
      CTeam* team = pWorld->GetTeam(i);
      double score = 0.0;
      if (team && team->GetStation()) {
        score = team->GetStation()->GetVinylStore();
      }
      teamScores[i] = score;
      if (score > bestScore) {
        bestScore = score;
      }
    }

    if (numTeams > 0 && std::isfinite(bestScore)) {
      auto makeEventId = [](const CTeam* team, const std::string& suffix) {
        if (!team) {
          return suffix;
        }
        int worldIndex = static_cast<int>(team->GetWorldIndex());
        return "team" + std::to_string(worldIndex + 1) + "." + suffix;
      };

      const double kScoreEpsilon = 1e-3;
      pWorld->bGameOver = true;

      for (unsigned int i = 0; i < numTeams; ++i) {
        CTeam* team = pWorld->GetTeam(i);
        bool isWinner =
            std::fabs(teamScores[i] - bestScore) <= kScoreEpsilon;
        std::string suffix =
            isWinner ? "game_won.default" : "";
        std::string metadata;
        if (team && team->GetName()) {
          metadata = team->GetName();
        }
        int teamIndex =
            team ? static_cast<int>(team->GetWorldIndex()) : -1;
        if (!suffix.empty()) {
          pWorld->LogAudioEvent(makeEventId(team, suffix),
                                teamIndex, teamScores[i], 1, metadata);
        }
      }

      // Push final snapshot so observer hears game result cues.
      myServ.BroadcastWorld();
      myServ.WaitForObserver();
      myServ.SendWorldToObserver();
      pWorld->ClearAudioEvents();
    }
  }

  // Log final scores
  printf("\n========================================\n");
  printf("           FINAL SCORES\n");
  printf("========================================\n");
  if (pWorld) {
    for (unsigned int i = 0; i < pWorld->GetNumTeams(); ++i) {
      CTeam* pTeam = pWorld->GetTeam(i);
      if (pTeam && pTeam->GetStation()) {
        printf("%s: %.2f vinyl\n", pTeam->GetName(),
               pTeam->GetStation()->GetVinylStore());
      }
    }
  }
  printf("========================================\n\n");

  return 0;
}
