#pragma once

namespace Jde::Windows::Service
{
	void ReportStatus(unsigned long dwCurrentState, unsigned long dwWin32ExitCode, unsigned long dwWaitHint )noexcept;
	void ReportEvent( sv function )noexcept;
	void Main(unsigned long dwArgc, LPTSTR *lpszArgv )noexcept;
}