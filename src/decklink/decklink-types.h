#pragma once

#include <functional>
#include <stdint.h>
#include <DeckLinkAPI.h>

#define DECKLINK_TIME_BASE 1000000000

struct DecklinkVideoFrame {
  uint8_t* data[8];
  uint32_t linesize[8];
  uint32_t width;
  uint32_t height;
  uint64_t timestamp;
  bool interlaced;
};

struct DecklinkAudioPacket {
  uint32_t frames;
  uint64_t timestamp;
  uint8_t* data;
};

enum DecklinkEventType {
  DECKLINK_EVENT_TYPE_ERROR,
  DECKLINK_EVENT_TYPE_STREAM_RESTART
};

typedef std::function<void (const DecklinkEventType type)> DecklinkEventCallback;
typedef std::function<void (const bool locked)> DecklinkInputSignalCallback;
typedef std::function<bool (const DecklinkVideoFrame* video_frame, const DecklinkAudioPacket* audio_packet)> DecklinkVideoFrameCallback;
typedef std::function<bool (IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result, const BMDTimeValue time)> DecklinkCompletedFrameCallback;
typedef std::function<bool (bool preroll )> DecklinkRenderAudioCallback;
