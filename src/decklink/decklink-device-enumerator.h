#pragma once

#include <mutex>
#include <vector>

#include "platform.h"

#include <iostream>

namespace sesame_decklink {

class DecklinkDeviceEnumerator {
 private:
  std::mutex mutex;
  std::vector<IDeckLink*> devices;
  IDeckLinkIterator *iterator = nullptr;
 public:
  DecklinkDeviceEnumerator() {
    iterator = create_decklink_iterator_instance();
    if (iterator == nullptr) {
      return;
    }

    IDeckLink *device;

    while (iterator->Next(&device) == S_OK) {
      devices.push_back(device);
    }
  }

  ~DecklinkDeviceEnumerator() {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto device : devices) {
      device->Release();
    }
    devices.clear();

    if (iterator != nullptr) {
      iterator->Release();
    }
  }

  static std::string profile_id_to_string(int64_t profile_id) {
    switch (profile_id) {
      case bmdProfileFourSubDevicesHalfDuplex:
        return "Four sub-devices, half-duplex";
      case bmdProfileOneSubDeviceFullDuplex:
        return "One sub-device, full-duplex";
      case bmdProfileOneSubDeviceHalfDuplex:
        return "One sub-device, half-duplex";
      case bmdProfileTwoSubDevicesFullDuplex:
        return "Two sub-devices, full-duplex";
      case bmdProfileTwoSubDevicesHalfDuplex:
        return "Two sub-devices, half-duplex";
      default:
        return "Unknown";
    }
  }

  std::vector<IDeckLink*> get_devices() {
    std::lock_guard<std::mutex> lock(mutex);
    return devices;
  }
};

}