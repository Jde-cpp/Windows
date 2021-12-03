﻿#include "WindowsDrive.h"
#include "WindowsUtilities.h"
#include <jde/io/File.h>
#include "../../Framework/source/io/drive/DriveApi.h"
#include "../../Framework/source/DateTime.h"
#define var const auto

namespace Jde
{
	IO::Drive::WindowsDrive _native;
	α IO::Native()noexcept->IDrive&{ return _native; }
}
namespace Jde::IO
{
	static const LogTag& _logLevel{ Logging::TagLevel("io") };
	α FileIOArg::Open()noexcept(false)->void
	{
		const DWORD access = IsRead ? GENERIC_READ : GENERIC_WRITE;
		const DWORD sharing = IsRead ? FILE_SHARE_READ : 0;
		const DWORD creationDisposition = IsRead ? OPEN_EXISTING : CREATE_ALWAYS;
		const DWORD dwFlagsAndAttributes = IsRead ? FILE_FLAG_SEQUENTIAL_SCAN : FILE_ATTRIBUTE_ARCHIVE;
		Handle = HandlePtr( WinHandle(::CreateFile((string("\\\\?\\")+Path.string()).c_str(), access, sharing, nullptr, creationDisposition, FILE_FLAG_OVERLAPPED | dwFlagsAndAttributes, nullptr), [&](){return IOException(move(Path), GetLastError(), "CreateFile");}) );
		if( IsRead )
		{
			LARGE_INTEGER fileSize; 
			THROW_IFX( !::GetFileSizeEx(Handle.get(), &fileSize), IOException(move(Path), GetLastError(), "GetFileSizeEx") );
			std::visit( [fileSize](auto&& b){b->resize(fileSize.QuadPart);}, Buffer );
		}
	}

	α OverlappedCompletionRoutine( DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED pOverlapped )->void;
	α Send( FileChunkArg& chunk )noexcept->void
	{
		//uint i = chunk.StartIndex();
		uint endByteIndex = chunk.EndIndex;
		chunk.Sent = true;
		auto& arg = chunk.FileArg();
		if( arg.IsRead )
		{
			RETURN_IF( !::ReadFileEx(arg.Handle.get(), chunk.Buffer(), (DWORD)(chunk.Bytes), &chunk.Overlap, OverlappedCompletionRoutine), "ReadFileEx({}) returned false - {}", arg.Path.string().c_str(), GetLastError() );
		}
		else
		{
			LOG( "Writing bytes {} - {}"sv, chunk.StartIndex(), std::min(chunk.StartIndex()+DriveWorker::ChunkSize(), endByteIndex) );
			var h = arg.Handle.get();
			RETURN_IF( !::WriteFileEx(h, chunk.Buffer(), (DWORD)(chunk.Bytes), &chunk.Overlap, OverlappedCompletionRoutine), "WriteFileEx({}) returned false - {}", arg.Path.string(), GetLastError() );
		}
		//Overlaps.emplace_back( move(pChunk) );
	}

	α OverlappedCompletionRoutine( DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED pOverlapped )->void
	{
		static uint i=0;
		try
		{
			THROW_IFX( dwErrorCode!=ERROR_SUCCESS, OSException(dwErrorCode, format("#{} - OverlappedCompletionRoutine xfered='{}'", i++, dwNumberOfBytesTransfered)) );//no pOverlapped
			FileChunkArg& chunk = *(FileChunkArg*)pOverlapped->hEvent;
			FileIOArg& arg = chunk.FileArg();
			auto& returnObject = arg.CoHandle.promise().get_return_object();
			if( auto pp = find_if( arg.Chunks.begin(), arg.Chunks.end(), [](var& x){ return !x->Sent;} ); pp!=arg.Chunks.end() )
				Send( dynamic_cast<FileChunkArg&>(**pp) );
			else if( arg.Chunks.size()==1 )
				returnObject.SetResult( std::visit([](auto&& x){return (sp<void>)x;}, arg.Buffer) );
			if( returnObject.HasResult() )
			{
				LOG( "({})OverlappedCompletionRoutine - {}"sv, i++, dwNumberOfBytesTransfered );
				WinDriveWorker::Remove( &arg );
				Coroutine::CoroutinePool::Resume( move(arg.CoHandle) );
			}
			else if( auto p = find_if( arg.Chunks.begin(), arg.Chunks.end(), [pChunk=&chunk](var& x){ return x.get()==pChunk;} ); p!=arg.Chunks.end() )
				arg.Chunks.erase( p );
		}
		catch( IException&  )
		{}
		//LOG( "~OverlappedCompletionRoutine"sv );
	}

	α FileIOArg::Send( coroutine_handle<Task2::promise_type>&& h )noexcept->void
	{
		CoHandle = move( h );
		for( uint i=0; i*DriveWorker::ChunkSize()<Size(); ++i )
			Chunks.emplace_back( make_unique<FileChunkArg>(*this, i) );
		
		WinDriveWorker::Push( this );
	}

