#define NOMINMAX
#include "cam_pixels.h"

#include <windows.h>
#include <dshow.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <vector>

// Link libs (MSVC). If you use another toolchain, link these manually.
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "strmiids.lib")

// ---- Sample Grabber definitions (qedit.h is deprecated; define minimal here) ----
#ifndef __qedit_h__
struct __declspec(uuid("{0579154A-2B53-4994-B0D0-E773148EFF85}")) ISampleGrabberCB : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE SampleCB(double SampleTime, IMediaSample* pSample) = 0;
  virtual HRESULT STDMETHODCALLTYPE BufferCB(double SampleTime, BYTE* pBuffer, long BufferLen) = 0;
};

struct __declspec(uuid("{6B652FFF-11FE-4FCE-92AD-0266B5D7C78F}")) ISampleGrabber : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE SetOneShot(BOOL OneShot) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetMediaType(const AM_MEDIA_TYPE* pType) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetConnectedMediaType(AM_MEDIA_TYPE* pType) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetBufferSamples(BOOL BufferThem) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetCurrentBuffer(long* pBufferSize, long* pBuffer) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetCurrentSample(IMediaSample** ppSample) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetCallback(ISampleGrabberCB* pCallback, long WhichMethodToCallback) = 0;
};

// CLSIDs from qedit.h
static const CLSID CLSID_SampleGrabber =
{ 0xC1F400A0,0x3F08,0x11D3,{0x9F,0x0B,0x00,0x60,0x08,0x03,0x9E,0x37} };

static const CLSID CLSID_NullRenderer =
{ 0xC1F400A4,0x3F08,0x11D3,{0x9F,0x0B,0x00,0x60,0x08,0x03,0x9E,0x37} };
#endif

static void SafeRelease(IUnknown*& p) { if (p) { p->Release(); p = nullptr; } }

static bool ieq_contains(const std::wstring& hay, const std::wstring& needle) {
  if (needle.empty()) return true;
  auto H = hay; auto N = needle;
  std::transform(H.begin(), H.end(), H.begin(), ::towlower);
  std::transform(N.begin(), N.end(), N.begin(), ::towlower);
  return H.find(N) != std::wstring::npos;
}

static CameraPixels::PixelFormat subtypeToFormat(const GUID& st, const BITMAPINFOHEADER& bih) {
  if (st == MEDIASUBTYPE_RGB32) return CameraPixels::PixelFormat::BGRA32; // actually DIB is BGRA in memory
  if (st == MEDIASUBTYPE_RGB24) return CameraPixels::PixelFormat::BGR24;
  if (st == MEDIASUBTYPE_YUY2)  return CameraPixels::PixelFormat::YUY2;

  // For NV12/I420 we can also detect by biCompression FOURCC.
  const DWORD c = bih.biCompression;
  if (c == MAKEFOURCC('N','V','1','2')) return CameraPixels::PixelFormat::NV12;
  if (c == MAKEFOURCC('I','4','2','0') || c == MAKEFOURCC('I','Y','U','V')) return CameraPixels::PixelFormat::I420;

  return CameraPixels::PixelFormat::Unknown;
}

static uint8_t clamp8(int v) { return (v < 0) ? 0 : (v > 255 ? 255 : (uint8_t)v); }

// Integer BT.601-ish YUV->RGB
static inline void yuv_to_bgr(int Y, int U, int V, uint8_t& b, uint8_t& g, uint8_t& r) {
  int C = Y - 16;
  int D = U - 128;
  int E = V - 128;
  int R = (298 * C + 409 * E + 128) >> 8;
  int G = (298 * C - 100 * D - 208 * E + 128) >> 8;
  int B = (298 * C + 516 * D + 128) >> 8;
  r = clamp8(R); g = clamp8(G); b = clamp8(B);
}

