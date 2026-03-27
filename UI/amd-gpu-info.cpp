#ifdef OBS_AMD_LITE

#include "amd-gpu-info.hpp"
#include <dxgi1_4.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <util/base.h>

#include "window-basic-main.hpp"

using Microsoft::WRL::ComPtr;

/* ============================================================
 * RDNA Generation Detection by PCI Device ID
 * Source: AMD public PCI ID database + Mesa driver radeonsi
 * ============================================================ */

static AMDGeneration ClassifyAMDGPU(uint32_t deviceId)
{
	/* RDNA 4 — Navi 48/44 (RX 9070, 9070 XT, etc.) */
	if ((deviceId >= 0x7540 && deviceId <= 0x755F) ||
	    (deviceId >= 0x7580 && deviceId <= 0x759F))
		return AMDGeneration::RDNA4;

	/* RDNA 3 — Navi 31/32/33 (RX 7900, 7800, 7700, 7600, etc.) */
	if ((deviceId >= 0x7440 && deviceId <= 0x747F) || /* Navi 31 */
	    (deviceId >= 0x7480 && deviceId <= 0x74BF) || /* Navi 32 */
	    (deviceId >= 0x7400 && deviceId <= 0x743F))   /* Navi 33 */
		return AMDGeneration::RDNA3;

	/* RDNA 2 — Navi 21/22/23/24 (RX 6000 series) */
	if ((deviceId >= 0x73A0 && deviceId <= 0x73BF) || /* Navi 21 */
	    (deviceId >= 0x73C0 && deviceId <= 0x73DF) || /* Navi 22 */
	    (deviceId >= 0x73E0 && deviceId <= 0x73FF) || /* Navi 23 */
	    (deviceId >= 0x7420 && deviceId <= 0x743F))   /* Navi 24 */
		return AMDGeneration::RDNA2;

	/* RDNA 1 — Navi 10/12/14 (RX 5000 series) */
	if ((deviceId >= 0x7310 && deviceId <= 0x731F) || /* Navi 10 */
	    (deviceId >= 0x7340 && deviceId <= 0x734F) || /* Navi 14 */
	    (deviceId >= 0x7360 && deviceId <= 0x736F))   /* Navi 12 */
		return AMDGeneration::RDNA1;

	/* Vega — Vega 10/12/20 */
	if ((deviceId >= 0x6860 && deviceId <= 0x689F) || /* Vega 10 */
	    (deviceId >= 0x69A0 && deviceId <= 0x69BF) || /* Vega 12 */
	    (deviceId >= 0x66A0 && deviceId <= 0x66BF))   /* Vega 20 */
		return AMDGeneration::Vega;

	/* Polaris — RX 400/500 */
	if ((deviceId >= 0x67C0 && deviceId <= 0x67FF) || /* Polaris 10 */
	    (deviceId >= 0x6980 && deviceId <= 0x69BF) || /* Polaris 11 */
	    (deviceId >= 0x6940 && deviceId <= 0x695F))   /* Polaris 12 */
		return AMDGeneration::Polaris;

	return AMDGeneration::Unknown;
}

const char *GetGenerationName(AMDGeneration gen)
{
	switch (gen) {
	case AMDGeneration::RDNA4:   return "RDNA 4 (VCN 5.0)";
	case AMDGeneration::RDNA3:   return "RDNA 3 (VCN 4.0)";
	case AMDGeneration::RDNA2:   return "RDNA 2 (VCN 3.0)";
	case AMDGeneration::RDNA1:   return "RDNA 1 (VCN 2.0)";
	case AMDGeneration::Vega:    return "Vega (VCE 4.0)";
	case AMDGeneration::Polaris: return "Polaris (VCE 3.x)";
	default:                     return "Unknown";
	}
}

