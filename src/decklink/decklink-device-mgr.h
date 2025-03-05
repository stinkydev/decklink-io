#pragma once
#include "decklink-device.h"
#include "decklink-device-enumerator.h"
#include "decklink-input.h"
#include "decklink-output.h"

#include <memory>
#include <vector>
#include <DeckLinkAPI.h>
#include <mutex>

namespace sesame_decklink {
  class DecklinkDeviceInstance;
  class DecklinkDevice;

  class DeviceManager {
   private:
    std::mutex device_mutex; 
    std::vector<std::unique_ptr<DecklinkDevice>> devices;
    DecklinkDeviceEnumerator enumerator;
    void initialize_devices();
    void update_peers_for_device(DecklinkDevice* device);
   public:
    void refresh_devices();
    std::vector<DecklinkDeviceInfo> get_devices();

    DecklinkInput* get_input_device(const int device_index, IDeckLinkMemoryAllocator* allocator, const int64_t group = 0);
    DecklinkOutput* get_output_device(const int device_index, IDeckLinkMemoryAllocator* allocator);
    void release_output_device(DecklinkOutput* output);

    bool set_device_profile(const int device_index, const BMDProfileID profile_id);

    DeviceManager() : enumerator() {};
    ~DeviceManager() = default;
  };
}