static bool convert_to_bgra(const CameraPixels::Frame& in, CameraPixels::Frame& out) {
  if (in.width <= 0 || in.height <= 0 || in.data.empty()) return false;

  out = {};
  out.format = CameraPixels::PixelFormat::BGRA32;
  out.width  = in.width;
  out.height = in.height;
  out.stride = in.width * 4;
  out.timestamp100ns = in.timestamp100ns;
  out.data.resize((size_t)out.stride * (size_t)out.height);

  const int w = in.width;
  const int h = in.height;

  if (in.format == CameraPixels::PixelFormat::BGRA32) {
    // Already BGRA: just copy (respect stride heuristically)
    if ((int)in.data.size() == out.stride * h) {
      out.data = in.data;
      return true;
    }
    // Fallback: copy row-by-row with min stride
    int inStride = in.stride > 0 ? in.stride : w * 4;
    for (int y = 0; y < h; ++y) {
      memcpy(out.data.data() + (size_t)y * out.stride,
             in.data.data()  + (size_t)y * inStride,
             (size_t)std::min(out.stride, inStride));
    }
    return true;
  }

  if (in.format == CameraPixels::PixelFormat::BGR24) {
    int inStride = in.stride > 0 ? in.stride : w * 3;
    for (int y = 0; y < h; ++y) {
      const uint8_t* src = in.data.data() + (size_t)y * inStride;
      uint8_t* dst = out.data.data() + (size_t)y * out.stride;
      for (int x = 0; x < w; ++x) {
        dst[4*x + 0] = src[3*x + 0];
        dst[4*x + 1] = src[3*x + 1];
        dst[4*x + 2] = src[3*x + 2];
        dst[4*x + 3] = 255;
      }
    }
    return true;
  }

  if (in.format == CameraPixels::PixelFormat::YUY2) {
    int inStride = in.stride > 0 ? in.stride : w * 2;
    for (int y = 0; y < h; ++y) {
      const uint8_t* s = in.data.data() + (size_t)y * inStride;
      uint8_t* d = out.data.data() + (size_t)y * out.stride;
      for (int x = 0; x < w; x += 2) {
        int Y0 = s[0], U = s[1], Y1 = s[2], V = s[3];
        uint8_t b,g,r;
        yuv_to_bgr(Y0, U, V, b,g,r);
        d[0]=b; d[1]=g; d[2]=r; d[3]=255;
        yuv_to_bgr(Y1, U, V, b,g,r);
        d[4]=b; d[5]=g; d[6]=r; d[7]=255;
        s += 4;
        d += 8;
      }
    }
    return true;
  }

  if (in.format == CameraPixels::PixelFormat::NV12) {
    // NV12: Y plane (h rows) + UV plane (h/2 rows), UV interleaved, subsampled 2x2.
    // Estimate stride if not provided: typical is width, but some drivers pad.
    int yStride = in.stride > 0 ? in.stride : w;

    // If size suggests padding, recompute stride from total bytes: size = yStride*h + yStride*(h/2)
    // => yStride ≈ size * 2 / (h * 3)
    if (in.stride <= 0) {
      int64_t sz = (int64_t)in.data.size();
      int64_t denom = (int64_t)h * 3;
      if (denom > 0) yStride = (int)((sz * 2) / denom);
      if (yStride < w) yStride = w;
    }

    const uint8_t* Yp  = in.data.data();
    const uint8_t* UVp = in.data.data() + (size_t)yStride * (size_t)h;

    for (int y = 0; y < h; ++y) {
      uint8_t* dst = out.data.data() + (size_t)y * out.stride;
      const uint8_t* yrow = Yp + (size_t)y * yStride;
      const uint8_t* uvrow = UVp + (size_t)(y/2) * yStride;
      for (int x = 0; x < w; ++x) {
        int Yv = yrow[x];
        int U  = uvrow[(x & ~1) + 0];
        int V  = uvrow[(x & ~1) + 1];
        uint8_t b,g,r;
        yuv_to_bgr(Yv, U, V, b,g,r);
        dst[4*x + 0] = b;
        dst[4*x + 1] = g;
        dst[4*x + 2] = r;
        dst[4*x + 3] = 255;
      }
    }
    return true;
  }

  return false;
}

// ---- Callback ----
class GrabberCB final : public ISampleGrabberCB {
public:
  GrabberCB() : ref_(1) {}

  // IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == __uuidof(ISampleGrabberCB)) {
      *ppv = static_cast<ISampleGrabberCB*>(this);
      AddRef();
      return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return (ULONG)++ref_; }
  ULONG STDMETHODCALLTYPE Release() override {
    auto v = --ref_;
    if (v == 0) delete this;
    return (ULONG)v;
  }

  HRESULT STDMETHODCALLTYPE BufferCB(double, BYTE*, long) override {
    // We use SampleCB mode.
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE SampleCB(double sampleTime, IMediaSample* pSample) override {
    if (!pSample) return E_POINTER;

    BYTE* ptr = nullptr;
    if (FAILED(pSample->GetPointer(&ptr)) || !ptr) return S_OK;

    const long len = pSample->GetActualDataLength();
    if (len <= 0) return S_OK;

    int64_t ts100ns = (int64_t)(sampleTime * 10000000.0);

    std::vector<uint8_t> copy((size_t)len);
    memcpy(copy.data(), ptr, (size_t)len);

    {
      std::lock_guard<std::mutex> lk(m_);
      if (stopped_) return S_OK;
      frameData_.swap(copy);
      timestamp100ns_ = ts100ns;
      counter_++;
    }
    cv_.notify_all();
    return S_OK;
  }

  void shutdown() {
    {
      std::lock_guard<std::mutex> lk(m_);
      stopped_ = true;
    }
    cv_.notify_all();
  }

  bool waitNext(std::vector<uint8_t>& outData, int64_t& outTs100ns, uint64_t& ioLastCounter, uint32_t timeoutMs) {
    std::unique_lock<std::mutex> lk(m_);
    const auto pred = [&] { return stopped_ || counter_ != ioLastCounter; };

    if (!pred()) {
      if (timeoutMs == 0) {
        cv_.wait(lk, pred);
      } else {
        if (!cv_.wait_for(lk, std::chrono::milliseconds(timeoutMs), pred)) return false; // timeout
      }
    }
    if (stopped_) return false;

    ioLastCounter = counter_;
    outData = frameData_;
    outTs100ns = timestamp100ns_;
    return true;
  }

private:
  std::atomic<long> ref_;
  std::mutex m_;
  std::condition_variable cv_;
  bool stopped_ = false;

  std::vector<uint8_t> frameData_;
  int64_t timestamp100ns_ = 0;
  uint64_t counter_ = 0;
};

struct CameraPixels::Impl {
  bool comInited = false;

  IGraphBuilder* graph = nullptr;
  ICaptureGraphBuilder2* capBuilder = nullptr;
  IMediaControl* control = nullptr;

  IBaseFilter* source = nullptr;
  IBaseFilter* grabberF = nullptr;
  IBaseFilter* nullR = nullptr;
  ISampleGrabber* grabber = nullptr;

  GrabberCB* cb = nullptr;

  int width = 0;
  int height = 0;
  CameraPixels::PixelFormat fmt = CameraPixels::PixelFormat::Unknown;

  bool obsWorkaround = false;
  uint64_t lastCounter = 0;

  std::wstring deviceName;
};

static std::wstring monikerFriendlyName(IMoniker* mon) {
  std::wstring out;
  IPropertyBag* bag = nullptr;
  if (SUCCEEDED(mon->BindToStorage(nullptr, nullptr, IID_PPV_ARGS(&bag))) && bag) {
    VARIANT v; VariantInit(&v);
    if (SUCCEEDED(bag->Read(L"FriendlyName", &v, nullptr)) && v.vt == VT_BSTR && v.bstrVal) {
      out = v.bstrVal;
    }
    VariantClear(&v);
    bag->Release();
  }
  return out;
}

std::vector<std::wstring> CameraPixels::enumerate() {
  std::vector<std::wstring> names;

  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  bool didInit = SUCCEEDED(hr);

  ICreateDevEnum* devEnum = nullptr;
  IEnumMoniker* enumMon = nullptr;

  if (SUCCEEDED(CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&devEnum))) && devEnum) {
    if (SUCCEEDED(devEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &enumMon, 0)) && enumMon) {
      IMoniker* mon = nullptr;
      ULONG fetched = 0;
      while (enumMon->Next(1, &mon, &fetched) == S_OK && mon) {
        auto name = monikerFriendlyName(mon);
        if (!name.empty()) names.push_back(name);
        mon->Release();
      }
      enumMon->Release();
    }
    devEnum->Release();
  }

  if (didInit) CoUninitialize();
  return names;
}

CameraPixels::CameraPixels() : impl_(new Impl) {}

CameraPixels::~CameraPixels() {
  close();
  delete impl_;
  impl_ = nullptr;
}

bool CameraPixels::isOpen() const {
  return impl_ && impl_->control != nullptr;
}

