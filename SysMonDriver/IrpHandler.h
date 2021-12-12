#pragma once

#include <ntddk.h>

class IrpHandler
{
public:
	IrpHandler(PIRP& irp);
	~IrpHandler();

	ULONG  get_parameters_len(int irp_type) const;
	NTSTATUS get_status() const;
	void set_status(NTSTATUS status);
	void set_info(ULONG_PTR info);

private:
	PIRP& _irp;
	PIO_STACK_LOCATION _stack;
	NTSTATUS _status;
	ULONG_PTR _info;
};

