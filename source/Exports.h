#pragma once
#pragma once

#ifdef JDE_DRIVE_WINDOWS_EXPORTS
	#ifdef _MSC_VER
		#define JDE_DRIVE_WINDOWS_EXPORT __declspec( dllexport )
	#else
		#define JDE_DRIVE_WINDOWS_EXPORT __attribute__((visibility("default")))
	#endif
#else 
	#ifdef _MSC_VER
		#define JDE_DRIVE_WINDOWS_EXPORT __declspec( dllimport )
		#if NDEBUG
			#pragma comment(lib, "Jde.IO.Drive.Windows.lib")
		#else
			#pragma comment(lib, "Jde.IO.Drive.Windows.lib")
		#endif
	#else
		#define JDE_DRIVE_WINDOWS_EXPORT
	#endif
#endif