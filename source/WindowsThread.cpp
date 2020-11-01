#include <processthreadsapi.h>
#include <codecvt>
#include "../../Framework/source/threading/Thread.h"
#include "../../Framework/source/threading/Thread.cpp"

namespace Jde
{
	typedef HRESULT (WINAPI *TSetThreadDescription)( HANDLE, PCWSTR );
	TSetThreadDescription pSetThreadDescription = nullptr;

	typedef HRESULT (WINAPI *TGetThreadDescription)( HANDLE, PWSTR* );
	TGetThreadDescription pGetThreadDescription = nullptr;
	 
	void Initialize()noexcept
	{
		HMODULE hKernelBase = GetModuleHandleA("KernelBase.dll");
		if( hKernelBase )
		{
			pSetThreadDescription = reinterpret_cast<TSetThreadDescription>( ::GetProcAddress(hKernelBase, "SetThreadDescription") );
			if( !pSetThreadDescription )
				ERR( "FATAL: failed to get SetThreadDescription() address, error:  {}"sv, ::GetLastError() );
			pGetThreadDescription = reinterpret_cast<TGetThreadDescription>( ::GetProcAddress(hKernelBase, "GetThreadDescription") );
			if( !pGetThreadDescription )
				ERR( "FATAL: failed to get GetThreadDescription() address, error:  {}"sv, ::GetLastError() );
		}
		else
			ERR( "FATAL: failed to get kernel32.dll module handle, error:  {}"sv, ::GetLastError() );
	}
	void WinSetThreadDscrptn( HANDLE h, std::string_view ansiDescription )noexcept
	{
		//std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		//std::wstring wdescription = converter.from_bytes( description );
		std::wstring description; description.reserve( ansiDescription.size() );
		::MultiByteToWideChar( CP_ACP, MB_COMPOSITE, ansiDescription.data(), (int)ansiDescription.size(), description.data(), (int)ansiDescription.size() );
		if( !pSetThreadDescription )
			Initialize();
		if( pSetThreadDescription )
		{
			HRESULT hr = (*pSetThreadDescription)( h, description.c_str() );
			if( FAILED(hr) )
				WARN( "Could not set name for thread({}) {} - ({}) - {} "sv, h, ansiDescription, hr, ::GetLastError() );
		}
	}

		uint Threading::GetThreadId()noexcept
		{
			return ThreadId ? ThreadId : ThreadId = ::GetCurrentThreadId();
		}

		void Threading::SetThreadDscrptn( std::thread& thread, std::string_view pszDescription )noexcept
		{
			WinSetThreadDscrptn( static_cast<HANDLE>(thread.native_handle()), pszDescription );
		}
		void Threading::SetThreadDscrptn( const std::string& description )noexcept
		{
			WinSetThreadDscrptn( ::GetCurrentThread(), description );
		}

		const char* Threading::GetThreadDescription()noexcept
		{
			if( std::strlen(ThreadName)==0 )
			{
				PWSTR pszThreadDescription;
				HANDLE threadId = 0;
				threadId = reinterpret_cast<HANDLE>( (UINT_PTR)::GetCurrentThreadId() );
				if( !pGetThreadDescription )
					Initialize();
				if( !pGetThreadDescription )
					return ThreadName;
				var hr = (*pGetThreadDescription)( threadId, &pszThreadDescription );
				if( SUCCEEDED(hr) )
				{
					var size = wcslen(pszThreadDescription);
					//auto pDescription = unique_ptr<char[]>( new char[ size ] );
					auto pDescription = make_unique<char[]>( 12 );
					uint size2;
					wcstombs_s( &size2, pDescription.get(), size, pszThreadDescription, size );
					::LocalFree( pszThreadDescription );
					std::strncpy( ThreadName, pDescription.get(), sizeof(ThreadName)/sizeof(ThreadName[0]) );
				}
			}
			return ThreadName;
		}
}