#pragma once

#include <functional>
#include <cstdint>
#include "platform.h"

#define DECKLINK_TIME_BASE 1000000000

struct DecklinkVideoFrame {
  uint8_t* data[8];
  uint32_t linesize[8];
  uint32_t width;
  uint32_t height;
  uint64_t timestamp;
  uint64_t hardware_time;
  bool interlaced;
  bool timecode_valid;
  uint8_t tc_hours;
  uint8_t tc_minutes;
  uint8_t tc_seconds;
  uint8_t tc_frames;
  bool tc_drop_frame;
};

struct DecklinkAudioPacket {
  uint32_t frames;
  uint64_t timestamp;
  uint8_t* data;
  uint32_t channels;
};

struct DecklinkTimecode {
  bool valid;
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
  uint8_t frames;
  bool drop_frame;
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
