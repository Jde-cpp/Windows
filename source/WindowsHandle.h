#pragma once
#include <jde/Exception.h>

namespace Jde
{
   struct WinHandle 
   {
       WinHandle( std::nullptr_t=nullptr )noexcept : _value(nullptr) {}
       WinHandle( HANDLE value, function<IOException()> e )noexcept(false):
          _value( value )
       {
          if( _value==INVALID_HANDLE_VALUE )
          {
             auto e2 = e();
             throw e2;
          }
          
       }

       explicit operator bool() const { return _value != nullptr && _value != INVALID_HANDLE_VALUE; }
       operator HANDLE() const { return _value; }

      struct Deleter 
      {
         typedef WinHandle pointer;
         void operator()( WinHandle handle )const noexcept{ WARN_IF( handle && !::CloseHandle(handle), "CloseHandle returned {}", ::GetLastError() ); }
      };
   private:
      HANDLE _value;
   };
   using HandlePtr=unique_ptr<WinHandle, WinHandle::Deleter>;
}