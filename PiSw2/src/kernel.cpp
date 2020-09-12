/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BusRaider
// Using Circle bare-metal Pi library
//
// Rob Dobson 2016-2020
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "kernel.h"
#include "httpbootserver.h"
#include "tftpbootserver.h"
#include <circle/chainboot.h>
#include <circle/sysconfig.h>
#include <assert.h>
#include <stdint.h>
#include "string.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Globals
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Send log data to display (as opposed to merging in the ESP32 log output)
// #define LOG_TO_DISPLAY 1

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Consts
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// #define IMPLEMENT_NETWORK_CONN

#ifdef IMPLEMENT_NETWORK_CONN
#define HTTP_BOOT_PORT		8080
// Network configuration
#define USE_DHCP
#ifndef USE_DHCP
static const u8 IPAddress[]      = {192, 168, 0, 250};
static const u8 NetMask[]        = {255, 255, 255, 0};
static const u8 DefaultGateway[] = {192, 168, 0, 1};
static const u8 DNSServer[]      = {192, 168, 0, 1};
#endif
#endif

// Debug
static const char MODULE_PREFIX[] = "kernel";

// Singleton kernel
CKernel* CKernel::m_pKernel = 0;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CKernel::CKernel (void)
:	m_Display(),
	m_Serial (&m_Interrupt, false),
	m_Timer (&m_Interrupt),
	m_Logger (m_Options.GetLogLevel (), &m_Timer),
	m_USBHCI (&m_Interrupt, &m_Timer),
#ifdef IMPLEMENT_NETWORK_CONN
#ifndef USE_DHCP
	, m_Net (IPAddress, NetMask, DefaultGateway, DNSServer)
#endif
#endif
	m_CommsManager(&m_Serial, NULL),
	m_BusAccess(),
	m_TargetProgrammer(),
	m_HwManager(m_CommsManager.getCommandHandler(), m_BusAccess),
	m_BusControlAPI(m_CommsManager.getCommandHandler(), m_HwManager, m_BusAccess),
	m_McManager(&m_Display, m_CommsManager.getCommandHandler(), m_HwManager, m_BusAccess, m_TargetProgrammer),
	m_BusRaiderApp(m_Display, m_CommsManager, m_McManager)
{
	m_pKernel = this;
	// m_ActLED.Blink (5);	// show we are alive
}

CKernel::~CKernel (void)
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Init
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

boolean CKernel::Initialize (void)
{
	boolean bOK = TRUE;

	if (bOK)
	{
		bOK = m_Interrupt.Initialize ();
	}

	if (bOK)
	{
		bOK = m_Display.init();
	}

	if (bOK)
	{
		CDevice *pTarget = m_DeviceNameService.GetDevice (m_Options.GetLogDevice (), FALSE);
		if (pTarget == 0)
		{
			pTarget = &m_Display;
		}

		bOK = m_Logger.Initialize (pTarget);
	}

	if (bOK)
	{
		bOK = m_Serial.Initialize (460800, 1000000, 1000000);
	}

	if (bOK)
	{
		bOK = m_Timer.Initialize ();
	}

	if (bOK)
	{
		bOK = m_USBHCI.Initialize ();
	}

#ifdef IMPLEMENT_NETWORK_CONN
	if (bOK)
	{
		bOK = m_Net.Initialize ();
	}
#endif

	if (bOK)
	{
		bOK = m_CommsManager.setup();
		m_BusAccess.init();
		m_TargetProgrammer.init();
		m_HwManager.init();
		m_BusControlAPI.init();
		m_McManager.init();
		m_BusRaiderApp.init();
	}

	return bOK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Run
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TShutdownMode CKernel::Run (void)
{
	m_Logger.Write(MODULE_PREFIX, LogNotice, "Compile time: " __DATE__ " " __TIME__);

	// Main loop
	for (unsigned nCount = 0; !IsChainBootEnabled(); nCount++)
	{
		// Screen alive indicator
		m_Display.rotor (0, micros() / 100000);

		// Service comms
		m_CommsManager.service();
		m_BusAccess.service();
		m_TargetProgrammer.service();
		m_HwManager.service();
		m_BusControlAPI.service();
		m_McManager.service();
		m_BusRaiderApp.service();
	}

	m_Logger.Write (MODULE_PREFIX, LogNotice, "Rebooting ...");

	m_Scheduler.Sleep (1);

	return ShutdownReboot;
}
