/* ModPlayer.C
 * Minimal SDL_mixer-based MOD playback diagnostic tool.
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

void PrintUsage(const char* argv0) {
  std::cout << "Usage: " << argv0 << " -s <path/to/file.mod> [--loops N]\n"
            << "Plays the specified tracker module using SDL_mixer and emits\n"
            << "diagnostic information about available decoders.\n"
            << std::endl;
}

void PrintDecoders() {
  int musicDecoders = Mix_GetNumMusicDecoders();
  std::cout << "[modtest] Music decoders (" << musicDecoders << "):";
  for (int i = 0; i < musicDecoders; ++i) {
    std::cout << " " << Mix_GetMusicDecoder(i);
  }
  std::cout << std::endl;

  int chunkDecoders = Mix_GetNumChunkDecoders();
  std::cout << "[modtest] Chunk decoders (" << chunkDecoders << "):";
  for (int i = 0; i < chunkDecoders; ++i) {
    std::cout << " " << Mix_GetChunkDecoder(i);
  }
  std::cout << std::endl;
}

}  // namespace

int main(int argc, char* argv[]) {
  std::string songPath;
  int requestedLoops = 1;

  for (int i = 1; i < argc; ++i) {
    std::string arg(argv[i]);
    if ((arg == "-s" || arg == "--song") && i + 1 < argc) {
      songPath = argv[++i];
    } else if (arg == "--loops" && i + 1 < argc) {
      requestedLoops = std::stoi(argv[++i]);
    } else if (arg == "-h" || arg == "--help") {
      PrintUsage(argv[0]);
      return 0;
    } else {
      std::cerr << "[modtest] Unknown argument: " << arg << std::endl;
      PrintUsage(argv[0]);
      return 1;
    }
  }

  if (songPath.empty()) {
    std::cerr << "[modtest] Missing required -s <song> argument.\n";
    PrintUsage(argv[0]);
    return 1;
  }

  std::filesystem::path modPath(songPath);
  if (!std::filesystem::exists(modPath)) {
    std::cerr << "[modtest] File does not exist: " << modPath << std::endl;
    return 1;
  }

  std::cout << "[modtest] Attempting to play: " << std::filesystem::absolute(modPath)
            << std::endl;

  if (SDL_Init(SDL_INIT_AUDIO) != 0) {
    std::cerr << "[modtest] SDL_Init failed: " << SDL_GetError() << std::endl;
    return 1;
  }

  int mixFlags = MIX_INIT_MOD | MIX_INIT_OGG | MIX_INIT_MP3;
  int initResult = Mix_Init(mixFlags);
  if ((initResult & MIX_INIT_MOD) == 0) {
    std::cerr << "[modtest] Mix_Init missing MOD support. Requested flags=" << mixFlags
              << " got=" << initResult << " error=" << Mix_GetError() << std::endl;
  } else {
    std::cout << "[modtest] SDL_mixer initialized with MOD support." << std::endl;
  }
  if ((initResult & MIX_INIT_MP3) == 0 || (initResult & MIX_INIT_OGG) == 0) {
    std::cout << "[modtest] Optional codecs unavailable (this is informational)." << std::endl;
  }

  PrintDecoders();

  const int sampleRate = 44100;
  const int channels = 2;
  const int chunkSize = 4096;
  if (Mix_OpenAudio(sampleRate, MIX_DEFAULT_FORMAT, channels, chunkSize) != 0) {
    std::cerr << "[modtest] Mix_OpenAudio failed: " << Mix_GetError() << std::endl;
    Mix_Quit();
    SDL_Quit();
    return 1;
  }

  Mix_Music* music = Mix_LoadMUS(modPath.string().c_str());
  if (!music) {
    std::cerr << "[modtest] Mix_LoadMUS failed: " << Mix_GetError() << std::endl;
    Mix_CloseAudio();
    Mix_Quit();
    SDL_Quit();
    return 1;
  }

  auto type = Mix_GetMusicType(music);
  std::cout << "[modtest] Loaded music type=" << static_cast<int>(type) << std::endl;
  std::cout << "[modtest] Playing (" << requestedLoops
            << " loop(s)); press Ctrl+C to stop." << std::endl;

  if (Mix_PlayMusic(music, requestedLoops - 1) != 0) {
    std::cerr << "[modtest] Mix_PlayMusic failed: " << Mix_GetError() << std::endl;
    Mix_FreeMusic(music);
    Mix_CloseAudio();
    Mix_Quit();
    SDL_Quit();
    return 1;
  }

  while (Mix_PlayingMusic() != 0) {
    SDL_Delay(100);
  }

  std::cout << "[modtest] Playback complete." << std::endl;

  Mix_FreeMusic(music);
  Mix_CloseAudio();
  Mix_Quit();
  SDL_Quit();
  return 0;
}
