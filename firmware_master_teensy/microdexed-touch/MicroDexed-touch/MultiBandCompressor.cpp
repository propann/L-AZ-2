#include "MultiBandCompressor.h"
#include <math.h>

void AudioMultiBandCompressor::setUI(const MultiBandCompressor_UI_t& new_ui) {
    if (new_ui.crossover1 != params.ui_data.crossover1 ||
        new_ui.crossover2 != params.ui_data.crossover2 ||
        new_ui.crossover3 != params.ui_data.crossover3 ||
        new_ui.solo_mask != params.ui_data.solo_mask)
    {
        _redrawRequired = true;
    }

    params.ui_data = new_ui;
    updateParameterValues();
}

bool AudioMultiBandCompressor::getAndClearRedrawFlag() {
    bool temp = _redrawRequired;
    _redrawRequired = false;
    return temp;
}

AudioMultiBandCompressor::AudioMultiBandCompressor() : AudioStream(2, inputQueueArray) {
    // Initialize 4th-order filters (2 biquads each)
    arm_biquad_cascade_df1_init_f32(&lp1_l, 2, coeffs_lp1_l, state_lp1_l);
    arm_biquad_cascade_df1_init_f32(&lp1_r, 2, coeffs_lp1_r, state_lp1_r);
    arm_biquad_cascade_df1_init_f32(&hp1_l, 2, coeffs_hp1_l, state_hp1_l);
    arm_biquad_cascade_df1_init_f32(&hp1_r, 2, coeffs_hp1_r, state_hp1_r);

    arm_biquad_cascade_df1_init_f32(&lp2_l, 2, coeffs_lp2_l, state_lp2_l);
    arm_biquad_cascade_df1_init_f32(&lp2_r, 2, coeffs_lp2_r, state_lp2_r);
    arm_biquad_cascade_df1_init_f32(&hp2_l, 2, coeffs_hp2_l, state_hp2_l);
    arm_biquad_cascade_df1_init_f32(&hp2_r, 2, coeffs_hp2_r, state_hp2_r);

    arm_biquad_cascade_df1_init_f32(&lp3_l, 2, coeffs_lp3_l, state_lp3_l);
    arm_biquad_cascade_df1_init_f32(&lp3_r, 2, coeffs_lp3_r, state_lp3_r);
    arm_biquad_cascade_df1_init_f32(&hp3_l, 2, coeffs_hp3_l, state_hp3_l);
    arm_biquad_cascade_df1_init_f32(&hp3_r, 2, coeffs_hp3_r, state_hp3_r);

    // Initialize envelopes
    env1_l = env1_r = env2_l = env2_r = env3_l = env3_r = env4_l = env4_r = 0.0f;

    updateParameterValues();
}

