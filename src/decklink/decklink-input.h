#pragma once

#include "decklink-device.h"
#include "decklink-types.h"

#include <atomic>
#include <exception>
#include <iostream>
#include <mutex>

namespace sesame_decklink {

class DecklinkDevice;

class DecklinkInput : public IDeckLinkInputCallback, public IDeckLinkNotificationCallback {
private:
  DecklinkDevice* device;
  IDeckLinkInput* input = nullptr;
  IDeckLinkNotification* notification = nullptr;
  IDeckLinkMemoryAllocator* allocator = nullptr;

  void unsubscribe_notification() {
    if (notification != nullptr) {
      notification->Unsubscribe(bmdStatusChanged, this);
      notification->Release();
      notification = nullptr;
    }
  }

  void abort_failed_start();
  bool supports_format_detection();
  bool enable_video(BMDDisplayMode mode);

  int64_t capture_group = 0;
  bool started = false;
  BMDVideoInputFlags video_flags = bmdVideoInputFlagDefault;

  // The mode the card captures in. Written while streams are paused, read per frame on the capture thread.
  std::mutex mode_mutex;
  DecklinkVideoMode current_mode;
  // A mode the card detected on the wire, until apply_detected_mode() switches to it.
  std::atomic<BMDDisplayMode> detected_mode{bmdModeUnknown};

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

  /**
   * Starts capture in `mode`. With `detect_format`, and a card that supports it, capture then follows the format
   * detected on the wire through apply_detected_mode().
   */
  bool start(const BMDDisplayMode mode, const bool detect_format = false);
  void stop();

  /**
   * Switches capture to the mode the card detected on the wire, when that differs from the current one. Returns
   * true when the mode changed. Call from the thread that calls restart_streams(), never from a capture callback.
   */
  bool apply_detected_mode();

  DecklinkVideoMode get_mode() {
    const std::scoped_lock lock(mode_mutex);
    return current_mode;
  }

  bool has_format_detection() const { return (video_flags & bmdVideoInputEnableFormatDetection) != 0; }

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
    if (input != nullptr) {
      input->SetCallback(nullptr);
      if (started) {
        stop();
      } else {
        unsubscribe_notification();
      }
      input->SetVideoInputFrameMemoryAllocator(nullptr);
      input->DisableAudioInput();
      input->DisableVideoInput();
      input->Release();
      input = nullptr;
    }
    // Nulled only after the notification subscription is gone; Notify()
    // dereferences device.
    device = nullptr;
  };

  ULONG STDMETHODCALLTYPE AddRef(void);
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID* ppv);
  ULONG STDMETHODCALLTYPE Release(void);

  HRESULT STDMETHODCALLTYPE VideoInputFrameArrived(IDeckLinkVideoInputFrame* video, IDeckLinkAudioInputPacket* audio);
  HRESULT STDMETHODCALLTYPE VideoInputFormatChanged(BMDVideoInputFormatChangedEvents events, IDeckLinkDisplayMode *, BMDDetectedVideoInputFormatFlags flags);

  HRESULT STDMETHODCALLTYPE Notify(BMDNotifications topic, uint64_t param1, uint64_t param2);
};

}