AMDGPUInfo DetectAMDGPU()
{
	AMDGPUInfo info;

	ComPtr<IDXGIFactory1> factory;
	if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void **)&factory)))
		return info;

	ComPtr<IDXGIAdapter1> adapter;
	for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++) {
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		if (desc.VendorId != 0x1002) {
			adapter.Reset();
			continue;
		}

		/* Found AMD GPU */
		info.detected = true;
		info.vendorId = desc.VendorId;
		info.deviceId = desc.DeviceId;
		info.dedicatedVRAM = desc.DedicatedVideoMemory;
		info.name = QString::fromWCharArray(desc.Description);
		info.generation = ClassifyAMDGPU(desc.DeviceId);

		/* Set optimal defaults based on generation */
		switch (info.generation) {
		case AMDGeneration::RDNA4:
			info.defaultEncoder = SIMPLE_ENCODER_AMD_AV1;
			info.defaultRecEncoder = SIMPLE_ENCODER_AMD_AV1;
			info.defaultCQP = 18;
			info.defaultPreset = "speed";
			info.supportsAV1 = true;
			info.supportsHEVC = true;
			break;
		case AMDGeneration::RDNA3:
			info.defaultEncoder = SIMPLE_ENCODER_AMD_HEVC;
			info.defaultRecEncoder = SIMPLE_ENCODER_AMD_HEVC;
			info.defaultCQP = 20;
			info.defaultPreset = "speed";
			info.supportsAV1 = true;
			info.supportsHEVC = true;
			break;
		case AMDGeneration::RDNA2:
			info.defaultEncoder = SIMPLE_ENCODER_AMD_HEVC;
			info.defaultRecEncoder = SIMPLE_ENCODER_AMD_HEVC;
			info.defaultCQP = 20;
			info.defaultPreset = "balanced";
			info.supportsAV1 = false;
			info.supportsHEVC = true;
			break;
		case AMDGeneration::RDNA1:
			info.defaultEncoder = SIMPLE_ENCODER_AMD;
			info.defaultRecEncoder = SIMPLE_ENCODER_AMD;
			info.defaultCQP = 22;
			info.defaultPreset = "speed";
			info.supportsAV1 = false;
			info.supportsHEVC = true;
			break;
		default: /* Vega, Polaris, Unknown */
			info.defaultEncoder = SIMPLE_ENCODER_AMD;
			info.defaultRecEncoder = SIMPLE_ENCODER_AMD;
			info.defaultCQP = 23;
			info.defaultPreset = "speed";
			info.supportsAV1 = false;
			info.supportsHEVC = false;
			break;
		}

		blog(LOG_INFO, "OBS Lite AMD: Detected %s (DeviceID: 0x%04X, Gen: %s, VRAM: %.1f GB)",
		     info.name.toUtf8().constData(), info.deviceId, GetGenerationName(info.generation),
		     info.dedicatedVRAM / (1024.0 * 1024.0 * 1024.0));

		adapter.Reset();
		break; /* Use first AMD GPU found */
	}

	return info;
}

/* ============================================================
 * GPU Stats via D3DKMT (Kernel Mode Thunks)
 * Works on all Windows 10+ without external SDK
 * ============================================================ */

/* D3DKMT structures for GPU usage query */
typedef UINT D3DKMT_HANDLE;

typedef struct _D3DKMT_QUERYSTATISTICS_COUNTER {
	ULONG Count;
	ULONGLONG Bytes;
} D3DKMT_QUERYSTATISTICS_COUNTER;

AMDGPUStats QueryAMDGPUStats()
{
	AMDGPUStats stats;

	/* Query GPU memory via DXGI */
	ComPtr<IDXGIFactory1> factory;
	if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void **)&factory)))
		return stats;

	ComPtr<IDXGIAdapter1> adapter;
	for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++) {
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		if (desc.VendorId != 0x1002) {
			adapter.Reset();
			continue;
		}

		stats.vramTotalMB = desc.DedicatedVideoMemory / (1024.0 * 1024.0);

		/* Try DXGI 1.4 for current VRAM usage */
		ComPtr<IDXGIAdapter3> adapter3;
		if (SUCCEEDED(adapter.As(&adapter3))) {
			DXGI_QUERY_VIDEO_MEMORY_INFO memInfo;
			if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo))) {
				stats.vramUsedMB = memInfo.CurrentUsage / (1024.0 * 1024.0);
			}
		}

		stats.valid = true;
		adapter.Reset();
		break;
	}

	return stats;
}

#endif /* OBS_AMD_LITE */
