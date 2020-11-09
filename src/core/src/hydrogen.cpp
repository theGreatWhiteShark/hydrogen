/*
 * Hydrogen
 * Copyright(c) 2002-2008 by Alex >Comix< Cominu [comix@users.sourceforge.net]
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
#include "hydrogen/config.h"

#ifdef WIN32
#    include "hydrogen/timehelper.h"
#else
#    include <unistd.h>
#    include <sys/time.h>
#endif

#include <pthread.h>
#include <cassert>
#include <cstdio>
#include <deque>
#include <queue>
#include <iostream>
#include <ctime>
#include <cmath>
#include <algorithm>

#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>

#include <hydrogen/event_queue.h>
#include <hydrogen/basics/adsr.h>
#include <hydrogen/basics/drumkit.h>
#include <hydrogen/basics/drumkit_component.h>
#include <hydrogen/h2_exception.h>
#include <hydrogen/audio_engine.h>
#include <hydrogen/basics/instrument.h>
#include <hydrogen/basics/instrument_component.h>
#include <hydrogen/basics/instrument_list.h>
#include <hydrogen/basics/instrument_layer.h>
#include <hydrogen/basics/playlist.h>
#include <hydrogen/basics/sample.h>
#include <hydrogen/basics/automation_path.h>
#include <hydrogen/hydrogen.h>
#include <hydrogen/basics/pattern.h>
#include <hydrogen/basics/pattern_list.h>
#include <hydrogen/basics/note.h>
#include <hydrogen/helpers/filesystem.h>
#include <hydrogen/fx/LadspaFX.h>
#include <hydrogen/fx/Effects.h>

#include <hydrogen/Preferences.h>
#include <hydrogen/sampler/Sampler.h>
#include <hydrogen/midi_map.h>
#include <hydrogen/timeline.h>

#ifdef H2CORE_HAVE_OSC
#include <hydrogen/nsm_client.h>
#include <hydrogen/osc_server.h>
#endif

#include <hydrogen/IO/AudioOutput.h>
#include <hydrogen/IO/jack_audio_driver.h>
#include <hydrogen/IO/NullDriver.h>
#include <hydrogen/IO/MidiInput.h>
#include <hydrogen/IO/MidiOutput.h>
#include <hydrogen/IO/CoreMidiDriver.h>
#include <hydrogen/IO/TransportInfo.h>
#include <hydrogen/IO/OssDriver.h>
#include <hydrogen/IO/FakeDriver.h>
#include <hydrogen/IO/AlsaAudioDriver.h>
#include <hydrogen/IO/PortAudioDriver.h>
#include <hydrogen/IO/DiskWriterDriver.h>
#include <hydrogen/IO/AlsaMidiDriver.h>
#include <hydrogen/IO/JackMidiDriver.h>
#include <hydrogen/IO/PortMidiDriver.h>
#include <hydrogen/IO/CoreAudioDriver.h>
#include <hydrogen/IO/PulseAudioDriver.h>

namespace H2Core
{

// GLOBALS

//----------------------------------------------------------------------------
//
// Implementation of Hydrogen class
//
//----------------------------------------------------------------------------

Hydrogen* Hydrogen::__instance = nullptr;
const char* Hydrogen::__class_name = "Hydrogen";

Hydrogen::Hydrogen()
	: Object( __class_name )
{
	if ( __instance ) {
		ERRORLOG( "Hydrogen audio engine is already running" );
		throw H2Exception( "Hydrogen audio engine is already running" );
	}

	INFOLOG( "[Hydrogen]" );

	__song = nullptr;

	m_bExportSessionIsActive = false;
	m_pTimeline = new Timeline();
	m_pCoreActionController = new CoreActionController();
	m_bActiveGUI = false;
	m_nMaxTimeHumanize = 2000;

	initBeatcounter();
	InstrumentComponent::setMaxLayers( Preferences::get_instance()->getMaxLayers() );
	m_pAudioEngine = std::make_shared<AudioEngine>();
	m_pAudioEngine->audioEngine_init();

	// Prevent double creation caused by calls from MIDI thread
	__instance = this;

	m_pAudioEngine->audioEngine_startAudioDrivers();
	for(int i = 0; i< MAX_INSTRUMENTS; i++){
		m_nInstrumentLookupTable[i] = i;
	}

#ifdef H2CORE_HAVE_OSC
	if( Preferences::get_instance()->getOscServerEnabled() )
	{
		toggleOscServer( true );
	}
#endif
}

Hydrogen::~Hydrogen()
{
	INFOLOG( "[~Hydrogen]" );

#ifdef H2CORE_HAVE_OSC
	NsmClient* pNsmClient = NsmClient::get_instance();
	if( pNsmClient ) {
		pNsmClient->shutdown();
		delete pNsmClient;
	}
	OscServer* pOscServer = OscServer::get_instance();
	if( pOscServer ) {
		delete pOscServer;
	}
#endif

	if ( m_pAudioEngine->getState() == STATE_PLAYING ) {
		m_pAudioEngine->audioEngine_stop();
	}
	removeSong();
	m_pAudioEngine->audioEngine_stopAudioDrivers();
	m_pAudioEngine->audioEngine_destroy();
	__kill_instruments();

	delete m_pCoreActionController;
	delete m_pTimeline;

	__instance = nullptr;
}

void Hydrogen::create_instance()
{
	// Create all the other instances that we need
	// ....and in the right order
	Logger::create_instance();
	MidiMap::create_instance();
	Preferences::create_instance();
	EventQueue::create_instance();
	MidiActionManager::create_instance();

#ifdef H2CORE_HAVE_OSC
	NsmClient::create_instance();
	OscServer::create_instance( Preferences::get_instance() );
#endif

	if ( __instance == nullptr ) {
		__instance = new Hydrogen;
	}

	// See audioEngine_init() for:
	// AudioEngine::create_instance();
	// Effects::create_instance();
	// Playlist::create_instance();
}

void Hydrogen::initBeatcounter()
{
	m_ntaktoMeterCompute = 1;
	m_nbeatsToCount = 4;
	m_nEventCount = 1;
	m_nTempoChangeCounter = 0;
	m_nBeatCount = 1;
	m_nCoutOffset = 0;
	m_nStartOffset = 0;
}

/// Start the internal sequencer
void Hydrogen::sequencer_play()
{
	Song* pSong = getSong();
	pSong->get_pattern_list()->set_to_old();
	m_pAudioEngine->m_pAudioDriver->play();
}

/// Stop the internal sequencer
void Hydrogen::sequencer_stop()
{
	if( Hydrogen::get_instance()->getAudioEngine()->getMidiOutput() != nullptr ){
		Hydrogen::get_instance()->getAudioEngine()->getMidiOutput()->handleQueueAllNoteOff();
	}

	m_pAudioEngine->m_pAudioDriver->stop();
	Preferences::get_instance()->setRecordEvents(false);
}

bool Hydrogen::setPlaybackTrackState( const bool state )
{
	Song* pSong = getSong();
	if ( pSong == nullptr ) {
		return false;
	}

	return pSong->set_playback_track_enabled(state);
}

void Hydrogen::loadPlaybackTrack( const QString filename )
{
	Song* pSong = getSong();
	pSong->set_playback_track_filename(filename);

	m_pAudioEngine->getSampler()->reinitialize_playback_track();
}

void Hydrogen::setSong( Song *pSong )
{
	assert ( pSong );
	
	// Move to the beginning.
	m_pAudioEngine->setSelectedPatternNumber( 0 );

	Song* pCurrentSong = getSong();
	if ( pSong == pCurrentSong ) {
		DEBUGLOG( "pSong == pCurrentSong" );
		return;
	}

	if ( pCurrentSong ) {
		/* NOTE: 
		 *       - this is actually some kind of cleanup 
		 *       - removeSong cares itself for acquiring a lock
		 */
		removeSong();

		m_pAudioEngine->lock( RIGHT_HERE );
		delete pCurrentSong;
		pCurrentSong = nullptr;

		m_pAudioEngine->unlock();

	}

	/* Reset GUI */
	EventQueue::get_instance()->push_event( EVENT_SELECTED_PATTERN_CHANGED, -1 );
	EventQueue::get_instance()->push_event( EVENT_PATTERN_CHANGED, -1 );
	EventQueue::get_instance()->push_event( EVENT_SELECTED_INSTRUMENT_CHANGED, -1 );

	// In order to allow functions like audioEngine_setupLadspaFX() to
	// load the settings of the new song, like whether the LADSPA FX
	// are activated, __song has to be set prior to the call of
	// audioEngine_setSong().
	__song = pSong;

	
	// Update the audio engine to work with the new song.
	m_pAudioEngine->audioEngine_setSong( pSong );

	// load new playback track information
	m_pAudioEngine->getSampler()->reinitialize_playback_track();
	
	// Push current state of Hydrogen to attached control interfaces,
	// like OSC clients.
	m_pCoreActionController->initExternalControlInterfaces();
}

