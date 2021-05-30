/*
 * Hydrogen
 * Copyright (C) 2021 The hydrogen development team <hydrogen-devel@lists.sourceforge.net>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef COMMON_STRINGS_H
#define COMMON_STRINGS_H

#include <QString>
#include <core/Object.h>

class CommonStrings : public H2Core::Object {
	H2_OBJECT
	Q_DECLARE_TR_FUNCTIONS(CommonStrings)
	
public:
    CommonStrings();
	~CommonStrings();
 
	const QString& getSmallSoloButton() { return m_sSmallSoloButton; }
	const QString& getSmallMuteButton() { return m_sSmallMuteButton; }
	const QString& getBigMuteButton() { return m_sBigMuteButton; }
	const QString& getBypassButton() { return m_sBypassButton; }
	const QString& getEditButton() { return m_sEditButton; }
	const QString& getClearButton() { return m_sClearButton; }
	const QString& getPlaybackTrackButton() { return m_sPlaybackTrackButton; }
	const QString& getTimelineButton() { return m_sTimelineButton; }
	const QString& getTimelineBigButton() { return m_sTimelineBigButton; }
	const QString& getFXButton() { return m_sFXButton; }
	const QString& getPeakButton() { return m_sPeakButton; }
	const QString& getGeneralButton() { return m_sGeneralButton; }
	const QString& getInstrumentButton() { return m_sInstrumentButton; }
	const QString& getSoundLibraryButton() { return m_sSoundLibraryButton; }
	const QString& getLayersButton() { return m_sLayersButton; }
	const QString& getLoadLayerButton() { return m_sLoadLayerButton; }
	const QString& getDeleteLayerButton() { return m_sDeleteLayerButton; }
	const QString& getEditLayerButton() { return m_sEditLayerButton; }
	const QString& getBeatCounterButton() { return m_sBeatCounterButton; }
	const QString& getBeatCounterSetPlayButtonOff() { return m_sBeatCounterSetPlayButtonOff; }
	const QString& getBeatCounterSetPlayButtonOn() { return m_sBeatCounterSetPlayButtonOn; }
	const QString& getRubberbandButton() { return m_sRubberbandButton; }
	const QString& getJackTransportButton() { return m_sJackTransportButton; }
	const QString& getJackMasterButton() { return m_sJackMasterButton; }
	const QString& getMixerButton() { return m_sMixerButton; }
	const QString& getInstrumentRackButton() { return m_sInstrumentRackButton; }
	const QString& getPatternModeButton() { return m_sPatternModeButton; }
	const QString& getSongModeButton() { return m_sSongModeButton; }
	
private:
	QString m_sSmallSoloButton;
	QString m_sSmallMuteButton;
	QString m_sBigMuteButton;
	QString m_sBypassButton;
	QString m_sEditButton;
	QString m_sClearButton;
	QString m_sPlaybackTrackButton;
	QString m_sTimelineButton;
	QString m_sTimelineBigButton;
	QString m_sFXButton;
	QString m_sPeakButton;
	QString m_sGeneralButton;
	QString m_sInstrumentButton;
	QString m_sSoundLibraryButton;
	QString m_sLayersButton;
	QString m_sLoadLayerButton;
	QString m_sEditLayerButton;
	QString m_sDeleteLayerButton;
	QString m_sBeatCounterButton;
	QString m_sBeatCounterSetPlayButtonOff;
	QString m_sBeatCounterSetPlayButtonOn;
	QString m_sRubberbandButton;
	QString m_sJackTransportButton;
	QString m_sJackMasterButton;
	QString m_sMixerButton;
	QString m_sInstrumentRackButton;
	QString m_sPatternModeButton;
	QString m_sSongModeButton;
};
#endif