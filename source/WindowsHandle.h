#pragma once
#include <jde/Exception.h>

namespace Jde
{
   struct WinHandle 
   {
       WinHandle( std::nullptr_t=nullptr ) : _value(nullptr) {}
       WinHandle( HANDLE value, function<Exception()> e )noexcept:_value( value )
       {
			THROW_IF( _value==INVALID_HANDLE_VALUE, e() );
       }

       explicit operator bool() const { return _value != nullptr && _value != INVALID_HANDLE_VALUE; }
       operator HANDLE() const { return _value; }

       //friend bool operator ==(WinHandle l, WinHandle r) { return l.value_ == r.value_; }
       //friend bool operator !=(WinHandle l, WinHandle r) { return !(l == r); }

      struct Deleter 
      {
         typedef WinHandle pointer;
         void operator()( WinHandle handle )const noexcept{ WARN_IF( handle && !::CloseHandle(handle), "CloseHandle returned {}"sv, ::GetLastError() ); }
      };
   private:
      HANDLE _value;
   };
   using HandlePtr=unique_ptr<WinHandle, WinHandle::Deleter>;
   //inline bool operator ==(HANDLE l, WinHandle r) { return WinHandle(l) == r; }
   //inline bool operator !=(HANDLE l, WinHandle r) { return !(l == r); }
   //inline bool operator ==(WinHandle l, HANDLE r) { return l == WinHandle(r); }
   //inline bool operator !=(WinHandle l, HANDLE r) { return !(l == r); }
}