#include "../../Framework/source/application/Application.h"
#include "../../Framework/source/threading/InterruptibleThread.h"

namespace Jde
{
	set<string> OSApp::Startup( int argc, char** argv, string_view appName )noexcept
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
		CRITICAL0("AsService not implemented"sv);
		throw "not implemented";
	}
	void OSApp::OSPause()noexcept
	{
		INFO( "Pausing main thread. {}"sv, _getpid() );
		std::this_thread::sleep_for(9000h); 
	}
}