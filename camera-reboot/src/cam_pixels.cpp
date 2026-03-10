#define NOMINMAX
#include "cam_pixels.h"

#include <windows.h>
#include <dshow.h>
#include <amvideo.h>
#include <wincodec.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <vector>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "windowscodecs.lib")

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

static const CLSID CLSID_SampleGrabber =
{ 0xC1F400A0,0x3F08,0x11D3,{0x9F,0x0B,0x00,0x60,0x08,0x03,0x9E,0x37} };

static const CLSID CLSID_NullRenderer =
{ 0xC1F400A4,0x3F08,0x11D3,{0x9F,0x0B,0x00,0x60,0x08,0x03,0x9E,0x37} };
#endif

template <typename T>
static void SafeRelease(T*& p) {
  if (p) {
    p->Release();
    p = nullptr;
  }
}

static void FreeMediaType(AM_MEDIA_TYPE& mt) {
  if (mt.cbFormat && mt.pbFormat) CoTaskMemFree(mt.pbFormat);
  mt.cbFormat = 0;
  mt.pbFormat = nullptr;
  if (mt.pUnk) mt.pUnk->Release();
  mt.pUnk = nullptr;
}

static bool ieq_contains(const std::wstring& hay, const std::wstring& needle) {
  if (needle.empty()) return true;
  auto H = hay;
  auto N = needle;
  std::transform(H.begin(), H.end(), H.begin(), ::towlower);
  std::transform(N.begin(), N.end(), N.begin(), ::towlower);
  return H.find(N) != std::wstring::npos;
}

static bool guidIsVideoFourccSubtype(const GUID& guid, DWORD fourcc) {
  static const BYTE kVideoSubtypeTail[8] = { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 };
  return guid.Data1 == fourcc && guid.Data2 == 0x0000 && guid.Data3 == 0x0010 &&
         std::memcmp(guid.Data4, kVideoSubtypeTail, sizeof(kVideoSubtypeTail)) == 0;
}

struct VideoFormatInfo {
  BITMAPINFOHEADER bih{};
  int width = 0;
  int height = 0;
};

struct VideoInfoHeader2Compat {
  RECT rcSource;
  RECT rcTarget;
  DWORD dwBitRate;
  DWORD dwBitErrorRate;
  REFERENCE_TIME AvgTimePerFrame;
  DWORD dwInterlaceFlags;
  DWORD dwCopyProtectFlags;
  DWORD dwPictAspectRatioX;
  DWORD dwPictAspectRatioY;
  union {
    DWORD dwControlFlags;
    DWORD dwReserved2;
  };
  BITMAPINFOHEADER bmiHeader;
};
static bool extractVideoFormatInfo(const AM_MEDIA_TYPE& mt, VideoFormatInfo& out) {
  if (!mt.pbFormat) return false;

  if (mt.formattype == FORMAT_VideoInfo && mt.cbFormat >= sizeof(VIDEOINFOHEADER)) {
    const auto* vih = reinterpret_cast<const VIDEOINFOHEADER*>(mt.pbFormat);
    out.bih = vih->bmiHeader;
  } else if (mt.formattype == FORMAT_VideoInfo2 && mt.cbFormat >= sizeof(VideoInfoHeader2Compat)) {
    const auto* vih2 = reinterpret_cast<const VideoInfoHeader2Compat*>(mt.pbFormat);
    out.bih = vih2->bmiHeader;
  } else {
    return false;
  }

  out.width = (int)out.bih.biWidth;
  out.height = out.bih.biHeight < 0 ? -(int)out.bih.biHeight : (int)out.bih.biHeight;
  return out.width > 0 && out.height > 0;
}

