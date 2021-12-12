#pragma once

#include <ntddk.h>


void on_process_notify(PEPROCESS process, HANDLE process_id, PPS_CREATE_NOTIFY_INFO create_info);
void on_thread_notify(HANDLE process_id, HANDLE thread_id, BOOLEAN create);
void on_image_load(PUNICODE_STRING image_name, HANDLE proc_id, PIMAGE_INFO image_info);