#!/usr/bin/env python3
"""Static guard for the two AZ-2 firmwares' shared GB audio V1 contract.

This complements, but does not replace, PlatformIO builds and hardware tests.
Run: python tools/check_firmware_contract.py
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
protocol = (ROOT / "lib/AZ2_Protocol/AZ2_Protocol.h").read_text(encoding="utf-8")
ini = (ROOT / "platformio.ini").read_text(encoding="utf-8")
gb = (ROOT / "src_esp32/az2_screen/gb_emulator.cpp").read_text(encoding="utf-8")
teensy = (ROOT / "src_teensy/az2_audio/main.cpp").read_text(encoding="utf-8")
screen = (ROOT / "src_esp32/az2_screen/main.cpp").read_text(encoding="utf-8")

def require(condition: bool, detail: str) -> None:
    if not condition:
        raise AssertionError(detail)

def value(pattern: str, source: str, detail: str) -> int:
    match = re.search(pattern, source, re.MULTILINE)
    require(match is not None, "Missing " + detail)
    return int(match.group(1))

sample_rate = value(r"^constexpr\s+uint32_t\s+kGbAudioSampleRate\s*=\s*(\d+)\s*;", protocol, "shared sample rate")
build_rate = value(r"^\s*-D\s+AUDIO_SAMPLE_RATE=(\d+)\s*$", ini, "ESP32 audio build flag")
baud = value(r"^constexpr\s+uint32_t\s+kControlBaud\s*=\s*(\d+)\s*;", protocol, "shared UART baud")
require(sample_rate == build_rate, f"GB sample-rate mismatch: protocol={sample_rate} ESP32={build_rate}")
require(0 < sample_rate // 60 <= 255 and sample_rate < 15200,
        "V1 audio packet requires a one-byte length; use coordinated V2 for higher rates")
require(baud == 921600, "UART baud changed: verify BOTH firmware endpoints and throughput")
require("static_assert(AUDIO_SAMPLE_RATE == az2::kGbAudioSampleRate" in gb,
        "Missing C++ compile-time assertion for ESP32↔Teensy rate")
require("static_assert(AUDIO_SAMPLES == az2::kGbAudioSamplesPerPacket" in gb,
        "Missing C++ compile-time assertion for ESP32↔Teensy payload size")
for name, source in [("Teensy", teensy), ("ESP32 screen", screen)]:
    require("#include <AZ2_Protocol.h>" in source, name + " does not import shared protocol")
    require("az2::kControlBaud" in source, name + " does not use shared baud")
require("az2::kGbAudioSamplesPerPacket" in teensy, "Teensy does not validate GB audio packet size")
require("[env:master_teensy]" in ini and "[env:screen_esp]" in ini,
        "Both firmware PlatformIO environments must exist")
require("default_envs = master_teensy, screen_esp" in ini,
        "Default build must compile both linked firmwares")
print(f"PASS: linked firmware V1 contract — {sample_rate} Hz, UART {baud} baud, dual PlatformIO targets")