/* Mean: remove current song from memory */
void Hydrogen::removeSong()
{
	__song = nullptr;
	m_pAudioEngine->audioEngine_removeSong();
}

void Hydrogen::midi_noteOn( Note *note )
{
	m_pAudioEngine->audioEngine_noteOn( note );
}

void Hydrogen::addRealtimeNote(	int		instrument,
								float	velocity,
								float	pan_L,
								float	pan_R,
								float	pitch,
								bool	noteOff,
								bool	forcePlay,
								int		msg1 )
{
	UNUSED( pitch );

	Preferences *pPreferences = Preferences::get_instance();
	unsigned int nRealColumn = 0;
	unsigned res = pPreferences->getPatternEditorGridResolution();
	int nBase = pPreferences->isPatternEditorUsingTriplets() ? 3 : 4;
	int scalar = ( 4 * MAX_NOTES ) / ( res * nBase );
	bool hearnote = forcePlay;
	int currentPatternNumber;

	m_pAudioEngine->lock( RIGHT_HERE );

	Song *pSong = getSong();
	if ( !pPreferences->__playselectedinstrument ) {
		if ( instrument >= ( int ) pSong->get_instrument_list()->size() ) {
			// unused instrument
			m_pAudioEngine->unlock();
			return;
		}
	}

	// Get current partern and column, compensating for "lookahead" if required
	const Pattern* currentPattern = nullptr;
	unsigned int column = 0;
	float fTickSize = m_pAudioEngine->m_pAudioDriver->m_transport.m_fTickSize;
	unsigned int lookaheadTicks = calculateLookahead( fTickSize ) / fTickSize;
	bool doRecord = pPreferences->getRecordEvents();
	if ( pSong->get_mode() == Song::SONG_MODE && doRecord &&
		 m_pAudioEngine->getState() == STATE_PLAYING )
	{

		// Recording + song playback mode + actually playing
		PatternList *pPatternList = pSong->get_pattern_list();
		int ipattern = m_pAudioEngine->getPatternPos(); // playlist index
		if ( ipattern < 0 || ipattern >= (int) pPatternList->size() ) {
			m_pAudioEngine->unlock(); // unlock the audio engine
			return;
		}
		// Locate column -- may need to jump back in the pattern list
		column = m_pAudioEngine->getTickPosition();
		while ( column < lookaheadTicks ) {
			ipattern -= 1;
			if ( ipattern < 0 || ipattern >= (int) pPatternList->size() ) {
				m_pAudioEngine->unlock(); // unlock the audio engine
				return;
			}

			// Convert from playlist index to actual pattern index
			std::vector<PatternList*> *pColumns = pSong->get_pattern_group_vector();
			for ( int i = 0; i <= ipattern; ++i ) {
				PatternList *pColumn = ( *pColumns )[i];
				currentPattern = pColumn->get( 0 );
				currentPatternNumber = i;
			}
			column = column + currentPattern->get_length();
			// WARNINGLOG( "Undoing lookahead: corrected (" + to_string( ipattern+1 ) +
			// "," + to_string( (int) ( column - currentPattern->get_length() ) -
			// (int) lookaheadTicks ) + ") -> (" + to_string(ipattern) +
			// "," + to_string( (int) column - (int) lookaheadTicks ) + ")." );
		}
		column -= lookaheadTicks;
		// Convert from playlist index to actual pattern index (if not already done above)
		if ( currentPattern == nullptr ) {
			std::vector<PatternList*> *pColumns = pSong->get_pattern_group_vector();
			for ( int i = 0; i <= ipattern; ++i ) {
				PatternList *pColumn = ( *pColumns )[i];
				currentPattern = pColumn->get( 0 );
				currentPatternNumber = i;
			}
		}

		// Cancel recording if punch area disagrees
		doRecord = pPreferences->inPunchArea( ipattern );

	} else { // Not song-record mode
		PatternList *pPatternList = pSong->get_pattern_list();
		int nSelectedPatternNumber = m_pAudioEngine->getSelectedPatternNumber();

		if ( ( nSelectedPatternNumber != -1 )
			 && ( nSelectedPatternNumber < ( int )pPatternList->size() ) )
		{
			currentPattern = pPatternList->get( nSelectedPatternNumber );
			currentPatternNumber = nSelectedPatternNumber;
		}

		if ( ! currentPattern ) {
			m_pAudioEngine->unlock(); // unlock the audio engine
			return;
		}

		// Locate column -- may need to wrap around end of pattern
		column = m_pAudioEngine->getTickPosition();
		if ( column >= lookaheadTicks ) {
			column -= lookaheadTicks;
		} else {
			lookaheadTicks %= currentPattern->get_length();
			column = (column + currentPattern->get_length() - lookaheadTicks)
					% currentPattern->get_length();
		}
	}

	nRealColumn = getRealtimeTickPosition();

	if ( pPreferences->getQuantizeEvents() ) {
		// quantize it to scale
		unsigned qcolumn = ( unsigned )::round( column / ( double )scalar ) * scalar;

		//we have to make sure that no beat is added on the last displayed note in a bar
		//for example: if the pattern has 4 beats, the editor displays 5 beats, so we should avoid adding beats an note 5.
		if ( qcolumn == currentPattern->get_length() ) qcolumn = 0;
		column = qcolumn;
	}

	unsigned position = column;
	m_naddrealtimenotetickposition = column;

	Instrument *instrRef = nullptr;
	if ( pSong ) {
		//getlookuptable index = instrument+36, ziel wert = der entprechende wert -36
		instrRef = pSong->get_instrument_list()->get( m_nInstrumentLookupTable[ instrument ] );
	}

	if ( currentPattern && ( m_pAudioEngine->getState() == STATE_PLAYING ) ) {
		if ( doRecord && pPreferences->getDestructiveRecord() && pPreferences->m_nRecPreDelete>0 ) {
			// Delete notes around current note if option toggled
			int postdelete = 0;
			int predelete = 0;
			int prefpredelete = pPreferences->m_nRecPreDelete-1;
			int prefpostdelete = pPreferences->m_nRecPostDelete;
			int length = currentPattern->get_length();
			bool fp = false;
			postdelete = column;

			switch (prefpredelete) {
			case 0: predelete = length ; postdelete = 0; fp = true; break;
			case 1: predelete = length ; fp = true; break;
			case 2: predelete = length / 2; fp = true; break;
			case 3: predelete = length / 4; fp = true; break;
			case 4: predelete = length / 8; fp = true; break;
			case 5: predelete = length / 16; fp = true; break;
			case 6: predelete = length / 32; fp = true; break;
			case 7: predelete = length / 64; fp = true; break;
			case 8: predelete = length / 64; break;
			case 9: predelete = length / 32; break;
			case 10: predelete = length / 16; break;
			case 11: predelete = length / 8; break;
			case 12: predelete = length / 4; break;
			case 13: predelete = length / 2; break;
			case 14: predelete = length; break;
			case 15: break;
			default : predelete = 1; break;
			}

			if (!fp ) {
				switch (prefpostdelete) {
				case 0: postdelete = column; break;
				case 1: postdelete -= length / 64; break;
				case 2: postdelete -= length / 32; break;
				case 3: postdelete -= length / 16; break;
				case 4: postdelete -= length / 8; break;
				case 5: postdelete -= length / 4; break;
				case 6: postdelete -= length / 2; break;
				case 7: postdelete -= length ; break;
				default : postdelete = column; break;
				}

				if (postdelete<0) postdelete = 0;
			}

			Pattern::notes_t* notes = (Pattern::notes_t*)currentPattern->get_notes();
			FOREACH_NOTE_IT_BEGIN_END(notes,it) {
				Note *pNote = it->second;
				assert( pNote );

				int currentPosition = pNote->get_position();
				if ( pPreferences->__playselectedinstrument ) {//fix me
					if ( pSong->get_instrument_list()->get( m_pAudioEngine->getSelectedInstrumentNumber()) == pNote->get_instrument() )
					{
						if (prefpredelete>=1 && prefpredelete <=14 ) pNote->set_just_recorded( false );

						if ( (prefpredelete == 15) && (pNote->get_just_recorded() == false))
						{
							bool replaceExisting = false;
							if (column == currentPosition) replaceExisting = true;
							EventQueue::AddMidiNoteVector noteAction;
							noteAction.m_column = currentPosition;
							noteAction.m_row = pNote->get_instrument_id(); //getSelectedInstrumentNumber();
							noteAction.m_pattern = currentPatternNumber;
							noteAction.f_velocity = velocity;
							noteAction.f_pan_L = pan_L;
							noteAction.f_pan_R = pan_R;
							noteAction.m_length = -1;
							int divider = msg1 / 12;
							noteAction.no_octaveKeyVal = (Note::Octave)(divider -3);
							noteAction.nk_noteKeyVal = (Note::Key)(msg1 - (12 * divider));
							noteAction.b_isInstrumentMode = replaceExisting;
							noteAction.b_isMidi = true;
							noteAction.b_noteExist = replaceExisting;
							EventQueue::get_instance()->m_addMidiNoteVector.push_back(noteAction);
							continue;
						}
						if ( ( pNote->get_just_recorded() == false )
							 && (static_cast<int>( pNote->get_position() ) >= postdelete
								 && pNote->get_position() < column + predelete +1 )
							 ) {
							bool replaceExisting = false;
							if ( column == currentPosition ) {
								replaceExisting = true;
							}
							EventQueue::AddMidiNoteVector noteAction;
							noteAction.m_column = currentPosition;
							noteAction.m_row = pNote->get_instrument_id(); //getSelectedInstrumentNumber();
							noteAction.m_pattern = currentPatternNumber;
							noteAction.f_velocity = velocity;
							noteAction.f_pan_L = pan_L;
							noteAction.f_pan_R = pan_R;
							noteAction.m_length = -1;
							int divider = msg1 / 12;
							noteAction.no_octaveKeyVal = (Note::Octave)(divider -3);
							noteAction.nk_noteKeyVal = (Note::Key)(msg1 - (12 * divider));
							noteAction.b_isInstrumentMode = replaceExisting;
							noteAction.b_isMidi = true;
							noteAction.b_noteExist = replaceExisting;
							EventQueue::get_instance()->m_addMidiNoteVector.push_back(noteAction);
						}
					}
					continue;
				}

				if ( !fp && pNote->get_instrument() != instrRef ) {
					continue;
				}

				if (prefpredelete>=1 && prefpredelete <=14 ) {
					pNote->set_just_recorded( false );
				}

				if ( (prefpredelete == 15) && (pNote->get_just_recorded() == false)) {
					bool replaceExisting = false;
					if (column == currentPosition) replaceExisting = true;
					EventQueue::AddMidiNoteVector noteAction;
					noteAction.m_column = currentPosition;
					noteAction.m_row =  pNote->get_instrument_id();//m_nInstrumentLookupTable[ instrument ];
					noteAction.m_pattern = currentPatternNumber;
					noteAction.f_velocity = velocity;
					noteAction.f_pan_L = pan_L;
					noteAction.f_pan_R = pan_R;
					noteAction.m_length = -1;
					noteAction.no_octaveKeyVal = (Note::Octave)0;
					noteAction.nk_noteKeyVal = (Note::Key)0;
					noteAction.b_isInstrumentMode = false;
					noteAction.b_isMidi = false;
					noteAction.b_noteExist = replaceExisting;
					EventQueue::get_instance()->m_addMidiNoteVector.push_back(noteAction);
					continue;
				}

				if ( ( pNote->get_just_recorded() == false )
					 && ( static_cast<int>( pNote->get_position() ) >= postdelete
						  && pNote->get_position() <column + predelete +1 )
					 ) {
					bool replaceExisting = false;
					if (column == currentPosition) replaceExisting = true;
					EventQueue::AddMidiNoteVector noteAction;
					noteAction.m_column = currentPosition;
					noteAction.m_row =  pNote->get_instrument_id();//m_nInstrumentLookupTable[ instrument ];
					noteAction.m_pattern = currentPatternNumber;
					noteAction.f_velocity = velocity;
					noteAction.f_pan_L = pan_L;
					noteAction.f_pan_R = pan_R;
					noteAction.m_length = -1;
					noteAction.no_octaveKeyVal = (Note::Octave)0;
					noteAction.nk_noteKeyVal = (Note::Key)0;
					noteAction.b_isInstrumentMode = false;
					noteAction.b_isMidi = false;
					noteAction.b_noteExist = replaceExisting;
					EventQueue::get_instance()->m_addMidiNoteVector.push_back(noteAction);
				}
			} /* FOREACH */
		} /* if dorecord ... */

		assert( currentPattern );
		if ( doRecord ) {
			EventQueue::AddMidiNoteVector noteAction;
			noteAction.m_column = column;
			noteAction.m_pattern = currentPatternNumber;
			noteAction.f_velocity = velocity;
			noteAction.f_pan_L = pan_L;
			noteAction.f_pan_R = pan_R;
			noteAction.m_length = -1;
			noteAction.b_isMidi = true;

			if ( pPreferences->__playselectedinstrument ) {
				instrRef = pSong->get_instrument_list()->get( m_pAudioEngine->getSelectedInstrumentNumber() );
				int divider = msg1 / 12;
				noteAction.m_row = m_pAudioEngine->getSelectedInstrumentNumber();
				noteAction.no_octaveKeyVal = (Note::Octave)(divider -3);
				noteAction.nk_noteKeyVal = (Note::Key)(msg1 - (12 * divider));
				noteAction.b_isInstrumentMode = true;
			} else {
				instrRef = pSong->get_instrument_list()->get( m_nInstrumentLookupTable[ instrument ] );
				noteAction.m_row =  m_nInstrumentLookupTable[ instrument ];
				noteAction.no_octaveKeyVal = (Note::Octave)0;
				noteAction.nk_noteKeyVal = (Note::Key)0;
				noteAction.b_isInstrumentMode = false;
			}

			Note* pNoteold = currentPattern->find_note( noteAction.m_column, -1, instrRef, noteAction.nk_noteKeyVal, noteAction.no_octaveKeyVal );
			noteAction.b_noteExist = ( pNoteold ) ? true : false;

			EventQueue::get_instance()->m_addMidiNoteVector.push_back(noteAction);

			// hear note if its not in the future
			if ( pPreferences->getHearNewNotes() && position <= m_pAudioEngine->getTickPosition() ) {
				hearnote = true;
			}
		} /* if doRecord */
	} else if ( pPreferences->getHearNewNotes() ) {
			hearnote = true;
	} /* if .. STATE_PLAYING */

	if ( !pPreferences->__playselectedinstrument ) {
		if ( hearnote && instrRef ) {
			Note *pNote2 = new Note( instrRef, nRealColumn, velocity, pan_L, pan_R, -1, 0 );
			midi_noteOn( pNote2 );
		}
	} else if ( hearnote  ) {
		Instrument* pInstr = pSong->get_instrument_list()->get( m_pAudioEngine->getSelectedInstrumentNumber() );
		Note *pNote2 = new Note( pInstr, nRealColumn, velocity, pan_L, pan_R, -1, 0 );

		int divider = msg1 / 12;
		Note::Octave octave = (Note::Octave)(divider -3);
		Note::Key notehigh = (Note::Key)(msg1 - (12 * divider));

		//ERRORLOG( QString( "octave: %1, note: %2, instrument %3" ).arg( octave ).arg(notehigh).arg(instrument));
		pNote2->set_midi_info( notehigh, octave, msg1 );
		midi_noteOn( pNote2 );
	}

	m_pAudioEngine->unlock(); // unlock the audio engine
}

