#ifndef AudioMultiBandCompressor_h
#define AudioMultiBandCompressor_h

#include "Arduino.h"
#include "AudioStream.h"
#include "arm_math.h"
#include "dspinst.h"

// UI struct: all values 0-100
struct MultiBandCompressor_UI_t {
    // Master controls (unchanged)
    uint8_t master_gain = 50; // unity gain
    uint8_t master_mix = 50;  // fully wet (compressed)

    uint8_t crossover1 = 41;  // ~150 Hz (low/mid split)
    uint8_t crossover2 = 41;  // ~1 kHz (mid/high split)  
    uint8_t crossover3 = 18;  // ~5 kHz (high/very high split)

    // Low band - gentle but present compression
    uint8_t band1_thresh = 60;   // -19.6dB threshold
    uint8_t band1_ratio = 45;    // ~8:1 ratio (was too high before)
    uint8_t band1_attack = 15;   // ~0.3ms attack (much faster)
    uint8_t band1_release = 40;  // ~80ms release (more musical)
    uint8_t band1_gain = 55;     // slight boost

    // Mid-low band - more aggressive
    uint8_t band2_thresh = 50;   // -23dB threshold
    uint8_t band2_ratio = 55;    // ~15:1 ratio
    uint8_t band2_attack = 8;    // ~0.1ms attack (very fast)
    uint8_t band2_release = 30;  // ~45ms release
    uint8_t band2_gain = 60;     // moderate boost

    // Mid-high band - aggressive for presence
    uint8_t band3_thresh = 40;   // -26.4dB threshold  
    uint8_t band3_ratio = 65;    // ~25:1 ratio
    uint8_t band3_attack = 5;    // ~0.05ms attack (ultra-fast)
    uint8_t band3_release = 25;  // ~35ms release
    uint8_t band3_gain = 65;     // good boost

    // High band - most aggressive for sparkle
    uint8_t band4_thresh = 30;   // -29.8dB threshold
    uint8_t band4_ratio = 70;    // ~35:1 ratio  
    uint8_t band4_attack = 2;    // ~0.02ms attack (extreme)
    uint8_t band4_release = 20;  // ~25ms release (tight)
    uint8_t band4_gain = 70;     // significant boost

    uint8_t solo_mask = 0;
    uint8_t crossover_refresh_id = 0; 
};

// Optimized band parameters struct
struct BandParams {
    float32_t thresh_lin;
    float32_t thresh_inv;      // 1.0f / thresh_lin
    float32_t inv_ratio;       // 1.0f / ratio
    float32_t attack_coeff;
    float32_t release_coeff;
    float32_t makeup_gain;
    float32_t gain_lin;

    // NEW: Band-specific noise gate threshold
    float32_t noise_gate_thresh;  // Below this level, no processing occurs
    float32_t upward_limit;       // Maximum upward compression boost

};

// Internal DSP parameters
struct MultiBandCompressor_Parameters_t {
    MultiBandCompressor_UI_t ui_data;

    float32_t crossover1_freq_hz;
    float32_t crossover2_freq_hz;
    float32_t crossover3_freq_hz;

    float32_t master_gain_lin;
    float32_t master_mix_lin;

    // Pre-computed linear values to avoid runtime conversions
    float32_t band1_thresh_lin, band1_gain_lin, band1_ratio, band1_inv_ratio;
    float32_t band1_attack_coeff, band1_release_coeff, band1_makeup;
    
    float32_t band2_thresh_lin, band2_gain_lin, band2_ratio, band2_inv_ratio;
    float32_t band2_attack_coeff, band2_release_coeff, band2_makeup;
    
    float32_t band3_thresh_lin, band3_gain_lin, band3_ratio, band3_inv_ratio;
    float32_t band3_attack_coeff, band3_release_coeff, band3_makeup;
    
