#pragma once

enum class EventType : short
{
	process_create=1,
	process_exit,
	thread_create,
	thread_exit,
	remote_thread,
	image_load
};


struct EventHeader
{
	EventType type;
	USHORT size;
	LARGE_INTEGER time;
};

struct ProcessExitInfo : EventHeader
{
	ULONG proc_id;
};

struct ProcessCreateInfo: EventHeader
{
	ULONG proc_id;
	ULONG parent_proc_id;
	USHORT cmd_len;
	USHORT cmd_offset;
	USHORT image_len;
	USHORT image_offset;
};

struct ThreadInfo : EventHeader
{
	ULONG thread_id;
	ULONG proc_id;
};

struct RemoteThreadInfo : ThreadInfo
{
	ULONG creator_id;
};

struct ImageLoadInfo : EventHeader
{
	ULONG proc_id;
	ULONG64 base_addr;
	USHORT image_len;
	USHORT image_offset;;
};