unsigned long Hydrogen::getRealtimeTickPosition()
{
	// Get the realtime transport position in frames and convert
	// it into ticks.
	unsigned int initTick = ( unsigned int )( getRealtimeFrames() /
						  m_pAudioEngine->m_pAudioDriver->m_transport.m_fTickSize );
	unsigned long retTick;

	struct timeval currtime;
	struct timeval deltatime;

	double sampleRate = ( double ) m_pAudioEngine->m_pAudioDriver->getSampleRate();
	gettimeofday ( &currtime, nullptr );

	// Definition macro from timehelper.h calculating the time
	// difference between `currtime` and `m_currentTickTime`
	// (`currtime`-`m_currentTickTime`) and storing the results in
	// `deltatime`. It uses both the .tv_sec (seconds) and
	// .tv_usec (microseconds) members of the timeval struct.
	timersub( &currtime, &m_currentTickTime, &deltatime );

	// add a buffers worth for jitter resistance
	double deltaSec =
			( double ) deltatime.tv_sec
			+ ( deltatime.tv_usec / 1000000.0 )
			+ ( m_pAudioEngine->m_pAudioDriver->getBufferSize() / ( double )sampleRate );

	retTick = ( unsigned long ) ( ( sampleRate / ( double ) m_pAudioEngine->m_pAudioDriver->m_transport.m_fTickSize ) * deltaSec );

	retTick += initTick;

	return retTick;
}

