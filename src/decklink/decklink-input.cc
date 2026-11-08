#include "decklink-input.h"
#include "platform.h"
#include <cstring>
#include <stdexcept>
#include <atomic>

#include <iostream>

namespace sesame_decklink {

constexpr unsigned int DECKLINK_AUDIO_CHANNELS = 16;

bool DecklinkInput::get_hardware_time(BMDTimeValue* time) {
  BMDTimeValue hardware_time;
  BMDTimeValue time_in_frame;
  BMDTimeValue ticks_per_frame;
  HRESULT ok;

  if (input) {
    ok = input->GetHardwareReferenceClock(DECKLINK_TIME_BASE, &hardware_time, &time_in_frame, &ticks_per_frame);
  } else {
    return false;
  }

  if (ok == S_OK) {
    *time = hardware_time;
    return true;
  }
  return false;
}

ULONG STDMETHODCALLTYPE DecklinkInput::AddRef() {
  return ++ref_count;
}

HRESULT STDMETHODCALLTYPE DecklinkInput::QueryInterface(REFIID iid, LPVOID* ppv) {
  if (refiid_equal(iid, IID_IUnknown) || refiid_equal(iid, IID_IDeckLinkInputCallback)) {
    *ppv = static_cast<IDeckLinkInputCallback*>(this);
    AddRef();
    return S_OK;
  }
  if (refiid_equal(iid, IID_IDeckLinkNotificationCallback)) {
    *ppv = static_cast<IDeckLinkNotificationCallback*>(this);
    AddRef();
    return S_OK;
  }

  *ppv = nullptr;
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE DecklinkInput::Release() {
  auto new_ref_count = --ref_count;
  if (new_ref_count == 0) {
    delete this;
    return 0;
  }
  return new_ref_count;
}

HRESULT STDMETHODCALLTYPE DecklinkInput::VideoInputFrameArrived(IDeckLinkVideoInputFrame* video_frame, IDeckLinkAudioInputPacket* audio_packet) {
  const auto has_audio = handle_audio(audio_packet);
  const auto has_video = handle_video(video_frame);

  if (has_audio && has_video) {
    if (on_frame != nullptr) {
      on_frame(&video, &audio);
    }
  }

  return S_OK;
}

bool DecklinkInput::handle_audio(IDeckLinkAudioInputPacket* packet) {
  BMDTimeValue timestamp;

  void* data;
  if ((packet == nullptr) || (packet->GetPacketTime(&timestamp, DECKLINK_TIME_BASE) != S_OK) || (packet->GetBytes(&data) != S_OK)) {
    audio.data = nullptr;
    audio.timestamp = 0;
    audio.frames = 0;
    audio.channels = 0;
    return false;
  }

  audio.timestamp = (uint64_t)timestamp;
  audio.frames = packet->GetSampleFrameCount();
  audio.data = reinterpret_cast<uint8_t*>(data);
  audio.channels = DECKLINK_AUDIO_CHANNELS;
  return true;
}

bool DecklinkInput::handle_video(IDeckLinkVideoInputFrame* frame) {
  BMDTimeValue timestamp;
  BMDTimeValue duration;
  void* data;

	if ((frame == nullptr) || (frame->GetStreamTime(&timestamp, &duration, DECKLINK_TIME_BASE) != S_OK) || (frame->GetBytes(&data) != S_OK)) {
    video.data[0] = nullptr;
    video.linesize[0] = 0;
    video.width = 0;
    video.height = 0;
    video.timestamp = 0;
    video.hardware_time = 0;
    return false;
  }

  video.timestamp = (uint64_t)timestamp;
  video.data[0] = (uint8_t*)data;
  const auto flags = frame->GetFlags();
  const bool has_signal_  = !(flags & bmdFrameHasNoInputSource);
	if (!has_signal_) return false;

  video.linesize[0] = frame->GetRowBytes();
  video.width = frame->GetWidth();
  video.height = frame->GetHeight();
  {
    const std::scoped_lock lock(mode_mutex);
    video.frame_duration = current_mode.frame_duration;
    video.time_scale = current_mode.time_scale;
    video.interlaced = current_mode.interlaced;
    video.lower_field_first = current_mode.lower_field_first;
  }

  BMDTimeValue hardware_time;
  BMDTimeValue frame_duration;
  if (frame->GetHardwareReferenceTimestamp(DECKLINK_TIME_BASE, &hardware_time, &frame_duration) == S_OK) {
    video.hardware_time = hardware_time;
  } else {
    video.hardware_time = 0;
  }

  video.timecode_valid = false;
  IDeckLinkTimecode* timecode = nullptr;
  if (frame->GetTimecode(bmdTimecodeRP188Any, &timecode) == S_OK && timecode != nullptr) {
    uint8_t hours, minutes, seconds, frames;
    if (timecode->GetComponents(&hours, &minutes, &seconds, &frames) == S_OK) {
      video.tc_hours = hours;
      video.tc_minutes = minutes;
      video.tc_seconds = seconds;
      video.tc_frames = frames;
      video.tc_drop_frame = (timecode->GetFlags() & bmdTimecodeIsDropFrame) != 0;
      video.timecode_valid = true;
    }
    timecode->Release();
  }
  return true;
}

HRESULT STDMETHODCALLTYPE DecklinkInput::VideoInputFormatChanged(BMDVideoInputFormatChangedEvents, IDeckLinkDisplayMode* new_mode, BMDDetectedVideoInputFormatFlags) {
  // Reconfiguring the card from its own callback would race restart_streams(); the owner applies it.
  if (new_mode != nullptr) {
    detected_mode.store(new_mode->GetDisplayMode());
  }
  return S_OK;
}

bool DecklinkInput::apply_detected_mode() {
  const BMDDisplayMode mode = detected_mode.exchange(bmdModeUnknown);
  if (mode == bmdModeUnknown || input == nullptr || !started) {
    return false;
  }
  const BMDDisplayMode previous = get_mode().mode;
  if (mode == previous) {
    return false;
  }

  input->PauseStreams();
  const bool switched = enable_video(mode);
  if (!switched) {
    std::cout << "Failed to switch decklink input to detected mode" << std::endl;
    enable_video(previous);
  }
  input->FlushStreams();
  input->StartStreams();

  if (switched && on_event != nullptr) {
    on_event(DECKLINK_EVENT_TYPE_STREAM_RESTART);
  }
  return switched;
}

bool DecklinkInput::supports_format_detection() {
  IDeckLinkProfileAttributes* attributes = nullptr;
  if (device->get_device()->QueryInterface(IID_IDeckLinkProfileAttributes, (void**)&attributes) != S_OK) {
    return false;
  }
  decklink_bool_t supported = false;
  const bool ok = attributes->GetFlag(BMDDeckLinkSupportsInputFormatDetection, &supported) == S_OK;
  attributes->Release();
  return ok && supported;
}

bool DecklinkInput::enable_video(const BMDDisplayMode mode) {
  IDeckLinkDisplayMode* display_mode = nullptr;
  if (input->GetDisplayMode(mode, &display_mode) != S_OK || display_mode == nullptr) {
    return false;
  }

  DecklinkVideoMode info;
  info.mode = mode;
  info.width = static_cast<uint32_t>(display_mode->GetWidth());
  info.height = static_cast<uint32_t>(display_mode->GetHeight());
  BMDTimeValue frame_duration = 0;
  BMDTimeScale time_scale = 0;
  display_mode->GetFrameRate(&frame_duration, &time_scale);
  info.frame_duration = frame_duration;
  info.time_scale = time_scale;
  // Segmented frames arrive woven into whole frames, so only true fields count as interlaced.
  const BMDFieldDominance dominance = display_mode->GetFieldDominance();
  info.interlaced = dominance == bmdUpperFieldFirst || dominance == bmdLowerFieldFirst;
  info.lower_field_first = dominance == bmdLowerFieldFirst;
  decklink_string_t name = nullptr;
  if (display_mode->GetName(&name) == S_OK) {
    decklink_string_to_std_string(name, info.name);
    decklink_free_string(name);
  }
  display_mode->Release();

  if (input->EnableVideoInput(mode, bmdFormat8BitYUV, video_flags) != S_OK) {
    return false;
  }
  const std::scoped_lock lock(mode_mutex);
  current_mode = std::move(info);
  return true;
}

HRESULT STDMETHODCALLTYPE DecklinkInput::Notify(BMDNotifications topic, uint64_t param1, uint64_t param2) {
	if (topic == bmdStatusChanged) {
		if (param1 == bmdDeckLinkStatusVideoInputSignalLocked) {
      const auto locked = device->get_input_locked();
			if (on_input_signal_changed != nullptr) {
				on_input_signal_changed(locked);
			}
  	}
	}

	return S_OK;
}


bool DecklinkInput::start(const BMDDisplayMode mode, const bool detect_format) {
  auto res = input->SetCallback(this);
  if (res != S_OK) {
    return false;
  }

  res = input->QueryInterface(IID_IDeckLinkNotification, (void **)&notification);
	if (res != S_OK) {
		notification = nullptr;
	} else {
		notification->Subscribe(bmdStatusChanged, this);
	}

  video_flags = bmdVideoInputFlagDefault;
  if (capture_group > 0) {
    video_flags |= bmdVideoInputSynchronizeToCaptureGroup;
  }
  if (detect_format && supports_format_detection()) {
    video_flags |= bmdVideoInputEnableFormatDetection;
  }

  bool enabled = enable_video(mode);
  if (!enabled && has_format_detection()) {
    video_flags &= ~bmdVideoInputEnableFormatDetection;
    enabled = enable_video(mode);
  }
  if (!enabled) {
    abort_failed_start();
    return false;
  }

  res = input->EnableAudioInput(bmdAudioSampleRate48kHz, bmdAudioSampleType16bitInteger, DECKLINK_AUDIO_CHANNELS);
  if (res != S_OK) {
    abort_failed_start();
    return false;
  }

  res = input->StartStreams();
  if (res != S_OK) {
    abort_failed_start();
    return false;
  }

  started = true;

  return true;
}

void DecklinkInput::abort_failed_start() {
  unsubscribe_notification();
  input->SetCallback(nullptr);
}

void DecklinkInput::stop() {
  started = false;

  unsubscribe_notification();

  auto res = input->StopStreams();
  if (res != S_OK) {
    std::cout << "Failed to stop streams" << std::endl;
  }
}

DecklinkInput::DecklinkInput(DecklinkDevice* device, IDeckLinkMemoryAllocator* allocator,  const int64_t group)
  : device(device), capture_group(group), allocator(allocator) {
  const auto dev = device->get_device();

  IDeckLinkConfiguration* config = nullptr;
  if (dev->QueryInterface(IID_IDeckLinkConfiguration, (void**)&config) != S_OK) {
    throw std::runtime_error("Failed to get configuration interface for decklink device");
  }
  const bool group_set = config->SetInt(bmdDeckLinkConfigCaptureGroup, group) == S_OK;
  config->Release();
  if (!group_set) {
    throw std::runtime_error("Failed to set capture group for decklink device");
  }

  if (dev->QueryInterface(IID_IDeckLinkInput, (void**)&input) != S_OK) {
    input = nullptr;
    throw std::runtime_error("Failed to get input interface for decklink device");
  }

  if (allocator != nullptr) {
    input->SetVideoInputFrameMemoryAllocator(allocator);
  }
}


}
