#pragma once

#include <Windows.h>
#include <string>
#include <vector>
#include <memory>

#include "decklink-device.h"
#include "decklink-input.h"
#include "decklink-output.h"
#include "decklink-types.h"

#define DECKLINK_IO_API

namespace sesame_decklink {
  namespace api {
    extern DECKLINK_IO_API bool initialize();
    extern DECKLINK_IO_API bool deinitialize();
    extern DECKLINK_IO_API std::vector<DecklinkDeviceInfo> get_devices();
    extern DECKLINK_IO_API bool set_paired_connectors(const int device_index, const bool enable);

    extern DECKLINK_IO_API DecklinkInput* get_input_device(const int device_index, IDeckLinkMemoryAllocator* allocator, const int64_t group = 0);
    extern DECKLINK_IO_API DecklinkOutput* get_output_device(const int device_index, IDeckLinkMemoryAllocator* allocator);
  }
}