void CameraPixels::close() {
  if (!impl_) return;

  if (impl_->cb) impl_->cb->shutdown();

  if (impl_->control) impl_->control->Stop();

  SafeRelease((IUnknown*&)impl_->control);
  SafeRelease((IUnknown*&)impl_->capBuilder);
  SafeRelease((IUnknown*&)impl_->grabber);
  SafeRelease((IUnknown*&)impl_->nullR);
  SafeRelease((IUnknown*&)impl_->grabberF);
  SafeRelease((IUnknown*&)impl_->source);
  SafeRelease((IUnknown*&)impl_->graph);

  if (impl_->cb) { impl_->cb->Release(); impl_->cb = nullptr; }

  impl_->width = impl_->height = 0;
  impl_->fmt = PixelFormat::Unknown;
  impl_->lastCounter = 0;
  impl_->deviceName.clear();

  if (impl_->comInited) {
    CoUninitialize();
    impl_->comInited = false;
  }
}

bool CameraPixels::open(int deviceIndex) {
  lastError_.clear();
  close();

  if (!impl_) return false;

  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  if (FAILED(hr)) { lastError_ = L"CoInitializeEx failed"; return false; }
  impl_->comInited = true;

  // Enumerate to find moniker by index
  ICreateDevEnum* devEnum = nullptr;
  IEnumMoniker* enumMon = nullptr;

  hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&devEnum));
  if (FAILED(hr) || !devEnum) { lastError_ = L"CreateDevEnum failed"; return false; }

  hr = devEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &enumMon, 0);
  devEnum->Release();
  devEnum = nullptr;

  if (FAILED(hr) || !enumMon) { lastError_ = L"No video capture devices found"; return false; }

  IMoniker* chosen = nullptr;
  ULONG fetched = 0;
  int idx = 0;
  while (enumMon->Next(1, &chosen, &fetched) == S_OK && chosen) {
    if (idx == deviceIndex) break;
    chosen->Release();
    chosen = nullptr;
    idx++;
  }
  enumMon->Release();
  enumMon = nullptr;

  if (!chosen) { lastError_ = L"Device index out of range"; return false; }

  impl_->deviceName = monikerFriendlyName(chosen);
  impl_->obsWorkaround = ieq_contains(impl_->deviceName, L"OBS");

  // Bind source filter
  hr = chosen->BindToObject(nullptr, nullptr, IID_IBaseFilter, (void**)&impl_->source);
  chosen->Release();
  chosen = nullptr;
  if (FAILED(hr) || !impl_->source) { lastError_ = L"BindToObject failed"; return false; }

  // Create graph + builder
  hr = CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&impl_->graph));
  if (FAILED(hr) || !impl_->graph) { lastError_ = L"CLSID_FilterGraph failed"; return false; }

  hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&impl_->capBuilder));
  if (FAILED(hr) || !impl_->capBuilder) { lastError_ = L"CLSID_CaptureGraphBuilder2 failed"; return false; }

  impl_->capBuilder->SetFiltergraph(impl_->graph);

  // Sample grabber + null renderer
  hr = CoCreateInstance(CLSID_SampleGrabber, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&impl_->grabberF));
  if (FAILED(hr) || !impl_->grabberF) { lastError_ = L"CLSID_SampleGrabber failed (qedit.dll?)"; return false; }

  hr = impl_->grabberF->QueryInterface(__uuidof(ISampleGrabber), (void**)&impl_->grabber);
  if (FAILED(hr) || !impl_->grabber) { lastError_ = L"QueryInterface(ISampleGrabber) failed"; return false; }

  hr = CoCreateInstance(CLSID_NullRenderer, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&impl_->nullR));
  if (FAILED(hr) || !impl_->nullR) { lastError_ = L"CLSID_NullRenderer failed"; return false; }

  hr = impl_->graph->AddFilter(impl_->source, L"Source");
  if (FAILED(hr)) { lastError_ = L"AddFilter(Source) failed"; return false; }

  hr = impl_->graph->AddFilter(impl_->grabberF, L"SampleGrabber");
  if (FAILED(hr)) { lastError_ = L"AddFilter(SampleGrabber) failed"; return false; }

  hr = impl_->graph->AddFilter(impl_->nullR, L"NullRenderer");
  if (FAILED(hr)) { lastError_ = L"AddFilter(NullRenderer) failed"; return false; }

  // Configure grabber: accept ANY video type (do NOT force RGB32; OBS often outputs NV12/YUY2). :contentReference[oaicite:2]{index=2}
  AM_MEDIA_TYPE mt = {};
  mt.majortype = MEDIATYPE_Video;
  mt.subtype = GUID_NULL;          // accept anything
  mt.formattype = FORMAT_VideoInfo;
  impl_->grabber->SetMediaType(&mt);

  impl_->grabber->SetOneShot(FALSE);
  impl_->grabber->SetBufferSamples(FALSE);

  impl_->cb = new GrabberCB();
  impl_->cb->AddRef();
  // Use SampleCB (0) to avoid buffer-copy mode requirements.
  // See SetCallback semantics. :contentReference[oaicite:3]{index=3}
  impl_->grabber->SetCallback(impl_->cb, 0);

  // Connect graph
  hr = impl_->capBuilder->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, impl_->source, impl_->grabberF, impl_->nullR);
  if (FAILED(hr)) {
    // Some devices only expose PREVIEW nicely.
    hr = impl_->capBuilder->RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, impl_->source, impl_->grabberF, impl_->nullR);
    if (FAILED(hr)) {
      lastError_ = L"RenderStream failed";
      return false;
    }
  }

  // Detect negotiated media type
  AM_MEDIA_TYPE cmt = {};
  hr = impl_->grabber->GetConnectedMediaType(&cmt);
  if (SUCCEEDED(hr) && cmt.pbFormat && cmt.cbFormat >= sizeof(VIDEOINFOHEADER) && cmt.formattype == FORMAT_VideoInfo) {
    auto* vih = reinterpret_cast<VIDEOINFOHEADER*>(cmt.pbFormat);
    const BITMAPINFOHEADER& bih = vih->bmiHeader;
    impl_->width = (int)bih.biWidth;
    impl_->height = (int)std::abs(bih.biHeight);
    impl_->fmt = subtypeToFormat(cmt.subtype, bih);
  } else {
    impl_->width = impl_->height = 0;
    impl_->fmt = PixelFormat::Unknown;
  }
  if (cmt.cbFormat && cmt.pbFormat) CoTaskMemFree(cmt.pbFormat);
  if (cmt.pUnk) cmt.pUnk->Release();

  // OBS workaround: disable clock (some DirectShow graphs stall/only deliver one frame otherwise). :contentReference[oaicite:4]{index=4}
  if (impl_->obsWorkaround) {
    IMediaFilter* mf = nullptr;
    if (SUCCEEDED(impl_->graph->QueryInterface(IID_PPV_ARGS(&mf))) && mf) {
      mf->SetSyncSource(nullptr);
      mf->Release();
    }
  }

  hr = impl_->graph->QueryInterface(IID_PPV_ARGS(&impl_->control));
  if (FAILED(hr) || !impl_->control) { lastError_ = L"QueryInterface(IMediaControl) failed"; return false; }

  hr = impl_->control->Run();
  if (FAILED(hr)) { lastError_ = L"IMediaControl::Run failed"; return false; }

  // Optional: wait for running (Run can return S_FALSE while transitioning). :contentReference[oaicite:5]{index=5}
  OAFilterState st = State_Stopped;
  impl_->control->GetState(2000, &st);

  // Reset counter tracking
  impl_->lastCounter = 0;
  return true;
}