/* Return pattern for selected song tick position */
int Hydrogen::getPosForTick( unsigned long TickPos, int* nPatternStartTick )
{
	Song* pSong = getSong();
	if ( pSong == nullptr ) {
		return 0;
	}

	return findPatternInTick( TickPos, pSong->is_loop_enabled(), nPatternStartTick );
}

int Hydrogen::calculateLeadLagFactor( float fTickSize ){
	return fTickSize * 5;
}

int Hydrogen::calculateLookahead( float fTickSize ){
	// Introduce a lookahead of 5 ticks. Since the ticksize is
	// depending of the current tempo of the song, this component does
	// make the lookahead dynamic.
	int nLeadLagFactor = calculateLeadLagFactor( fTickSize );

	// We need to look ahead in the song for notes with negative offsets
	// from LeadLag or Humanize.
	return nLeadLagFactor + m_nMaxTimeHumanize + 1;
}

void Hydrogen::restartDrivers()
{
	m_pAudioEngine->audioEngine_restartAudioDrivers();
}

void Hydrogen::startExportSession(int sampleRate, int sampleDepth )
{
	if ( m_pAudioEngine->getState() == STATE_PLAYING ) {
		sequencer_stop();
	}
	
	unsigned nSamplerate = (unsigned) sampleRate;
	
	m_pAudioEngine->getSampler()->stop_playing_notes();

	Song* pSong = getSong();
	
	m_oldEngineMode = pSong->get_mode();
	m_bOldLoopEnabled = pSong->is_loop_enabled();

	pSong->set_mode( Song::SONG_MODE );
	pSong->set_loop_enabled( true );
	
	/*
	 * Currently an audio driver is loaded
	 * which is not the DiskWriter driver.
	 * Stop the current driver and fire up the DiskWriter.
	 */
	m_pAudioEngine->audioEngine_stopAudioDrivers();

	m_pAudioEngine->m_pAudioDriver = new DiskWriterDriver( aE_process, nSamplerate, sampleDepth );
	
	m_bExportSessionIsActive = true;
}

void Hydrogen::stopExportSession()
{
	m_bExportSessionIsActive = false;
	
 	m_pAudioEngine->audioEngine_stopAudioDrivers();
	
	delete m_pAudioEngine->m_pAudioDriver;
	m_pAudioEngine->m_pAudioDriver = nullptr;
	
	Song* pSong = getSong();
	pSong->set_mode( m_oldEngineMode );
	pSong->set_loop_enabled( m_bOldLoopEnabled );
	
	m_pAudioEngine->audioEngine_startAudioDrivers();

	if ( m_pAudioEngine->m_pAudioDriver ) {
		m_pAudioEngine->m_pAudioDriver->setBpm( pSong->__bpm );
	} else {
		ERRORLOG( "m_pAudioEngine->m_pAudioDriver = NULL" );
	}
}

