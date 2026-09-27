#pragma once

#include <Arduino.h>

constexpr uint8_t kNgpRomNameLen = 160;
constexpr uint8_t kNgpMaxRoms = 100;

uint8_t ngpScanRoms(char names[][kNgpRomNameLen]);
bool ngpLoadRom(const char *filename);
bool ngpIsLoaded();
void ngpRunFrame();
bool ngpUnload();
const char *ngpRomTitle();

enum class NgpButton : uint8_t { Up, Down, Left, Right, A, B, Select, Start };
void ngpSetButton(NgpButton button, bool pressed);

struct NgpRuntimeStats {
  uint16_t fpsX10 = 0;
  uint32_t avgCoreUs = 0;
  uint32_t maxCoreUs = 0;
  uint32_t totalFrames = 0;
};
NgpRuntimeStats ngpRuntimeStats();

// Implementee dans main.cpp : le coeur fournit un framebuffer 160x152 RGB565.
void ngpBlitFrame(const uint16_t *pixels);
void ngpBlitWaitIdle();
