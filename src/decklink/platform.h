#pragma once

#include <string>
#include <cstring>

#if defined(_WIN32)
#include <combaseapi.h>
#include <DeckLinkAPI.h>
#include "../win/decklink-sdk/DeckLinkAPIVersion.h"
#include <wrl/client.h>
#include <string>

using namespace Microsoft::WRL;

typedef BOOL decklink_bool_t;
typedef BSTR decklink_string_t;
IDeckLinkDiscovery *create_decklink_discovery_instance(void);
IDeckLinkIterator *create_decklink_iterator_instance(void);
IDeckLinkVideoConversion *create_video_conversion_instance(void);
#define IUnknownUUID IID_IUnknown
typedef REFIID CFUUIDBytes;
#define CFUUIDGetUUIDBytes(x) x
#elif defined(__APPLE__)
#include "mac/decklink-sdk/DeckLinkAPI.h"
#include "mac/decklink-sdk/DeckLinkAPIVersion.h"
#include <CoreFoundation/CoreFoundation.h>
typedef bool decklink_bool_t;
typedef CFStringRef decklink_string_t;
IDeckLinkIterator *create_decklink_iterator_instance(void);
IDeckLinkDiscovery *create_decklink_discovery_instance(void);
IDeckLinkVideoConversion *create_video_conversion_instance(void);
#elif defined(__linux__)
#include "linux/decklink-sdk/DeckLinkAPI.h"
#include "linux/decklink-sdk/DeckLinkAPIVersion.h"
typedef bool decklink_bool_t;
typedef const char *decklink_string_t;
IDeckLinkIterator *create_decklink_iterator_instance(void);
IDeckLinkDiscovery *create_decklink_discovery_instance(void);
IDeckLinkVideoConversion *create_video_conversion_instance(void);

// Helper for REFIID comparison on Linux
inline bool refiid_equal(REFIID a, REFIID b) {
	return memcmp(&a, &b, sizeof(REFIID)) == 0;
}
#endif

bool decklink_string_to_std_string(decklink_string_t input, std::string &output);
void decklink_free_string(decklink_string_t input);

// Platform abstraction for initialization/deinitialization
bool decklink_platform_init(void);
void decklink_platform_deinit(void);