/// Export a song to a wav file
void Hydrogen::startExportSong( const QString& filename)
{
	// reset
	m_pAudioEngine->m_pAudioDriver->m_transport.m_nFrames = 0; // reset total frames
	// TODO: not -1 instead?
	m_pAudioEngine->setPatternPos( 0 );
	m_pAudioEngine->setTickPosition( 0 );
	m_pAudioEngine->setState( STATE_PLAYING );
	m_pAudioEngine->resetPatternStartTick();

	Preferences *pPref = Preferences::get_instance();

	int res = m_pAudioEngine->m_pAudioDriver->init( pPref->m_nBufferSize );
	if ( res != 0 ) {
		ERRORLOG( "Error starting disk writer driver [DiskWriterDriver::init()]" );
	}

	m_pAudioEngine->setMainBuffer_L( m_pAudioEngine->m_pAudioDriver->getOut_L() );
	m_pAudioEngine->setMainBuffer_R( m_pAudioEngine->m_pAudioDriver->getOut_R() );

	m_pAudioEngine->audioEngine_setupLadspaFX( m_pAudioEngine->m_pAudioDriver->getBufferSize() );

	m_pAudioEngine->audioEngine_seek( 0, false );

	DiskWriterDriver* pDiskWriterDriver = (DiskWriterDriver*) m_pAudioEngine->m_pAudioDriver;
	pDiskWriterDriver->setFileName( filename );
	
	res = m_pAudioEngine->m_pAudioDriver->connect();
	if ( res != 0 ) {
		ERRORLOG( "Error starting disk writer driver [DiskWriterDriver::connect()]" );
	}
}

void Hydrogen::stopExportSong()
{
	if ( m_pAudioEngine->m_pAudioDriver->class_name() != DiskWriterDriver::class_name() ) {
		return;
	}

	m_pAudioEngine->getSampler()->stop_playing_notes();
	
	m_pAudioEngine->m_pAudioDriver->disconnect();

	m_pAudioEngine->setPatternPos( -1 );
	m_pAudioEngine->setTickPosition( 0 );
}

/// Used to display audio driver info
AudioOutput* Hydrogen::getAudioOutput()
{
	return m_pAudioEngine->m_pAudioDriver;
}

// Setting conditional to true will keep instruments that have notes if new kit has less instruments than the old one
int Hydrogen::loadDrumkit( Drumkit *pDrumkitInfo )
{
	return loadDrumkit( pDrumkitInfo, true );
}

int Hydrogen::loadDrumkit( Drumkit *pDrumkitInfo, bool conditional )
{
	assert ( pDrumkitInfo );

	int old_ae_state = m_pAudioEngine->getState();
	if( old_ae_state >= STATE_READY ) {
		m_pAudioEngine->setState( STATE_PREPARED );
	}

	INFOLOG( pDrumkitInfo->get_name() );
	m_currentDrumkit = pDrumkitInfo->get_name();

	std::vector<DrumkitComponent*>* pSongCompoList= getSong()->get_components();
	std::vector<DrumkitComponent*>* pDrumkitCompoList = pDrumkitInfo->get_components();
	
	m_pAudioEngine->lock( RIGHT_HERE );	
	for( auto &pComponent : *pSongCompoList ){
		delete pComponent;
	}
	pSongCompoList->clear();
	m_pAudioEngine->unlock();
	
	for (std::vector<DrumkitComponent*>::iterator it = pDrumkitCompoList->begin() ; it != pDrumkitCompoList->end(); ++it) {
		DrumkitComponent* pSrcComponent = *it;
		DrumkitComponent* pNewComponent = new DrumkitComponent( pSrcComponent->get_id(), pSrcComponent->get_name() );
		pNewComponent->load_from( pSrcComponent );

		pSongCompoList->push_back( pNewComponent );
	}

	//current instrument list
	InstrumentList *pSongInstrList = getSong()->get_instrument_list();
	
	//new instrument list
	InstrumentList *pDrumkitInstrList = pDrumkitInfo->get_instruments();
	
	/*
	 * If the old drumkit is bigger then the new drumkit,
	 * delete all instruments with a bigger pos then
	 * pDrumkitInstrList->size(). Otherwise the instruments
	 * from our old instrumentlist with
	 * pos > pDrumkitInstrList->size() stay in the
	 * new instrumentlist
	 *
	 * wolke: info!
	 * this has moved to the end of this function
	 * because we get lost objects in memory
	 * now:
	 * 1. the new drumkit will loaded
	 * 2. all not used instruments will complete deleted
	 * old function:
	 * while ( pDrumkitInstrList->size() < songInstrList->size() )
	 * {
	 *  songInstrList->del(songInstrList->size() - 1);
	 * }
	 */
	
	//needed for the new delete function
	int instrumentDiff =  pSongInstrList->size() - pDrumkitInstrList->size();
	
	for ( unsigned nInstr = 0; nInstr < pDrumkitInstrList->size(); ++nInstr ) {
		Instrument *pInstr = nullptr;
		if ( nInstr < pSongInstrList->size() ) {
			//instrument exists already
			pInstr = pSongInstrList->get( nInstr );
			assert( pInstr );
		} else {
			pInstr = new Instrument();
			// The instrument isn't playing yet; no need for locking
			// :-) - Jakob Lund.  m_pAudioEngine->lock(
			// "Hydrogen::loadDrumkit" );
			pSongInstrList->add( pInstr );
			// m_pAudioEngine->unlock();
		}

		Instrument *pNewInstr = pDrumkitInstrList->get( nInstr );
		assert( pNewInstr );
		INFOLOG( QString( "Loading instrument (%1 of %2) [%3]" )
				 .arg( nInstr + 1 )
				 .arg( pDrumkitInstrList->size() )
				 .arg( pNewInstr->get_name() ) );

		// Moved code from here right into the Instrument class - Jakob Lund.
		pInstr->load_from( pDrumkitInfo, pNewInstr );
	}

	//wolke: new delete function
	if ( instrumentDiff >= 0 ) {
		for ( int i = 0; i < instrumentDiff ; i++ ){
			removeInstrument(
						getSong()->get_instrument_list()->size() - 1,
						conditional
						);
		}
	}

#ifdef H2CORE_HAVE_JACK
	m_pAudioEngine->lock( RIGHT_HERE );
	renameJackPorts( getSong() );
	m_pAudioEngine->unlock();
#endif

	m_pAudioEngine->setState( old_ae_state );
	
	m_pCoreActionController->initExternalControlInterfaces();

	return 0;	//ok
}

// This will check if an instrument has any notes
bool Hydrogen::instrumentHasNotes( Instrument *pInst )
{
	Song* pSong = getSong();
	PatternList* pPatternList = pSong->get_pattern_list();

	for ( int nPattern = 0 ; nPattern < (int)pPatternList->size() ; ++nPattern )
	{
		if( pPatternList->get( nPattern )->references( pInst ) )
		{
			DEBUGLOG("Instrument " + pInst->get_name() + " has notes" );
			return true;
		}
	}

	// no notes for this instrument
	return false;
}

