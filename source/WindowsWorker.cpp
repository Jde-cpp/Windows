#pragma once
#include "WindowsWorker.h"
#include "WindowsSvc.h"
#include "../../Framework/source/coroutine/Coroutine.h"
#include "../../Framework/source/threading/Mutex.h"

#define var const auto

namespace Jde::Windows
{
	up<WindowsWorkerMain> WindowsWorkerMain::_pInstance;

#define CREATE_EVENT ::CreateEvent(nullptr, TRUE, FALSE, nullptr)
#define WORKER_INIT _eventQueue{ CREATE_EVENT }, _eventStop{ CREATE_EVENT }

	WindowsWorker::WindowsWorker( bool runOnMainThread )noexcept:
		WORKER_INIT,
		_pThread{ runOnMainThread ? nullptr : make_unique<jthread>([&](){Loop();}) }
	{}

	WindowsWorker::WindowsWorker( Event&& initial )noexcept:
		_queue{ move(initial) },
		WORKER_INIT,
		_pThread{ make_unique<jthread>( [&](){Loop();}) }
	{}

	void WindowsWorker::Stop()noexcept
	{}

	α WindowsWorkerMain::Push( coroutine_handle<>&& h, HANDLE hEvent, bool close )noexcept->void
	{
		ASSERT( _pInstance );
		if( _pInstance )
		{
			_pInstance->_queue.Push( {{move(h), close}, hEvent} );
			LOG_IFL( !::SetEvent(_pInstance->_eventQueue), ELogLevel::Error, "SetEvent returned false" );
		}
	}

	α WindowsWorker::SubPush( Event& e )noexcept->bool
	{
		AtomicGuard l{ _lock };
		var set = !Stopped() && _queue.size()+_coroutines.size()<MaxEvents();
		if( set )
		{
			_queue.Push( move(e) );
			LOG_IFL( !::SetEvent(_eventQueue), ELogLevel::Error, "SetEvent returned false" );
		}
		return set;
	}

	α WindowsWorker::AddWaitRoutine( Event&& e )noexcept->void
	{
		if( ((TimePoint)_stop)==TimePoint{} )
		{
			_coroutines.push_back( e );
			_objects.push_back( e.WindowsEvent );
		}
		else
			ERR( "Stopped can't add event."sv );
	}

	void WindowsWorker::HandleEvent( Event&& e )noexcept
	{
		ASSERT( _coroutines.size()<MaxEvents() );
		AddWaitRoutine( move(e) );
	}

	void WindowsWorkerMain::HandleEvent( Event&& e )noexcept
	{
		if( _coroutines.size()<MaxEvents() )
			AddWaitRoutine( move(e) );
		else
		{
			bool used = false;
			for( auto pp = _workerBuffers.begin(); !used && pp!=_workerBuffers.end(); ++pp )
				used = (*pp)->SubPush( e );
			if( !used )
				_workerBuffers.push_back( make_shared<WindowsWorker>(move(e)) );
		}
	}

	α WindowsWorkerMain::HandleWorkerEvent()noexcept->void
	{
		for( auto pp = _workerBuffers.begin(); pp!=_workerBuffers.end(); pp = (*pp)->Stopped() ? _workerBuffers.erase(pp) : next(pp) );
	}
	static var _logLevel{ Logging::TagLevel("threads") };
	DWORD WindowsWorker::Loop()noexcept
	{
		PreLoop();
		DWORD waitResult;
		for( ;; )
		{
			LOG( "WaitForMultipleObjects" );
			waitResult = ::WaitForMultipleObjects( (DWORD)_objects.size(), _objects.data(), FALSE, INFINITE );
			LOG( "WaitForMultipleObjects - returned {}", waitResult );
			if( waitResult==1 )//_eventStop
			{
				LOG_IFL( !::ResetEvent(_objects[waitResult]), ELogLevel::Error, "ResetEvent failed for event object" );
				break;
			}
			ASSERT( false );//not sure of use case here.
			if( waitResult<_coroutines.size() )
			{
				auto pCoroutine = _coroutines.begin() + waitResult;
				if( pCoroutine->CoEvent )
					Coroutine::CoroutinePool::Resume( move(pCoroutine->CoEvent) );
				else
					ERR( "cohandle is empty!"sv );
				auto p = _objects.begin() + waitResult;
				if( pCoroutine->Close )
					::CloseHandle( *p );
				_objects.erase( p );
				_coroutines.erase( pCoroutine );
				if( !IsMainThread() )
				{
					AtomicGuard l{ _lock };
					if( _queue.size()+_coroutines.size()==0 )
					{
						_stop = Clock::now();
						break;
					}
				}
			}
			else if( waitResult==_coroutines.size() )
			{
				LOG_IFL( !::ResetEvent(_objects[waitResult]), ELogLevel::Error, "ResetEvent failed for event object" );
				vector<Event> events = _queue.PopAll();
				for( auto& e : events )
					HandleEvent( move(e) );
			}
			else
			{
				bool handled = IsMainThread();
				var stop = waitResult==_coroutines.size()+1;
				if( handled )
				{
					if( stop )
						_stop = Clock::now();	//TODO deal with stop.
					else if( _eventWorker && waitResult==_coroutines.size()+2 )
						HandleWorkerEvent();
					else
						handled = false;
				}
				else
					BREAK_IF( stop, ELogLevel::Debug, "Exiting with {} coroutins waiting.", _coroutines.size() );
				if( !handled )
				{
					BREAK_IF( waitResult==WAIT_FAILED, ELogLevel::Critical, "WaitForMultipleObjects returned {}", ::GetLastError() );
					ASSERT_DESC( false, format("Unknown result:  {}, count={}", waitResult, _objects.size()) );
				}
			}
		}
		return waitResult==WAIT_FAILED ? ERROR_BAD_COMMAND : NO_ERROR;
	}
	WindowsWorkerMain::WindowsWorkerMain( bool runOnMainThread )noexcept:
		WindowsWorker{ runOnMainThread }
	{}

	α WindowsWorkerMain::Start( optional<bool> pService )noexcept->void
	{
		var runOnMainThread = pService.has_value();
		var service = runOnMainThread && *pService;
		if( !_pInstance )
			_pInstance = up<WindowsWorkerMain>( new WindowsWorkerMain{runOnMainThread} );
		if( service )
			Service::ReportStatus( SERVICE_RUNNING, NO_ERROR, 0 );
		if( runOnMainThread )
		{
			var result = _pInstance->Loop();
			if( service )
				Service::ReportStatus( SERVICE_STOPPED, result, 0 );
		}
	}

	α WindowsWorkerMain::Stop()noexcept->void
	{
		if( _pInstance )
		{
			LOG( "Stopping" );
			LOG_IFL( !::SetEvent(_pInstance->_eventStop), ELogLevel::Error, "SetEvent returned false" );
		}
		else
			LOG( "Stopping but no instance" );
	}
}