static CameraPixels::PixelFormat subtypeToFormat(const GUID& st, const BITMAPINFOHEADER& bih) {
  if (st == MEDIASUBTYPE_RGB32) return CameraPixels::PixelFormat::BGRA32;
  if (st == MEDIASUBTYPE_RGB24) return CameraPixels::PixelFormat::BGR24;

  const DWORD c = bih.biCompression;

  if (st == MEDIASUBTYPE_YUY2 || guidIsVideoFourccSubtype(st, MAKEFOURCC('Y','U','Y','2')) || c == MAKEFOURCC('Y','U','Y','2')) {
    return CameraPixels::PixelFormat::YUY2;
  }
  if (guidIsVideoFourccSubtype(st, MAKEFOURCC('U','Y','V','Y')) || c == MAKEFOURCC('U','Y','V','Y')) {
    return CameraPixels::PixelFormat::UYVY;
  }
  if (guidIsVideoFourccSubtype(st, MAKEFOURCC('N','V','1','2')) || c == MAKEFOURCC('N','V','1','2')) {
    return CameraPixels::PixelFormat::NV12;
  }
  if (guidIsVideoFourccSubtype(st, MAKEFOURCC('I','4','2','0')) ||
      guidIsVideoFourccSubtype(st, MAKEFOURCC('I','Y','U','V')) ||
      c == MAKEFOURCC('I','4','2','0') || c == MAKEFOURCC('I','Y','U','V')) {
    return CameraPixels::PixelFormat::I420;
  }
  if (guidIsVideoFourccSubtype(st, MAKEFOURCC('Y','V','1','2')) || c == MAKEFOURCC('Y','V','1','2')) {
    return CameraPixels::PixelFormat::YV12;
  }
  if (guidIsVideoFourccSubtype(st, MAKEFOURCC('M','J','P','G')) || c == MAKEFOURCC('M','J','P','G') || c == BI_JPEG) {
    return CameraPixels::PixelFormat::MJPG;
  }

  return CameraPixels::PixelFormat::Unknown;
}

static const wchar_t* pixelFormatName(CameraPixels::PixelFormat fmt) {
  switch (fmt) {
    case CameraPixels::PixelFormat::BGRA32: return L"BGRA32";
    case CameraPixels::PixelFormat::BGR24:  return L"BGR24";
    case CameraPixels::PixelFormat::YUY2:   return L"YUY2";
    case CameraPixels::PixelFormat::UYVY:   return L"UYVY";
    case CameraPixels::PixelFormat::NV12:   return L"NV12";
    case CameraPixels::PixelFormat::I420:   return L"I420";
    case CameraPixels::PixelFormat::YV12:   return L"YV12";
    case CameraPixels::PixelFormat::MJPG:   return L"MJPG";
    default:                                return L"Unknown";
  }
}

static uint8_t clamp8(int v) {
  return (v < 0) ? 0 : (v > 255 ? 255 : (uint8_t)v);
}

static inline void yuv_to_bgr(int Y, int U, int V, uint8_t& b, uint8_t& g, uint8_t& r) {
  int C = Y - 16;
  int D = U - 128;
  int E = V - 128;
  int R = (298 * C + 409 * E + 128) >> 8;
  int G = (298 * C - 100 * D - 208 * E + 128) >> 8;
  int B = (298 * C + 516 * D + 128) >> 8;
  r = clamp8(R);
  g = clamp8(G);
  b = clamp8(B);
}