void AudioMultiBandCompressor::updateParameterValues() {
    // Master controls (these are fine)
    params.master_gain_lin = powf(10.0f, (params.ui_data.master_gain - 50.0f) * 0.6f / 20.0f);
    params.master_mix_lin = params.ui_data.master_mix * 0.01f;

    // Crossover frequencies (these are fine)
    const float log_30 = logf(30.0f);
    const float log_250 = logf(250.0f);
    const float log_2000 = logf(2000.0f);
    const float log_8000 = logf(8000.0f);
    
    params.crossover1_freq_hz = expf(log_30 + (params.ui_data.crossover1 * 0.01f) * (log_250 - log_30));
    params.crossover2_freq_hz = expf(log_250 + (params.ui_data.crossover2 * 0.01f) * (log_2000 - log_250));
    params.crossover3_freq_hz = expf(log_2000 + (params.ui_data.crossover3 * 0.01f) * (log_8000 - log_2000));

    // Update filter coefficients (same as before)
    calculate_biquad_coeffs(&lp1_l, params.crossover1_freq_hz, true);
    calculate_biquad_coeffs(&lp1_r, params.crossover1_freq_hz, true);
    calculate_biquad_coeffs(&hp1_l, params.crossover1_freq_hz, false);
    calculate_biquad_coeffs(&hp1_r, params.crossover1_freq_hz, false);

    calculate_biquad_coeffs(&lp2_l, params.crossover2_freq_hz, true);
    calculate_biquad_coeffs(&lp2_r, params.crossover2_freq_hz, true);
    calculate_biquad_coeffs(&hp2_l, params.crossover2_freq_hz, false);
    calculate_biquad_coeffs(&hp2_r, params.crossover2_freq_hz, false);

    calculate_biquad_coeffs(&lp3_l, params.crossover3_freq_hz, true);
    calculate_biquad_coeffs(&lp3_r, params.crossover3_freq_hz, true);
    calculate_biquad_coeffs(&hp3_l, params.crossover3_freq_hz, false);
    calculate_biquad_coeffs(&hp3_r, params.crossover3_freq_hz, false);

    // Band gains 
    params.band1_gain_lin = powf(10.0f, (params.ui_data.band1_gain - 50.0f) * 0.4f / 20.0f);
    params.band2_gain_lin = powf(10.0f, (params.ui_data.band2_gain - 50.0f) * 0.4f / 20.0f);
    params.band3_gain_lin = powf(10.0f, (params.ui_data.band3_gain - 35.0f) * 0.4f / 20.0f);
    params.band4_gain_lin = powf(10.0f, (params.ui_data.band4_gain - 40.0f) * 0.4f / 20.0f);

    // Thresholds 
    params.band1_thresh_lin = powf(10.0f, (-40.0f + params.ui_data.band1_thresh * 0.34f) / 20.0f);
    params.band2_thresh_lin = powf(10.0f, (-40.0f + params.ui_data.band2_thresh * 0.34f) / 20.0f);
    params.band3_thresh_lin = powf(10.0f, (-40.0f + params.ui_data.band3_thresh * 0.34f) / 20.0f);
    params.band4_thresh_lin = powf(10.0f, (-40.0f + params.ui_data.band4_thresh * 0.34f) / 20.0f);

    // Calculate band-specific noise gate thresholds
    // Lower frequencies need higher thresholds, higher frequencies can be more sensitive
    const float32_t band1_gate = params.band1_thresh_lin * 0.05f; // -26dB below threshold
    const float32_t band2_gate = params.band2_thresh_lin * 0.08f; // -22dB below threshold  
    const float32_t band3_gate = params.band3_thresh_lin * 0.12f; // -18dB below threshold
    const float32_t band4_gate = params.band4_thresh_lin * 0.15f; // -16dB below threshold
    
   // Conservative upward compression limits per band
    const float32_t band1_limit = 2.0f;  // 6dB max boost for bass
    const float32_t band2_limit = 2.5f;  // 8dB max boost for mids
    const float32_t band3_limit = 3.0f;  // 9.5dB max boost for mid-highs
    const float32_t band4_limit = 4.0f;  // 12dB max boost for highs (sparkle)

    // This gives more musical ratios instead of limiting
    params.band1_ratio = 1.0f + powf(params.ui_data.band1_ratio * 0.01f, 2.0f) * 19.0f;
    params.band2_ratio = 1.0f + powf(params.ui_data.band2_ratio * 0.01f, 2.0f) * 19.0f;
    params.band3_ratio = 1.0f + powf(params.ui_data.band3_ratio * 0.01f, 2.0f) * 19.0f;
    params.band4_ratio = 1.0f + powf(params.ui_data.band4_ratio * 0.01f, 2.0f) * 19.0f;
    
    params.band1_inv_ratio = 1.0f / params.band1_ratio;
    params.band2_inv_ratio = 1.0f / params.band2_ratio;
    params.band3_inv_ratio = 1.0f / params.band3_ratio;
    params.band4_inv_ratio = 1.0f / params.band4_ratio;

    // UI 0-100 now maps to 0.1ms - 20ms (prevents ultra-fast attack distortion)
    const float sample_rate_ms = AUDIO_SAMPLE_RATE_EXACT * 0.001f;
    
    params.band1_attack_coeff = expf(-1.0f / (sample_rate_ms * (0.1f + powf(params.ui_data.band1_attack * 0.01f, 2.0f) * 19.9f)));
    params.band2_attack_coeff = expf(-1.0f / (sample_rate_ms * (0.1f + powf(params.ui_data.band2_attack * 0.01f, 2.0f) * 19.9f)));
    params.band3_attack_coeff = expf(-1.0f / (sample_rate_ms * (0.1f + powf(params.ui_data.band3_attack * 0.01f, 2.0f) * 19.9f)));
    params.band4_attack_coeff = expf(-1.0f / (sample_rate_ms * (0.1f + powf(params.ui_data.band4_attack * 0.01f, 2.0f) * 19.9f)));
    
    // UI 0-100 now maps to 20ms - 500ms (prevents pumping on bass)
    params.band1_release_coeff = expf(-1.0f / (sample_rate_ms * (20.0f + powf(params.ui_data.band1_release * 0.01f, 1.2f) * 480.0f)));
    params.band2_release_coeff = expf(-1.0f / (sample_rate_ms * (20.0f + powf(params.ui_data.band2_release * 0.01f, 1.2f) * 480.0f)));
    params.band3_release_coeff = expf(-1.0f / (sample_rate_ms * (20.0f + powf(params.ui_data.band3_release * 0.01f, 1.2f) * 480.0f)));
    params.band4_release_coeff = expf(-1.0f / (sample_rate_ms * (20.0f + powf(params.ui_data.band4_release * 0.01f, 1.2f) * 480.0f)));
    
    // Higher ratios need more makeup gain to compensate for level reduction
    params.band1_makeup = 1.0f + (params.band1_ratio - 1.0f) * 0.15f;
    params.band2_makeup = 1.0f + (params.band2_ratio - 1.0f) * 0.15f;
    params.band3_makeup = 1.0f + (params.band3_ratio - 1.0f) * 0.15f;
    params.band4_makeup = 1.0f + (params.band4_ratio - 1.0f) * 0.15f;

    // Update band parameter structs
    band_params[0] = {params.band1_thresh_lin, 1.0f/params.band1_thresh_lin, params.band1_inv_ratio, 
                      params.band1_attack_coeff, params.band1_release_coeff, params.band1_makeup, 
                      params.band1_gain_lin, band1_gate, band1_limit};
    band_params[1] = {params.band2_thresh_lin, 1.0f/params.band2_thresh_lin, params.band2_inv_ratio, 
                      params.band2_attack_coeff, params.band2_release_coeff, params.band2_makeup, 
                      params.band2_gain_lin, band2_gate, band2_limit};
    band_params[2] = {params.band3_thresh_lin, 1.0f/params.band3_thresh_lin, params.band3_inv_ratio, 
                      params.band3_attack_coeff, params.band3_release_coeff, params.band3_makeup, 
                      params.band3_gain_lin, band3_gate, band3_limit};
    band_params[3] = {params.band4_thresh_lin, 1.0f/params.band4_thresh_lin, params.band4_inv_ratio, 
                      params.band4_attack_coeff, params.band4_release_coeff, params.band4_makeup, 
                      params.band4_gain_lin, band4_gate, band4_limit};
}

