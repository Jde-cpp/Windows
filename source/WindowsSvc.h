#pragma once

namespace Jde::Windows::Service
{
	void ReportStatus( DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint )noexcept;
	void ReportEvent( sv function )noexcept;
	void Main( DWORD dwArgc, LPTSTR *lpszArgv )noexcept;
}