#pragma once

#include "decklink-device.h"
#include "decklink-types.h"

#include <atomic>
#include <exception>
#include <iostream>

namespace sesame_decklink {

class DecklinkDevice;

class DecklinkInput : public IDeckLinkInputCallback, public IDeckLinkNotificationCallback {
private:
  DecklinkDevice* device;
  IDeckLinkInput* input = nullptr;
  IDeckLinkNotification* notification = nullptr;
  IDeckLinkMemoryAllocator* allocator = nullptr;

  int64_t capture_group = 0;
  bool started = false;

  DecklinkVideoFrameCallback on_frame = nullptr;
  DecklinkInputSignalCallback on_input_signal_changed = nullptr;
  DecklinkEventCallback on_event = nullptr;

  DecklinkVideoFrame video;
  DecklinkAudioPacket audio;

  std::atomic<long> ref_count{1};

  bool handle_audio(IDeckLinkAudioInputPacket* packet);
  bool handle_video(IDeckLinkVideoInputFrame* frame);
 public:
 bool get_hardware_time(BMDTimeValue* time);
 std::string get_display_name() const {
    return device->get_info().displayName;
  }

 bool start(const BMDDisplayMode mode);
  void stop();

  void restart_streams() {
    if (input) {
      input->StopStreams();
      input->FlushStreams();
      input->StartStreams();

      if (on_event != nullptr) {
        on_event(DECKLINK_EVENT_TYPE_STREAM_RESTART);
      }
    }
  }

  void set_frame_callback(DecklinkVideoFrameCallback callback) {
    on_frame = callback;
  }

  void set_input_signal_changed_callback(DecklinkInputSignalCallback callback) {
    on_input_signal_changed = callback;
  }

  void set_event_callback(DecklinkEventCallback callback) {
    on_event = callback;
  }

  DecklinkInput(DecklinkDevice* device, IDeckLinkMemoryAllocator* allocator,  const int64_t group = 0);
  ~DecklinkInput() {
    device = nullptr;
    if (input != nullptr) {
      input->SetCallback(nullptr);
      if (started) {
        stop();
      }
      input->SetVideoInputFrameMemoryAllocator(nullptr);
      input->DisableAudioInput();
      input->DisableVideoInput();
      input->Release();
      input = nullptr;
    }
  };

  ULONG STDMETHODCALLTYPE AddRef(void);
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID* ppv);
  ULONG STDMETHODCALLTYPE Release(void);

  HRESULT STDMETHODCALLTYPE VideoInputFrameArrived(IDeckLinkVideoInputFrame* video, IDeckLinkAudioInputPacket* audio);
  HRESULT STDMETHODCALLTYPE VideoInputFormatChanged(BMDVideoInputFormatChangedEvents events, IDeckLinkDisplayMode *, BMDDetectedVideoInputFormatFlags flags);

  HRESULT STDMETHODCALLTYPE Notify(BMDNotifications topic, uint64_t param1, uint64_t param2);
};

}