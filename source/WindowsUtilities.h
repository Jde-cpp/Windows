namespace Jde::Windows
{
	#define Φ Γ α
	Φ ToWString( const string& value)noexcept->std::wstring;
	Φ ToString( const std::wstring& value)noexcept->string;
	Φ ToSystemTime( TimePoint time )noexcept->SYSTEMTIME;
	Φ ToTimePoint( SYSTEMTIME systemTime )noexcept->TimePoint;
#undef Φ
}