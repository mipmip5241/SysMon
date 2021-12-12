#include "IrpHandler.h"

IrpHandler::IrpHandler(PIRP& irp)
: _irp(irp), _stack(IoGetCurrentIrpStackLocation(irp)), _status(STATUS_SUCCESS), _info(0) {}

IrpHandler::~IrpHandler()
{
    this->_irp->IoStatus.Status = this->_status;
    this->_irp->IoStatus.Information = this->_info;
    IoCompleteRequest(this->_irp,IO_NO_INCREMENT);
}

ULONG IrpHandler::get_parameters_len(const int irp_type) const
{
	switch (irp_type)
	{
		case IRP_MJ_READ:
			return this->_stack->Parameters.Read.Length;

		default:
			return 0;
	}
}

NTSTATUS IrpHandler::get_status() const
{
	return this->_status;
}

void IrpHandler::set_status(const NTSTATUS status)
{
	this->_status = status;
}

void IrpHandler::set_info(const ULONG_PTR info)
{
	this->_info = info;
}
