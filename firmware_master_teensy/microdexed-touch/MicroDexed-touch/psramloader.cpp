#include "psramloader.h"
#include <Audio.h>
#include "wavheaderparser.h"

FLASHMEM PsramLoader::PsramLoader(uint32_t &freeBytes) : 
  freeBytesPSRAM(freeBytes) {
}


FLASHMEM void PsramLoader::loadSampleToDrums(const String path, drum_config_t &slot,  SDClass &sd) {
  File file = sd.open(path.c_str());
  if((slot.drum_data != nullptr) && (slot.len > 0)) {
    DBG_LOG(printf("unload...\n"));
    unloadSample(slot.drum_data, slot.len * 2); // len is 16bit
  }
  uint32_t bytesRead = 0;
  slot.drum_data = loadSample(path, bytesRead, slot.numChannels);
  slot.len = bytesRead / 2;

  // test mute left for stereo samples (plotter / sample points)
  /*if(slot.numChannels == 2) {
    for(uint i = 0; i < slot.len; i += 2) {
      int16_t *d = (int16_t*)&slot.drum_data[i*2];
      *d = 0;
    }
  }*/
}

FLASHMEM uint8_t* PsramLoader::loadSample(const String filename, uint32_t &bytesRead, uint8_t &channels) {
  uint8_t *data = nullptr;
  DBG_LOG(printf("Reading %s\n", filename.c_str()));
  
  File f = SD.open(filename.c_str(), O_READ);
  
  if (f) {
    if (f.size() < freeBytesPSRAM) {
      AudioNoInterrupts();
      
      WavHeader header = { 0 };
      WavHeaderParser wavHeaderParser;
      bool success = wavHeaderParser.readWaveHeader(header, f);
      if(success) {
        f.seek(header.dataOffset); // skip header
        uint32_t total_read = 0;
        data = (uint8_t*)extmem_malloc(header.data_bytes);
        if(data != nullptr) {
          uint8_t *index = data;
          while (f.available()) {
            uint32_t readNumBytes = std::min(header.data_bytes - total_read, DEFAULT_SD_BUFSIZE);
            size_t bytesRead = f.read(index, readNumBytes);
            if (bytesRead == 0) {
              break;
            }
            total_read += bytesRead;
            index += bytesRead;
          }
        }

        DBG_LOG(printf("read %i of %i bytes.\n", total_read, header.data_bytes));

        freeBytesPSRAM -= total_read;
        DBG_LOG(printf("loaded %i bytes. remaining %i\n", total_read, freeBytesPSRAM));
        bytesRead = total_read;
        channels = header.num_channels;
      }
      AudioInterrupts();
    }
    f.close();
  }
  
  return data;
}

FLASHMEM void PsramLoader::unloadSample(const uint8_t *data, uint32_t length) {
  extmem_free((uint8_t*)data);
  freeBytesPSRAM += length;
  DBG_LOG(printf("freeing %i bytes...\n", length));
}

