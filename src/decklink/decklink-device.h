#pragma once

#include <string>
#include <vector>
#include "platform.h"

struct IDeckLink;

namespace sesame_decklink {

struct DecklinkDeviceInfo {
  int index;
  int device_id;
  int sub_devices;
  int sub_device_index;
  std::string model;
  std::string displayName;
  int profile_id;
  int max_audio_channels = 0;
  bool supports_external_keying = false;
  std::vector<int> peer_devices;
};

class DecklinkDevice {
  friend class DeviceManager;
 private: 
  DecklinkDeviceInfo info;
  IDeckLink* device;
  IDeckLinkStatus* status;

  std::vector<IDeckLinkDisplayMode *> input_modes;
  std::vector<IDeckLinkDisplayMode *> output_modes;

  bool wait_for_profile_id(const BMDProfileID profile_id);
 public:
  DecklinkDevice(IDeckLink* device, const int index);
  ~DecklinkDevice();

  void initialize();
  DecklinkDeviceInfo get_info();
  bool set_profile(const BMDProfileID profile_id);
  IDeckLink* get_device() {
    return device;
  }
  std::vector<std::string> get_peer_devices();

  bool get_input_locked();
};

}