#include <processthreadsapi.h>
#include <codecvt>
#include "../../Framework/source/threading/Thread.h"
#include "../../Framework/source/threading/Thread.cpp"

typedef struct tagTHREADNAME_INFO  
{  
    DWORD dwType; // Must be 0x1000.  
    LPCSTR szName; // Pointer to name (in user addr space).  
    DWORD dwThreadID; // Thread ID (-1=caller thread).  
    DWORD dwFlags; // Reserved for future use, must be zero.  
 } THREADNAME_INFO;
constexpr const DWORD MS_VC_EXCEPTION = 0x406D1388;  

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
	void SetThreadName( HANDLE h, const char* ansiDescription )noexcept
	{
		THREADNAME_INFO info{ 0x1000, ansiDescription, GetThreadId(h), 0 };
		//info.dwType=0x1000;
		//info.szName = ansiDescription;
		//info.dwThreadID = GetThreadId( h );
		//info.dwFlags = 0;
		__try
		{
			RaiseException( MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info );
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{}  
	}
	void WinSetThreadDscrptn( HANDLE h, std::string_view ansiDescription )noexcept
	{
		//std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		//std::wstring wdescription = converter.from_bytes( description );
		int wcharCount = ::MultiByteToWideChar( CP_UTF8, 0, string{ansiDescription}.c_str() , -1, NULL , 0 );
		auto p = new wchar_t[wcharCount];
		::MultiByteToWideChar( CP_UTF8, 0, ansiDescription.data(), -1, p, wcharCount );
		if( !pSetThreadDescription )
			Initialize();
		if( pSetThreadDescription )
		{
			HRESULT hr = (*pSetThreadDescription)( h, p );
			if( FAILED(hr) )
				WARN( "Could not set name for thread({}) {} - ({}) - {} "sv, h, ansiDescription, hr, ::GetLastError() );
		}
		SetThreadName( h, string{ansiDescription}.c_str() );
		var x = Threading::GetThreadDescription();
		delete[] p;
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
				//HANDLE threadId = reinterpret_cast<HANDLE>( (UINT_PTR)::GetCurrentThreadId() );
				var threadId = ::GetCurrentThread();
				if( !pGetThreadDescription )
					Initialize();
				if( !pGetThreadDescription )
					return ThreadName;
				var hr = (*pGetThreadDescription)( threadId, &pszThreadDescription );
				if( SUCCEEDED(hr) )
				{
					var size = wcslen(pszThreadDescription);
					//auto pDescription = unique_ptr<char[]>( new char[ size ] );
					auto pDescription = make_unique<char[]>( size+1 );
					uint size2;
					wcstombs_s( &size2, pDescription.get(), size+1, pszThreadDescription, size );
					::LocalFree( pszThreadDescription );
					std::strncpy( ThreadName, pDescription.get(), sizeof(ThreadName)/sizeof(ThreadName[0]) );
				}
			}
			return ThreadName;
		}
}