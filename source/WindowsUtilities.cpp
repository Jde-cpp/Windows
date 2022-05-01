﻿using namespace std::chrono;
#include <codecvt>
#include "WindowsUtilities.h"
#include "../../Framework/source/DateTime.h"

namespace Jde
{
	α Windows::ToWString( const string& value)noexcept->std::wstring
	{
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		return converter.from_bytes( value );
	}

	α  Windows::ToString( const std::wstring& value)noexcept->string
	{
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		return converter.to_bytes( value );
	}

	α Windows::ToSystemTime( TimePoint time )noexcept->SYSTEMTIME
	{
		DateTime date{time};
		SYSTEMTIME systemTime;
		systemTime.wYear = static_cast<WORD>( date.Year() );
		systemTime.wMonth = date.Month(); 
		systemTime.wDay = date.Day(); 
		systemTime.wHour = date.Hour();
		systemTime.wMinute = date.Minute();
		systemTime.wSecond = date.Second();
		systemTime.wMilliseconds = (WORD)( duration_cast<milliseconds>(time.time_since_epoch()).count()-duration_cast<seconds>(time.time_since_epoch()).count()*1000 );
		return systemTime;
	}

	α Windows::ToTimePoint( SYSTEMTIME systemTime )noexcept->TimePoint
	{
		return DateTime{ systemTime.wYear, static_cast<uint8>(systemTime.wMonth), static_cast<uint8>(systemTime.wDay), static_cast<uint8>(systemTime.wHour), static_cast<uint8>(systemTime.wMinute), static_cast<uint8>(systemTime.wSecond), milliseconds(systemTime.wMilliseconds) }.GetTimePoint();
	}
}