void AudioMultiBandCompressor::calculate_biquad_coeffs(
    arm_biquad_casd_df1_inst_f32* filter,
    float32_t freq_hz,
    bool is_lowpass)
{
    const float32_t Fs = AUDIO_SAMPLE_RATE_EXACT;
    const float32_t Q = 0.7071f; // Butterworth
    const float32_t w0 = 2.0f * PI * freq_hz / Fs;
    
    // Use standard math functions
    const float32_t cos_w0 = cosf(w0);
    const float32_t sin_w0 = sinf(w0);
    
    const float32_t alpha = sin_w0 / (2.0f * Q);
    float32_t b0, b1, b2;

    if (is_lowpass) {
        b0 = (1.0f - cos_w0) * 0.5f;
        b1 = 1.0f - cos_w0;
        b2 = b0;
    } else { // highpass
        b0 = (1.0f + cos_w0) * 0.5f;
        b1 = -(1.0f + cos_w0);
        b2 = b0;
    }

    const float32_t a0 = 1.0f + alpha;
    const float32_t a1 = -2.0f * cos_w0;
    const float32_t a2 = 1.0f - alpha;
    
    const float32_t inv_a0 = 1.0f / a0; // Single division

    // Normalize and store coefficients
    filter->pCoeffs[0] = b0 * inv_a0;
    filter->pCoeffs[1] = b1 * inv_a0;
    filter->pCoeffs[2] = b2 * inv_a0;
    filter->pCoeffs[3] = -a1 * inv_a0;
    filter->pCoeffs[4] = -a2 * inv_a0;

    // Duplicate for second stage (4th-order)
    filter->pCoeffs[5] = filter->pCoeffs[0];
    filter->pCoeffs[6] = filter->pCoeffs[1];
    filter->pCoeffs[7] = filter->pCoeffs[2];
    filter->pCoeffs[8] = filter->pCoeffs[3];
    filter->pCoeffs[9] = filter->pCoeffs[4];
}

