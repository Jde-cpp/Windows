#include "../../Framework/source/DateTime.h"
#include "../../Framework/source/StringUtilities.h"
#include "WindowsUtilities.h"
#define var const auto
//https://stackoverflow.com/questions/3623471/how-do-you-get-info-for-an-arbitrary-time-zone-in-windows
namespace Jde::Timezone
{
	Duration GetGmtOffset( string_view name, TimePoint utc )noexcept(false)
	{
		std::wstring wstr = Windows::ToWString( string(name) );
		DYNAMIC_TIME_ZONE_INFORMATION dynamicTimezone = {};
		DWORD result=0;
		for( DWORD i = 0; result!=ERROR_NO_MORE_ITEMS; ++i )
		{
			result = ::EnumDynamicTimeZoneInformation( i, &dynamicTimezone );
			if( result==ERROR_SUCCESS && wstr==dynamicTimezone.StandardName )
				break;
		}
		if( result!=ERROR_SUCCESS )
			THROW( Exception("Could not find timezone '{}'", name) );
		
		DateTime myTime{utc};
		TIME_ZONE_INFORMATION tz;
		if( !GetTimeZoneInformationForYear(static_cast<USHORT>(myTime.Year()), &dynamicTimezone, &tz) )
			THROW( Exception("GetTimeZoneInformationForYear failed '{}'", GetLastError()) );

		var systemTime =  Windows::ToSystemTime( utc );
		SYSTEMTIME localTime;
		if( !SystemTimeToTzSpecificLocalTime(&tz, &systemTime, &localTime) )
			THROW( Exception("SystemTimeToTzSpecificLocalTime failed - {}", GetLastError()) );
		
		return Windows::ToTimePoint( localTime )-Windows::ToTimePoint( systemTime );
	}

	Duration EasternTimezoneDifference( TimePoint time )noexcept
	{
		return GetGmtOffset( "Eastern Standard Time", time );
	}
}