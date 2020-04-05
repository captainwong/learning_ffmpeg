#include "screen_recorder_directx.h"
#include <d3d9.h>
#include <QElapsedTimer>

#pragma comment(lib, "d3d9.lib")

static IDirect3D9* g_d3d = nullptr;
static IDirect3DDevice9* g_device = nullptr;
static IDirect3DSurface9* g_surface = nullptr;

static bool captureSceen(void* data, int w, int h)
{
	if (!g_device || !g_surface) { return false; }
	g_device->GetFrontBufferData(0, g_surface);
	D3DLOCKED_RECT rc = { 0 };
	if (g_surface->LockRect(&rc, nullptr, 0) != S_OK) {
		return false;
	}
	memcpy(data, rc.pBits, w * h * 4);
	g_surface->UnlockRect();
	return true;
}

bool screen_recorder_directx::start(int outFPS, int maxCachedBgra)
{
	__super::start(outFPS, maxCachedBgra);

	outWidth_ = GetSystemMetrics(SM_CXSCREEN);
	outHeight_ = GetSystemMetrics(SM_CYSCREEN);

	g_d3d = Direct3DCreate9(D3D_SDK_VERSION);
	if (!g_d3d) { return false; }

	D3DPRESENT_PARAMETERS pa = { 0 };
	pa.Windowed = true;
	pa.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
	pa.SwapEffect = D3DSWAPEFFECT_DISCARD;
	pa.hDeviceWindow = GetDesktopWindow();
	g_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, nullptr, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pa, &g_device);
	if (!g_device) { return false; }

	// D3DFMT_A8R8G8B8 ¶ÔÓ¦FFmpegµÄ AV_PIX_FMT_BGRA
	g_device->CreateOffscreenPlainSurface(outWidth_, outHeight_, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &g_surface, nullptr);
	if (!g_surface) { return false; }

	running_ = true;
	thread_ = std::thread(&screen_recorder_directx::run, this);
	return true;
}

void screen_recorder_directx::stop()
{
	__super::stop();
	g_surface = nullptr;
	g_device = nullptr;
	g_d3d = nullptr;
}

void screen_recorder_directx::run()
{
	QElapsedTimer t;

	while (running_) {
		t.restart();
		int ms = 1000 / outFPS_;

		do
		{
			std::lock_guard<std::mutex> lg(mutex_);
			if (bgras_.size() >= maxCachedBgra_) {
				break;
			}
			bgra p;
			p.len = outWidth_ * outHeight_ * 4;
			p.data = new char[p.len];
			if (captureSceen(p.data, outWidth_, outHeight_)) {
				bgras_.push_back(p);
			}
		} while (0);

		ms -= t.restart();
		if (ms <= 0 || ms > 1000) {
			ms = 10;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(ms));
	}
}