void AudioMultiBandCompressor::process_band_ott(float32_t* bufL, float32_t* bufR,
                                               float32_t& envL, float32_t& envR,
                                               const BandParams& band) {
    
    const float32_t upper_thresh = band.thresh_lin * 1.05f;  // Small hysteresis
    const float32_t lower_thresh = band.thresh_lin * 0.95f;
    
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        const float32_t inputL = bufL[i];
        const float32_t inputR = bufR[i];
        
        // Envelope detection
        const float32_t absL = fabsf(inputL);
        const float32_t absR = fabsf(inputR);

        if (absL > envL) {
            envL = absL + (envL - absL) * band.attack_coeff;
        } else {
            envL = absL + (envL - absL) * band.release_coeff;
        }

        if (absR > envR) {
            envR = absR + (envR - absR) * band.attack_coeff;
        } else {
            envR = absR + (envR - absR) * band.release_coeff;
        }

        float32_t gainL = 1.0f, gainR = 1.0f;
        
        // Left channel - only process if above noise gate
        if (envL > band.noise_gate_thresh) {
            if (envL > upper_thresh) {
                // Downward compression
                const float32_t ratio_factor = envL / band.thresh_lin;
                gainL = powf(ratio_factor, band.inv_ratio - 1.0f);
            } else if (envL < lower_thresh) {
                // Gentle upward compression with band-specific limit  0.12f 
                const float32_t ratio_factor = band.thresh_lin / envL;
                const float32_t boost_factor = powf(ratio_factor, (1.0f - band.inv_ratio) * 0.2f);
                gainL = fminf(boost_factor, band.upward_limit);
            }
        }
        
        // Right channel (same logic)
        if (envR > band.noise_gate_thresh) {
            if (envR > upper_thresh) {
                const float32_t ratio_factor = envR / band.thresh_lin;
                gainR = powf(ratio_factor, band.inv_ratio - 1.0f);
            } else if (envR < lower_thresh) {
                const float32_t ratio_factor = band.thresh_lin / envR;
                const float32_t boost_factor = powf(ratio_factor, (1.0f - band.inv_ratio) * 0.2f);
                gainR = fminf(boost_factor, band.upward_limit);
            }
        }
        
        bufL[i] = inputL * gainL * band.makeup_gain * band.gain_lin;
        bufR[i] = inputR * gainR * band.makeup_gain * band.gain_lin;
    }
}