//this is also a new function and will used from the new delete function in
//Hydrogen::loadDrumkit to delete the instruments by number
void Hydrogen::removeInstrument( int instrumentNumber, bool conditional )
{
	Song* pSong = getSong();
	Instrument *pInstr = pSong->get_instrument_list()->get( instrumentNumber );
	PatternList* pPatternList = pSong->get_pattern_list();

	if ( conditional ) {
		// new! this check if a pattern has an active note if there is an note
		//inside the pattern the instrument would not be deleted
		for ( int nPattern = 0 ;
			  nPattern < (int)pPatternList->size() ;
			  ++nPattern ) {
			if( pPatternList
					->get( nPattern )
					->references( pInstr ) ) {
				DEBUGLOG("Keeping instrument #" + QString::number( instrumentNumber ) );
				return;
			}
		}
	} else {
		getSong()->purge_instrument( pInstr );
	}

	InstrumentList* pList = pSong->get_instrument_list();
	if ( pList->size()==1 ){
		m_pAudioEngine->lock( RIGHT_HERE );
		Instrument* pInstr = pList->get( 0 );
		pInstr->set_name( (QString( "Instrument 1" )) );
		for (std::vector<InstrumentComponent*>::iterator it = pInstr->get_components()->begin() ; it != pInstr->get_components()->end(); ++it) {
			InstrumentComponent* pCompo = *it;
			// remove all layers
			for ( int nLayer = 0; nLayer < InstrumentComponent::getMaxLayers(); nLayer++ ) {
				pCompo->set_layer( nullptr, nLayer );
			}
		}
		m_pAudioEngine->unlock();
		EventQueue::get_instance()->push_event( EVENT_SELECTED_INSTRUMENT_CHANGED, -1 );
		INFOLOG("clear last instrument to empty instrument 1 instead delete the last instrument");
		return;
	}

	// if the instrument was the last on the instruments list, select the
	// next-last
	if ( instrumentNumber >= (int)getSong()->get_instrument_list()->size() - 1 ) {
		m_pAudioEngine()->setSelectedInstrumentNumber(
					std::max(0, instrumentNumber - 1 )
					);
	}
	//
	// delete the instrument from the instruments list
	m_pAudioEngine->lock( RIGHT_HERE );
	getSong()->get_instrument_list()->del( instrumentNumber );
	// Ensure the selected instrument is not a deleted one
	m_pAudioEngine->setSelectedInstrumentNumber( instrumentNumber - 1 );
	getSong()->set_is_modified( true );
	m_pAudioEngine->unlock();

	// At this point the instrument has been removed from both the
	// instrument list and every pattern in the song.  Hence there's no way
	// (NOTE) to play on that instrument, and once all notes have stopped
	// playing it will be save to delete.
	// the ugly name is just for debugging...
	QString xxx_name = QString( "XXX_%1" ) . arg( pInstr->get_name() );
	pInstr->set_name( xxx_name );
	__instrument_death_row.push_back( pInstr );
	__kill_instruments(); // checks if there are still notes.

	// this will force a GUI update.
	EventQueue::get_instance()->push_event( EVENT_SELECTED_INSTRUMENT_CHANGED, -1 );
}

void Hydrogen::raiseError( unsigned nErrorCode )
{
	m_pAudioEngine->audioEngine_raiseError( nErrorCode );
}

unsigned long Hydrogen::getTotalFrames()
{
	return m_pAudioEngine->m_pAudioDriver->m_transport.m_nFrames;
}

void Hydrogen::setRealtimeFrames( unsigned long frames )
{
	m_nRealtimeFrames = frames;
}

unsigned long Hydrogen::getRealtimeFrames()
{
	return m_nRealtimeFrames;
}


long Hydrogen::getTickForPosition( int pos )
{
	Song* pSong = getSong();

	int nPatternGroups = pSong->get_pattern_group_vector()->size();
	if ( nPatternGroups == 0 ) {
		return -1;
	}

	if ( pos >= nPatternGroups ) {
		// The position is beyond the end of the Song, we
		// set periodic boundary conditions or return the
		// beginning of the Song as a fallback.
		if ( pSong->is_loop_enabled() ) {
			pos = pos % nPatternGroups;
		} else {
			WARNINGLOG( QString( "patternPos > nPatternGroups. pos:"
								 " %1, nPatternGroups: %2")
						.arg( pos ) .arg(  nPatternGroups )
						);
			return -1;
		}
	}

	std::vector<PatternList*> *pColumns = pSong->get_pattern_group_vector();
	long totalTick = 0;
	int nPatternSize;
	Pattern *pPattern = nullptr;
	
	for ( int i = 0; i < pos; ++i ) {
		PatternList *pColumn = ( *pColumns )[ i ];
		
		if( pColumn->size() > 0)
		{
			pPattern = pColumn->get( 0 );
			if ( pPattern ) {
				nPatternSize = pPattern->get_length();
			} else {
				nPatternSize = MAX_NOTES;
			}
		} else {
			nPatternSize = MAX_NOTES;
		}
		totalTick += nPatternSize;
	}
	
	return totalTick;
}

void Hydrogen::setPatternPos( int pos )
{
	if ( pos < -1 ) {
		pos = -1;
	}
	
	m_pAudioEngine->lock( RIGHT_HERE );
	// TODO: why?
	EventQueue::get_instance()->push_event( EVENT_METRONOME, 1 );
	long totalTick = getTickForPosition( pos );
	if ( totalTick < 0 ) {
		m_pAudioEngine->unlock();
		return;
	}

	if ( m_pAudioEngine->getState() != STATE_PLAYING ) {
		// find pattern immediately when not playing
		//		int dummy;
		// 		m_nSongPos = findPatternInTick( totalTick,
		//					        pSong->is_loop_enabled(),
		//					        &dummy );
		m_pAudioEngine->setPatternPos( pos );
		m_pAudioEngine->setTickPosition( 0 );
	}
	m_pAudioEngine->m_pAudioDriver->locate(
				( int ) ( totalTick * m_pAudioEngine->m_pAudioDriver->m_transport.m_fTickSize )
				);

	m_pAudioEngine->unlock();
}

void Hydrogen::onTapTempoAccelEvent()
{
#ifndef WIN32
	INFOLOG( "tap tempo" );
	static timeval oldTimeVal;

	struct timeval now;
	gettimeofday(&now, nullptr);

	float fInterval =
			(now.tv_sec - oldTimeVal.tv_sec) * 1000.0
			+ (now.tv_usec - oldTimeVal.tv_usec) / 1000.0;

	oldTimeVal = now;

	if ( fInterval < 1000.0 ) {
		setTapTempo( fInterval );
	}
#endif
}

