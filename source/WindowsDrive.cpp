#include "WindowsDrive.h"
#include "WindowsUtilities.h"
#include <jde/io/File.h>
#include "../../Framework/source/io/drive/DriveApi.h"
#include "../../Framework/source/DateTime.h"
#define var const auto

std::shared_ptr<Jde::IO::IDrive> Jde::IO::LoadNativeDrive(){ return std::make_shared<Jde::IO::Drive::WindowsDrive>(); }


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

	IDirEntryPtr WindowsDrive::Get( const fs::path& path )noexcept(false)
	{
		return make_shared<const DirEntry>( path );
	}

	tuple<FILETIME,FILETIME,FILETIME> GetTimes( const IDirEntry& dirEntry )
	{
		FILETIME createTime, modifiedTime;
		var entryCreateTime = Windows::ToSystemTime( dirEntry.CreatedTime );
		SystemTimeToFileTime( &entryCreateTime, &createTime );
		if( dirEntry.ModifiedTime.time_since_epoch()!=Duration::zero() )
		{
			var entryModifiedTime = Windows::ToSystemTime( dirEntry.ModifiedTime );
			SystemTimeToFileTime( &entryModifiedTime, &modifiedTime );
		}
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
				THROW( IOException( "Could not create '{}' - {}."sv, dir, GetLastError()) );
			if( !SetFileTime(hFile, &createTime, &lastAccessedTime, &modifiedTime) )
				WARN( "Could not update dir times '{}' - {}."sv, dir, GetLastError() );
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
				THROW( IOException("Could not create '{}' - {}."sv, path.string(), GetLastError()) );
			if( !SetFileTime(hFile, &createTime, &lastAccessedTime, &modifiedTime) )
				WARN( "Could not update file times '{}' - {}."sv, path.string(), GetLastError() );
			CloseHandle( hFile );
		}
		return make_shared<DirEntry>( path );
	}

	//VectorPtr<char> WindowsDrive::Load( const fs::path& path )noexcept(false)
	VectorPtr<char> WindowsDrive::Load( const IDirEntry& dirEntry )noexcept(false)
	{
		return IO::FileUtilities::LoadBinary( dirEntry.Path );
	}

	void WindowsDrive::SoftLink( path from, path to )noexcept(false)
	{
		THROW_IF( !CreateSymbolicLinkW(((const std::wstring&)to).c_str(), ((const std::wstring&)from).c_str(), 0), Exception("Creating symbolic link from '{}' to '{}' has failed - {}", from.string().c_str(), to.string().c_str(), GetLastError()) );
	}

	TaskResult DriveAwaitable::await_resume()noexcept
	{
		base::AwaitResume();
		return _pPromise ? TaskResult{ _pPromise->get_return_object().GetResult() } : TaskResult{ ExceptionPtr };
	}
	bool DriveAwaitable::await_ready()noexcept
	{
		try
		{
			DWORD access = _arg.IsRead ? GENERIC_READ : GENERIC_WRITE;
			DWORD sharing = _arg.IsRead ? FILE_SHARE_READ : 0;
			DWORD creationDisposition = _arg.IsRead ? OPEN_EXISTING : CREATE_ALWAYS;
			DWORD dwFlagsAndAttributes = _arg.IsRead ? FILE_FLAG_SEQUENTIAL_SCAN : FILE_ATTRIBUTE_ARCHIVE;
			_arg.FileHandle = HandlePtr( WinHandle(::CreateFile((string("\\\\?\\")+_arg.Path.string()).c_str(), access, sharing, nullptr, creationDisposition, FILE_FLAG_OVERLAPPED | dwFlagsAndAttributes, nullptr), [&](){return IOException(move(_arg.Path), GetLastError(), "CreateFile");}) );
			if( _arg.IsRead )
			{
				LARGE_INTEGER fileSize;
				THROW_IF( !::GetFileSizeEx(_arg.FileHandle.get(), &fileSize), IOException(move(_arg.Path), GetLastError(), "GetFileSizeEx") );
				std::visit( [fileSize](auto&& b){b->resize(fileSize.QuadPart);}, _arg.Buffer );
			}
		}
		catch( const IOException& e )
		{
			ExceptionPtr = std::make_exception_ptr( e );
		}
		return ExceptionPtr!=nullptr;
	}
	void DriveAwaitable::await_suspend( typename base::THandle h )noexcept
	{
		base::await_suspend( h );
		_arg.CoHandle = h;
		DriveWorker::Push( move(_arg) );
		//auto hPort = ::CreateIoCompletionPort( hFile, nullptr, 0, 0 );
	}
	void OverlappedCompletionRoutine( DWORD dwErrorCode, DWORD /*dwNumberOfBytesTransfered*/, LPOVERLAPPED pOverlapped )
	{
		DriveArg* pArg = (DriveArg*)pOverlapped->hEvent;
		auto& returnObject = pArg->CoHandle.promise().get_return_object();
		try
		{
			THROW_IF( dwErrorCode!=ERROR_SUCCESS, IOException{move(pArg->Path), dwErrorCode, "OverlappedCompletionRoutine"} );
			if( auto p = find_if( pArg->OverLaps.begin(), pArg->OverLaps.end(), [pOverlapped](var& x){ return &x==pOverlapped;} ); p!=pArg->OverLaps.end() )
				pArg->OverLaps.erase( p );
			if( pArg->OverLaps.empty() )
				returnObject.SetResult( std::visit([](auto&& x){return (sp<void>)x;}, pArg->Buffer) );
		}
		catch( Exception& e )
		{
			returnObject.SetResult( move(e) );
		}
		if( returnObject.HasResult() )
		{
			Coroutine::CoroutinePool::Resume( move(pArg->CoHandle) );
			DriveWorker::Remove( pArg );
		}
	}
	bool DriveWorker::Poll()noexcept
	{
		var newQueueItem = base::Poll();
		var ioItem = Args.size() && ::SleepEx( 0, false );
		return newQueueItem || ioItem;
	}
	void DriveWorker::Remove( DriveArg* x )noexcept
	{
		if( auto p=dynamic_pointer_cast<DriveWorker>(_pInstance); p )
		{
			if( auto pArg = find_if(p->Args.begin(), p->Args.end(), [x](auto& iArg){return &iArg==x;}); pArg!=p->Args.end() )
				p->Args.erase( pArg );
			else
				WARN( "Could not find arg. {}"sv, x->Path.string() );
		}
	}
	void DriveWorker::HandleRequest( DriveArg&& arg )noexcept
	{
		auto pArg = &Args.emplace_back( move(arg) );
		var size = std::visit( [](auto&& x){return x->size();}, pArg->Buffer );
		char* pData = std::visit( [](auto&& x){return x->data();}, pArg->Buffer );
		for( uint i=0; i<size; i+=ChunkSize )
		{
			OVERLAPPED* pOverlap = &( pArg->OverLaps.emplace_back( OVERLAPPED{} ) ); pArg->OverLaps.rbegin()->Pointer = (PVOID)i; pArg->OverLaps.rbegin()->hEvent = pArg;
			bool success;
			if( pArg->IsRead )
				success = ::ReadFileEx( pArg->FileHandle.get(), pData+i, ChunkSize, pOverlap, OverlappedCompletionRoutine  );
			else
				success = ::WriteFileEx( pArg->FileHandle.get(), pData+i, ChunkSize, pOverlap, OverlappedCompletionRoutine  );
		}
	}
}