void AudioMultiBandCompressor::update() {
    audio_block_t* blockL = receiveReadOnly(0);
    audio_block_t* blockR = receiveReadOnly(1);
    if (!blockL || !blockR) {
        if (blockL) release(blockL);
        if (blockR) release(blockR);
        return;
    }

    // Use in-place processing to reduce RAM usage
    // Convert to float and store in main buffers
    const float32_t scale = 1.0f / 32768.0f;
    
    // Use ARM DSP for vectorized conversion
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        bufL[i] = (float32_t)blockL->data[i] * scale;
        bufR[i] = (float32_t)blockR->data[i] * scale;
    }

    // Process bands in-place using shared buffers more efficiently
    // Band 1: Low (use band_buffer_l1/r1)
    memcpy(band_buffer_l1, bufL, sizeof(float32_t) * AUDIO_BLOCK_SAMPLES);
    memcpy(band_buffer_r1, bufR, sizeof(float32_t) * AUDIO_BLOCK_SAMPLES);
    arm_biquad_cascade_df1_f32(&lp1_l, band_buffer_l1, band_buffer_l1, AUDIO_BLOCK_SAMPLES);
    arm_biquad_cascade_df1_f32(&lp1_r, band_buffer_r1, band_buffer_r1, AUDIO_BLOCK_SAMPLES);
    process_band_ott(band_buffer_l1, band_buffer_r1, env1_l, env1_r, band_params[0]);

    // Band 2: Mid-Low (reuse band_buffer_l2/r2)
    memcpy(band_buffer_l2, bufL, sizeof(float32_t) * AUDIO_BLOCK_SAMPLES);
    memcpy(band_buffer_r2, bufR, sizeof(float32_t) * AUDIO_BLOCK_SAMPLES);
    arm_biquad_cascade_df1_f32(&hp1_l, band_buffer_l2, band_buffer_l2, AUDIO_BLOCK_SAMPLES);
    arm_biquad_cascade_df1_f32(&hp1_r, band_buffer_r2, band_buffer_r2, AUDIO_BLOCK_SAMPLES);
    arm_biquad_cascade_df1_f32(&lp2_l, band_buffer_l2, band_buffer_l2, AUDIO_BLOCK_SAMPLES);
    arm_biquad_cascade_df1_f32(&lp2_r, band_buffer_r2, band_buffer_r2, AUDIO_BLOCK_SAMPLES);
    process_band_ott(band_buffer_l2, band_buffer_r2, env2_l, env2_r, band_params[1]);

    // Band 3: Mid-High (reuse band_buffer_l3/r3)
    memcpy(band_buffer_l3, bufL, sizeof(float32_t) * AUDIO_BLOCK_SAMPLES);
    memcpy(band_buffer_r3, bufR, sizeof(float32_t) * AUDIO_BLOCK_SAMPLES);
    arm_biquad_cascade_df1_f32(&hp2_l, band_buffer_l3, band_buffer_l3, AUDIO_BLOCK_SAMPLES);
    arm_biquad_cascade_df1_f32(&hp2_r, band_buffer_r3, band_buffer_r3, AUDIO_BLOCK_SAMPLES);
    arm_biquad_cascade_df1_f32(&lp3_l, band_buffer_l3, band_buffer_l3, AUDIO_BLOCK_SAMPLES);
    arm_biquad_cascade_df1_f32(&lp3_r, band_buffer_r3, band_buffer_r3, AUDIO_BLOCK_SAMPLES);
    process_band_ott(band_buffer_l3, band_buffer_r3, env3_l, env3_r, band_params[2]);

    // Band 4: High (reuse band_buffer_l4/r4)
    memcpy(band_buffer_l4, bufL, sizeof(float32_t) * AUDIO_BLOCK_SAMPLES);
    memcpy(band_buffer_r4, bufR, sizeof(float32_t) * AUDIO_BLOCK_SAMPLES);
    arm_biquad_cascade_df1_f32(&hp3_l, band_buffer_l4, band_buffer_l4, AUDIO_BLOCK_SAMPLES);
    arm_biquad_cascade_df1_f32(&hp3_r, band_buffer_r4, band_buffer_r4, AUDIO_BLOCK_SAMPLES);
    process_band_ott(band_buffer_l4, band_buffer_r4, env4_l, env4_r, band_params[3]);

    // Allocate output blocks
    audio_block_t* outL = allocate();
    audio_block_t* outR = allocate();
    if (!outL || !outR) {
        release(blockL);
        release(blockR);
        if (outL) release(outL);
        if (outR) release(outR);
        return;
    }

    // Optimized mixing using pre-computed values
    const uint8_t mask = params.ui_data.solo_mask;
    const float32_t dry_gain = 1.0f - params.master_mix_lin;
    const float32_t wet_gain = params.master_mix_lin;
    const float32_t master_gain = params.master_gain_lin;
    
    // Vectorized final processing
    if (mask == 0) {
        // No solo - sum all bands (most common case)
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
            const float32_t wetL = band_buffer_l1[i] + band_buffer_l2[i] + 
                                   band_buffer_l3[i] + band_buffer_l4[i];
            const float32_t wetR = band_buffer_r1[i] + band_buffer_r2[i] + 
                                   band_buffer_r3[i] + band_buffer_r4[i];
            
            const float32_t finalL = (bufL[i] * dry_gain + wetL * wet_gain) * master_gain;
            const float32_t finalR = (bufR[i] * dry_gain + wetR * wet_gain) * master_gain;

            // Optimized saturation using ARM intrinsics
            outL->data[i] = (int16_t)__SSAT((int32_t)(finalL * 32767.0f), 16);
            outR->data[i] = (int16_t)__SSAT((int32_t)(finalR * 32767.0f), 16);
        }
    } else {
        // Solo mode - conditional summing
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
            float32_t wetL = 0.0f, wetR = 0.0f;
            
            if (mask & 0x01) { wetL += band_buffer_l1[i]; wetR += band_buffer_r1[i]; }
            if (mask & 0x02) { wetL += band_buffer_l2[i]; wetR += band_buffer_r2[i]; }
            if (mask & 0x04) { wetL += band_buffer_l3[i]; wetR += band_buffer_r3[i]; }
            if (mask & 0x08) { wetL += band_buffer_l4[i]; wetR += band_buffer_r4[i]; }

            const float32_t finalL = (bufL[i] * dry_gain + wetL * wet_gain) * master_gain;
            const float32_t finalR = (bufR[i] * dry_gain + wetR * wet_gain) * master_gain;

            outL->data[i] = (int16_t)__SSAT((int32_t)(finalL * 32767.0f), 16);
            outR->data[i] = (int16_t)__SSAT((int32_t)(finalR * 32767.0f), 16);
        }
    }

    transmit(outL, 0);
    transmit(outR, 1);

    release(outL);
    release(outR);
    release(blockL);
    release(blockR);
}