void Hydrogen::setTapTempo( float fInterval )
{

	//	infoLog( "set tap tempo" );
	static float fOldBpm1 = -1;
	static float fOldBpm2 = -1;
	static float fOldBpm3 = -1;
	static float fOldBpm4 = -1;
	static float fOldBpm5 = -1;
	static float fOldBpm6 = -1;
	static float fOldBpm7 = -1;
	static float fOldBpm8 = -1;

	float fBPM = 60000.0 / fInterval;

	if ( fabs( fOldBpm1 - fBPM ) > 20 ) {	// troppa differenza, niente media
		fOldBpm1 = fBPM;
		fOldBpm2 = fBPM;
		fOldBpm3 = fBPM;
		fOldBpm4 = fBPM;
		fOldBpm5 = fBPM;
		fOldBpm6 = fBPM;
		fOldBpm7 = fBPM;
		fOldBpm8 = fBPM;
	}

	if ( fOldBpm1 == -1 ) {
		fOldBpm1 = fBPM;
		fOldBpm2 = fBPM;
		fOldBpm3 = fBPM;
		fOldBpm4 = fBPM;
		fOldBpm5 = fBPM;
		fOldBpm6 = fBPM;
		fOldBpm7 = fBPM;
		fOldBpm8 = fBPM;
	}

	fBPM = ( fBPM + fOldBpm1 + fOldBpm2 + fOldBpm3 + fOldBpm4 + fOldBpm5
			 + fOldBpm6 + fOldBpm7 + fOldBpm8 ) / 9.0;

	INFOLOG( QString( "avg BPM = %1" ).arg( fBPM ) );
	fOldBpm8 = fOldBpm7;
	fOldBpm7 = fOldBpm6;
	fOldBpm6 = fOldBpm5;
	fOldBpm5 = fOldBpm4;
	fOldBpm4 = fOldBpm3;
	fOldBpm3 = fOldBpm2;
	fOldBpm2 = fOldBpm1;
	fOldBpm1 = fBPM;

	m_pAudioEngine->lock( RIGHT_HERE );

	setBPM( fBPM );

	m_pAudioEngine->unlock();
}

void Hydrogen::setBPM( float fBPM )
{
	Song* pSong = getSong();
	if ( ! m_pAudioEngine->m_pAudioDriver || ! pSong ){
		return;
	}

	if ( haveJackTimebaseClient() ) {
		ERRORLOG( "Unable to change tempo directly in the presence of an external JACK timebase master. Press 'J.MASTER' get tempo control." );
		return;
	}
	
	m_pAudioEngine->m_pAudioDriver->setBpm( fBPM );
	pSong->__bpm = fBPM;
	m_pAudioEngine->setNewBpmJTM( fBPM );
}

void Hydrogen::restartLadspaFX()
{
	if ( m_pAudioEngine->m_pAudioDriver ) {
		m_pAudioEngine->lock( RIGHT_HERE );
		m_pAudioEngine->audioEngine_setupLadspaFX( m_pAudioEngine->m_pAudioDriver->getBufferSize() );
		m_pAudioEngine->unlock();
	} else {
		ERRORLOG( "m_pAudioEngine->m_pAudioDriver = NULL" );
	}
}

void Hydrogen::refreshInstrumentParameters( int nInstrument )
{
	EventQueue::get_instance()->push_event( EVENT_PARAMETERS_INSTRUMENT_CHANGED, -1 );
}

#ifdef H2CORE_HAVE_JACK
void Hydrogen::renameJackPorts( Song *pSong )
{
	if( Preferences::get_instance()->m_bJackTrackOuts == true ){
		m_pAudioEngine->audioEngine_renameJackPorts(pSong);
	}
}
#endif

/** Updates #m_nbeatsToCount
 * \param beatstocount New value*/
void Hydrogen::setbeatsToCount( int beatstocount)
{
	m_nbeatsToCount = beatstocount;
}
/** \return #m_nbeatsToCount*/
int Hydrogen::getbeatsToCount()
{
	return m_nbeatsToCount;
}

void Hydrogen::setNoteLength( float notelength)
{
	m_ntaktoMeterCompute = notelength;
}

float Hydrogen::getNoteLength()
{
	return m_ntaktoMeterCompute;
}

int Hydrogen::getBcStatus()
{
	return m_nEventCount;
}

void Hydrogen::setBcOffsetAdjust()
{
	//individual fine tuning for the m_nBeatCounter
	//to adjust  ms_offset from different people and controller
	Preferences *pPreferences = Preferences::get_instance();

	m_nCoutOffset = pPreferences->m_countOffset;
	m_nStartOffset = pPreferences->m_startOffset;
}

void Hydrogen::handleBeatCounter()
{
	// Get first time value:
	if (m_nBeatCount == 1) {
		gettimeofday(&m_CurrentTime,nullptr);
	}

	m_nEventCount++;

	// Set wm_LastTime to wm_CurrentTime to remind the time:
	m_LastTime = m_CurrentTime;

	// Get new time:
	gettimeofday(&m_CurrentTime,nullptr);


	// Build doubled time difference:
	m_nLastBeatTime = (double)(
				m_LastTime.tv_sec
				+ (double)(m_LastTime.tv_usec * US_DIVIDER)
				+ (int)m_nCoutOffset * .0001
				);
	m_nCurrentBeatTime = (double)(
				m_CurrentTime.tv_sec
				+ (double)(m_CurrentTime.tv_usec * US_DIVIDER)
				);
	m_nBeatDiff = m_nBeatCount == 1 ? 0 : m_nCurrentBeatTime - m_nLastBeatTime;

	//if differences are to big reset the beatconter
	if( m_nBeatDiff > 3.001 * 1/m_ntaktoMeterCompute ) {
		m_nEventCount = 1;
		m_nBeatCount = 1;
		return;
	}
	// Only accept differences big enough
	if (m_nBeatCount == 1 || m_nBeatDiff > .001) {
		if (m_nBeatCount > 1) {
			m_nBeatDiffs[m_nBeatCount - 2] = m_nBeatDiff ;
		}
		// Compute and reset:
		if (m_nBeatCount == m_nbeatsToCount){
			//				unsigned long currentframe = getRealtimeFrames();
			double beatTotalDiffs = 0;
			for(int i = 0; i < (m_nbeatsToCount - 1); i++) {
				beatTotalDiffs += m_nBeatDiffs[i];
			}
			double m_nBeatDiffAverage =
					beatTotalDiffs
					/ (m_nBeatCount - 1)
					* m_ntaktoMeterCompute ;
			m_fBeatCountBpm	 =
					(float) ((int) (60 / m_nBeatDiffAverage * 100))
					/ 100;
			m_pAudioEngine->lock( RIGHT_HERE );
			if ( m_fBeatCountBpm > MAX_BPM) {
				m_fBeatCountBpm = MAX_BPM;
			}
			
			setBPM( m_fBeatCountBpm );
			m_pAudioEngine->unlock();
			if (Preferences::get_instance()->m_mmcsetplay
					== Preferences::SET_PLAY_OFF) {
				m_nBeatCount = 1;
				m_nEventCount = 1;
			}else{
				if ( m_pAudioEngine->getState() != STATE_PLAYING ){
					unsigned bcsamplerate =
							m_pAudioEngine->m_pAudioDriver->getSampleRate();
					unsigned long rtstartframe = 0;
					if ( m_ntaktoMeterCompute <= 1){
						rtstartframe =
								bcsamplerate
								* m_nBeatDiffAverage
								* ( 1/ m_ntaktoMeterCompute );
					}else
					{
						rtstartframe =
								bcsamplerate
								* m_nBeatDiffAverage
								/ m_ntaktoMeterCompute ;
					}

					int sleeptime =
							( (float) rtstartframe
							  / (float) bcsamplerate
							  * (int) 1000 )
							+ (int)m_nCoutOffset
							+ (int) m_nStartOffset;
#ifdef WIN32
					Sleep( sleeptime );
#else
					usleep( 1000 * sleeptime );
#endif

					sequencer_play();
				}

				m_nBeatCount = 1;
				m_nEventCount = 1;
				return;
			}
		}
		else {
			m_nBeatCount ++;
		}
	}
	return;
}
//~ m_nBeatCounter

