// #include "AudioOutputI2S_offline.h"
// #include "template_mixer.hpp"
// #include <Audio.h>

// extern AudioOutputI2S      i2s1;
// extern AudioMixer<3> finalized_mixer_r;
// extern AudioMixer<3> finalized_mixer_l;

// AudioOutputI2S_offline i2s_offline;
// File wav_file;

// // Define audio connections. These are set up in setup()
//  AudioConnection* patchCord_l;
//  AudioConnection* patchCord_r;

// // A simple state variable to track the current mode.
// bool is_offline_mode = false;

// // Function to write the WAV header to the file.
// void writeWavHeader(File& file, uint32_t dataSize) {
//   WAVHeader header;
//   header.data_size = dataSize;
//   header.file_size = dataSize + 36; // data size + header size excluding RIFF chunk

//   file.write(&header, sizeof(WAVHeader));
// }

// // Function to toggle between real-time and offline rendering.
// void toggleOutputMode() {
//   if (is_offline_mode) {
//     // Switch to real-time mode
//     Serial.println("Switching to real-time output.");

//     // Disconnect from the offline output and delete old connections
//     if (patchCord_l) {
//         patchCord_l->disconnect();
//         delete patchCord_l;
//         patchCord_l = nullptr;
//     }
//     if (patchCord_r) {
//         patchCord_r->disconnect();
//         delete patchCord_r;
//         patchCord_r = nullptr;
//     }

//     // Reconnect to the real-time output
//     patchCord_l = new AudioConnection(finalized_mixer_l, 0, i2s1, 1);
//     patchCord_r = new AudioConnection(finalized_mixer_r, 0, i2s1, 0);
    
//     // Close the WAV file
//     if (wav_file) {
//       // Before closing, update the file size in the header
//       uint32_t file_pos = wav_file.position();
//       wav_file.seek(0);
//       writeWavHeader(wav_file, file_pos - sizeof(WAVHeader));
//       wav_file.close();
//     }
    
//     is_offline_mode = false;

//   } else {
//     // Switch to offline mode
//     Serial.println("Switching to offline rendering to SD card.");

//     // Disconnect from the real-time output and delete old connections
//     if (patchCord_l) {
//         patchCord_l->disconnect();
//         delete patchCord_l;
//         patchCord_l = nullptr;
//     }
//     if (patchCord_r) {
//         patchCord_r->disconnect();
//         delete patchCord_r;
//         patchCord_r = nullptr;
//     }

//   SD.remove("rendered.wav");

//     // Open a file for writing the WAV data.
//     wav_file = SD.open("rendered.wav", FILE_WRITE);
//     if (!wav_file) {
//       Serial.println("Error opening rendered.wav!");
//       return;
//     }

//     // Write a dummy header to reserve space.
//     writeWavHeader(wav_file, 0);
    
//     i2s_offline.setFile(&wav_file);

//     finalized_mixer_l.gain(0, 0.5);
//     finalized_mixer_r.gain(0, 0.5);

//     // Reconnect to the offline output
//     patchCord_l = new AudioConnection(finalized_mixer_l, 0, i2s_offline, 0);
//     patchCord_r = new AudioConnection(finalized_mixer_r, 0, i2s_offline, 1);
    
//     is_offline_mode = true;
//   }
// }
