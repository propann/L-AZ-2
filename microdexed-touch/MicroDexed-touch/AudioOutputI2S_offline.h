// #ifndef AUDIO_OFFLINE_HEADERS_H
// #define AUDIO_OFFLINE_HEADERS_H

// #include <Audio.h>
// #include <SD.h>
// #include <SPI.h>
// #include "template_mixer.hpp"

// // Defines the size of the internal audio buffer for offline rendering.
// #define I2S_BUFFER_SAMPLES 128

// // Class to handle offline rendering to a stereo WAV file.
// // This is the core of the offline rendering functionality.
// class AudioOutputI2S_offline : public AudioStream
// {
// public:
//   AudioOutputI2S_offline() : AudioStream(2, inputQueueArray) {
//     // Constructor. Initialize the audio buffer to be empty.
//     bufferL = new int16_t[I2S_BUFFER_SAMPLES];
//     bufferR = new int16_t[I2S_BUFFER_SAMPLES];
//     buffer_idx = 0;
//   }

//   // Instead of sending to I2S hardware, we will capture it into a buffer.
//   virtual void update(void) {
   
//     audio_block_t *block_l, *block_r;
//     block_l = receiveReadOnly(0);
//     block_r = receiveReadOnly(1);
//     if (!block_l || !block_r) return;

//     // Check if the buffer is full. If so, call a function to write to SD.
//     for (int i=0; i < AUDIO_BLOCK_SAMPLES; i++) {
//       bufferL[buffer_idx] = block_l->data[i];
//       bufferR[buffer_idx] = block_r->data[i];
//       buffer_idx++;
//       if (buffer_idx >= I2S_BUFFER_SAMPLES) {
//         writeToSD();
//         buffer_idx = 0;
//       }
//     }

//     release(block_l);
//     release(block_r);
//   }

//   void setFile(File *file) {
//     wav_file = file;
//   }

// protected:
// void writeToSD() {
//     // Create a temporary buffer to interleave samples for a single block write.
//     int16_t interleaved_buffer[I2S_BUFFER_SAMPLES * 2];
//     for (int i = 0; i < I2S_BUFFER_SAMPLES; i++) {
//       interleaved_buffer[i * 2] = bufferL[i];
//       interleaved_buffer[i * 2 + 1] = bufferR[i];
//     }
    
//     // Write the entire interleaved buffer at once.
//     wav_file->write((const uint8_t*)interleaved_buffer, sizeof(interleaved_buffer));
//   }

// private:
//   int16_t *bufferL;
//   int16_t *bufferR;
//   int buffer_idx;
//   audio_block_t *inputQueueArray[2];
//   File *wav_file;
// };

// // Now, for the rest of the declarations from the main program's header.

// #define SD_CS_PIN BUILTIN_SDCARD
// extern AudioOutputI2S      i2s1;
// extern AudioOutputI2S_offline i2s_offline;
// extern File wav_file;
// extern AudioMixer<3> finalized_mixer_l;
// extern AudioMixer<3> finalized_mixer_r;
// extern AudioConnection* patchCord_l;
// extern AudioConnection* patchCord_r;

// extern bool is_offline_mode;

// // WAV header structure
// struct WAVHeader {
//   char riff[4] = {'R', 'I', 'F', 'F'};
//   uint32_t file_size = 0;
//   char wave[4] = {'W', 'A', 'V', 'E'};
//   char fmt[4] = {'f', 'm', 't', ' '};
//   uint32_t fmt_size = 16;
//   uint16_t audio_format = 1; // PCM
//   uint16_t num_channels = 2; // Stereo
//   uint32_t sample_rate = 44100;
//   uint32_t byte_rate = 44100 * 2 * (16 / 8); // sample_rate * num_channels * bits_per_sample/8
//   uint16_t block_align = 2 * (16 / 8); // num_channels * bits_per_sample/8
//   uint16_t bits_per_sample = 16;
//   char data[4] = {'d', 'a', 't', 'a'};
//   uint32_t data_size = 0;
// };

// // Function prototypes
// void writeWavHeader(File& file, uint32_t dataSize);
// void toggleOutputMode();

// #endif
