#include "decklink-device.h"
#include "decklink-output.h"

namespace sesame_decklink {

DecklinkOutput::DecklinkOutput(DecklinkDevice* device, IDeckLinkMemoryAllocator* allocator) : device(device), allocator(allocator) {
  const auto dev = device->get_device();
  if (dev->QueryInterface(IID_IDeckLinkOutput, (void**)&output) != S_OK) {
    output = nullptr;
    throw std::exception("Failed to get output interface for decklink device");
  }

  if (allocator != nullptr) {
    output->SetVideoOutputFrameMemoryAllocator(allocator);
  }
}

DecklinkOutput::~DecklinkOutput() {
  if (output != nullptr) {
    output->SetVideoOutputFrameMemoryAllocator(nullptr);
    output->Release();
    output = nullptr;
  }
}

bool DecklinkOutput::get_hardware_time(BMDTimeValue* time) {
  BMDTimeValue hardware_time;
  BMDTimeValue time_in_frame;
  BMDTimeValue ticks_per_frame;
  HRESULT ok;

  if (output) {
    ok = output->GetHardwareReferenceClock(DECKLINK_TIME_BASE, &hardware_time, &time_in_frame, &ticks_per_frame);
  } else {
    return false;
  }

  if (ok == S_OK) {
    *time = hardware_time;
    return true;
  }
  return false;
}

bool DecklinkOutput::start(const BMDDisplayMode mode, const int audio_channels, const bool rgba_mode) {
  if (output == nullptr) {
    return false;
  }

  auto res = output->GetDisplayMode(mode, &display_mode);
  if (res != S_OK) {
    return false;
  }

  res = display_mode->GetFrameRate(&frame_duration, &time_scale);
  if (res != S_OK) {
    return false;
  }

  if (rgba_mode) {
    pixel_format = bmdFormat8BitBGRA;
    row_bytes = width * 4;
  } else {
    pixel_format = bmdFormat8BitYUV;
    row_bytes = width * 2;
  }

  width = display_mode->GetWidth();
  height = display_mode->GetHeight();

  res = output->EnableVideoOutput(mode, bmdVideoOutputFlagDefault);
  if (res != S_OK) {
    return false;
  }

  res = output->EnableAudioOutput(bmdAudioSampleRate48kHz, bmdAudioSampleType16bitInteger, audio_channels, bmdAudioOutputStreamTimestamped);
  if (res != S_OK) {
    return false;
  }

  output->SetScheduledFrameCompletionCallback(this);
	output->SetAudioCallback(this);
	output->BeginAudioPreroll();

  return true;
}

void DecklinkOutput::stop() {
  started = false;

  if (output != nullptr) {
    output->DisableVideoOutput();
  }
}

bool DecklinkOutput::start_scheduled_playback() {
  if (output == nullptr) {
    return false;
  }

  auto res = output->StartScheduledPlayback(0, time_scale, 1.0);
  if (res != S_OK) {
    return false;
  }

  started = true;

  return true;
}

bool DecklinkOutput::schedule_video_frame(IDeckLinkVideoFrame* frame, const uint32_t frame_nbr) {
	IDeckLinkMutableVideoFrame* created_frame = nullptr;
	if (frame == nullptr) {
		if (output->CreateVideoFrame(width, height, (int)row_bytes, pixel_format, bmdFrameFlagDefault, &created_frame) != S_OK) {
      return false;
		}
	}

  bool res = true;
	if (output->ScheduleVideoFrame(frame != nullptr ? frame : created_frame, ((frame_nbr) * frame_duration), frame_duration, time_scale) != S_OK) {
		res = false;
	}

	if (created_frame != nullptr) { 
		created_frame->Release();
		created_frame = NULL;
	}

  return res;
}

bool DecklinkOutput::schedule_audio_packet(DecklinkAudioPacket* packet) {
  uint32_t written = 0;
  if (output->ScheduleAudioSamples(packet->data, packet->frames, ((packet->timestamp) * frame_duration), time_scale, &written) != S_OK) {
    return false;
  }

  return true;
}

uint32_t DecklinkOutput::get_buffered_audio_frames() {
  unsigned int buffered = 0;
  output->GetBufferedAudioSampleFrameCount(&buffered);
  return buffered;
}

uint32_t DecklinkOutput::get_buffered_video_frames() {
  unsigned int buffered = 0;
  output->GetBufferedVideoFrameCount(&buffered);
  return buffered;
}

HRESULT STDMETHODCALLTYPE DecklinkOutput::ScheduledFrameCompleted(IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result) {
	BMDTimeValue val;
	output->GetFrameCompletionReferenceTimestamp(completedFrame, DECKLINK_TIME_BASE, &val);
  if (on_completed_frame != nullptr) {
    return on_completed_frame(completedFrame, result, val) ? S_OK : E_FAIL;
  }

  return S_OK;
}

HRESULT STDMETHODCALLTYPE DecklinkOutput::ScheduledPlaybackHasStopped() {
  return S_OK;
}

HRESULT	STDMETHODCALLTYPE DecklinkOutput::RenderAudioSamples(decklink_bool_t preroll) {
  if (on_render_audio != nullptr) {
    return on_render_audio(preroll) ? S_OK : E_FAIL;
  }
  return S_OK;
}

HRESULT STDMETHODCALLTYPE DecklinkOutput::QueryInterface(REFIID iid, LPVOID* ppv) {
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE DecklinkOutput::AddRef() {
  ref_count++;
  return ref_count;
}

ULONG STDMETHODCALLTYPE DecklinkOutput::Release() {
  ref_count--;
  if (ref_count == 0) {
    delete this;
    return 0;
  }
  return ref_count;
}


}