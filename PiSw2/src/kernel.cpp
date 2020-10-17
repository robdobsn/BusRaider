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
#include <circle/usb/usbkeyboard.h>
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
	m_BusRaiderApp(m_Display, m_Serial)
{
	m_pKernel = this;
	_rebootRequested = false;
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
		bOK = m_Serial.Initialize (921600, 1000000, 1000000);
	}

	if (bOK)
	{
		bOK = m_Timer.Initialize ();
	}

	if (bOK)
	{
		// TODO 2020
		// NOTE: lines 158..165 of usbdevice.cpp are currently commented out and need to be for USB keyboards to work
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

	CUSBKeyboardDevice *pKeyboard = (CUSBKeyboardDevice *) m_DeviceNameService.GetDevice ("ukbd1", FALSE);
	if (pKeyboard == 0)
	{
		m_Logger.Write (MODULE_PREFIX, LogError, "Keyboard not found");
		m_BusRaiderApp.peripheralStatus(true, false);
	}
	else
	{
		m_BusRaiderApp.peripheralStatus(true, true);
	}

	pKeyboard->RegisterKeyStatusHandlerRaw(KeyStatusHandlerRaw);

#ifdef USE_OTA_COPY
	// Main loop
	while (!m_CommsManager.otaIsPending() && !_rebootRequested)
	{
		// Service comms
		m_BusRaiderApp.service();
	}

	// // Diable peripherals
	// pKeyboard->RemoveDevice();
	// m_Serial.RemoveDevice();
	// m_Interrupt.DisableIRQ(0);
	// m_Interrupt.DisableFIQ();

	// Check for OTA pending
	if (m_CommsManager.otaIsPending())
	{
		// Enable chain boot
		EnableChainBoot(m_CommsManager.otaBuffer(), m_CommsManager.otaBufLen());
		DoChainBoot();
	}
	
	// m_Logger.Write (MODULE_PREFIX, LogNotice, "Rebooting ...");

	// m_Scheduler.Sleep (1);

#else
	while (!IsChainBootEnabled() && !_rebootRequested)
	{
		m_BusRaiderApp.service();
	}

	m_Logger.Write (MODULE_PREFIX, LogNotice, "Rebooting ...");
#endif

	m_Scheduler.Sleep (1);

	return ShutdownReboot;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Keypresses
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CKernel::KeyStatusHandlerRaw(unsigned char ucModifiers, const unsigned char rawKeys[6])
{
	m_pKernel->m_BusRaiderApp.keyStatusHandlerRaw(ucModifiers, rawKeys);
}

