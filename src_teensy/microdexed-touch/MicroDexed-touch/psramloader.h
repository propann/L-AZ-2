#ifndef PSRAMLOADER_H
#define PSRAMLOADER_H

#include "config.h"
#include "drums.h"
#include <SD.h> 

class PsramLoader {
public:
  PsramLoader(uint32_t &freeBytes);

  void loadSampleToDrums(const String filename, drum_config_t &slot, SDClass &sd);
  uint8_t* loadSample(const String filename, uint32_t &bytesRead, uint8_t &channels);
  void unloadSample(const uint8_t *data, uint32_t length);
  
private:
  uint32_t &freeBytesPSRAM;
  static constexpr uint32_t DEFAULT_SD_BUFSIZE = 4 * 1024;
};

#endif //PSRAMLOADER_H