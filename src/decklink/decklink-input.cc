#include "decklink-input.h"

#include <iostream>

namespace sesame_decklink {

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
  return InterlockedIncrement(&ref_count);
}

HRESULT STDMETHODCALLTYPE DecklinkInput::QueryInterface(REFIID iid, LPVOID* ppv) {
  if (iid == IID_IUnknown || iid == IID_IDeckLinkInputCallback) {
    *ppv = this;
    AddRef();
    return S_OK;
  } else if (memcmp(&iid, &IID_IDeckLinkNotificationCallback,
    sizeof(REFIID)) == 0) {
    *ppv = (IDeckLinkNotificationCallback *)this;
    AddRef();
    return S_OK;
  }

  *ppv = nullptr;
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE DecklinkInput::Release() {
  auto new_ref_count = InterlockedDecrement(&ref_count);
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
    return false;
  }

  audio.timestamp = (uint64_t)timestamp;
  audio.frames = packet->GetSampleFrameCount();
  audio.data = reinterpret_cast<uint8_t*>(data);
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
  return true;
}

HRESULT STDMETHODCALLTYPE DecklinkInput::VideoInputFormatChanged(BMDVideoInputFormatChangedEvents events, IDeckLinkDisplayMode *, BMDDetectedVideoInputFormatFlags flags) {
  return S_OK;
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


bool DecklinkInput::start(const BMDDisplayMode mode) {
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

  BMDVideoInputFlags flags = bmdVideoInputFlagDefault;
  if (capture_group > 0) {
    flags |= bmdVideoInputSynchronizeToCaptureGroup;
  }

  res = input->EnableVideoInput(mode, bmdFormat8BitYUV, flags);
  if (res != S_OK) {
    return false;
  }

  res = input->EnableAudioInput(bmdAudioSampleRate48kHz, bmdAudioSampleType16bitInteger, 16);
  if (res != S_OK) {
    return false;
  }

  res = input->StartStreams();
  if (res != S_OK) {
    return false;
  }

  started = true;

  return true;
}

void DecklinkInput::stop() {
  started = false;

  if (notification != nullptr) {
		notification->Unsubscribe(bmdStatusChanged, this);
		notification->Release();
		notification = nullptr;
	}

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
    config = nullptr;
    throw std::exception("Failed to get configuration interface for decklink device");
  }

  if (config->SetInt(bmdDeckLinkConfigCaptureGroup, group) != S_OK) {
    throw std::exception("Failed to set capture group for decklink device");
  }

  if (dev->QueryInterface(IID_IDeckLinkInput, (void**)&input) != S_OK) {
    input = nullptr;
    throw std::exception("Failed to get input interface for decklink device");
  }

  if (allocator != nullptr) {
    input->SetVideoInputFrameMemoryAllocator(allocator);
  }
}


}
