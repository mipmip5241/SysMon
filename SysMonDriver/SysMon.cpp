#include <ntddk.h>

#include "SysMonCommon.h"
#include  "SysMon.h"
#include "IrpHandler.h"
#include "LockGuard.h"
#include  "Notifications.h"
#include  "FastMutex.h"

DRIVER_UNLOAD monitor_unload;

DRIVER_DISPATCH monitor_create_close, monitor_read;


extern "C"
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING)
{
	auto status = STATUS_SUCCESS;

	InitializeListHead(&g_globals.head);
	g_globals.fast_mutex.init();

	PDEVICE_OBJECT device_object = nullptr;
	UNICODE_STRING sym_link = RTL_CONSTANT_STRING(SYM_LINK_NAME);
	bool sym_link_valid = false;
	bool proc_cb = false;
	bool thread_cb = false;
	bool image_load_cb = false;
	do
	{
		/* Create device. */
		UNICODE_STRING device_name = RTL_CONSTANT_STRING(DEVICE_NAME);
		status = IoCreateDevice(DriverObject, 0, &device_name, FILE_DEVICE_UNKNOWN, 0, TRUE, &device_object);
		if(!NT_SUCCESS(status))
		{
			KdPrint(( "%sfailed to create device (0x%08X)\n", constants::DRIVER_PREFIX, status));
			break;
		}
		device_object->Flags |= DO_DIRECT_IO;

		status = IoCreateSymbolicLink(&sym_link, &device_name);
		if(!NT_SUCCESS(status))
		{
			KdPrint(("%sfailed to create sym link (0x%08X)\n", constants::DRIVER_PREFIX, status));
			break;
		}
		sym_link_valid = true;

		/* Define callback functions. */
		status = PsSetCreateProcessNotifyRoutineEx(on_process_notify, FALSE);
		if(!NT_SUCCESS(status))
		{
			KdPrint(("%sfailed to create process notifications (0x%08X)\n", constants::DRIVER_PREFIX, status));
			break;
		}
		proc_cb = true;

		status = PsSetCreateThreadNotifyRoutine(on_thread_notify);
		if(!NT_SUCCESS(status))
		{
			KdPrint(("%sfailed to create thread notifications (0x%08X)\n", constants::DRIVER_PREFIX, status));
			break;
		}
		thread_cb = true;

		status = PsSetLoadImageNotifyRoutine(on_image_load);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("%sfailed to create image load notifications (0x%08X)\n", constants::DRIVER_PREFIX, status));
			break;
		}
		image_load_cb = true;

	} while (false);

	if(!NT_SUCCESS(status))
	{
		if (sym_link_valid)
			IoDeleteSymbolicLink(&sym_link);

		if (device_object)
			IoDeleteDevice(device_object);

		if (proc_cb)
			PsSetCreateProcessNotifyRoutineEx(on_process_notify, TRUE);

		if (thread_cb)
			PsRemoveCreateThreadNotifyRoutine(on_thread_notify);

		if (image_load_cb)
			PsRemoveLoadImageNotifyRoutine(on_image_load);
	}
	
	
	DriverObject->DriverUnload = monitor_unload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverObject->MajorFunction[IRP_MJ_CLOSE] = monitor_create_close;
	DriverObject->MajorFunction[IRP_MJ_READ] = monitor_read;

	KdPrint(("%sdriver loaded\n", constants::DRIVER_PREFIX));

	return status;
}


void monitor_unload(PDRIVER_OBJECT DriverObject)
{
	PsSetCreateProcessNotifyRoutineEx(on_process_notify, TRUE);
	PsRemoveCreateThreadNotifyRoutine(on_thread_notify);
	PsRemoveLoadImageNotifyRoutine(on_image_load);

	UNICODE_STRING sym_link = RTL_CONSTANT_STRING(SYM_LINK_NAME);
	IoDeleteSymbolicLink(&sym_link);

	IoDeleteDevice(DriverObject->DeviceObject);

	/* Free remaining entries. */
	while(!IsListEmpty(&g_globals.head))
		ExFreePool(RemoveHeadList(&g_globals.head));

	KdPrint(("%sdriver unloaded\n", constants::DRIVER_PREFIX));
}

NTSTATUS monitor_create_close(PDEVICE_OBJECT, PIRP Irp)
{
	const IrpHandler handler (Irp);
	return handler.get_status();
}

NTSTATUS monitor_read(PDEVICE_OBJECT, PIRP Irp)
{
	IrpHandler handler(Irp);
	NT_ASSERT(Irp->MdlAddress);
	auto buffer = static_cast<UCHAR*>(MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority));
	auto len = handler.get_parameters_len(IRP_MJ_READ);
	int count = 0;
	if (buffer == nullptr)
	{
		handler.set_status(STATUS_INSUFFICIENT_RESOURCES);
	}
	else
	{
		LockGuard lock(g_globals.fast_mutex);
		while (g_globals.event_count > 0)
		{
			/* Get first node (oldest). */
			auto entry = RemoveHeadList(&g_globals.head);
			auto info = CONTAINING_RECORD(entry, EventNode<EventHeader>, entry);
			auto size = info->event_data.size;

			/* Check if buffer has space */
			if(len < size)
			{
				InsertHeadList(&g_globals.head, entry);
				break;
			}

			g_globals.event_count--;

			/* Add data to buffer */
			memcpy(buffer, &info->event_data, size);

			len -= size;
			buffer += size;
			count += size;

			ExFreePool(info);
		}
	}

	handler.set_info(count);
	return handler.get_status();
}