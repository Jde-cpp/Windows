namespace Jde::Windows
{
	std::wstring ToWString( const string& value)noexcept;
	std::string ToString( const std::wstring& value)noexcept;
	SYSTEMTIME ToSystemTime( TimePoint time )noexcept;
	TimePoint ToTimePoint( SYSTEMTIME systemTime )noexcept;
}