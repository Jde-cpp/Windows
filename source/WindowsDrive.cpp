#include "stdafx.h"
#include "WindowsDrive.h"
#include "io/File.h"
#ifndef __INTELLISENSE__
	#include <windows.h>
#endif
#define var const auto

Jde::IO::IDrive* LoadDrive()
{
	return new Jde::IO::Drive::WindowsDrive();
}

namespace Jde::IO::Drive
{
	std::wstring WindowsPath( const fs::path& path )
	{
		return std::wstring(L"\\\\?\\")+path.wstring();
	}
	WIN32_FILE_ATTRIBUTE_DATA GetInfo( const fs::path& path )noexcept(false)
	{
		WIN32_FILE_ATTRIBUTE_DATA fInfo;
		if( !GetFileAttributesExW(WindowsPath(path).c_str(), GetFileExInfoStandard, &fInfo) )
			THROW( IOException("Could not get file attributes for:  '{}' - '{}'.", path.string(), GetLastError()) );
		return fInfo;
	}

	struct DirEntry : IDirEntry
	{
		DirEntry( const fs::path& path ):
			DirEntry( path, GetInfo(path) )
		{}
		DirEntry( const fs::path& path, const WIN32_FILE_ATTRIBUTE_DATA& fInfo ):
			IDirEntry{ (EFileFlags)fInfo.dwFileAttributes, path, static_cast<size_t>(fInfo.nFileSizeHigh) <<32 | fInfo.nFileSizeLow }
		{
			SYSTEMTIME systemTime;
			FileTimeToSystemTime( &fInfo.ftCreationTime, &systemTime );
			CreatedTime = DateTime( systemTime.wYear, (uint8)systemTime.wMonth, (uint8)systemTime.wDay, (uint8)systemTime.wHour, (uint8)systemTime.wMinute, (uint8)systemTime.wSecond, std::chrono::milliseconds(systemTime.wMilliseconds) ).GetTimePoint();

			FileTimeToSystemTime( &fInfo.ftLastWriteTime, &systemTime );
			ModifiedTime = DateTime( systemTime.wYear, (uint8)systemTime.wMonth, (uint8)systemTime.wDay, (uint8)systemTime.wHour, (uint8)systemTime.wMinute, (uint8)systemTime.wSecond, std::chrono::milliseconds(systemTime.wMilliseconds) ).GetTimePoint();
		}

		//bool IsDirectory()const noexcept override{ return File.IsDirectory(); }

	};

	map<string,IDirEntryPtr>  WindowsDrive::Recursive( const fs::path& dir )noexcept(false)
	{
		if( !fs::exists(dir) )
			THROW( IOException( "'{}' does not exist.", dir) );
		var dirString = dir.string();
		map<string,IDirEntryPtr> entries;

		std::function<void(const fs::directory_entry&)> fnctn;
		fnctn = [&dir, &dirString, &entries, &fnctn]( const fs::directory_entry& entry )
		{
			var status = entry.status();
			var relativeDir = entry.path().string().substr( dirString.size()+1 );

			sp<DirEntry> pEntry;
			if( fs::is_directory(status) || fs::is_regular_file(status) )
			{
				var fInfo = GetInfo( entry.path() );
				entries.emplace( relativeDir, make_shared<DirEntry>(entry.path(), fInfo) );
				if( fs::is_directory(status) )
					FileUtilities::ForEachItem( entry.path(), fnctn );
			}
		};
		FileUtilities::ForEachItem( dir, fnctn );

		return entries;
	}

	SYSTEMTIME ToSystemTime( TimePoint time )noexcept
	{
		DateTime date{time};
		SYSTEMTIME systemTime;
		systemTime.wYear = date.Year();
		systemTime.wMonth = date.Month(); 
		systemTime.wDay = date.Day(); 
		systemTime.wHour = date.Hour();
		systemTime.wMinute = date.Minute();
		systemTime.wSecond = date.Second();
		systemTime.wMilliseconds = (WORD)( std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()).count()-std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count()*1000 );
		return systemTime;
	}

	tuple<FILETIME,FILETIME,FILETIME> GetTimes( const IDirEntry& dirEntry )
	{
		FILETIME createTime, modifiedTime;
		SystemTimeToFileTime( &ToSystemTime(dirEntry.CreatedTime), &createTime );
		if( dirEntry.ModifiedTime.time_since_epoch()!=Duration::zero() )
			SystemTimeToFileTime( &ToSystemTime(dirEntry.ModifiedTime), &modifiedTime );
		else
			modifiedTime = createTime;
		return std::make_tuple( createTime, modifiedTime, modifiedTime );
	}

	IDirEntryPtr WindowsDrive::CreateFolder( const fs::path& dir, const IDirEntry& dirEntry )noexcept(false)
	{
		if( !CreateDirectory(dir.string().c_str(), nullptr) )
			THROW( IOException( "Could not create '{}' - {}.", dir, GetLastError()) );
		if( dirEntry.CreatedTime.time_since_epoch()!=Duration::zero() )
		{
			var [createTime, modifiedTime, lastAccessedTime] = GetTimes( dirEntry );
			auto hFile = CreateFileW( WindowsPath(dir).c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL|FILE_FLAG_BACKUP_SEMANTICS, nullptr ); 
			if( hFile==INVALID_HANDLE_VALUE )
				THROW( IOException( "Could not create '{}' - {}.", dir, GetLastError()) );
			if( !SetFileTime(hFile, &createTime, &lastAccessedTime, &modifiedTime) )
				WARN( "Could not update dir times '{}' - {}.", dir, GetLastError() );
			CloseHandle( hFile );
		}
		return make_shared<DirEntry>( dir );
	}
	IDirEntryPtr WindowsDrive::Save( const fs::path& path, const vector<char>& bytes, const IDirEntry& dirEntry )noexcept(false)
	{
		IO::FileUtilities::SaveBinary( path, bytes );
		if( dirEntry.CreatedTime.time_since_epoch()!=Duration::zero() )
		{
			var [createTime, modifiedTime, lastAccessedTime] = GetTimes( dirEntry );
			auto hFile = CreateFileW( WindowsPath(path).c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL|FILE_FLAG_BACKUP_SEMANTICS, nullptr );
			if( hFile==INVALID_HANDLE_VALUE )
				THROW( IOException("Could not create '{}' - {}.", path.string(), GetLastError()) );
			if( !SetFileTime(hFile, &createTime, &lastAccessedTime, &modifiedTime) )
				WARN( "Could not update file times '{}' - {}.", path.string(), GetLastError() );
			CloseHandle( hFile );
		}
		return make_shared<DirEntry>( path );
	}
	
	//VectorPtr<char> WindowsDrive::Load( const fs::path& path )noexcept(false)
	VectorPtr<char> WindowsDrive::Load( const IDirEntry& dirEntry )noexcept(false)
	{
		return IO::FileUtilities::LoadBinary( dirEntry.Path );
	}
}