	sp<WinDriveWorker> _pInstance;
	uint8 _threadCount{ std::numeric_limits<uint8>::max() };
	uint _index{ 0 };
	vector<uint> _indexes; std::atomic_flag _indexMutex;
/*	α WinDriveWorker::Start()noexcept->uint
	{
		if( !_pInstance )
		{
			if( _threadCount==std::numeric_limits<uint8>::max() )
				_threadCount = Settings::TryGet<uint8>( "workers/drive/threads" ).value_or( 0 );
			if( _threadCount )
				IApplication::AddShutdown( _pInstance = make_shared<WinDriveWorker>() );
		}
		if( _pInstance )
			_pInstance->WakeUp();
		AtomicGuard l{ _indexMutex };
		_indexes.push_back( _index );
		return _index++;
	}
*/	
	Duration _keepAlive = Settings::TryGet<Duration>( "workers/drive/keepalive" ).value_or( 5s );
	α WinDriveWorker::Poll()noexcept->optional<bool>
	{
		var newQueueItem = base::Poll().value();
		AtomicGuard l{ _argMutex };
		bool ioItem = _args.size();
		if( ioItem )
		{
			l.unlock();
			var sleepResult = ::SleepEx( 0, true );
			ioItem = ioItem && sleepResult;
		}
		var result = newQueueItem || ioItem;
		if( result )
			_lastRequest = Clock::now();
		return result ? result : _lastRequest.load()+_keepAlive>Clock::now() ? optional<bool>{ false } : std::nullopt;
	}

	α WinDriveWorker::HandleRequest( FileIOArg*&& pArg )noexcept->void
	{
		AtomicGuard l{ _argMutex };
		_args.emplace_back( pArg );
		for( uint i=0; i<std::min<uint8>((uint8)pArg->Chunks.size(), DriveWorker::ThreadSize()); ++i )
			IO::Send( (FileChunkArg&)*pArg->Chunks[i] );
	}
	
	void WinDriveWorker::Remove( FileIOArg* pArg )noexcept
	{
		if( auto pInstance=dynamic_pointer_cast<WinDriveWorker>(_pInstance); pInstance )
		{
			AtomicGuard l{ pInstance->_argMutex };
			if( auto p = find(pInstance->_args.begin(), pInstance->_args.end(), pArg); p!=pInstance->_args.end() )
				pInstance->_args.erase( p );
			else
				WARN( "Could not find arg." );
		}
	}

	α DriveAwaitable::await_resume()noexcept->TaskResult
	{
		base::AwaitResume();
		return _pPromise ? TaskResult{ _pPromise->get_return_object().GetResult() } : TaskResult{ ExceptionPtr };
	}
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
		THROW_IFX( !GetFileAttributesExW(WindowsPath(path).c_str(), GetFileExInfoStandard, &fInfo), IOException(path, GetLastError(), "Could not get file attributes.") );
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

	flat_map<string,IDirEntryPtr> WindowsDrive::Recursive( path dir )noexcept(false)
	{
		CHECK_PATH( dir );
		var dirString = dir.string();
		flat_map<string,IDirEntryPtr> entries;

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
		THROW_IFX( !CreateDirectory(dir.string().c_str(), nullptr), IOException(dir, GetLastError(), "Could not create.") );
		if( dirEntry.CreatedTime.time_since_epoch()!=Duration::zero() )
		{
			var [createTime, modifiedTime, lastAccessedTime] = GetTimes( dirEntry );
			auto hFile = CreateFileW( WindowsPath(dir).c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL|FILE_FLAG_BACKUP_SEMANTICS, nullptr );  THROW_IFX( hFile==INVALID_HANDLE_VALUE, IOException(dir, GetLastError(), "Could not create."sv) );
			LOG_IFL( !SetFileTime(hFile, &createTime, &lastAccessedTime, &modifiedTime), ELogLevel::Warning, "Could not update dir times '{}' - {}.", dir.string(), GetLastError() );
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
			THROW_IFX( hFile==INVALID_HANDLE_VALUE, IOException(path, GetLastError(), "Could not create."sv) );
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
		THROW_IFX( !CreateSymbolicLinkW(((const std::wstring&)to).c_str(), ((const std::wstring&)from).c_str(), 0), IOException( from, GetLastError(), format("Creating symbolic link from to '{}'", to.string().c_str())) );
	}
	
	/*
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
	α DriveWorker::Poll()noexcept->optional<bool>
	{
		var newQueueItem = base::Poll();
		bool ioItem = Args.size();
		if( ioItem )
		{
			var sleepResult = ::SleepEx( 0, true );
			ioItem = ioItem && sleepResult;
		}
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
		for( uint i=0; i<size; i+=ChunkSize() )
		{
			auto pOverlap = make_unique<OVERLAPPED>(); pOverlap->Pointer = (PVOID)i; pOverlap->hEvent = (HANDLE)std::min( i+DriveWorker::ChunkSize(), size );
			if( pArg->Overlaps.size()<ThreadCount )
				pArg->Send( move(pOverlap) );
			else
				pArg->OverlapsOverflow.emplace_back( move(pOverlap) );
		}
	}
	*/
}