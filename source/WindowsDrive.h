#pragma once
//#include "Exports.h"
#include "../../Framework/source/io/DiskWatcher.h"
//#include "io/drive/DriveApi.h"

//extern "C" JDE_DRIVE_WINDOWS_EXPORT Jde::IO::IDrive* LoadDrive();

namespace Jde::IO::Drive
{
	struct WindowsDrive final:	public IDrive
	{
		IDirEntryPtr Get( const fs::path& path )noexcept(false) override;
		map<string,IDirEntryPtr> Recursive( const fs::path& dir )noexcept(false) override;
		IDirEntryPtr Save( const fs::path& path, const vector<char>& bytes, const IDirEntry& dirEntry )noexcept(false) override;
		IDirEntryPtr CreateFolder( const fs::path& path, const IDirEntry& dirEntry )noexcept(false) override;
		virtual void Remove( const fs::path& /*path*/ ){THROW( Exception("Not Implemented") );}
		virtual void Trash( const fs::path& /*path*/ ){THROW( Exception("Not Implemented") );}
		virtual void TrashDisposal( TimePoint /*latestDate*/ )override{THROW( Exception("Not Implemented") );}
		//VectorPtr<char> Load( const fs::path& path )noexcept(false) override;
		VectorPtr<char> Load( const IDirEntry& dirEntry )noexcept(false) override;
	};
}