/*
 * Hydrogen
 * Copyright(c) 2002-2008 by Alex >Comix< Cominu [comix@users.sourceforge.net]
 * Copyright(c) 2008-2021 The hydrogen development team [hydrogen-devel@lists.sourceforge.net]
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

#ifndef SOUND_LIBRARY_PANEL_H
#define SOUND_LIBRARY_PANEL_H


#include <QtGui>
#include <QtWidgets>

#include <vector>

#include <core/Object.h>
#include <core/Preferences/Preferences.h>

#include "../Widgets/WidgetWithScalableFont.h"
#include "../EventListener.h"

namespace H2Core
{
	class Song;
	class Drumkit;
	class SoundLibrary;
}

class SoundLibraryTree;
class ToggleButton;

/** \ingroup docGUI*/
class SoundLibraryPanel : public QWidget, protected WidgetWithScalableFont<8, 10, 12>, private H2Core::Object<SoundLibraryPanel>, public EventListener
{
	H2_OBJECT(SoundLibraryPanel)
Q_OBJECT
public:
	SoundLibraryPanel( QWidget* parent, bool bInItsOwnDialog );
	~SoundLibraryPanel();

	void updateDrumkitList();
	void test_expandedItems();
	void update_background_color();
	virtual void drumkitLoadedEvent() override;
	virtual void updateSongEvent( int nValue ) override;
	const QString& getMessageFailedPreDrumkitLoad() const;

public slots:
	void on_drumkitLoadAction();

private slots:
	void on_DrumkitList_ItemChanged( QTreeWidgetItem* current, QTreeWidgetItem* previous );
	void on_DrumkitList_itemActivated( QTreeWidgetItem* item, int column );
	void on_DrumkitList_leftClicked( QPoint pos );
	void on_DrumkitList_rightClicked( QPoint pos );
	void on_DrumkitList_mouseMove( QMouseEvent* event );

	void on_drumkitDeleteAction();
	void on_drumkitPropertiesAction();
	void on_drumkitExportAction();
	void on_instrumentDeleteAction();
	void on_songLoadAction();
	void on_patternLoadAction();
	void on_patternDeleteAction();
	void onPreferencesChanged( H2Core::Preferences::Changes changes );

signals:
	void item_changed(bool bDrumkitSelected);

private:
	SoundLibraryTree *__sound_library_tree;
	//FileBrowser *m_pFileBrowser;

	QPoint __start_drag_position;
	QMenu* __drumkit_menu;
	QMenu* __instrument_menu;
	QMenu* __song_menu;
	QMenu* __pattern_menu;
	QMenu* __pattern_menu_list;

	QTreeWidgetItem* __system_drumkits_item;
	QTreeWidgetItem* __user_drumkits_item;
	QTreeWidgetItem* __song_item;
	QTreeWidgetItem* __pattern_item;
	QTreeWidgetItem* __pattern_item_list;

	std::vector<H2Core::Drumkit*> __system_drumkit_info_list;
	std::vector<H2Core::Drumkit*> __user_drumkit_info_list;
	bool __expand_pattern_list;
	bool __expand_songs_list;
	void restore_background_color();
	void change_background_color();

	/** Whether the dialog was constructed via a click in the MainForm
	 * or as part of the GUI.
	 */
	bool m_bInItsOwnDialog;

	/**
	 * Retrieve the drumkit associated with the currently selected
	 * item of the tree widget.
	 */
	Drumkit* getSelectedDrumkit() const;
	/**
	 * List of the same order as #__system_drumkit_info_list
	 * containing the labels assigned to the drumkit tree items. Even
	 * when adding " (system)" suffix or adding the corresponding
	 * folder name " [GMRockKit_copy]" this allows for a reliable
	 * one-to-one mapping of the current item to the associated
	 * drumkit.
	 */
	QStringList m_systemDrumkitNames;
	QStringList m_userDrumkitNames;

	QString m_sMessageFailedPreDrumkitLoad;
};

inline const QString& SoundLibraryPanel::getMessageFailedPreDrumkitLoad() const {
	return m_sMessageFailedPreDrumkitLoad;
}

#endif
