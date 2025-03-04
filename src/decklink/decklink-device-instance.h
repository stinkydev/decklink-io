#pragma once

namespace sesame_decklink {

class DecklinkDevice;

class DecklinkDeviceInstance {
private:
  DecklinkDevice* device; 
public:
  DecklinkDeviceInstance(DecklinkDevice* device) : device(device) {};
  ~DecklinkDeviceInstance() {
    device = nullptr;
  };
};

}