    float32_t band4_thresh_lin, band4_gain_lin, band4_ratio, band4_inv_ratio;
    float32_t band4_attack_coeff, band4_release_coeff, band4_makeup;
};

class AudioMultiBandCompressor : public AudioStream {
public:
    AudioMultiBandCompressor();

    void resetUIParameters(MultiBandCompressor_UI_t &ui_params);
    void setUI(const MultiBandCompressor_UI_t &new_ui);
    virtual void update(void);

    bool getAndClearRedrawFlag();

    // Public getters for crossover frequencies
    float32_t getCrossover1Hz() const { return params.crossover1_freq_hz; }
    float32_t getCrossover2Hz() const { return params.crossover2_freq_hz; }
    float32_t getCrossover3Hz() const { return params.crossover3_freq_hz; }

private:
    MultiBandCompressor_Parameters_t params;
    audio_block_t *inputQueueArray[2];

    // Optimized compression method with pre-computed parameters
    void process_band_ott(float32_t* bufL, float32_t* bufR,
                         float32_t &envL, float32_t &envR,
                         const BandParams& band);
                         
    bool _redrawRequired = false;

    // Pre-computed band parameters for faster processing
    BandParams band_params[4];

    // Working buffers - avoid dynamic allocation
    float32_t bufL[AUDIO_BLOCK_SAMPLES];
    float32_t bufR[AUDIO_BLOCK_SAMPLES];
    
    // Separate band buffers to avoid memcpy overhead
    float32_t band_buffer_l1[AUDIO_BLOCK_SAMPLES];
    float32_t band_buffer_l2[AUDIO_BLOCK_SAMPLES];
    float32_t band_buffer_l3[AUDIO_BLOCK_SAMPLES];
    float32_t band_buffer_l4[AUDIO_BLOCK_SAMPLES];
    float32_t band_buffer_r1[AUDIO_BLOCK_SAMPLES];
    float32_t band_buffer_r2[AUDIO_BLOCK_SAMPLES];
    float32_t band_buffer_r3[AUDIO_BLOCK_SAMPLES];
    float32_t band_buffer_r4[AUDIO_BLOCK_SAMPLES];

    // State: envelopes
    float32_t env1_l, env1_r;
    float32_t env2_l, env2_r;
    float32_t env3_l, env3_r;
    float32_t env4_l, env4_r;

    // Filter instances
    arm_biquad_casd_df1_inst_f32 lp1_l, lp1_r;
    arm_biquad_casd_df1_inst_f32 hp1_l, hp1_r;
    arm_biquad_casd_df1_inst_f32 lp2_l, lp2_r;
    arm_biquad_casd_df1_inst_f32 hp2_l, hp2_r;
    arm_biquad_casd_df1_inst_f32 lp3_l, lp3_r;
    arm_biquad_casd_df1_inst_f32 hp3_l, hp3_r;

    // Filter coefficient and state arrays
    // Left filters
    float32_t coeffs_lp1_l[10], coeffs_lp2_l[10], coeffs_lp3_l[10];
    float32_t state_lp1_l[8], state_lp2_l[8], state_lp3_l[8];

    float32_t coeffs_hp1_l[10], coeffs_hp2_l[10], coeffs_hp3_l[10];
    float32_t state_hp1_l[8], state_hp2_l[8], state_hp3_l[8];

    // Right filters
    float32_t coeffs_lp1_r[10], coeffs_lp2_r[10], coeffs_lp3_r[10];
    float32_t state_lp1_r[8], state_lp2_r[8], state_lp3_r[8];

    float32_t coeffs_hp1_r[10], coeffs_hp2_r[10], coeffs_hp3_r[10];
    float32_t state_hp1_r[8], state_hp2_r[8], state_hp3_r[8];

    // Helper methods
    void updateParameterValues();
    void calculate_biquad_coeffs(arm_biquad_casd_df1_inst_f32 *filter,
                                 float32_t freq_hz,
                                 bool is_lowpass);
};

#endif