#include <jde/App.h>
#include <Psapi.h>
#include <shellapi.h>
#include <strsafe.h>
#include "../../Framework/source/threading/InterruptibleThread.h"
#include "WindowsDrive.h"
#include "WindowsSvc.h"
#include "WindowsWorker.h"
#include "WindowsUtilities.h"


#define var const auto

namespace Jde
{
	flat_set<string> OSApp::Startup( int argc, char** argv, sv appName, string serviceDescription )noexcept(false)
	{
		IApplication::_pInstance = make_shared<OSApp>();
		return IApplication::_pInstance->BaseStartup( argc, argv, appName, serviceDescription );	
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
		TRACE( "Caught signal {}", ctrlType );
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
			THROW( "Could not set control handler" );
	}
	void OSApp::AddSignals()noexcept(false)
	{
		AddSignals2();
	}

	size_t IApplication::MemorySize()noexcept
	{
		PROCESS_MEMORY_COUNTERS memCounter;
		/*BOOL result =*/ ::GetProcessMemoryInfo( ::GetCurrentProcess(), &memCounter, sizeof(memCounter) );
		return memCounter.WorkingSetSize;
	}
	fs::path IApplication::ExePath()noexcept
	{
		return fs::path( _pgmptr );
	}
	string IApplication::HostName()noexcept
	{
		DWORD maxHostName = 1024;
		char hostname[1024];
		THROW_IF( !::GetComputerNameA(hostname, &maxHostName), "GetComputerNameA failed" );

		return hostname;
	}
	uint OSApp::ProcessId()noexcept
	{
		return _getpid();
	}

	void IApplication::OnTerminate()noexcept
	{
		//TODO Implement
	}
	
	bool _isService{false};
	α OSApp::AsService()noexcept->bool 
	{
		_isService = true;
		Windows::Service::ReportStatus( SERVICE_START_PENDING, NO_ERROR, 3000 );
		return true;
	}
	
	up<flat_multimap<string,string>> _pArgs;
	α OSApp::Args()noexcept->flat_multimap<string,string>
	{
		if( !_pArgs )
		{
			_pArgs = make_unique<flat_multimap<string,string>>();
			int nArgs;
			LPWSTR* szArglist = ::CommandLineToArgvW( ::GetCommandLineW(), &nArgs );
			if( !szArglist )
				std::cerr << "CommandLineToArgvW failed\n";
		   else 
			{
				for( uint i=0; i<nArgs; ++i ) 
				{
					var current = Windows::ToString( szArglist[i] );
					if( current.starts_with('-') )
					{
						var value{ ++i<nArgs ? Windows::ToString(szArglist[i]) : string{} };
						if( value.starts_with('-') )
							--i;
						_pArgs->emplace( current, value );
					}
					else
						_pArgs->emplace( string{}, current );
				}
			   LocalFree(szArglist);
			}
		}
		return *_pArgs;
	}
	α OSApp::Executable()noexcept->fs::path
	{
		return fs::path{ Args().find( {} )->second };
	}

	α OSApp::UnPause()noexcept->void
	{
		Windows::WindowsWorkerMain::Stop();
	}

	α OSApp::Pause()noexcept->void
	{
		INFO( "Starting main thread loop...{}"sv, _getpid() );
		if( _isService )
		{
			SERVICE_TABLE_ENTRY DispatchTable[] = {  { (char*)IApplication::ApplicationName().data(), (LPSERVICE_MAIN_FUNCTION)Windows::Service::Main },  { nullptr, nullptr }  };
			var success = StartServiceCtrlDispatcher( DispatchTable );//blocks?
			if( !success )
				Windows::Service::ReportEvent( "StartServiceCtrlDispatcher" ); 
		}
		else
			Windows::WindowsWorkerMain::Start( false );
	}
	string _companyName;

//could get run before initialize logger.
#define CHECK_NOLOG(condition) if( !(condition) ) throw Jde::Exception{ SRCE_CUR, Jde::ELogLevel::NoLog, #condition }
	α LoadResource( sv key )noexcept->string
	{
		string y;
		try
		{
			DWORD _;
			var exe = OSApp::Executable().string();
			var size = ::GetFileVersionInfoSize( exe.c_str(), &_ ); CHECK_NOLOG( size );
			vector<BYTE> block( size );
			CHECK_NOLOG( ::GetFileVersionInfo(exe.c_str(), _, size, block.data()) );
			struct LANGANDCODEPAGE { WORD wLanguage; WORD wCodePage; } *lpTranslate; UINT cbTranslate;
			::VerQueryValue( block.data(), TEXT("\\VarFileInfo\\Translation"), (LPVOID*)&lpTranslate, &cbTranslate ); CHECK_NOLOG( (cbTranslate/sizeof(struct LANGANDCODEPAGE)) );
			char name[50];
			CHECK_NOLOG( SUCCEEDED(::StringCchPrintf(name, sizeof(name), format("\\StringFileInfo\\%04x%04x\\{}", key).c_str(),  lpTranslate[0].wLanguage, lpTranslate[0].wCodePage)) );
			char* pCompanyName; UINT bytes;
			::VerQueryValue( block.data(),  name, (LPVOID*)&pCompanyName, &bytes );
			y = sv{ pCompanyName, bytes-1 };
		}
		catch( IException& )
		{}
		return y;
	}

