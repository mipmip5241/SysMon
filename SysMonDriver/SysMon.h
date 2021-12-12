#pragma once
#include "FastMutex.h"

#define DEVICE_NAME L"\\Device\\sysmon"
#define SYM_LINK_NAME L"\\??\\sysmon"

template<typename  T>
struct EventNode
{
	LIST_ENTRY entry;
	T event_data;
};

struct Globals
{
	LIST_ENTRY head;
	int event_count;
	FastMutex fast_mutex;
};

namespace constants
{
	constexpr  char*  DRIVER_PREFIX = "SysMon: ";
	constexpr auto DRIVER_TAG = 'smn';
	constexpr int EVENT_LIMIT = 1024;
}

inline Globals g_globals;