#pragma once
#include <map>
#include <string_view>
#include <vector>

// Ready-made camera profiles, keyed by sensor type, served from /presets and offered in
// the web UI (the operator loads one into the form, then Applies / Saves to USB).
//
// Common approach: motion blur is bounded by capping the frame duration (which caps the
// AE shutter) or pinning ExposureTime outright; the resulting darkness is recovered with
// analogue gain plus a small ISP brightness/contrast lift. The low-light profiles are
// indoor-only and will over-expose in daylight.
//
// Per-sensor quirks found on-device:
//   imx519 - the AGC parks ANALOGUE_GAIN at ~8 and never raises it, so the low-light
//            profiles pin gain at the sensor max (16) manually. 2328x1748 is the
//            2x2-binned full-array readout; 30 fps / 33 ms is the mode floor.
//   imx296 - global shutter, single 1456x1088 mode, no AF. The AGC *does* raise gain when
//            the frame is capped, so gain is left on auto. Analogue gain tops out at
//            ~15.7x on this pipeline (the sensor advertises more, but the extra is served
//            as digital gain); the ISP lift covers the rest. 60 fps / 16.6 ms mode floor.

namespace mandeye::presets
{
struct Preset
{
	std::string_view name;
	std::string_view json;
};

// --- imx519 -------------------------------------------------------------------

// Neutral reset: full auto exposure / gain / AWB, full FOV. Resolution 2328x1748 is the
// IMX519's 2x2-binned full-array readout. The only tuning is a frame-duration cap so the
// AE loop can't stretch exposure past ~1/30 s and smear the frame while the rig is moving
// (bright scenes pick a much shorter time anyway).
inline constexpr std::string_view imx519_default = R"JSON({
  "width": 2328, "height": 1748, "rateMs": 1000,
  "picamera": {
    "AeEnable": true,
    "ExposureTimeMode": 0,
    "FrameDurationLimits": [16667, 33333],
    "AnalogueGainMode": 0,
    "AwbMode": 0,
    "NoiseReductionMode": 1,
    "ExposureValue": 0.0,
    "Brightness": 0.0,
    "Contrast": 1.0,
    "Saturation": 1.0,
    "Sharpness": 1.0
  }
})JSON";

// Low light, moving camera: 2328x1748 (2x2-binned readout), AE exposure capped at ~1/30 s,
// gain pinned at max, ISP brightness/contrast lift.
inline constexpr std::string_view imx519_lowlight = R"JSON({
  "width": 2328, "height": 1748, "rateMs": 500,
  "picamera": {
    "AeEnable": true,
    "ExposureTimeMode": 0,
    "FrameDurationLimits": [16667, 33333],
    "AnalogueGainMode": 1,
    "AnalogueGain": 16.0,
    "ExposureValue": 0,
    "AeConstraintMode": 0,
    "AeMeteringMode": 0,
    "NoiseReductionMode": 1,
    "AwbMode": 0,
    "Brightness": 0.25,
    "Contrast": 1.2,
    "Saturation": 1.3,
    "Sharpness": 0.5
  }
})JSON";

// Low light, fast motion: everything pinned - fixed 8 ms shutter + max gain, no AE.
inline constexpr std::string_view imx519_lowlight_fastmotion = R"JSON({
  "width": 2328, "height": 1748, "rateMs": 500,
  "picamera": {
    "ExposureTimeMode": 1,
    "ExposureTime": 8000,
    "AnalogueGainMode": 1,
    "AnalogueGain": 16.0,
    "NoiseReductionMode": 1,
    "AwbMode": 0,
    "Brightness": 0.3,
    "Contrast": 1.25,
    "Saturation": 1.3,
    "Sharpness": 0.5
  }
})JSON";

// --- imx296 (global shutter, colour or mono, 1456x1088, no AF) ---------------

// Neutral reset: full auto exposure / gain / AWB. Frame duration capped at 30-60 fps so
// AE can't stretch the global shutter past ~1/30 s on a moving rig.
inline constexpr std::string_view imx296_default = R"JSON({
  "width": 1456, "height": 1088, "rateMs": 1000,
  "picamera": {
    "AeEnable": true,
    "ExposureTimeMode": 0,
    "AnalogueGainMode": 0,
    "FrameDurationLimits": [16667, 33333],
    "AwbMode": 0,
    "NoiseReductionMode": 1,
    "ExposureValue": 0.0,
    "Brightness": 0.0,
    "Contrast": 1.0,
    "Saturation": 1.0,
    "Sharpness": 1.0
  }
})JSON";

// Low light, moving camera: frame pinned at 1/60 s so blur stays bounded; gain stays on
// AE (the imx296 AGC rides it up to its ~15.7x analogue ceiling on its own), and an ISP
// brightness/contrast lift covers what's left. Goes noisy below ~20 lux.
inline constexpr std::string_view imx296_lowlight = R"JSON({
  "width": 1456, "height": 1088, "rateMs": 500,
  "picamera": {
    "AeEnable": true,
    "ExposureTimeMode": 0,
    "FrameDurationLimits": [16667, 16667],
    "AnalogueGainMode": 0,
    "ExposureValue": 0.3,
    "AeConstraintMode": 0,
    "AeMeteringMode": 0,
    "NoiseReductionMode": 1,
    "AwbMode": 0,
    "Brightness": 0.15,
    "Contrast": 1.1,
    "Saturation": 1.15,
    "Sharpness": 0.7
  }
})JSON";

// Low light, fast motion: everything pinned - fixed 8 ms shutter + analogue gain at the
// ~15.7x ceiling (no forced digital gain), no AE. Constant exposure frame to frame (good
// for offline colouring); noisiest, and dark below ~20 lux - raise Brightness or
// ExposureTime if the scene needs it.
inline constexpr std::string_view imx296_lowlight_fastmotion = R"JSON({
  "width": 1456, "height": 1088, "rateMs": 500,
  "picamera": {
    "ExposureTimeMode": 1,
    "ExposureTime": 8000,
    "AnalogueGainMode": 1,
    "AnalogueGain": 16.0,
    "NoiseReductionMode": 1,
    "AwbMode": 0,
    "Brightness": 0.35,
    "Contrast": 1.2,
    "Saturation": 1.2,
    "Sharpness": 0.6
  }
})JSON";

// ----------------------------------------------------------------------------
// Key is matched as a substring of libcamera's camera id (e.g. the id
// ".../imx519@1a" matches "imx519"). Add a new entry to support another sensor.
inline const std::map<std::string_view, std::vector<Preset>> kPresetsByCamera = {
	{"imx519",
	 {
		 {"default", imx519_default},
		 {"lowlight", imx519_lowlight},
		 {"lowlight_fastmotion", imx519_lowlight_fastmotion},
	 }},
	{"imx296",
	 {
		 {"default", imx296_default},
		 {"lowlight", imx296_lowlight},
		 {"lowlight_fastmotion", imx296_lowlight_fastmotion},
	 }},
};
} // namespace mandeye::presets