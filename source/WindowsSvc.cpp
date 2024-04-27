﻿#include "WindowsSvc.h"
#include <jde/App.h>
#include "WindowsWorker.h"

#define var const auto
constexpr DWORD SVC_ERROR=0xC0020001L;

namespace Jde::Windows
{
	static const sp<LogTag> _logTag = Logging::Tag( "settings" );
	SERVICE_STATUS gSvcStatus{}; 
	SERVICE_STATUS_HANDLE gSvcStatusHandle{}; 
	//HANDLE ghSvcStopEvent = nullptr;
	DWORD dwCheckPoint{0};

	void WINAPI SvcCtrlHandler( DWORD dwCtrl )
	{
		switch(dwCtrl) 
		{  
		case SERVICE_CONTROL_STOP: 
			Service::ReportStatus( SERVICE_STOP_PENDING, NO_ERROR, 0 );
			WindowsWorkerMain::Stop( SERVICE_CONTROL_STOP );
			Service::ReportStatus( gSvcStatus.dwCurrentState, NO_ERROR, 0 );
			return;
		case SERVICE_CONTROL_INTERROGATE: 
			break; 
		default: 
			break;
		} 
	}

	void Service::ReportStatus( DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint )ι
	{
		 // Fill in the SERVICE_STATUS structure.

		 gSvcStatus.dwCurrentState = dwCurrentState;
		 gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
		 gSvcStatus.dwWaitHint = dwWaitHint;

		 gSvcStatus.dwControlsAccepted = dwCurrentState == SERVICE_START_PENDING ? 0 : SERVICE_ACCEPT_STOP;

		 gSvcStatus.dwCheckPoint = dwCurrentState == SERVICE_RUNNING || dwCurrentState == SERVICE_STOPPED ? 0 : ++dwCheckPoint;

		 // Report the status of the service to the SCM.
		 SetServiceStatus( gSvcStatusHandle, &gSvcStatus );
	}

	void Service::ReportEvent( sv function )ι
	{ 
		string buffer = Jde::format( "{} failed with {}", function, GetLastError() );
		HANDLE hEventSource = ::RegisterEventSource( nullptr, string{IApplication::ApplicationName()}.c_str() ); RETURN_IF( !hEventSource, ELogLevel::Critical, "RegisterEventSource returned null");
		const char* lpszStrings[2] = { IApplication::ApplicationName().data(), buffer.data() };
		::ReportEvent( hEventSource, EVENTLOG_ERROR_TYPE, 0, SVC_ERROR, nullptr, 2, 0, lpszStrings, nullptr );
		DeregisterEventSource( hEventSource );
	}

	void Service::Main( DWORD /*dwArgc*/, char** /*lpszArgv*/ )ι
	{
		 gSvcStatusHandle = RegisterServiceCtrlHandler( string{IApplication::ApplicationName()}.c_str(),  SvcCtrlHandler );
		 if( !gSvcStatusHandle )
			  return Service::ReportEvent( "RegisterServiceCtrlHandler" );

		 gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS; 
		 gSvcStatus.dwServiceSpecificExitCode = 0;    
		 WindowsWorkerMain::Start( true );
	}
}