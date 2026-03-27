#pragma once

#ifdef OBS_AMD_LITE

#include <QString>
#include <cstdint>

/* ============================================================
 * AMD GPU Detection & Monitoring for OBS Lite AMD Edition
 * Detects RDNA generation, provides optimal encoder defaults,
 * and queries real-time GPU telemetry via DXGI/D3DKMT.
 * ============================================================ */

enum class AMDGeneration {
	Unknown,
	Polaris,  /* RX 400/500 — VCE 3.x */
	Vega,     /* Vega 56/64 — VCE 4.0 */
	RDNA1,    /* RX 5000 — VCN 2.0 */
	RDNA2,    /* RX 6000 — VCN 3.0 */
	RDNA3,    /* RX 7000 — VCN 4.0 */
	RDNA4,    /* RX 9000 — VCN 5.0 */
};

struct AMDGPUInfo {
	bool detected = false;
	QString name;
	uint32_t vendorId = 0;
	uint32_t deviceId = 0;
	uint64_t dedicatedVRAM = 0; /* bytes */
	AMDGeneration generation = AMDGeneration::Unknown;

	/* Optimal encoder defaults for this GPU */
	const char *defaultEncoder;     /* SIMPLE_ENCODER_* constant */
	const char *defaultRecEncoder;  /* SIMPLE_ENCODER_* for recording */
	int defaultCQP;
	const char *defaultPreset;
	bool supportsAV1;
	bool supportsHEVC;
};

struct AMDGPUStats {
	double gpuUsage = 0;    /* 0-100% */
	double vramUsedMB = 0;
	double vramTotalMB = 0;
	double temperatureC = 0; /* 0 if unavailable */
	bool valid = false;
};

/* Detect AMD GPU on startup — call once */
AMDGPUInfo DetectAMDGPU();

/* Query real-time GPU stats — call on timer */
AMDGPUStats QueryAMDGPUStats();

/* Get a human-readable generation name */
const char *GetGenerationName(AMDGeneration gen);

#endif /* OBS_AMD_LITE */