	α OSApp::CompanyName()noexcept->str
	{
		if(! _companyName.size() )
		{
			_companyName = LoadResource( "CompanyName" );
			if( _companyName.empty() )
				_companyName = "Jde-cpp";
		}
		return _companyName;
	}
	α OSApp::ProductName()noexcept->string
	{
		return LoadResource( "ProductName" );
	}
	α OSApp::CompanyRootDir()noexcept->fs::path{ return CompanyName(); }

	α IApplication::EnvironmentVariable( str variable, SL sl )noexcept->string
	{
		char buffer[32767];
		string result;
		if( !::GetEnvironmentVariable(string(variable).c_str(), buffer, sizeof(buffer)) )
		{
			var& _logLevel = Logging::TagLevel( "settings" );
			LOGSL( "GetEnvironmentVariable('{}') failed return {}", variable, ::GetLastError() );//ERROR_ENVVAR_NOT_FOUND=203
		}
		else
			result = buffer;

		return result;
	}
	fs::path IApplication::ProgramDataFolder()noexcept
	{
		var env = EnvironmentVariable( "ProgramData" );
		return env.size() ? fs::path{env} : fs::path{};
	}

	struct SCDeleter
	{
		void operator()(SC_HANDLE p){ if( p ) ::CloseServiceHandle(p); }
	};
	using ServiceHandle = std::unique_ptr<SC_HANDLE__, SCDeleter>;

	ServiceHandle MyOpenSCManager()noexcept(false)
	{
		auto schSCManager = ServiceHandle{ ::OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS) };
		THROW_IF( !schSCManager.get(), ::GetLastError()==ERROR_ACCESS_DENIED ? "installation requires administrative privliges." : format("OpenSCManager failed - {}", ::GetLastError()) );
		return schSCManager;
	}

	α OSApp::Install( str serviceDescription )noexcept(false)->void
	{
		auto schSCManager = MyOpenSCManager();
		//auto schSCManager = ::OpenSCManager( nullptr, nullptr, SC_MANAGER_ALL_ACCESS ); THROW_IF( schSCManager==nullptr, "OpenSCManager failed - {}", ::GetLastError() );
		const string serviceName{ ApplicationName() };
		auto service = ServiceHandle{ ::CreateService(schSCManager.get(), serviceName.c_str(), (serviceName).c_str(), SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, ExePath().string().c_str(), nullptr, nullptr, nullptr, nullptr, nullptr) }; 
		THROW_IF( !service.get(), ::GetLastError()==ERROR_SERVICE_EXISTS ? "Service allready exists." : format("CreateService failed - {}", ::GetLastError()) );
		if( serviceDescription.size() )
		{
			SERVICE_DESCRIPTION d{ (LPSTR)serviceDescription.c_str() };
			if( !::ChangeServiceConfig2A(service.get(), SERVICE_CONFIG_DESCRIPTION, &d) )
				std::cerr << "ChangeServiceConfig2A failed" << std::endl;
		}
		INFO( "service '{}' installed successfully"sv, serviceName );
	}
	α OSApp::Uninstall()noexcept(false)->void
	{
		auto manager = MyOpenSCManager();
		auto service = ServiceHandle{ ::OpenService(manager.get(), string{ApplicationName()}.c_str(), DELETE) }; 
		THROW_IF( !service.get(), ::GetLastError()==ERROR_SERVICE_DOES_NOT_EXIST ? format("Service '{}' not found.", ApplicationName()) : format("DeleteService '{}' failed - {}", ::GetLastError()) );
		THROW_IF( !::DeleteService(service.get()), "DeleteService failed" );

		INFO( "Service '{}' deleted successfully"sv, ApplicationName() ); 
	}
	α OSApp::LoadLibrary( path path )noexcept(false)->void*
	{
		auto p = ::LoadLibrary( path.string().c_str() ); THROW_IFX( !p, IOException(path, GetLastError(), "Can not load library") );
		INFO( "({})Opened"sv, path.string() );
		return p;
	}
	α OSApp::FreeLibrary( void* p )noexcept->void
	{
		::FreeLibrary( (HMODULE)p );
	}
	α OSApp::GetProcAddress( void* pModule, str procName )noexcept(false)->void*
	{
		auto p = ::GetProcAddress( (HMODULE)pModule, procName.c_str() ); CHECK( p );
		return p;
	}
}