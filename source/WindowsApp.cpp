#include <jde/App.h>
#include <Psapi.h>
#include "../../Framework/source/threading/InterruptibleThread.h"
#include "WindowsDrive.h"
#include "WindowsSvc.h"
#include "WindowsWorker.h"
#define var const auto

namespace Jde
{
	set<string> OSApp::Startup( int argc, char** argv, sv appName )noexcept(false)
	{
		IApplication::_pInstance = make_shared<OSApp>();
		return IApplication::_pInstance->BaseStartup( argc, argv, appName );	
	}
	bool OSApp::KillInstance( uint /*processId*/ )noexcept
	{
		CRITICAL( "Kill not implemented"sv );
		return false;
	}
	void OSApp::SetConsoleTitle( sv title )noexcept
	{
		::SetConsoleTitle( format("{}({})", title, ProcessId()).c_str() );
	}

	[[noreturn]] BOOL HandlerRoutine( DWORD  ctrlType )
	{
		Jde::GetDefaultLogger()->trace( "Caught signal %d", ctrlType );
		for( auto pThread : IApplication::GetBackgroundThreads() )
			pThread->Interrupt();
		for( auto pThread : IApplication::GetBackgroundThreads() )
			pThread->Join();
		exit( 1 ); 
//		unreachable... return TRUE;
	}
	void AddSignals2()noexcept(false)
	{
		if( !SetConsoleCtrlHandler(HandlerRoutine, TRUE) )
			THROW( EnvironmentException("Could not set control handler") );
	}
	void OSApp::AddSignals()noexcept(false)
	{
		AddSignals2();
	}

	size_t IApplication::MemorySize()noexcept
	{
		PROCESS_MEMORY_COUNTERS memCounter;
		/*BOOL result =*/ GetProcessMemoryInfo( ::GetCurrentProcess(), &memCounter, sizeof( memCounter ) );
		return memCounter.WorkingSetSize;
	}
	fs::path IApplication::Path()noexcept
	{
		char* szExeFileName[MAX_PATH]; 
		::GetModuleFileNameA( NULL, (char*)szExeFileName, MAX_PATH );
		return fs::path( (char*)szExeFileName );
	}
	string IApplication::HostName()noexcept
	{
		DWORD maxHostName = 1024;
		char hostname[1024];
		THROW_IF( !::GetComputerNameA(hostname, &maxHostName), Exception("GetComputerNameA failed") );

		return hostname;
	}
	uint IApplication::ProcessId()noexcept
	{
		return _getpid();
	}

	void IApplication::OnTerminate()noexcept
	{
		//TODO Implement
	}
	
	sp<IO::IDrive> IApplication::DriveApi()noexcept
	{
		return make_shared<IO::Drive::WindowsDrive>();
	}
	bool isService{false};
	bool OSApp::AsService()noexcept
	{
		isService = true;
		Windows::Service::ReportStatus( SERVICE_START_PENDING, NO_ERROR, 3000 );
		return true;
	}
	void OSApp::OSPause()noexcept
	{
		INFO( "Starting main thread loop...{}"sv, _getpid() );
		if( isService )
		{
			SERVICE_TABLE_ENTRY DispatchTable[] = {  { (char*)IApplication::ApplicationName().data(), (LPSERVICE_MAIN_FUNCTION)Windows::Service::Main },  { nullptr, nullptr }  };
			var success = StartServiceCtrlDispatcher( DispatchTable );//blocks?
			if( !success )
				Windows::Service::ReportEvent( "StartServiceCtrlDispatcher" ); 
		}
		else
			Windows::WindowsWorkerMain::Start( false );
	}
	string OSApp::GetEnvironmentVariable( sv variable )noexcept
	{
		char buffer[32767];
		string result;
		if( !::GetEnvironmentVariable(string(variable).c_str(), buffer, sizeof(buffer)) )
			DBG( "GetEnvironmentVariable('{}') failed return {}"sv, variable, ::GetLastError() );//ERROR_ENVVAR_NOT_FOUND=203
		else
			result = buffer;

		return result;
	}
	fs::path OSApp::ProgramDataFolder()noexcept
	{
		var env = GetEnvironmentVariable( "ProgramData" );
		return env.size() ? fs::path{env} : fs::path{};
	}
}