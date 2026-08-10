// decklink-cli: headless replacement for the DesktopVideoSetup connector mapping UI.
//
// On Desktop Video 11+ the SDI connector mapping of multi-channel cards
// (DeckLink Duo 2, Quad 2, 8K Pro, ...) is controlled by device profiles.
// Activating a profile reconfigures how the physical SDI connectors are
// assigned to sub-devices and the driver stores the choice persistently,
// which is the same mechanism DesktopVideoSetup uses.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <decklink/platform.h>

namespace {

struct ProfileName {
  BMDProfileID id;
  const char* name;
  const char* description;
};

constexpr ProfileName kProfileNames[] = {
  {bmdProfileOneSubDeviceFullDuplex,   "1-full", "One sub-device, full-duplex"},
  {bmdProfileOneSubDeviceHalfDuplex,   "1-half", "One sub-device, half-duplex"},
  {bmdProfileTwoSubDevicesFullDuplex,  "2-full", "Two sub-devices, full-duplex"},
  {bmdProfileTwoSubDevicesHalfDuplex,  "2-half", "Two sub-devices, half-duplex"},
  {bmdProfileFourSubDevicesHalfDuplex, "4-half", "Four sub-devices, half-duplex"},
};

std::string fourcc_to_string(int64_t value) {
  char buf[5] = {};
  for (int i = 0; i < 4; i++) {
    buf[i] = static_cast<char>((value >> (24 - i * 8)) & 0xff);
  }
  return std::string(buf);
}

std::string profile_to_name(int64_t profile_id) {
  for (const auto& p : kProfileNames) {
    if (p.id == static_cast<BMDProfileID>(profile_id)) return p.name;
  }
  return profile_id == 0 ? "n/a" : fourcc_to_string(profile_id);
}

std::string profile_to_description(int64_t profile_id) {
  for (const auto& p : kProfileNames) {
    if (p.id == static_cast<BMDProfileID>(profile_id)) return p.description;
  }
  return profile_id == 0 ? "n/a" : fourcc_to_string(profile_id);
}

bool parse_profile(const std::string& name, BMDProfileID& out) {
  for (const auto& p : kProfileNames) {
    if (name == p.name) {
      out = p.id;
      return true;
    }
  }
  return false;
}

std::string duplex_to_string(int64_t duplex) {
  switch (duplex) {
    case bmdDuplexFull:     return "full";
    case bmdDuplexHalf:     return "half";
    case bmdDuplexSimplex:  return "simplex";
    case bmdDuplexInactive: return "inactive";
    default:                return "unknown";
  }
}

struct DeviceEntry {
  int index = -1;
  std::string display_name;
  std::string model;
  int64_t persistent_id = 0;
  int64_t group_id = 0;
  int64_t sub_device_index = 0;
  int64_t sub_device_count = 0;
  int64_t duplex = 0;
  int64_t active_profile = 0;
  bool capture = false;
  bool playback = false;
  bool signal_locked = false;
  bool in_use = false;
  std::string input_mode;
  std::vector<int64_t> available_profiles;
  IDeckLink* device = nullptr;
};

std::string display_mode_name(IDeckLink* device, const int64_t mode) {
  std::string name = fourcc_to_string(mode);
  IDeckLinkInput* input = nullptr;
  if (device->QueryInterface(IID_IDeckLinkInput, (void**)&input) != S_OK) {
    return name;
  }

  IDeckLinkDisplayModeIterator* iterator = nullptr;
  if (input->GetDisplayModeIterator(&iterator) == S_OK) {
    IDeckLinkDisplayMode* display_mode = nullptr;
    while (iterator->Next(&display_mode) == S_OK) {
      if (display_mode->GetDisplayMode() == (BMDDisplayMode)mode) {
        decklink_string_t mode_name;
        if (display_mode->GetName(&mode_name) == S_OK) {
          decklink_string_to_std_string(mode_name, name);
          decklink_free_string(mode_name);
        }
        display_mode->Release();
        break;
      }
      display_mode->Release();
    }
    iterator->Release();
  }
  input->Release();
  return name;
}

int64_t get_profile_id(IDeckLinkProfile* profile) {
  IDeckLinkProfileAttributes* attributes = nullptr;
  if (profile->QueryInterface(IID_IDeckLinkProfileAttributes, (void**)&attributes) != S_OK) {
    return 0;
  }
  int64_t value = 0;
  attributes->GetInt(BMDDeckLinkProfileID, &value);
  attributes->Release();
  return value;
}

DeviceEntry read_device(IDeckLink* device, const int index) {
  DeviceEntry entry;
  entry.index = index;
  entry.device = device;

  decklink_string_t name;
  if (device->GetDisplayName(&name) == S_OK) {
    decklink_string_to_std_string(name, entry.display_name);
    decklink_free_string(name);
  }

  decklink_string_t model;
  if (device->GetModelName(&model) == S_OK) {
    decklink_string_to_std_string(model, entry.model);
    decklink_free_string(model);
  }

  IDeckLinkProfileAttributes* attributes = nullptr;
  if (device->QueryInterface(IID_IDeckLinkProfileAttributes, (void**)&attributes) == S_OK) {
    attributes->GetInt(BMDDeckLinkPersistentID, &entry.persistent_id);
    attributes->GetInt(BMDDeckLinkDeviceGroupID, &entry.group_id);
    attributes->GetInt(BMDDeckLinkSubDeviceIndex, &entry.sub_device_index);
    attributes->GetInt(BMDDeckLinkNumberOfSubDevices, &entry.sub_device_count);
    attributes->GetInt(BMDDeckLinkDuplex, &entry.duplex);
    attributes->GetInt(BMDDeckLinkProfileID, &entry.active_profile);

    int64_t io_support = 0;
    if (attributes->GetInt(BMDDeckLinkVideoIOSupport, &io_support) == S_OK) {
      entry.capture = (io_support & bmdDeviceSupportsCapture) != 0;
      entry.playback = (io_support & bmdDeviceSupportsPlayback) != 0;
    }
    attributes->Release();
  }

  IDeckLinkProfileManager* manager = nullptr;
  if (device->QueryInterface(IID_IDeckLinkProfileManager, (void**)&manager) == S_OK) {
    IDeckLinkProfileIterator* iterator = nullptr;
    if (manager->GetProfiles(&iterator) == S_OK) {
      IDeckLinkProfile* profile = nullptr;
      while (iterator->Next(&profile) == S_OK) {
        const int64_t id = get_profile_id(profile);
        if (id != 0) {
          entry.available_profiles.push_back(id);
        }
        profile->Release();
      }
      iterator->Release();
    }
    manager->Release();
  }
  std::sort(entry.available_profiles.begin(), entry.available_profiles.end());

  return entry;
}

std::vector<DeviceEntry> enumerate_devices() {
  std::vector<DeviceEntry> entries;
  IDeckLinkIterator* iterator = create_decklink_iterator_instance();
  if (iterator == nullptr) {
    return entries;
  }

  IDeckLink* device = nullptr;
  int index = 0;
  while (iterator->Next(&device) == S_OK) {
    entries.push_back(read_device(device, index++));
  }
  iterator->Release();
  return entries;
}

void read_signal_status(DeviceEntry& entry) {
  IDeckLinkStatus* status = nullptr;
  if (entry.device->QueryInterface(IID_IDeckLinkStatus, (void**)&status) != S_OK) {
    return;
  }

  decklink_bool_t locked = false;
  if (status->GetFlag(bmdDeckLinkStatusVideoInputSignalLocked, &locked) == S_OK && locked) {
    entry.signal_locked = true;
    int64_t detected_mode = 0;
    if (status->GetInt(bmdDeckLinkStatusDetectedVideoInputMode, &detected_mode) == S_OK && detected_mode != 0) {
      entry.input_mode = display_mode_name(entry.device, detected_mode);
    }
  }
  status->Release();
}

// The driver only reports input signal lock while the input is enabled, so
// briefly start capture with format detection on every idle input, read the
// status, and stop again. Inputs owned by another application refuse to
// enable; their running capture keeps the status flag valid, so read it as-is.
void probe_signals(std::vector<DeviceEntry>& entries) {
  struct Probe {
    IDeckLinkInput* input;
    DeviceEntry* entry;
  };
  std::vector<Probe> probes;

  for (auto& entry : entries) {
    if (!entry.capture || entry.duplex == bmdDuplexInactive) continue;

    IDeckLinkInput* input = nullptr;
    if (entry.device->QueryInterface(IID_IDeckLinkInput, (void**)&input) != S_OK) {
      continue;
    }
    if (input->EnableVideoInput(bmdModeHD1080i50, bmdFormat8BitYUV, bmdVideoInputEnableFormatDetection) != S_OK ||
        input->StartStreams() != S_OK) {
      read_signal_status(entry);
      entry.in_use = true;
      input->Release();
      continue;
    }
    probes.push_back({input, &entry});
  }

  const auto start = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(2500)) {
    bool all_locked = true;
    for (auto& probe : probes) {
      if (!probe.entry->signal_locked) {
        read_signal_status(*probe.entry);
        if (!probe.entry->signal_locked) all_locked = false;
      }
    }
    if (all_locked) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  for (auto& probe : probes) {
    probe.input->StopStreams();
    probe.input->DisableVideoInput();
    probe.input->Release();
  }
}

void release_devices(std::vector<DeviceEntry>& entries) {
  for (auto& entry : entries) {
    if (entry.device != nullptr) {
      entry.device->Release();
      entry.device = nullptr;
    }
  }
}

std::string json_escape(const std::string& input) {
  std::string out;
  for (const char c : input) {
    switch (c) {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:   out += c; break;
    }
  }
  return out;
}

void print_json(const std::vector<DeviceEntry>& entries) {
  printf("[\n");
  for (size_t i = 0; i < entries.size(); i++) {
    const auto& e = entries[i];
    printf("  {\n");
    printf("    \"index\": %d,\n", e.index);
    printf("    \"name\": \"%s\",\n", json_escape(e.display_name).c_str());
    printf("    \"model\": \"%s\",\n", json_escape(e.model).c_str());
    printf("    \"persistent_id\": \"0x%llx\",\n", (unsigned long long)e.persistent_id);
    printf("    \"group_id\": \"0x%llx\",\n", (unsigned long long)e.group_id);
    printf("    \"sub_device_index\": %lld,\n", (long long)e.sub_device_index);
    printf("    \"sub_device_count\": %lld,\n", (long long)e.sub_device_count);
    printf("    \"duplex\": \"%s\",\n", duplex_to_string(e.duplex).c_str());
    printf("    \"capture\": %s,\n", e.capture ? "true" : "false");
    printf("    \"playback\": %s,\n", e.playback ? "true" : "false");
    printf("    \"input_signal\": %s,\n", e.signal_locked ? "true" : "false");
    printf("    \"input_in_use\": %s,\n", e.in_use ? "true" : "false");
    if (e.input_mode.empty()) {
      printf("    \"input_mode\": null,\n");
    } else {
      printf("    \"input_mode\": \"%s\",\n", json_escape(e.input_mode).c_str());
    }
    printf("    \"active_profile\": \"%s\",\n", profile_to_name(e.active_profile).c_str());
    printf("    \"available_profiles\": [");
    for (size_t j = 0; j < e.available_profiles.size(); j++) {
      printf("%s\"%s\"", j > 0 ? ", " : "", profile_to_name(e.available_profiles[j]).c_str());
    }
    printf("]\n");
    printf("  }%s\n", i + 1 < entries.size() ? "," : "");
  }
  printf("]\n");
}

void print_table(const std::vector<DeviceEntry>& entries) {
  if (entries.empty()) {
    printf("No DeckLink devices found\n");
    return;
  }

  printf("%-4s %-28s %-24s %-12s %-6s %-9s %-6s %-8s %-18s %s\n",
         "Idx", "Name", "Model", "PersistentID", "Sub", "Duplex", "I/O", "Profile", "Signal", "Available");
  for (const auto& e : entries) {
    std::string io;
    if (e.capture) io += "in";
    if (e.playback) io += io.empty() ? "out" : "+out";
    if (io.empty()) io = "-";

    char sub[48];
    snprintf(sub, sizeof(sub), "%lld/%lld", (long long)(e.sub_device_index + 1), (long long)e.sub_device_count);

    std::string available;
    for (const auto id : e.available_profiles) {
      if (!available.empty()) available += ",";
      available += profile_to_name(id);
    }
    if (available.empty()) available = "-";

    std::string signal = "-";
    if (e.capture && e.duplex != bmdDuplexInactive) {
      signal = e.signal_locked ? (e.input_mode.empty() ? "locked" : e.input_mode) : "none";
      if (e.in_use) signal += " (in use)";
    }

    printf("%-4d %-28s %-24s 0x%-10llx %-6s %-9s %-6s %-8s %-18s %s\n",
           e.index,
           e.display_name.c_str(),
           e.model.c_str(),
           (unsigned long long)e.persistent_id,
           sub,
           duplex_to_string(e.duplex).c_str(),
           io.c_str(),
           profile_to_name(e.active_profile).c_str(),
           signal.c_str(),
           available.c_str());
  }
}

int find_device(const std::vector<DeviceEntry>& entries, const std::string& selector) {
  if (selector.rfind("0x", 0) == 0 || selector.rfind("0X", 0) == 0) {
    const int64_t persistent_id = strtoll(selector.c_str(), nullptr, 16);
    for (const auto& e : entries) {
      if (e.persistent_id == persistent_id) return e.index;
    }
    return -1;
  }

  char* end = nullptr;
  const long index = strtol(selector.c_str(), &end, 10);
  if (end == selector.c_str() || *end != '\0') return -1;
  if (index < 0 || index >= (long)entries.size()) return -1;
  return (int)index;
}

// Activating a profile tears down and re-creates the sub-devices of the card
// group, so IDeckLink objects enumerated before the switch keep reporting the
// old profile. Poll with fresh enumerations and match by group id instead.
bool wait_for_profile(const int64_t group_id, const int64_t persistent_id, const BMDProfileID profile_id) {
  const auto start = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - start < std::chrono::seconds(15)) {
    auto entries = enumerate_devices();
    bool matched = false;
    bool found_by_id = false;
    for (const auto& e : entries) {
      if (e.persistent_id == persistent_id) {
        found_by_id = true;
        matched = e.active_profile == profile_id;
        break;
      }
    }
    if (!found_by_id && group_id != 0) {
      for (const auto& e : entries) {
        if (e.group_id == group_id && e.active_profile == profile_id) {
          matched = true;
          break;
        }
      }
    }
    release_devices(entries);
    if (matched) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  return false;
}

bool activate_profile(const DeviceEntry& entry, const BMDProfileID profile_id, const std::string& profile_name) {
  IDeckLinkProfileManager* manager = nullptr;
  if (entry.device->QueryInterface(IID_IDeckLinkProfileManager, (void**)&manager) != S_OK) {
    fprintf(stderr, "%s: device has a fixed profile, nothing to configure\n", entry.display_name.c_str());
    return false;
  }

  IDeckLinkProfile* profile = nullptr;
  const auto res = manager->GetProfile(profile_id, &profile);
  manager->Release();
  if (res != S_OK || profile == nullptr) {
    fprintf(stderr, "%s: profile '%s' not supported by this device\n", entry.display_name.c_str(), profile_name.c_str());
    return false;
  }

  const auto activate_res = profile->SetActive();
  profile->Release();
  if (activate_res != S_OK) {
    fprintf(stderr, "%s: failed to activate profile '%s' (device busy?)\n", entry.display_name.c_str(), profile_name.c_str());
    return false;
  }

  if (!wait_for_profile(entry.group_id, entry.persistent_id, profile_id)) {
    fprintf(stderr, "%s: timed out waiting for profile '%s' to activate\n", entry.display_name.c_str(), profile_name.c_str());
    return false;
  }

  fprintf(stderr, "%s: profile set to '%s' (%s), stored persistently by the driver\n",
          entry.display_name.c_str(), profile_name.c_str(), profile_to_description(profile_id).c_str());
  return true;
}

void print_result(std::vector<DeviceEntry>& entries, const bool json) {
  // The profile switch regroups sub-devices, so re-enumerate for the final listing.
  release_devices(entries);
  entries = enumerate_devices();
  if (json) {
    print_json(entries);
  } else {
    print_table(entries);
  }
}

bool supports_profile(const DeviceEntry& entry, const BMDProfileID profile_id) {
  return std::find(entry.available_profiles.begin(), entry.available_profiles.end(),
                   (int64_t)profile_id) != entry.available_profiles.end();
}

// Profiles apply per card group (or per connector pair on Quad 2 style cards),
// so activate on one mismatched device at a time and re-enumerate between
// activations until every device that offers the profile has it active.
int set_profile_all(std::vector<DeviceEntry>& entries, const BMDProfileID profile_id, const std::string& profile_name, const bool json) {
  int changed = 0;
  while (true) {
    const DeviceEntry* target = nullptr;
    for (const auto& e : entries) {
      if (e.active_profile != profile_id && supports_profile(e, profile_id)) {
        target = &e;
        break;
      }
    }
    if (target == nullptr) {
      break;
    }

    if (!activate_profile(*target, profile_id, profile_name)) {
      return 1;
    }
    changed++;

    release_devices(entries);
    entries = enumerate_devices();
  }

  int unsupported = 0;
  for (const auto& e : entries) {
    if (e.active_profile != profile_id && !supports_profile(e, profile_id)) {
      fprintf(stderr, "%s: does not offer profile '%s', left unchanged\n", e.display_name.c_str(), profile_name.c_str());
      unsupported++;
    }
  }

  fprintf(stderr, "%d profile activation(s), %d device(s) without support for '%s'\n", changed, unsupported, profile_name.c_str());
  print_result(entries, json);
  return 0;
}

int set_profile(std::vector<DeviceEntry>& entries, const std::string& selector, const std::string& profile_name, const bool json) {
  BMDProfileID profile_id;
  if (!parse_profile(profile_name, profile_id)) {
    fprintf(stderr, "Unknown profile '%s', valid profiles:\n", profile_name.c_str());
    for (const auto& p : kProfileNames) {
      fprintf(stderr, "  %-8s %s\n", p.name, p.description);
    }
    return 1;
  }

  if (selector == "all") {
    return set_profile_all(entries, profile_id, profile_name, json);
  }

  const int index = find_device(entries, selector);
  if (index < 0) {
    fprintf(stderr, "No device matching '%s' (use an index from 'list', a 0x persistent id, or 'all')\n", selector.c_str());
    return 1;
  }
  auto& entry = entries[index];

  if (entry.active_profile == profile_id) {
    fprintf(stderr, "%s: profile already '%s'\n", entry.display_name.c_str(), profile_name.c_str());
    if (json) print_json(entries);
    return 0;
  }

  if (!activate_profile(entry, profile_id, profile_name)) {
    return 1;
  }

  print_result(entries, json);
  return 0;
}

void print_usage() {
  printf("Usage: decklink-cli [--json] <command>\n");
  printf("\n");
  printf("Commands:\n");
  printf("  list                              List DeckLink devices and their active profile\n");
  printf("  set-profile <device> <profile>    Activate a profile on a device and its peers\n");
  printf("  help                              Show this help\n");
  printf("\n");
  printf("<device> is an index from 'list', a 0x-prefixed persistent id, or 'all' to\n");
  printf("apply the profile to every device that offers it.\n");
  printf("\n");
  printf("Profiles (the SDI connector mapping of the card):\n");
  for (const auto& p : kProfileNames) {
    printf("  %-8s %s\n", p.name, p.description);
  }
  printf("\n");
  printf("Half-duplex profiles give every sub-device its own SDI connector, usable as\n");
  printf("either input or output. Full-duplex profiles give each sub-device two\n");
  printf("connectors (one input, one output). Activating a profile also applies it to\n");
  printf("the peer sub-devices of the same card group. The driver stores the active\n");
  printf("profile persistently, so the mapping survives reboots.\n");
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<std::string> args;
  bool json = false;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--json") == 0) {
      json = true;
    } else {
      args.push_back(argv[i]);
    }
  }

  const std::string command = args.empty() ? "list" : args[0];
  if (command == "help" || command == "--help" || command == "-h") {
    print_usage();
    return 0;
  }

  decklink_platform_init();
  auto entries = enumerate_devices();

  int result = 1;
  if (command == "list") {
    probe_signals(entries);
    if (json) {
      print_json(entries);
    } else {
      print_table(entries);
    }
    result = 0;
  } else if (command == "set-profile") {
    if (args.size() != 3) {
      fprintf(stderr, "Usage: decklink-cli [--json] set-profile <device> <profile>\n");
    } else {
      result = set_profile(entries, args[1], args[2], json);
    }
  } else {
    fprintf(stderr, "Unknown command '%s'\n", command.c_str());
    print_usage();
  }

  release_devices(entries);
  decklink_platform_deinit();
  return result;
}