#ifdef H2CORE_HAVE_JACK
void Hydrogen::offJackMaster()
{
	if ( haveJackTransport() ) {
		static_cast< JackAudioDriver* >( m_pAudioEngine->m_pAudioDriver )->releaseTimebaseMaster();
	}
}

void Hydrogen::onJackMaster()
{
	if ( haveJackTransport() ) {
		static_cast< JackAudioDriver* >( m_pAudioEngine->m_pAudioDriver )->initTimebaseMaster();
	}
}
#endif

long Hydrogen::getPatternLength( int nPattern )
{
	Song* pSong = getSong();
	if ( pSong == nullptr ){
		return -1;
	}

	std::vector< PatternList* > *pColumns = pSong->get_pattern_group_vector();

	int nPatternGroups = pColumns->size();
	if ( nPattern >= nPatternGroups ) {
		if ( pSong->is_loop_enabled() ) {
			nPattern = nPattern % nPatternGroups;
		} else {
			return MAX_NOTES;
		}
	}

	if ( nPattern < 1 ){
		return MAX_NOTES;
	}

	PatternList* pPatternList = pColumns->at( nPattern - 1 );
	Pattern* pPattern = pPatternList->get( 0 );
	if ( pPattern ) {
		return pPattern->get_length();
	} else {
		return MAX_NOTES;
	}
}

//~ jack transport master

void Hydrogen::__kill_instruments()
{
	int c = 0;
	Instrument * pInstr = nullptr;
	while ( __instrument_death_row.size()
			&& __instrument_death_row.front()->is_queued() == 0 ) {
		pInstr = __instrument_death_row.front();
		__instrument_death_row.pop_front();
		INFOLOG( QString( "Deleting unused instrument (%1). "
						  "%2 unused remain." )
				 . arg( pInstr->get_name() )
				 . arg( __instrument_death_row.size() ) );
		delete pInstr;
		c++;
	}
	if ( __instrument_death_row.size() ) {
		pInstr = __instrument_death_row.front();
		INFOLOG( QString( "Instrument %1 still has %2 active notes. "
						  "Delaying 'delete instrument' operation." )
				 . arg( pInstr->get_name() )
				 . arg( pInstr->is_queued() ) );
	}
}



void Hydrogen::__panic()
{
	sequencer_stop();
	m_pAudioEngine->getSampler()->stop_playing_notes();
}

unsigned int Hydrogen::__getMidiRealtimeNoteTickPosition()
{
	return m_naddrealtimenotetickposition;
}

float Hydrogen::getTimelineBpm( int nBar )
{
	Song* pSong = getSong();

	// We need return something
	if ( pSong == nullptr ) {
		return m_pAudioEngine->getNewBpmJTM();
	}

	float fBPM = pSong->__bpm;

	// Pattern mode don't use timeline and will have a constant
	// speed.
	if ( pSong->get_mode() == Song::PATTERN_MODE ) {
		return fBPM;
	}

	// Check whether the user wants Hydrogen to determine the
	// speed by local setting along the timeline or whether she
	// wants to use a global speed instead.
	if ( ! Preferences::get_instance()->getUseTimelineBpm() ) {
		return fBPM;
	}

	// Determine the speed at the supplied beat.
	float fTimelineBpm = m_pTimeline->getTempoAtBar( nBar, true );
	if ( fTimelineBpm != 0 ) {
		/* TODO: For now the function returns 0 if the bar is
		 * positioned _before_ the first tempo marker. This will be
		 * taken care of with #854. */
		fBPM = fTimelineBpm;
	}

	return fBPM;
}

void Hydrogen::setTimelineBpm()
{
	if ( ! Preferences::get_instance()->getUseTimelineBpm() ||
		 haveJackTimebaseClient() ) {
		return;
	}

	Song* pSong = getSong();
	// Obtain the local speed specified for the current Pattern.
	float fBPM = getTimelineBpm( m_pAudioEngine()->getPatternPos() );

	if ( fBPM != pSong->__bpm ) {
		setBPM( fBPM );
	}

	// Get the realtime pattern position. This also covers
	// keyboard and MIDI input events in case the audio engine is
	// not playing.
	unsigned long PlayTick = getRealtimeTickPosition();
	int nStartPos;
	int nRealtimePatternPos = getPosForTick( PlayTick, &nStartPos );
	float fRealtimeBPM = getTimelineBpm( nRealtimePatternPos );

	// FIXME: this was already done in setBPM but for "engine" time
	//        so this is actually forcibly overwritten here
	m_pAudioEngine->setNewBpmJTM( fRealtimeBPM );
}

bool Hydrogen::haveJackAudioDriver() const {
#ifdef H2CORE_HAVE_JACK
	if ( m_pAudioEngine->m_pAudioDriver != nullptr ) {
		if ( JackAudioDriver::class_name() == m_pAudioEngine->m_pAudioDriver->class_name() ){
			return true;
		}
	}
	return false;
#else
	return false;
#endif	
}

bool Hydrogen::haveJackTransport() const {
#ifdef H2CORE_HAVE_JACK
	if ( m_pAudioEngine->m_pAudioDriver != nullptr ) {
		if ( JackAudioDriver::class_name() == m_pAudioEngine->m_pAudioDriver->class_name() &&
			 Preferences::get_instance()->m_bJackTransportMode ==
			 Preferences::USE_JACK_TRANSPORT ){
			return true;
		}
	}
	return false;
#else
	return false;
#endif	
}

bool Hydrogen::haveJackTimebaseClient() const {
#ifdef H2CORE_HAVE_JACK
	if ( haveJackTransport() ) {
		if ( static_cast<JackAudioDriver*>(m_pAudioEngine->m_pAudioDriver)->getIsTimebaseMaster() == 0 ) {
			return true;
		}
	} 
	return false;
#else
	return false;
#endif	
}

#ifdef H2CORE_HAVE_OSC

void Hydrogen::toggleOscServer( bool bEnable ) {
	if ( bEnable ) {
		OscServer::get_instance()->start();
	} else {
		OscServer::get_instance()->stop();
	}
}

void Hydrogen::recreateOscServer() {
	OscServer* pOscServer = OscServer::get_instance();
	if( pOscServer ) {
		delete pOscServer;
	}

	OscServer::create_instance( Preferences::get_instance() );
	
	if ( Preferences::get_instance()->getOscServerEnabled() ) {
		toggleOscServer( true );
	}
}

void Hydrogen::startNsmClient()
{
	//NSM has to be started before jack driver gets created
	NsmClient* pNsmClient = NsmClient::get_instance();

	if(pNsmClient){
		pNsmClient->createInitialClient();
	}
}
#endif

}; /* Namespace */
