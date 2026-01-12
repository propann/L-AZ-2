//
// Created by Nicholas Newdigate on 20/07/2020.
// edited by d.weber
//
#ifndef WAVHEADERPARSER_H
#define WAVHEADERPARSER_H

#include <string>
#include <cstdint>
#include <SD.h>
#include "config.h"

static constexpr uint32_t FOURCCRIFF = 0x52494646;	// RIFF
static constexpr uint32_t FOURCCWAVE = 0x57415645;	// WAVE
static constexpr uint32_t FOURCCFMT  = 0x666D7420;	// fmt 
static constexpr uint32_t FOURCCDATA = 0x64617461;	// data
static constexpr uint32_t FOURCCLIST = 0x4C495354;	// LIST

static constexpr uint8_t WAV_HEADER_SIZE = 44; // without LIST chunk

using namespace std;

struct WavHeader {
    // RIFF Header
    uint32_t header_chunk_size;// 04 - 07 - Size of the wav portion of the file, which follows the first 8 bytes. File size - 8

    // Format Header
    uint32_t fmt_chunk_size;   // 16 - 19 - Should be 16 for PCM
    uint16_t audio_format;     // 20 - 21 - Should be 1 for PCM. 3 for IEEE Float
    uint16_t num_channels;     // 22 - 23
    uint32_t sample_rate;      // 24 - 27
    uint32_t byte_rate;        // 28 - 31
    uint16_t sample_alignment; // 32 - 33
    uint16_t bit_depth;        // 34 - 35

    // Data
    uint32_t data_bytes; // number of audio bytes following
    uint16_t dataOffset; // byte offset where audio data starts
};

class WavHeaderParser {
public:
 FLASHMEM   bool readWaveHeader(const char *filename, WavHeader &header, WavHeader &wav_header) {
        __disable_irq();
        File wavFile = SD.open(filename);
        __enable_irq();
        if (!wavFile) {
            DBG_LOG(printf("Not able to open wave file... %s\n", filename));
            return false;
        }
        bool result = readWaveHeader(header, wavFile);
        wavFile.close();
        return result;
    }

  FLASHMEM  bool readWaveHeader(WavHeader &header, File wavFile) {
        static constexpr uint16_t MAX_HEADER_SIZE = 512; // this should be enough for most LIST chunks...
        uint8_t buffer[MAX_HEADER_SIZE];
        __disable_irq();
        int bytesRead = wavFile.read(buffer, MAX_HEADER_SIZE);
        __enable_irq();
        if (bytesRead != MAX_HEADER_SIZE) {
            return false;
        }
        return readWaveHeaderFromBuffer(buffer, header);
    }

  FLASHMEM  bool readWaveHeaderFromBuffer(uint8_t *buffer, WavHeader &header) {
        const uint32_t riff = parse_uint32_BE(&buffer[0]);
        if(riff != FOURCCRIFF) {
            DBG_LOG(printf("expected RIFF (was %s)\n", buffer));
            return false;
        }

        header.header_chunk_size = parse_uint32_LE(&buffer[4]);
        
        const uint32_t wave = parse_uint32_BE(&buffer[8]);
        if(wave != FOURCCWAVE) {
            DBG_LOG(printf("expected WAVE (was %s)\n", buffer[8]));
            return false;
        }

        const uint32_t fmt = parse_uint32_BE(&buffer[12]);
        if(fmt != FOURCCFMT) {
            DBG_LOG(printf("expected 'fmt ' (was %s)\n",  buffer[12]));
            return false;
        }

        header.fmt_chunk_size = parse_uint32_LE(&buffer[16]);
        if(header.fmt_chunk_size != 16) {
            DBG_LOG(printf("chunk size should be 16 for PCM wave data... (was %d)\n", header.fmt_chunk_size));
            return false;
        }

        header.audio_format = parse_uint16_LE(&buffer[20]);
        if(header.audio_format != 1) {
            DBG_LOG(printf("unsupported format %i\n", header.audio_format));
            return false;
        }

        header.num_channels = parse_uint16_LE(&buffer[22]);
        header.sample_rate = parse_uint32_LE(&buffer[24]);
        if(header.sample_rate != 44100) {
            DBG_LOG(printf("unsupported samplerate: %i\n", header.sample_rate));
            //return false; // we currently have some DRUMS samples with other sample rates (30kHz and 48kHz)
        }

        header.byte_rate = parse_uint32_LE(&buffer[28]);
        header.sample_alignment = parse_uint16_LE(&buffer[32]);
        
        header.bit_depth = parse_uint16_LE(&buffer[34]);
        if(header.bit_depth != 16) {
            DBG_LOG(printf("unsupported bit depth: %i\n", header.bit_depth));
            return false;
        }

        uint8_t *d = &buffer[36];
        uint32_t nextChunk = parse_uint32_BE(d);
        uint16_t listSize = 0;

        if(nextChunk == FOURCCLIST) {
            listSize = parse_uint32_LE(&d[4]);
            DBG_LOG(printf("skipping list size %i\n", listSize));
            listSize += 8; // skip "LIST" and listSize
            d += listSize;
            nextChunk = parse_uint32_BE(d); // load next chunk id
        }

        if(nextChunk != FOURCCDATA) {
            DBG_LOG(printf("expected 'd'... (was %d)\n", d[0]));
            return false;
        }

        uint32_t data_bytes = parse_uint32_LE(&d[4]);
        header.data_bytes = data_bytes;
        header.dataOffset = WAV_HEADER_SIZE + listSize;
        DBG_LOG(printf("size %d bytes\n", data_bytes));
        return true;
    }

private:
    uint16_t parse_uint16_LE(const uint8_t *pos) {
        return pos[0] | pos[1] << 8;
    }

    uint16_t parse_uint16_BE(const uint8_t *pos) {
        return pos[1] | pos[0] << 8;
    }

    uint32_t parse_uint32_LE(const uint8_t *pos) {
        return pos[0] | pos[1] << 8 | pos[2] << 16 | pos[3] << 24;
    }

    uint32_t parse_uint32_BE(const uint8_t *pos) {
        return pos[3] | pos[2] << 8 | pos[1] << 16 | pos[0] << 24;
    }
};

#endif //WAVHEADERPARSER_H
