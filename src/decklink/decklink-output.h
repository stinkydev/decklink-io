#pragma once

#include "decklink-types.h"

#include <atomic>

namespace sesame_decklink {

class DecklinkDevice;

class DecklinkOutput : public IDeckLinkVideoOutputCallback, public IDeckLinkAudioOutputCallback {
 private:
  DecklinkDevice* device;
  IDeckLinkOutput* output = nullptr;
  IDeckLinkDisplayMode* display_mode = nullptr;
  IDeckLinkMemoryAllocator* allocator = nullptr;

  std::atomic<long> ref_count = 1;
  
  BMDTimeScale time_scale = 0;
  BMDTimeValue frame_duration = 0;
  
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t row_bytes = 0;
  BMDPixelFormat pixel_format = bmdFormat8BitYUV;

  DecklinkCompletedFrameCallback on_completed_frame = nullptr;
  DecklinkRenderAudioCallback on_render_audio = nullptr;
 public:
  DecklinkOutput(DecklinkDevice* device, IDeckLinkMemoryAllocator* allocator);
  ~DecklinkOutput();

  bool start(const BMDDisplayMode mode, const int audio_channels, const bool rgba_mode);
  void stop();

  bool start_scheduled_playback();
  bool schedule_video_frame(IDeckLinkVideoFrame* frame, const uint32_t frame_nbr);
  bool schedule_audio_packet(DecklinkAudioPacket* packet);

  void set_completed_frame_callback(DecklinkCompletedFrameCallback callback) {
    on_completed_frame = callback;
  }

  void set_render_audio_callback(DecklinkRenderAudioCallback callback) {
    on_render_audio = callback;
  }

  HRESULT STDMETHODCALLTYPE ScheduledFrameCompleted(IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result);
  HRESULT STDMETHODCALLTYPE ScheduledPlaybackHasStopped();
  HRESULT	STDMETHODCALLTYPE RenderAudioSamples(BOOL preroll);

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID* ppv);
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();
};

}