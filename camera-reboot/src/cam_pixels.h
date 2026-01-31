#pragma once
#define NOMINMAX
#include <cstdint>
#include <string>
#include <vector>

class CameraPixels {
public:
  enum class PixelFormat {
    Unknown = 0,
    BGRA32,   // 4 bytes per pixel
    BGR24,    // 3 bytes per pixel
    YUY2,     // packed YUV 4:2:2
    NV12,     // planar Y + interleaved UV (4:2:0)
    I420      // planar Y + U + V (4:2:0)
  };

  struct Frame {
    PixelFormat format = PixelFormat::Unknown;
    int width = 0;
    int height = 0;
    int stride = 0;                 // bytes per row of the first plane (or pixels row for packed)
    int64_t timestamp100ns = 0;     // best-effort stream time
    std::vector<uint8_t> data;      // raw bytes in the camera/native format (or BGRA if from readBGRA)
  };

  CameraPixels();
  ~CameraPixels();

  CameraPixels(const CameraPixels&) = delete;
  CameraPixels& operator=(const CameraPixels&) = delete;

  // Enumerate DirectShow video input devices (includes OBS Virtual Camera).
  static std::vector<std::wstring> enumerate();

  // Open by index in enumerate().
  bool open(int deviceIndex);

  // Stop + release everything.
  void close();

  bool isOpen() const;

  // Read one frame (native pixel format). Default timeout prevents deadlocks.
  // Returns false on timeout or if closed.
  bool read(Frame& out, uint32_t timeoutMs = 2000);

  // Read one frame and convert to BGRA32 if possible (YUY2/NV12/BGR24/BGRA32).
  bool readBGRA(Frame& outBGRA, uint32_t timeoutMs = 2000);

  // If something fails, this explains why.
  const std::wstring& lastError() const { return lastError_; }

private:
  struct Impl;
  Impl* impl_ = nullptr;
  std::wstring lastError_;
};