static bool decode_mjpg_to_bgra(const CameraPixels::Frame& in, CameraPixels::Frame& out) {
  IWICImagingFactory* factory = nullptr;
  IWICStream* stream = nullptr;
  IWICBitmapDecoder* decoder = nullptr;
  IWICBitmapFrameDecode* frame = nullptr;
  IWICFormatConverter* converter = nullptr;
  bool ok = false;

  HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
  if (FAILED(hr) || !factory) goto cleanup;

  hr = factory->CreateStream(&stream);
  if (FAILED(hr) || !stream) goto cleanup;

  hr = stream->InitializeFromMemory(const_cast<BYTE*>(in.data.data()), (DWORD)in.data.size());
  if (FAILED(hr)) goto cleanup;

  hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
  if (FAILED(hr) || !decoder) goto cleanup;

  hr = decoder->GetFrame(0, &frame);
  if (FAILED(hr) || !frame) goto cleanup;

  UINT w = 0;
  UINT h = 0;
  hr = frame->GetSize(&w, &h);
  if (FAILED(hr) || w == 0 || h == 0) goto cleanup;

  hr = factory->CreateFormatConverter(&converter);
  if (FAILED(hr) || !converter) goto cleanup;

  hr = converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
  if (FAILED(hr)) goto cleanup;

  out = {};
  out.format = CameraPixels::PixelFormat::BGRA32;
  out.width = (int)w;
  out.height = (int)h;
  out.stride = (int)w * 4;
  out.timestamp100ns = in.timestamp100ns;
  out.data.resize((size_t)out.stride * (size_t)out.height);

  hr = converter->CopyPixels(nullptr, (UINT)out.stride, (UINT)out.data.size(), out.data.data());
  if (FAILED(hr)) {
    out = {};
    goto cleanup;
  }

  ok = true;

cleanup:
  SafeRelease(converter);
  SafeRelease(frame);
  SafeRelease(decoder);
  SafeRelease(stream);
  SafeRelease(factory);
  return ok;
}

