#pragma once
#include "WindowsHandle.h"
#include <jde/coroutine/Task.h>
#include "../../Framework/source/io/DiskWatcher.h"
#include "../../Framework/source/coroutine/Awaitable.h"
#include "../../Framework/source/threading/Worker.h"

namespace Jde::Windows
{
	struct CoEvent
	{
		coroutine_handle<> CoEvent;
		bool Close;
	};

	struct Event : CoEvent
	{
		HANDLE WindowsEvent;
	};

	struct WindowsWorker /*final*/: std::enable_shared_from_this<WindowsWorker>
	{
		WindowsWorker( bool runOnMainThread )noexcept;
		WindowsWorker( Event&& initial )noexcept;
		~WindowsWorker(){ DBG("~WindowsWorker"); }
		void Stop()noexcept;
		α SubPush( Event& e )noexcept->bool;
		α Stopped()noexcept{ return ((TimePoint)_stop)!=TimePoint{}; }
	protected:
		WindowsWorker()noexcept;
		α AddWaitRoutine( Event&& e )noexcept->void;
		α PreLoop()noexcept{ _coroutines.reserve( MAXIMUM_WAIT_OBJECTS ); _objects.reserve( MAXIMUM_WAIT_OBJECTS ); AddInternalEvents(); }
		α MainLoop()noexcept->optional<DWORD>;
		α Loop()noexcept->DWORD;
		virtual α HandleEvent( Event&& e )noexcept->void;
		HANDLE _eventStop;
		sp<WindowsWorker> _pKeepAlive;
		atomic<TimePoint> _stop;
		vector<CoEvent> _coroutines;
		vector<HANDLE> _objects;
		QueueMove<Event> _queue;
		HANDLE _eventQueue;
	private:
		α AddInternalEvents()noexcept->void{ _objects.push_back( _eventQueue );  _objects.push_back( _eventStop ); if( _eventWorker ) _objects.push_back( _eventWorker ); }
		constexpr virtual uint MaxEvents()noexcept{return MAXIMUM_WAIT_OBJECTS-2; }
		virtual α HandleWorkerEvent()noexcept->void{ ASSERT(false); }
		α IsMainThread()const noexcept{ return _eventWorker!=nullptr; }
		Duration _shutdownWait{5s};
		HANDLE _eventWorker{ nullptr };
		up<jthread> _pThread;//3rd class...
		atomic<bool> _lock{false};

	};

	struct WindowsWorkerMain final: WindowsWorker
	{
		//WindowsWorker()=default;
		//~WindowsWorkerMain(){ DBG("~WindowsWorker"sv); }
		
		JDE_NATIVE_VISIBILITY Ω Push( coroutine_handle<>&& h, HANDLE hEvent, bool close=true )noexcept->void;
		JDE_NATIVE_VISIBILITY Ω Start( optional<bool> service )noexcept->void;
		JDE_NATIVE_VISIBILITY Ω Stop()noexcept->void;
	private:
		WindowsWorkerMain( bool runOnMainThread )noexcept;
		vector<sp<WindowsWorker>> _workerBuffers;
		constexpr virtual uint MaxEvents()noexcept{return MAXIMUM_WAIT_OBJECTS-3; }
		void HandleEvent( Event&& e )noexcept override;
		void HandleWorkerEvent()noexcept override;
		static up<WindowsWorkerMain> _pInstance;
	};
}