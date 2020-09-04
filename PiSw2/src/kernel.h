
#ifndef _kernel_h
#define _kernel_h

#include <circle/memory.h>
#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/serial.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/usb/usbhcidevice.h>
#include <circle/sched/scheduler.h>
#include <circle/net/netsubsystem.h>
#include <circle/types.h>
#include "UartMaxiSerialDevice.h"
#include "CommsManager.h"
#include "CommandHandler.h"
#include "Hardware/HwManager.h"
#include "Machines/McManager.h"
#include "TargetBus/BusAccess.h"
#include "BusControlAPI/BusControlAPI.h"
#include "BusRaiderApp.h"
#include "Display.h"
#include "TargetBus/TargetProgrammer.h"

enum TShutdownMode
{
	ShutdownNone,
	ShutdownHalt,
	ShutdownReboot
};

class CKernel
{
public:
	CKernel (void);
	~CKernel (void);

	boolean Initialize (void);

	TShutdownMode Run (void);

private:
	// Display
	Display m_Display;

	// do not change this order
	CMemorySystem		m_Memory;
	CActLED			m_ActLED;
	CKernelOptions		m_Options;
	CDeviceNameService	m_DeviceNameService;
	CExceptionHandler	m_ExceptionHandler;
	CInterruptSystem	m_Interrupt;
	CUartMaxiSerialDevice		m_Serial;
	CTimer			m_Timer;
	CLogger			m_Logger;
	CUSBHCIDevice		m_USBHCI;
	CScheduler		m_Scheduler;
	CNetSubSystem		m_Net;

	// Pointer to singleton kernel
	static CKernel* m_pKernel;

	// Comms Manager
	CommsManager m_CommsManager;

	// BusAccess
	BusAccess m_BusAccess;

	// TargetProgrammer
	TargetProgrammer m_TargetProgrammer;
	
	// Hardware manager
	HwManager m_HwManager;

	// BusControlAPI
	BusControlAPI m_BusControlAPI;

	// Machine Manager
	McManager m_McManager;

	// BusRaiderApp
	BusRaiderApp m_BusRaiderApp;

};

#endif
