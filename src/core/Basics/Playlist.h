/*
 * Hydrogen
 * Copyright(c) 2002-2008 by Alex >Comix< Cominu [comix@users.sourceforge.net]
 * Copyright(c) 2008-2024 The hydrogen development team [hydrogen-devel@lists.sourceforge.net]
 *
 * http://www.hydrogen-music.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY, without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see https://www.gnu.org/licenses
 *
 */

#ifndef H2C_PLAYLIST_H
#define H2C_PLAYLIST_H

#include <core/Object.h>
#include <core/Helpers/Xml.h>

#include <memory>
#include <vector>

namespace H2Core
{

struct PlaylistEntry : public H2Core::Object<PlaylistEntry> {
	H2_OBJECT(PlaylistEntry)

	QString sFilePath;
	bool bFileExists;
	QString sScriptPath;
	bool bScriptEnabled;

	QString toQString( const QString& sPrefix = "", bool bShort = true ) const override;
};

/** \ingroup docCore docDataStructure */
class Playlist : public H2Core::Object<Playlist>

{
		H2_OBJECT(Playlist)

	public:
		
		Playlist();

		void	activateSong (int SongNumber );

		int		size() const;
		std::shared_ptr<PlaylistEntry>	get( int idx ) const;

		std::vector<std::shared_ptr<PlaylistEntry>>::iterator begin() {
			return __entries.begin();
		}
		std::vector<std::shared_ptr<PlaylistEntry>>::iterator end() {
			return __entries.end();
		}

		void	clear();
		/** Adds a new song/ entry to the current playlist.
		 *
		 * If @a nIndex is set to a value of -1, @a pEntry will be appended at
		 * the end of the playlist. */
		bool	add( std::shared_ptr<PlaylistEntry> entry, int nIndex = -1 );
		/** Removes a song from the current playlist.
		 *
		 * If @a nIndex is set to a value of -1, the first occurrance of @a
		 * pEntry will be deleted. */
		bool	remove( std::shared_ptr<PlaylistEntry> entry, int nIndex = -1 );

		void	setNextSongByNumber( int SongNumber );

		int		getActiveSongNumber() const;
		void	setActiveSongNumber( int ActiveSongNumber );
		
		bool	getSongFilenameByNumber( int songNumber, QString& fileName) const;

		const QString& getFilename() const;
		void setFilename( const QString& filename );
		bool getIsModified() const;
		void setIsModified( bool IsModified );

		static std::shared_ptr<Playlist> load( const QString& sPath );
		bool saveAs( const QString& sTargetPath, bool bSilent = false );
		bool save( bool bSilent = false ) const;
		/** Formatted string version for debugging purposes.
		 * \param sPrefix String prefix which will be added in front of
		 * every new line
		 * \param bShort Instead of the whole content of all classes
		 * stored as members just a single unique identifier will be
		 * displayed without line breaks.
		 *
		 * \return String presentation of current object.*/
		QString toQString( const QString& sPrefix = "", bool bShort = true ) const override;

	private:
		QString __filename;

		std::vector<std::shared_ptr<PlaylistEntry>> __entries;

		int m_nActiveSongNumber;

		bool m_bIsModified;

		void execScript( int index ) const;

		void saveTo( XMLNode& node ) const;
		static std::shared_ptr<Playlist> load_from( const XMLNode& root, const QString& sPath );
};

inline int Playlist::size() const
{
	return __entries.size();
}

inline std::shared_ptr<PlaylistEntry> Playlist::get( int idx ) const
{
	assert( idx >= 0 && idx < size() );
	return __entries[ idx ];
}

inline int Playlist::getActiveSongNumber() const
{
	return m_nActiveSongNumber;
}

inline void Playlist::setActiveSongNumber( int ActiveSongNumber )
{
	m_nActiveSongNumber = ActiveSongNumber ;
}

inline const QString& Playlist::getFilename() const
{
	return __filename;
}

inline void Playlist::setFilename( const QString& filename )
{
	__filename = filename;
}

inline bool Playlist::getIsModified() const
{
	return m_bIsModified;
}

inline void Playlist::setIsModified( bool IsModified )
{
	m_bIsModified = IsModified;
}

};

#endif // H2C_PLAYLIST_H