static bool convert_to_bgra(const CameraPixels::Frame& in, CameraPixels::Frame& out) {
  if (in.width <= 0 || in.height <= 0 || in.data.empty()) return false;

  if (in.format == CameraPixels::PixelFormat::MJPG) {
    return decode_mjpg_to_bgra(in, out);
  }

  out = {};
  out.format = CameraPixels::PixelFormat::BGRA32;
  out.width = in.width;
  out.height = in.height;
  out.stride = in.width * 4;
  out.timestamp100ns = in.timestamp100ns;
  out.data.resize((size_t)out.stride * (size_t)out.height);

  const int w = in.width;
  const int h = in.height;

  if (in.format == CameraPixels::PixelFormat::BGRA32) {
    if ((int)in.data.size() == out.stride * h) {
      out.data = in.data;
      return true;
    }

    int inStride = in.stride > 0 ? in.stride : w * 4;
    for (int y = 0; y < h; ++y) {
      std::memcpy(out.data.data() + (size_t)y * out.stride,
                  in.data.data() + (size_t)y * inStride,
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
        dst[4 * x + 0] = src[3 * x + 0];
        dst[4 * x + 1] = src[3 * x + 1];
        dst[4 * x + 2] = src[3 * x + 2];
        dst[4 * x + 3] = 255;
      }
    }
    return true;
  }

  if (in.format == CameraPixels::PixelFormat::YUY2 || in.format == CameraPixels::PixelFormat::UYVY) {
    int inStride = in.stride > 0 ? in.stride : w * 2;
    for (int y = 0; y < h; ++y) {
      const uint8_t* s = in.data.data() + (size_t)y * inStride;
      uint8_t* d = out.data.data() + (size_t)y * out.stride;
      for (int x = 0; x < w; x += 2) {
        int Y0 = 0;
        int U = 0;
        int Y1 = 0;
        int V = 0;
        if (in.format == CameraPixels::PixelFormat::YUY2) {
          Y0 = s[0];
          U = s[1];
          Y1 = s[2];
          V = s[3];
        } else {
          U = s[0];
          Y0 = s[1];
          V = s[2];
          Y1 = s[3];
        }

        uint8_t b = 0, g = 0, r = 0;
        yuv_to_bgr(Y0, U, V, b, g, r);
        d[0] = b; d[1] = g; d[2] = r; d[3] = 255;
        if (x + 1 < w) {
          yuv_to_bgr(Y1, U, V, b, g, r);
          d[4] = b; d[5] = g; d[6] = r; d[7] = 255;
        }

        s += 4;
        d += 8;
      }
    }
    return true;
  }

  if (in.format == CameraPixels::PixelFormat::NV12) {
    int yStride = in.stride > 0 ? in.stride : w;
    if (in.stride <= 0) {
      int64_t sz = (int64_t)in.data.size();
      int64_t denom = (int64_t)h * 3;
      if (denom > 0) yStride = (int)((sz * 2) / denom);
      if (yStride < w) yStride = w;
    }

    const size_t yPlaneBytes = (size_t)yStride * (size_t)h;
    if (yPlaneBytes >= in.data.size()) return false;

    const uint8_t* Yp = in.data.data();
    const uint8_t* UVp = in.data.data() + yPlaneBytes;

    for (int y = 0; y < h; ++y) {
      uint8_t* dst = out.data.data() + (size_t)y * out.stride;
      const uint8_t* yrow = Yp + (size_t)y * yStride;
      const uint8_t* uvrow = UVp + (size_t)(y / 2) * yStride;
      for (int x = 0; x < w; ++x) {
        int Yv = yrow[x];
        int U = uvrow[(x & ~1) + 0];
        int V = uvrow[(x & ~1) + 1];
        uint8_t b = 0, g = 0, r = 0;
        yuv_to_bgr(Yv, U, V, b, g, r);
        dst[4 * x + 0] = b;
        dst[4 * x + 1] = g;
        dst[4 * x + 2] = r;
        dst[4 * x + 3] = 255;
      }
    }
    return true;
  }

  if (in.format == CameraPixels::PixelFormat::I420 || in.format == CameraPixels::PixelFormat::YV12) {
    int yStride = in.stride > 0 ? in.stride : w;
    if (in.stride <= 0) {
      int64_t sz = (int64_t)in.data.size();
      int64_t denom = (int64_t)h * 3;
      if (denom > 0) yStride = (int)((sz * 2) / denom);
      if (yStride < w) yStride = w;
    }

    const int chromaHeight = (h + 1) / 2;
    const int uvStride = (yStride + 1) / 2;
    const size_t yPlaneBytes = (size_t)yStride * (size_t)h;
    const size_t chromaPlaneBytes = (size_t)uvStride * (size_t)chromaHeight;
    if (yPlaneBytes + chromaPlaneBytes * 2 > in.data.size()) return false;

    const uint8_t* Yp = in.data.data();
    const uint8_t* plane1 = in.data.data() + yPlaneBytes;
    const uint8_t* plane2 = plane1 + chromaPlaneBytes;
    const uint8_t* Up = (in.format == CameraPixels::PixelFormat::I420) ? plane1 : plane2;
    const uint8_t* Vp = (in.format == CameraPixels::PixelFormat::I420) ? plane2 : plane1;

    for (int y = 0; y < h; ++y) {
      uint8_t* dst = out.data.data() + (size_t)y * out.stride;
      const uint8_t* yrow = Yp + (size_t)y * yStride;
      const uint8_t* urow = Up + (size_t)(y / 2) * uvStride;
      const uint8_t* vrow = Vp + (size_t)(y / 2) * uvStride;
      for (int x = 0; x < w; ++x) {
        int Yv = yrow[x];
        int U = urow[x / 2];
        int V = vrow[x / 2];
        uint8_t b = 0, g = 0, r = 0;
        yuv_to_bgr(Yv, U, V, b, g, r);
        dst[4 * x + 0] = b;
        dst[4 * x + 1] = g;
        dst[4 * x + 2] = r;
        dst[4 * x + 3] = 255;
      }
    }
    return true;
  }

  return false;
}

