#include "../../Framework/source/application/Application.h"
#include "../../Framework/source/threading/InterruptibleThread.h"

#define var const auto

namespace Jde
{
	set<string> OSApp::Startup( int argc, char** argv, string_view appName )noexcept(false)
	{
		IApplication::_pInstance = make_shared<OSApp>();
		return IApplication::_pInstance->BaseStartup( argc, argv, appName );	
	}
	bool OSApp::KillInstance( uint /*processId*/ )noexcept
	{
		CRITICAL0( "Kill not implemented"sv );
		return false;
	}
	void OSApp::SetConsoleTitle( string_view title )noexcept
	{
		::SetConsoleTitle( string(title).c_str() );
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

	void IApplication::OnTerminate()noexcept
	{
		//TODO Implement
	}

	bool OSApp::AsService()noexcept
	{
		std::cerr << "AsService not implemented use -c switch" << std::endl;
		CRITICAL0("AsService not implemented"sv);
		throw "not implemented";
	}
	void OSApp::OSPause()noexcept
	{
		INFO( "Pausing main thread. {}"sv, _getpid() );
		std::this_thread::sleep_for(9000h); 
	}
	string OSApp::GetEnvironmentVariable( string_view variable )noexcept
	{
		char buffer[32767];
		string result;
		if( !::GetEnvironmentVariable(string(variable).c_str(), buffer, sizeof(buffer)) )
			DBG( "GetEnvironmentVariable('{}') failed return {}"sv, variable, ::GetLastError() );
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