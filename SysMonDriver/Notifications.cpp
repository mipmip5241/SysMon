#include "Notifications.h"

#include "LockGuard.h"
#include "SysMon.h"
#include "SysMonCommon.h"
#include "FastMutex.h"

void add_event(LIST_ENTRY* entry);
void process_create(const HANDLE& process_id, const PPS_CREATE_NOTIFY_INFO& create_info);
void process_exit(const HANDLE& process_id);
void remote_thread_create(const HANDLE& process_id, const HANDLE& thread_id);
void thread_create(const HANDLE& process_id, const HANDLE& thread_id, const BOOLEAN& create);

void on_process_notify(PEPROCESS process, HANDLE process_id, PPS_CREATE_NOTIFY_INFO create_info)
{
	UNREFERENCED_PARAMETER(process);
	if (create_info)
	{
		process_create(process_id, create_info);
	}
	else
	{
		process_exit(process_id);
	}
}

void on_thread_notify(HANDLE process_id, HANDLE thread_id, BOOLEAN create)
{
	/* Checking if thread was remotely created */
	if (PsGetProcessId(PsGetCurrentProcess()) != process_id && create)
	{
		remote_thread_create(process_id, thread_id);
	}
	else
	{
		thread_create(process_id, thread_id, create);
	}
}

void on_image_load(PUNICODE_STRING image_name, HANDLE proc_id, PIMAGE_INFO image_info)
{
	if (!image_info->SystemModeImage)
	{
		auto info = static_cast<EventNode<ImageLoadInfo>*>(ExAllocatePoolWithTag(PagedPool, sizeof(EventNode<ImageLoadInfo>) + image_name->Length, constants::DRIVER_TAG));
		if (info == nullptr)
		{
			KdPrint(("%sAlloc fail", constants::DRIVER_PREFIX));
			return;
		}
		KeQuerySystemTimePrecise(&(info->event_data.time));
		info->event_data.type = EventType::image_load;
		info->event_data.proc_id = HandleToLong(proc_id);
		info->event_data.base_addr = reinterpret_cast<ULONG64>(image_info->ImageBase);
		info->event_data.size = sizeof(ImageLoadInfo) + image_name->Length;
		info->event_data.image_len = 0;
		info->event_data.image_offset = 0;
		if(image_name->Length > 0)
		{
			memcpy((reinterpret_cast<UCHAR*>(&info->event_data) + sizeof(info->event_data)), image_name->Buffer, image_name->Length);
			info->event_data.image_len = image_name->Length / sizeof(WCHAR);
			info->event_data.image_offset = sizeof(info->event_data);
		}

		add_event(&info->entry);
	}
}

void process_create(const HANDLE& process_id, const PPS_CREATE_NOTIFY_INFO& create_info)
{
	auto info = static_cast<EventNode<ProcessCreateInfo>*>(ExAllocatePoolWithTag(PagedPool, sizeof(EventNode<ProcessCreateInfo>) + create_info->CommandLine->Length + create_info->ImageFileName->Length, constants::DRIVER_TAG));
	if (info == nullptr)
	{
		KdPrint(("%sAlloc fail", constants::DRIVER_PREFIX));
		return;
	}

	KeQuerySystemTimePrecise(&info->event_data.time);
	info->event_data.type = EventType::process_create;
	info->event_data.size = sizeof(ProcessCreateInfo) + create_info->CommandLine->Length + create_info->ImageFileName->Length;
	info->event_data.proc_id = HandleToULong(process_id);
	info->event_data.parent_proc_id = HandleToULong(create_info->ParentProcessId);
	info->event_data.cmd_len = 0;
	info->event_data.cmd_offset = 0;
	info->event_data.image_len = 0;
	info->event_data.image_offset = 0;

	if(create_info->CommandLine->Length > 0)
	{
		memcpy((reinterpret_cast<UCHAR*>(&info->event_data) + sizeof(info->event_data)), create_info->CommandLine->Buffer, create_info->CommandLine->Length);
		info->event_data.cmd_len = create_info->CommandLine->Length / sizeof(WCHAR);
		info->event_data.cmd_offset = sizeof(info->event_data);
	}

	if (create_info->ImageFileName->Length > 0)
	{
		memcpy((reinterpret_cast<UCHAR*>(&info->event_data) + sizeof(info->event_data) + create_info->CommandLine->Length), create_info->ImageFileName->Buffer, create_info->ImageFileName->Length);
		info->event_data.image_len = create_info->ImageFileName->Length / sizeof(WCHAR);
		info->event_data.image_offset = sizeof(info->event_data) + create_info->CommandLine->Length;
	}

	add_event(&info->entry);
}


void process_exit(const HANDLE& process_id)
{
	auto info = static_cast<EventNode<ProcessExitInfo>*>(ExAllocatePoolWithTag(PagedPool, sizeof(EventNode<ProcessExitInfo>), constants::DRIVER_TAG));
	if (info == nullptr)
	{
		KdPrint(("%sAlloc fail", constants::DRIVER_PREFIX));
		return;
	}

	KeQuerySystemTimePrecise(&info->event_data.time);
	info->event_data.type = EventType::process_exit;
	info->event_data.proc_id = HandleToULong(process_id);
	info->event_data.size = sizeof(ProcessExitInfo);

	add_event(&info->entry);
}

/*
 * This function adds data to the event list.
 */
void add_event(LIST_ENTRY* entry)
{
	LockGuard lock(g_globals.fast_mutex);

	/* If list out of space remove oldest node and replace with a new one. */
	if(g_globals.event_count > constants::EVENT_LIMIT)
	{
		auto old_head = RemoveHeadList(&g_globals.head);
		g_globals.event_count--;
		auto data = CONTAINING_RECORD(old_head, EventNode<EventHeader>, entry);
		ExFreePool(data);
	}
	InsertTailList(&g_globals.head, entry);
	g_globals.event_count++;
}

void remote_thread_create(const HANDLE& process_id, const HANDLE& thread_id)
{
	auto info = static_cast<EventNode<RemoteThreadInfo>*>(ExAllocatePoolWithTag(PagedPool, sizeof(EventNode<ThreadInfo>), constants::DRIVER_TAG));
	if (info == nullptr)
	{
		KdPrint(("%sAlloc fail", constants::DRIVER_PREFIX));
		return;
	}

	KeQuerySystemTimePrecise(&(info->event_data.time));
	info->event_data.type = EventType::remote_thread;
	info->event_data.thread_id = HandleToULong(thread_id);
	info->event_data.proc_id = HandleToULong(PsGetProcessId(PsGetCurrentProcess()));
	info->event_data.creator_id = HandleToULong(process_id);
	info->event_data.size = sizeof(RemoteThreadInfo);

	add_event(&info->entry);

}
void thread_create(const HANDLE& process_id, const HANDLE& thread_id, const BOOLEAN& create)
{
	auto info = static_cast<EventNode<ThreadInfo>*>(ExAllocatePoolWithTag(PagedPool, sizeof(EventNode<ThreadInfo>), constants::DRIVER_TAG));
	if (info == nullptr)
	{
		KdPrint(("%sAlloc fail", constants::DRIVER_PREFIX));
		return;
	}

	KeQuerySystemTimePrecise(&(info->event_data.time));
	info->event_data.type = create ? EventType::thread_create : EventType::thread_exit;
	info->event_data.thread_id = HandleToULong(thread_id);
	info->event_data.proc_id = HandleToULong(process_id);
	info->event_data.size = sizeof(ThreadInfo);

	add_event(&info->entry);
}