class GrabberCB final : public ISampleGrabberCB {
public:
  GrabberCB() : ref_(1) {}

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
    std::memcpy(copy.data(), ptr, (size_t)len);

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
      } else if (!cv_.wait_for(lk, std::chrono::milliseconds(timeoutMs), pred)) {
        return false;
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
    VARIANT v;
    VariantInit(&v);
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

  SafeRelease(impl_->control);
  SafeRelease(impl_->capBuilder);
  SafeRelease(impl_->grabber);
  SafeRelease(impl_->nullR);
  SafeRelease(impl_->grabberF);
  SafeRelease(impl_->source);
  SafeRelease(impl_->graph);

  if (impl_->cb) {
    impl_->cb->Release();
    impl_->cb = nullptr;
  }

  impl_->width = 0;
  impl_->height = 0;
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
  if (FAILED(hr)) {
    lastError_ = L"CoInitializeEx failed";
    return false;
  }
  impl_->comInited = true;

  ICreateDevEnum* devEnum = nullptr;
  IEnumMoniker* enumMon = nullptr;

  hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&devEnum));
  if (FAILED(hr) || !devEnum) {
    lastError_ = L"CreateDevEnum failed";
    close();
    return false;
  }

  hr = devEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &enumMon, 0);
  devEnum->Release();
  devEnum = nullptr;

  if (FAILED(hr) || !enumMon) {
    lastError_ = L"No video capture devices found";
    close();
    return false;
  }

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

  if (!chosen) {
    lastError_ = L"Device index out of range";
    close();
    return false;
  }

  impl_->deviceName = monikerFriendlyName(chosen);
  impl_->obsWorkaround = ieq_contains(impl_->deviceName, L"OBS");

  struct MediaRequest {
    GUID subtype;
    const wchar_t* label;
  };
  const MediaRequest requests[] = {
    { MEDIASUBTYPE_RGB32, L"RGB32" },
    { MEDIASUBTYPE_RGB24, L"RGB24" },
    { GUID_NULL,          L"native" }
  };

  auto cleanupAttempt = [&]() {
    if (impl_->cb) impl_->cb->shutdown();
    if (impl_->control) impl_->control->Stop();

    SafeRelease(impl_->control);
    SafeRelease(impl_->capBuilder);
    SafeRelease(impl_->grabber);
    SafeRelease(impl_->nullR);
    SafeRelease(impl_->grabberF);
    SafeRelease(impl_->source);
    SafeRelease(impl_->graph);

    if (impl_->cb) {
      impl_->cb->Release();
      impl_->cb = nullptr;
    }

    impl_->width = 0;
    impl_->height = 0;
    impl_->fmt = PixelFormat::Unknown;
    impl_->lastCounter = 0;
  };

  std::wstring failureSummary;

  for (const auto& request : requests) {
    cleanupAttempt();

    hr = chosen->BindToObject(nullptr, nullptr, IID_IBaseFilter, (void**)&impl_->source);
    if (FAILED(hr) || !impl_->source) {
      failureSummary = L"BindToObject failed";
      continue;
    }

    hr = CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&impl_->graph));
    if (FAILED(hr) || !impl_->graph) {
      failureSummary = L"CLSID_FilterGraph failed";
      continue;
    }

    hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&impl_->capBuilder));
    if (FAILED(hr) || !impl_->capBuilder) {
      failureSummary = L"CLSID_CaptureGraphBuilder2 failed";
      continue;
    }

    impl_->capBuilder->SetFiltergraph(impl_->graph);

    hr = CoCreateInstance(CLSID_SampleGrabber, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&impl_->grabberF));
    if (FAILED(hr) || !impl_->grabberF) {
      failureSummary = L"CLSID_SampleGrabber failed (qedit.dll?)";
      continue;
    }

    hr = impl_->grabberF->QueryInterface(__uuidof(ISampleGrabber), (void**)&impl_->grabber);
    if (FAILED(hr) || !impl_->grabber) {
      failureSummary = L"QueryInterface(ISampleGrabber) failed";
      continue;
    }

    hr = CoCreateInstance(CLSID_NullRenderer, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&impl_->nullR));
    if (FAILED(hr) || !impl_->nullR) {
      failureSummary = L"CLSID_NullRenderer failed";
      continue;
    }

    hr = impl_->graph->AddFilter(impl_->source, L"Source");
    if (FAILED(hr)) {
      failureSummary = L"AddFilter(Source) failed";
      continue;
    }

    hr = impl_->graph->AddFilter(impl_->grabberF, L"SampleGrabber");
    if (FAILED(hr)) {
      failureSummary = L"AddFilter(SampleGrabber) failed";
      continue;
    }

    hr = impl_->graph->AddFilter(impl_->nullR, L"NullRenderer");
    if (FAILED(hr)) {
      failureSummary = L"AddFilter(NullRenderer) failed";
      continue;
    }

    AM_MEDIA_TYPE mt = {};
    mt.majortype = MEDIATYPE_Video;
    mt.subtype = request.subtype;
    mt.formattype = GUID_NULL;
    hr = impl_->grabber->SetMediaType(&mt);
    if (FAILED(hr)) {
      failureSummary = L"SetMediaType failed";
      continue;
    }

    impl_->grabber->SetOneShot(FALSE);
    impl_->grabber->SetBufferSamples(FALSE);

    impl_->cb = new GrabberCB();
    impl_->cb->AddRef();
    impl_->grabber->SetCallback(impl_->cb, 0);

    hr = impl_->capBuilder->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, impl_->source, impl_->grabberF, impl_->nullR);
    if (FAILED(hr)) {
      hr = impl_->capBuilder->RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, impl_->source, impl_->grabberF, impl_->nullR);
    }
    if (FAILED(hr)) {
      failureSummary = std::wstring(L"RenderStream failed for ") + request.label;
      continue;
    }

    AM_MEDIA_TYPE cmt = {};
    hr = impl_->grabber->GetConnectedMediaType(&cmt);
    if (FAILED(hr)) {
      failureSummary = std::wstring(L"GetConnectedMediaType failed for ") + request.label;
      continue;
    }

    VideoFormatInfo vfi;
    if (extractVideoFormatInfo(cmt, vfi)) {
      impl_->width = vfi.width;
      impl_->height = vfi.height;
      impl_->fmt = subtypeToFormat(cmt.subtype, vfi.bih);
    } else {
      impl_->width = 0;
      impl_->height = 0;
      impl_->fmt = PixelFormat::Unknown;
    }
    FreeMediaType(cmt);

    if (impl_->obsWorkaround) {
      IMediaFilter* mf = nullptr;
      if (SUCCEEDED(impl_->graph->QueryInterface(IID_PPV_ARGS(&mf))) && mf) {
        mf->SetSyncSource(nullptr);
        mf->Release();
      }
    }

    hr = impl_->graph->QueryInterface(IID_PPV_ARGS(&impl_->control));
    if (FAILED(hr) || !impl_->control) {
      failureSummary = L"QueryInterface(IMediaControl) failed";
      continue;
    }

    hr = impl_->control->Run();
    if (FAILED(hr)) {
      failureSummary = std::wstring(L"IMediaControl::Run failed for ") + request.label;
      continue;
    }

    OAFilterState st = State_Stopped;
    impl_->control->GetState(2000, &st);
    impl_->lastCounter = 0;

    chosen->Release();
    return true;
  }

  chosen->Release();
  lastError_ = failureSummary.empty() ? L"Unable to build camera graph" : failureSummary;
  close();
  return false;
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

  if (out.height > 0) {
    switch (out.format) {
      case PixelFormat::BGRA32:
        out.stride = out.width * 4;
        break;
      case PixelFormat::BGR24:
        out.stride = out.width * 3;
        break;
      case PixelFormat::YUY2:
      case PixelFormat::UYVY:
        out.stride = out.width * 2;
        break;
      case PixelFormat::NV12:
      case PixelFormat::I420:
      case PixelFormat::YV12: {
        int64_t sz = (int64_t)out.data.size();
        out.stride = (int)((sz * 2) / ((int64_t)out.height * 3));
        if (out.stride < out.width) out.stride = out.width;
        break;
      }
      default:
        out.stride = (int)(out.data.size() / (size_t)out.height);
        break;
    }
  }
  return true;
}

bool CameraPixels::readBGRA(Frame& outBGRA, uint32_t timeoutMs) {
  Frame tmp;
  if (!read(tmp, timeoutMs)) return false;

  if (tmp.format == PixelFormat::BGRA32) {
    outBGRA = std::move(tmp);
    return true;
  }

  if (!convert_to_bgra(tmp, outBGRA)) {
    lastError_ = std::wstring(L"Conversion to BGRA32 not supported for ") + pixelFormatName(tmp.format);
    return false;
  }
  return true;
}





