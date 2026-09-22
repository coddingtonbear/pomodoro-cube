#include "sim_rtc.h"

#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <unistd.h>

#include "rtc_state.h"

namespace {

// Beside the executable, which puts it in the build directory -- already
// ignored by git, and it follows the binary wherever it is run from.
std::string statePath() {
  char exe[4096];
  const ssize_t len = ::readlink("/proc/self/exe", exe, sizeof(exe) - 1);
  if (len <= 0) return "sim_rtc.bin";
  exe[len] = '\0';

  std::string path(exe);
  const size_t slash = path.find_last_of('/');
  if (slash == std::string::npos) return "sim_rtc.bin";
  return path.substr(0, slash + 1) + "sim_rtc.bin";
}

// A real cold boot leaves whatever was in RTC memory before, not zeroes. Filling
// with junk means the magic-word check is genuinely exercised -- if it were
// wrong, the simulator would visibly resume a nonsense timer rather than
// quietly getting away with a benign block of zeroes.
void fillWithJunk(RtcState::Data &data) {
  std::mt19937 rng(0xC01DB007);
  auto *bytes = reinterpret_cast<unsigned char *>(&data);
  for (size_t i = 0; i < sizeof(RtcState::Data); i++) {
    bytes[i] = (unsigned char)(rng() & 0xFF);
  }
}

}  // namespace

void SimRtc::restore(bool wokeFromSleep) {
  RtcState::Data &data = RtcState::data();

  if (!wokeFromSleep) {
    ::remove(statePath().c_str());
    fillWithJunk(data);
    std::printf("[sim] cold boot: RTC memory filled with junk\n");
    return;
  }

  std::FILE *file = std::fopen(statePath().c_str(), "rb");
  if (!file) {
    fillWithJunk(data);
    return;
  }

  const size_t read = std::fread(&data, 1, sizeof(RtcState::Data), file);
  std::fclose(file);
  if (read != sizeof(RtcState::Data)) fillWithJunk(data);
}

void SimRtc::persist() {
  std::FILE *file = std::fopen(statePath().c_str(), "wb");
  if (!file) return;
  std::fwrite(&RtcState::data(), 1, sizeof(RtcState::Data), file);
  std::fclose(file);
}
