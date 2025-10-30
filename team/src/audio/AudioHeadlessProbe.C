/* AudioHeadlessProbe.C
 * Lightweight harness that initializes the audio system, replays a few
 * synthetic world events, and emits the resulting mixer logs. Used by
 * automation to validate catalog wiring in headless environments.
 */

#include <filesystem>
#include <iostream>
#include <string>

#include "ParserModern.h"
#include "World.h"
#include "audio/AudioSystem.h"
#include "audio/AudioTypes.h"

// Satisfy legacy globals expected by mm4_common.
CParser* g_pParser = nullptr;

namespace {
std::string LocateSoundConfig() {
  namespace fs = std::filesystem;
  const char* candidates[] = {"sound/defaults.txt", "../sound/defaults.txt",
                              "../../sound/defaults.txt"};
  for (const char* candidate : candidates) {
    fs::path path(candidate);
    if (fs::exists(path)) {
      return path.string();
    }
  }
  return "sound/defaults.txt";
}

void SeedWorldAudio(CWorld& world) {
  // Launch event for team 0 (default namespace)
  world.LogAudioEvent("team.launch.default", 0, 0.0, 2, "probe_launch");
  // Dock event for team 1 (mapped to team2 namespace)
  world.LogAudioEvent("team2.dock.default", 1, 0.0, 1, "probe_dock");
  // Delivery event using quantity metadata to exercise scaling
  world.LogAudioEvent("team.deliver_vinyl.default", 0, 48.0, 3, "probe_deliver",
                      0, 1, false);
}
}  // namespace

int main() {
  auto& audioSystem = mm4::audio::AudioSystem::Instance();

  std::string configPath = LocateSoundConfig();
  std::string assetsRoot;
  auto configDir = std::filesystem::path(configPath).parent_path();
  if (!configDir.empty()) {
    assetsRoot = configDir.string();
  }

  if (!audioSystem.Initialize(configPath, assetsRoot, true)) {
    std::cerr << "[audio-probe] failed to initialize audio system" << std::endl;
    return 1;
  }

  CWorld world(/*numTeams=*/2);
  SeedWorldAudio(world);

  const auto& events = world.GetAudioEvents();
  if (events.empty()) {
    std::cerr << "[audio-probe] no audio events captured" << std::endl;
    audioSystem.Shutdown();
    return 2;
  }
  const std::size_t eventCount = events.size();

  audioSystem.BeginSubtick();
  for (const auto& event : events) {
    audioSystem.QueueEffect(event);
  }
  audioSystem.EndSubtick();
  audioSystem.FlushPending(/*currentTurn=*/0);
  world.ClearAudioEvents();

  // Exercise manual track advancement and the auto-advance callback.
  audioSystem.NextTrack(/*fromManual=*/true);
  audioSystem.OnTrackFinished();

  // Toggle mute state to ensure gating plays nicely with pending tracks.
  audioSystem.SetMusicMuted(true);
  audioSystem.SetMusicMuted(false);

  // Advance one more turn to service queued playback gracefully.
  audioSystem.FlushPending(/*currentTurn=*/1);
  audioSystem.Shutdown();

  std::cout << "[audio-probe] queued " << eventCount
            << " events via CWorld harness" << std::endl;
  return 0;
}