bool CameraPixels::read(Frame& out, uint32_t timeoutMs) {
  out = {};
  lastError_.clear();
  if (!impl_ || !impl_->cb) return false;

  std::vector<uint8_t> data;
  int64_t ts = 0;
  if (!impl_->cb->waitNext(data, ts, impl_->lastCounter, timeoutMs)) {
    lastError_ = L"Timeout or closed";
    return false;
  }

  out.width = impl_->width;
  out.height = impl_->height;
  out.format = impl_->fmt;
  out.timestamp100ns = ts;
  out.data = std::move(data);

  // Heuristic stride estimation from buffer size (works for packed; reasonable for NV12).
  if (out.height > 0) {
    if (out.format == PixelFormat::NV12 || out.format == PixelFormat::I420) {
      // size ≈ stride*h*3/2 => stride ≈ size*2/(h*3)
      int64_t sz = (int64_t)out.data.size();
      out.stride = (int)((sz * 2) / ((int64_t)out.height * 3));
      if (out.stride < out.width) out.stride = out.width;
    } else {
      out.stride = (int)(out.data.size() / (size_t)out.height);
    }
  }
  return true;
}

bool CameraPixels::readBGRA(Frame& outBGRA, uint32_t timeoutMs) {
  Frame tmp;
  if (!read(tmp, timeoutMs)) return false;

  if (tmp.format == PixelFormat::BGRA32) { outBGRA = std::move(tmp); return true; }

  if (!convert_to_bgra(tmp, outBGRA)) {
    lastError_ = L"Conversion to BGRA32 not supported for this format";
    return false;
  }
  return true;
}
