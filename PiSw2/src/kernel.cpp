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
// Consts
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const char* EMUPI_VERSION = "0.1.1";

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
:	m_Screen (m_Options.GetWidth (), m_Options.GetHeight ()),
	m_Serial (&m_Interrupt, false),
	m_Timer (&m_Interrupt),
	m_Logger (m_Options.GetLogLevel (), &m_Timer),
	m_USBHCI (&m_Interrupt, &m_Timer),
#ifdef IMPLEMENT_NETWORK_CONN
#ifndef USE_DHCP
	, m_Net (IPAddress, NetMask, DefaultGateway, DNSServer)
#endif
#endif
	// m_I2CMonitor(&m_Interrupt, m_CommsManager),
	// m_CommsManager(&m_Serial, AppSerialIF - derived)
	m_CommsManager(&m_Serial, NULL)
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
		bOK = m_Screen.Initialize ();
	}

	if (bOK)
	{
		CDevice *pTarget = m_DeviceNameService.GetDevice (m_Options.GetLogDevice (), FALSE);
		if (pTarget == 0)
		{
			pTarget = &m_Screen;
		}

		bOK = m_Logger.Initialize (pTarget);
		if (bOK)
			m_Logger.Write("****** BusRaider", LogNotice, "%s Rob Dobson 2020 ******", EMUPI_VERSION);
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
	}

	// if (bOK)
	// {
	// 	bOK = m_I2CMonitor.setup();
	// }
	
	return bOK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Run
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TShutdownMode CKernel::Run (void)
{
	m_Logger.Write (MODULE_PREFIX, LogNotice, "Compile time: " __DATE__ " " __TIME__);

	// Start I2CMonitor
	// m_I2CMonitor.start();

	// Opening message
#ifdef IMPLEMENT_NETWORK_CONN
	CString IPString;
	m_Net.GetConfig ()->GetIPAddress ()->Format (&IPString);
	m_Logger.Write (MODULE_PREFIX, LogNotice, "Open \"http://%s:%u/\" in your web browser!",
			(const char *) IPString, HTTP_BOOT_PORT);
	m_Logger.Write (MODULE_PREFIX, LogNotice,
			"Try \"tftp -m binary %s -c put kernel.img\" from another computer!",
			(const char *) IPString);

	// Servers
	new CHTTPBootServer (&m_Net, HTTP_BOOT_PORT, KERNEL_MAX_SIZE + 2000);
	new CTFTPBootServer (&m_Net, KERNEL_MAX_SIZE);
#endif

	// Main loop
	for (unsigned nCount = 0; !IsChainBootEnabled (); nCount++)
	{
		// Screen alive indicator
		m_Screen.Rotor (0, micros() / 100000);

		// Service comms
		m_CommsManager.service();

		// // I2C Service
		// m_I2CMonitor.service();
	}

	m_Logger.Write (MODULE_PREFIX, LogNotice, "Rebooting ...");

	m_Scheduler.Sleep (1);

	return ShutdownReboot;
}
