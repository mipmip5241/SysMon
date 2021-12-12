#include <Windows.h>

#include <string>
#include "../SysMonDriver/SysMonCommon.h"


int print_error(const char* msg);
void display_info(BYTE* buffer, DWORD count);
void display_time(const LARGE_INTEGER& time);
std::string get_image_name(DWORD process_id);

int main()
{
	HANDLE file_handle = CreateFile(L"\\\\.\\sysmon", GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (file_handle == INVALID_HANDLE_VALUE)
		return print_error("Failed to open file");

	BYTE buffer[1 << 16]{};
	DWORD bytes = 0;
	while (true)
	{
		if (!ReadFile(file_handle, buffer, sizeof(buffer), &bytes, nullptr))
			return print_error("Failed to read");

		if (bytes != 0)
			display_info(buffer, bytes);

		Sleep(1000);
		bytes = 0;
	}

}

void display_info(BYTE* buffer, DWORD count)
{

	while (count > 0)
	{
		const auto header = reinterpret_cast<EventHeader*>(buffer);
		switch (header->type)
		{
			case EventType::process_exit:
			{
				display_time(header->time);
				const auto info = reinterpret_cast<ProcessExitInfo*>(buffer);
				printf("Process %lu Exited\n", info->proc_id);
				break;
			}
			case EventType::process_create:
			{
				display_time(header->time);
				const auto info = reinterpret_cast<ProcessCreateInfo*>(buffer);
				std::wstring cmd(reinterpret_cast<WCHAR*>(buffer + info->cmd_offset), info->cmd_len);
				std::wstring image_file(reinterpret_cast<WCHAR*>(buffer + info->image_offset), info->image_len);
				printf("Process %lu Created. Command line: %ls. Image file: %ls\n", info->proc_id, cmd.c_str(), image_file.c_str());
				break;
			}
			case EventType::thread_create:
			{
				display_time(header->time);
				const auto info = reinterpret_cast<ThreadInfo*>(buffer);
				printf("Thread %lu Created in process %lu %s\n", info->thread_id, info->proc_id, get_image_name(info->proc_id).c_str());
				break;
			}
			case EventType::thread_exit:
			{
				display_time(header->time);
				const auto info = reinterpret_cast<ThreadInfo*>(buffer);
				printf("Thread %lu Exited from process %lu %s\n", info->thread_id, info->proc_id, get_image_name(info->proc_id).c_str());
				break;
			}
			case EventType::remote_thread:
			{
				display_time(header->time);
				const auto info = reinterpret_cast<RemoteThreadInfo*>(buffer);
				printf("Thread %lu remotely created by pid: %lu %s for pid: %lu %s\n", info->thread_id, info->creator_id, get_image_name(info->creator_id).c_str(),info->proc_id, get_image_name(info->proc_id).c_str());
				break;
			}
			case EventType::image_load:
			{
				display_time(header->time);
				const auto info = reinterpret_cast<ImageLoadInfo*>(buffer);
				std::wstring image_file(reinterpret_cast<WCHAR*>(buffer + info->image_offset), info->image_len);
				printf("Image name: %ls Image base address: %llu process: %lu\n", image_file.c_str(), info->base_addr, info->proc_id);
				break;
			}
		}

		buffer += header->size;
		count -= header->size;
	}
}


void display_time(const LARGE_INTEGER& time)
{
	SYSTEMTIME st;
	FileTimeToSystemTime(reinterpret_cast<const FILETIME*>(&time), &st);
	printf("%02d:%02d:%02d.%03d: ",st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

int print_error(const char* msg)
{
	printf("%s error code = %lu\n", msg, GetLastError());
	return 1;
}

std::string get_image_name(DWORD process_id)
{
	std::string ret;
	HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
	if (handle)
	{
		DWORD buffSize = 1024;
		CHAR buffer[1024];
		if (QueryFullProcessImageNameA(handle, 0, buffer, &buffSize))
		{
			ret += "Image: ";
			ret += buffer;
		}
		CloseHandle(handle);
	}
	return ret;
}