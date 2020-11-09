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

#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include "hydrogen/config.h"
#include <hydrogen/object.h>
#include <hydrogen/sampler/Sampler.h>
#include <hydrogen/synth/Synth.h>

#include <string>
#include <cassert>
#include <mutex>
#include <chrono>
#include <memory>

/** \def RIGHT_HERE
 * Macro intended to be used for the logging of the locking of the
 * H2Core::AudioEngine. But this feature is not implemented yet.
 *
 * It combines two standard macros of the C language \_\_FILE\_\_ and
 * \_\_LINE\_\_ and one macro introduced by the GCC compiler called
 * \_\_PRETTY_FUNCTION\_\_.
 */
#ifndef RIGHT_HERE
#define RIGHT_HERE __FILE__, __LINE__, __PRETTY_FUNCTION__
#endif

namespace H2Core
{

/**
 * Audio Engine main class (Singleton).
 *
 * It serves as a container for the Sampler and Synth stored in the
 * #m_pSampler and #m_pSynth member objects and provides a mutex
 * #m_engineMutex enabling the user to synchronize the access of the
 * Song object and the AudioEngine itself. lock() and tryLock() can
 * be called by a thread to lock the engine and unlock() to make it
 * accessible for other threads once again.
 */ 
class AudioEngine : public H2Core::Object
{
	H2_OBJECT
public:
	AudioEngine();
	~AudioEngine();

	/** Mutex locking of the AudioEngine.
	 *
	 * Lock the AudioEngine for exclusive access by this thread.
	 *
	 * The documentation below may serve as a guide for future
	 * implementations. At the moment the logging of the locking
	 * is __not supported yet__ and the arguments will be just
	 * stored in the #m_locker variable, which itself won't be
	 * ever used.
	 *
	 * Easy usage:  Use the #RIGHT_HERE macro like this...
	 * \code{.cpp}
	 *     Hydrogen::get_instance()->getAudioEngine()->lock( RIGHT_HERE );
	 * \endcode
	 *
	 * More complex usage: The parameters @a file and @a function
	 * need to be pointers to null-terminated strings that are
	 * persistent for the entire session.  This does *not* include
	 * the return value of std::string::c_str(), or
	 * QString::toLocal8Bit().data().
	 *
	 * Tracing the locks:  Enable the Logger::AELockTracing
	 * logging level.  When you do, there will be a performance
	 * penalty because the strings will be converted to a
	 * QString.  At the moment, you'll have to do that with
	 * your debugger.
	 *
	 * Notes: The order of the parameters match GCC's
	 * implementation of the assert() macros.
	 *
	 * \param file File the locking occurs in.
	 * \param line Line of the file the locking occurs in.
	 * \param function Function the locking occurs in.
	 */
	void lock( const char* file, unsigned int line, const char* function );
	/**
	 * Mutex locking of the AudioEngine.
	 *
	 * This function is equivalent to lock() but returns false
	 * immediaely if the lock canot be obtained immediately.
	 *
	 * \param file File the locking occurs in.
	 * \param line Line of the file the locking occurs in.
	 * \param function Function the locking occurs in.
	 *
	 * \return
	 * - true : On success
	 * - false : Else
	 */
	bool tryLock( const char* file, unsigned int line, const char* function );

	/**
	 * Mutex locking of the AudioEngine.
	 *
	 * This function is equivalent to lock() but will only wait for a
	 * given period of time. If the lock cannot be acquired in this
	 * time, it will return false.
	 *
	 * \param duration Time (in microseconds) to wait for the lock.
	 * \param file File the locking occurs in.
	 * \param line Line of the file the locking occurs in.
	 * \param function Function the locking occurs in.
	 *
	 * \return
	 * - true : On successful acquisition of the lock
	 * - false : On failure
	 */
	bool tryLockFor( std::chrono::microseconds duration, const char* file, unsigned int line, const char* function );
	/**
	 * Mutex unlocking of the AudioEngine.
	 *
	 * Unlocks the AudioEngine to allow other threads acces, and leaves #m_locker untouched.
	 */
	void unlock();
	
	
	 static float computeTickSize(int nSampleRate, float fBpm, int nResolution);

	/** Returns #m_pSampler */
	std::shared_ptr<Sampler> getSampler();
	/** Returns #m_pSynth */
	std::shared_ptr<Synth> getSynth();
	// PROTOTYPES
	/**
	 * Initialization of the H2Core::AudioEngine called in Hydrogen::Hydrogen().
	 *
	 * -# It creates two new instances of the H2Core::PatternList and stores them
	 in #H2Core::m_pPlayingPatterns and #H2Core::m_pNextPatterns.
	 * -# It sets #H2Core::m_nSongPos = -1.
	 * -# It sets #H2Core::m_nSelectedPatternNumber, #H2Core::m_nSelectedInstrumentNumber,
	 and #H2Core::m_nPatternTickPosition to 0.
	 * -# It sets #H2Core::m_pMetronomeInstrument, #H2Core::m_pAudioDriver,
	 #H2Core::m_pMainBuffer_L, #H2Core::m_pMainBuffer_R to NULL.
	 * -# It uses the current time to a random seed via std::srand(). This
	 way the states of the pseudo-random number generator are not
	 cross-correlated between different runs of Hydrogen.
	 * -# It initializes the metronome with the sound stored in
	 H2Core::Filesystem::click_file_path() by creating a new
	 Instrument with #METRONOME_INSTR_ID as first argument.
	 * -# It sets the H2Core::AudioEngine state #H2Core::m_audioEngineState to
	 #STATE_INITIALIZED.
	 * -# It calls H2Core::Effects::create_instance() (if the
	 #H2CORE_HAVE_LADSPA is set),
	 H2Core::AudioEngine::create_instance(), and
	 H2Core::Playlist::create_instance().
	 * -# Finally, it pushes the H2Core::EVENT_STATE, #STATE_INITIALIZED
	 on the H2Core::EventQueue using
	 H2Core::EventQueue::push_event().
	 *
	 * If the current state of the H2Core::AudioEngine #H2Core::m_audioEngineState is not
	 * ::STATE_UNINITIALIZED, it will thrown an error and
	 * H2Core::AudioEngine::unlock() it.
	 */
	void				audioEngine_init();
	void				audioEngine_destroy();
	/**
	 * If the audio engine is in state #H2Core::m_audioEngineState #STATE_READY,
	 * this function will
	 * - sets #H2Core::m_fMasterPeak_L and #H2Core::m_fMasterPeak_R to 0.0f
	 * - sets TransportInfo::m_nFrames to @a nTotalFrames
	 * - sets H2Core::m_nSongPos and H2Core::m_nPatternStartTick to -1
	 * - H2Core::m_nPatternTickPosition to 0
	 * - calls H2Core::updateTickSize()
	 * - sets #H2Core::m_audioEngineState to #STATE_PLAYING
	 * - pushes the #EVENT_STATE #STATE_PLAYING using EventQueue::push_event()
	 *
	 * \param bLockEngine Whether or not to lock the audio engine before
	 *   performing any actions. The audio engine __must__ be locked! This
	 *   option should only be used, if the process calling this function
	 *   did already locked it.
	 * \param nTotalFrames New value of the transport position.
	 * \return 0 regardless what happens inside the function.
	 */
	int				audioEngine_start( bool bLockEngine = false, unsigned nTotalFrames = 0 );
	/**
	 * If the audio engine is in state #H2Core::m_audioEngineState #STATE_PLAYING,
	 * this function will
	 * - sets #H2Core::m_fMasterPeak_L and #H2Core::m_fMasterPeak_R to 0.0f
	 * - sets #H2Core::m_audioEngineState to #STATE_READY
	 * - sets #H2Core::m_nPatternStartTick to -1
	 * - deletes all copied Note in song notes queue #H2Core::m_songNoteQueue and
	 *   MIDI notes queue #H2Core::m_midiNoteQueue
	 * - calls the _clear()_ member of #H2Core::m_midiNoteQueue
	 *
	 * \param bLockEngine Whether or not to lock the audio engine before
	 *   performing any actions. The audio engine __must__ be locked! This
	 *   option should only be used, if the process calling this function
	 *   did already locked it.
	 */
	void				audioEngine_stop( bool bLockEngine = false );
	/**
	 * Updates the global objects of the audioEngine according to new #Song.
	 *
	 * Calls H2Core::audioEngine_setupLadspaFX() on
	 * H2Core::m_pAudioDriver->getBufferSize(),
	 * H2Core::audioEngine_process_checkBPMChanged(),
	 * H2Core::audioEngine_renameJackPorts(), adds its first pattern to
	 * #H2Core::m_pPlayingPatterns, relocates the audio driver to the beginning of
	 * the #Song, and updates the BPM.
	 *
	 * \param pNewSong #Song to load.
	 */
	void				audioEngine_setSong(Song *pNewSong );
	/**
	 * Does the necessary cleanup of the global objects in the audioEngine.
	 *
	 * Class the clear() member of #H2Core::m_pPlayingPatterns and
	 * #H2Core::m_pNextPatterns as well as H2Core::audioEngine_clearNoteQueue();
	 */
	void				audioEngine_removeSong();
	static void			audioEngine_noteOn( Note *note );

	/**
	 * Main audio processing function called by the audio drivers whenever
	 * there is work to do.
	 *
	 * In short, it resets the audio buffers, checks the current transport
	 * position and configuration, updates the queue of notes, which are
	 * about to be played, plays those notes and writes their output to
	 * the audio buffers, and, finally, increment the transport position
	 * in order to move forward in time.
	 *
	 * In detail the function
	 * - calls H2Core::audioEngine_process_clearAudioBuffers() to reset all audio
	 * buffers with zeros.
	 * - calls H2Core::audioEngine_process_transport() to verify the current
	 * TransportInfo stored in AudioOutput::m_transport. If e.g. the
	 * JACK server is used, an external JACK client might have changed the
	 * speed of the transport (as JACK timebase master) or the transport
	 * position. In such cases, Hydrogen has to sync its internal transport
	 * state AudioOutput::m_transport to reflect these changes. Else our
	 * playback would be off.
	 * - calls H2Core::audioEngine_process_checkBPMChanged() to check whether the
	 * tick size, the number of frames per bar (size of a pattern), has
	 * changed (see TransportInfo::m_nFrames in case you are unfamiliar
	 * with the term _frames_). This is necessary because the transport
	 * position is often given in ticks within Hydrogen and changing the
	 * speed of the Song, e.g. via Hydrogen::setBPM(), would thus result
	 * in a(n unintended) relocation of the transport location.
	 * - calls H2Core::audioEngine_updateNoteQueue() and
	 * H2Core::audioEngine_process_playNotes(), two functions which handle the
	 * selection and playback of notes and will documented at a later
	 * point in time
	 * - If H2Core::audioEngine_updateNoteQueue() returns with 2, the
	 * EVENT_PATTERN_CHANGED event will be pushed to the EventQueue.
	 * - writes the audio output of the Sampler, Synth, and the LadspaFX
	 * (if #H2CORE_HAVE_LADSPA is defined) to #H2Core::m_pMainBuffer_L and
	 * #H2Core::m_pMainBuffer_R and sets we peak values for #H2Core::m_fFXPeak_L,
	 * #H2Core::m_fFXPeak_R, #H2Core::m_fMasterPeak_L, and #H2Core::m_fMasterPeak_R.
	 * - finally increments the transport position
	 * TransportInfo::m_nFrames with the buffersize @a nframes. So, if
	 * this function is called during the next cycle, the transport is
	 * already in the correct position.
	 *
	 * If the H2Core::H2Core::m_audioEngineState is neither in #STATE_READY nor
	 * #STATE_PLAYING or the locking of the AudioEngine failed, the
	 * function will return 0 without performing any actions.
	 *
	 * \param nframes Buffersize. If it doesn't match #H2Core::m_nBufferSize, the
	 latter will be set to @a nframes.
	 * \param arg Unused.
	 * \return
	 * - __2__ : Failed to aquire the audio engine lock, no processing took place.
	 * - __1__ : kill the audio driver thread. This will be used if either
	 * the DiskWriterDriver or FakeDriver are used and the end of the Song
	 * is reached (H2Core::audioEngine_updateNoteQueue() returned -1 ). 
	 * - __0__ : else
	 */
	int				audioEngine_process( uint32_t nframes, void *arg );
	static int aE_process( uint32_t nframes, void *arg );
	inline void			audioEngine_clearNoteQueue();
	/**
	 * Update the tick size based on the current tempo without affecting
	 * the current transport position.
	 *
	 * To access a change in the tick size, the value stored in
	 * TransportInfo::m_fTickSize will be compared to the one calculated
	 * from the AudioOutput::getSampleRate(), Song::__bpm, and
	 * Song::__resolution. Thus, if any of those quantities did change,
	 * the transport position will be recalculated.
	 *
	 * The new transport position gets calculated by 
	 * \code{.cpp}
	 * ceil( H2Core::m_pAudioDriver->m_transport.m_nFrames/
	 *       H2Core::m_pAudioDriver->m_transport.m_fTickSize ) *
	 * H2Core::m_pAudioDriver->getSampleRate() * 60.0 / Song::__bpm / Song::__resolution 
	 * \endcode
	 *
	 * If the JackAudioDriver is used and the audio engine is playing, a
	 * potential mismatch in the transport position is determined by
	 * JackAudioDriver::calculateFrameOffset() and covered by
	 * JackAudioDriver::updateTransportInfo() in the next cycle.
	 *
	 * Finally, EventQueue::push_event() is called with
	 * #EVENT_RECALCULATERUBBERBAND and -1 as arguments.
	 *
	 * Called in H2Core::audioEngine_process() and H2Core::audioEngine_setSong(). The
	 * function will only perform actions if #H2Core::m_audioEngineState is in
	 * either #STATE_READY or #STATE_PLAYING.
	 */
	inline void			audioEngine_process_checkBPMChanged(Song *pSong);
	inline void			audioEngine_process_playNotes( unsigned long nframes );
	/**
	 * Updating the TransportInfo of the audio driver.
	 *
	 * Firstly, it calls AudioOutput::updateTransportInfo() and then
	 * updates the state of the audio engine #H2Core::m_audioEngineState depending
	 * on the status of the audio driver.  E.g. if the JACK transport was
	 * started by another client, the audio engine has to be started as
	 * well. If TransportInfo::m_status is TransportInfo::ROLLING,
	 * H2Core::audioEngine_start() is called with
	 * TransportInfo::m_nFrames as argument if the engine is in
	 * #STATE_READY. If #H2Core::m_audioEngineState is then still not in
	 * #STATE_PLAYING, the function will return. Otherwise, the current
	 * speed is getting updated by calling Hydrogen::setBPM using
	 * TransportInfo::m_fBPM and #H2Core::m_nRealtimeFrames is set to
	 * TransportInfo::m_nFrames.
	 *
	 * If the status is TransportInfo::STOPPED but the engine is still
	 * running, H2Core::audioEngine_stop() will be called. In any case,
	 * #H2Core::m_nRealtimeFrames will be incremented by #H2Core::m_nBufferSize to support
	 * realtime keyboard and MIDI event timing.
	 *
	 * If the H2Core::H2Core::m_audioEngineState is neither in #STATE_READY nor
	 * #STATE_PLAYING the function will immediately return.
	 */
	inline void			audioEngine_process_transport();

	inline unsigned		audioEngine_renderNote( Note* pNote, const unsigned& nBufferSize );
	// TODO: Add documentation of doErase, inPunchArea, and
	// m_addMidiNoteVector
	/**
	 * Takes all notes from the current patterns, from the MIDI queue
	 * #H2Core::m_midiNoteQueue, and those triggered by the metronome and pushes
	 * them onto #H2Core::m_songNoteQueue for playback.
	 *
	 * Apart from the MIDI queue, the extraction of all notes will be
	 * based on their position measured in ticks. Since Hydrogen does
	 * support humanization, which also involves triggering a Note
	 * earlier or later than its actual position, the loop over all ticks
	 * won't be done starting from the current position but at some
	 * position in the future. This value, also called @e lookahead, is
	 * set to the sum of the maximum offsets introduced by both the random
	 * humanization (2000 frames) and the deterministic lead-lag offset (5
	 * times TransportInfo::m_nFrames) plus 1 (note that it's not given in
	 * ticks but in frames!). Hydrogen thus loops over @a nFrames frames
	 * starting at the current position + the lookahead (or at 0 when at
	 * the beginning of the Song).
	 *
	 * Within this loop all MIDI notes in #H2Core::m_midiNoteQueue with a
	 * Note::__position smaller or equal the current tick will be popped
	 * and added to #H2Core::m_songNoteQueue and the #EVENT_METRONOME Event is
	 * pushed to the EventQueue at a periodic rate. If in addition
	 * Preferences::m_bUseMetronome is set to true,
	 * #H2Core::m_pMetronomeInstrument will be used to push a 'click' to the
	 * #H2Core::m_songNoteQueue too. All patterns enclosing the current tick will
	 * be added to #H2Core::m_pPlayingPatterns and all their containing notes,
	 * which position enclose the current tick too, will be added to the
	 * #H2Core::m_songNoteQueue. If the Song is in Song::PATTERN_MODE, the
	 * patterns used are not chosen by the actual position but by
	 * #H2Core::m_nSelectedPatternNumber and #H2Core::m_pNextPatterns. 
	 *
	 * All notes obtained by the current patterns (and only those) are
	 * also subject to humanization in the onset position of the created
	 * Note. For now Hydrogen does support three options of altering
	 * these:
	 * - @b Swing - A deterministic offset determined by Song::__swing_factor
	 * will be added for some notes in a periodic way.
	 * - @b Humanize - A random offset drawn from Gaussian white noise
	 * with a variance proportional to Song::__humanize_time_value will be
	 * added to every Note.
	 * - @b Lead/Lag - A deterministic offset determined by
	 * Note::__lead_lag will be added for every note.
	 *
	 * If the AudioEngine it not in #STATE_PLAYING, the loop jumps right
	 * to the next tick.
	 *
	 * \return
	 * - -1 if in Song::SONG_MODE and no patterns left.
	 * - 2 if the current pattern changed with respect to the last
	 * cycle.
	 */
	inline int			audioEngine_updateNoteQueue( unsigned nFrames );
	inline void			audioEngine_prepNoteQueue();

	/**
	 * Find a PatternList corresponding to the supplied tick position @a
	 * nTick.
	 *
	 * Adds up the lengths of all pattern columns until @a nTick lies in
	 * between the bounds of a Pattern.
	 *
	 * \param nTick Position in ticks.
	 * \param bLoopMode Whether looping is enabled in the Song, see
	 *   Song::is_loop_enabled(). If true, @a nTick is allowed to be
	 *   larger than the total length of the Song.
	 * \param pPatternStartTick Pointer to an integer the beginning of the
	 *   found pattern list will be stored in (in ticks).
	 * \return
	 *   - -1 : pattern list couldn't be found.
	 *   - >=0 : PatternList index in Song::__pattern_group_sequence.
	 */
	inline int			findPatternInTick( int nTick, bool bLoopMode, int* pPatternStartTick );

	void				audioEngine_seek( long long nFrames, bool bLoopMode = false );

	void				audioEngine_restartAudioDrivers();
	/** 
	 * Creation and initialization of all audio and MIDI drivers called in
	 * Hydrogen::Hydrogen().
	 *
	 * Which audio driver to use is specified in
	 * Preferences::m_sAudioDriver. If "Auto" is selected, it will try to
	 * initialize drivers using H2Core::createDriver() in the following order: 
	 * - Windows:  "PortAudio", "Alsa", "CoreAudio", "Jack", "Oss",
	 *   and "PulseAudio" 
	 * - all other systems: "Jack", "Alsa", "CoreAudio", "PortAudio",
	 *   "Oss", and "PulseAudio".
	 * If all of them return NULL, #H2Core::m_pAudioDriver will be initialized
	 * with the NullDriver instead. If a specific choice is contained in
	 * Preferences::m_sAudioDriver and H2Core::createDriver() returns NULL, the
	 * NullDriver will be initialized too.
	 *
	 * It probes Preferences::m_sMidiDriver to create a midi driver using
	 * either AlsaMidiDriver::AlsaMidiDriver(),
	 * PortMidiDriver::PortMidiDriver(), CoreMidiDriver::CoreMidiDriver(),
	 * or JackMidiDriver::JackMidiDriver(). Afterwards, it sets
	 * #H2Core::m_pMidiDriverOut and #H2Core::m_pMidiDriver to the freshly created midi
	 * driver and calls their open() and setActive( true ) functions.
	 *
	 * If a Song is already present, the state of the AudioEngine
	 * #H2Core::m_audioEngineState will be set to #STATE_READY, the bpm of the
	 * #H2Core::m_pAudioDriver will be set to the tempo of the Song Song::__bpm
	 * using AudioOutput::setBpm(), and #STATE_READY is pushed on the
	 * EventQueue. If no Song is present, the state will be
	 * #STATE_PREPARED and no bpm will be set.
	 *
	 * All the actions mentioned so far will be performed after locking
	 * both the AudioEngine using AudioEngine::lock() and the mutex of the
	 * audio output buffer #H2Core::mutex_OutputPointer. When they are completed
	 * both mutex are unlocked and the audio driver is connected via
	 * AudioOutput::connect(). If this is not successful, the audio driver
	 * will be overwritten with the NullDriver and this one is connected
	 * instead.
	 *
	 * Finally, H2Core::audioEngine_renameJackPorts() (if #H2CORE_HAVE_JACK is set)
	 * and H2Core::audioEngine_setupLadspaFX() are called.
	 *
	 * The state of the AudioEngine #H2Core::m_audioEngineState must not be in
	 * #STATE_INITIALIZED or the function will just unlock both mutex and
	 * returns.
	 */
	void				audioEngine_startAudioDrivers();
	/**
	 * Stops all audio and MIDI drivers.
	 *
	 * Uses H2Core::audioEngine_stop() if the AudioEngine is still in state
	 * #H2Core::m_audioEngineState #STATE_PLAYING, sets its state to
	 * #STATE_INITIALIZED, locks the AudioEngine using
	 * AudioEngine::lock(), deletes #H2Core::m_pMidiDriver and #H2Core::m_pAudioDriver and
	 * reinitializes them to NULL. 
	 *
	 * If #H2Core::m_audioEngineState is neither in #STATE_PREPARED or
	 * #STATE_READY, the function returns before deleting anything.
	 */
	void				audioEngine_stopAudioDrivers();

	timeval currentTime2();
	int randomValue( int max );
	float getGaussian( float z );
	void updateTickSize();
	void audioEngine_setupLadspaFX( unsigned nBufferSize );
	void audioEngine_renameJackPorts( Song *pSong);
	void audioEngine_raiseError( unsigned nErrorCode );
	AudioOutput* createDriver( const QString& sDriver );
	void audioEngine_process_clearAudioBuffers( uint32_t nFrames );
	float getMasterPeak_R();
	float getMasterPeak_L();
	void setMasterPeak_R( float value );
	void setMasterPeak_L( float value );
	/** Returns the fallback speed.
	 * \return #m_fNewBpmJTM */
	float			getNewBpmJTM();
	/** Set the fallback speed #m_fNewBpmJTM.
	 * \param bpmJTM New default tempo. */ 
	void			setNewBpmJTM( float bpmJTM);
	/**
	 * Pointer to the current instance of the audio driver.
	 *
	 * Initialized with NULL inside audioEngine_init(). Inside
	 * audioEngine_startAudioDrivers() either the audio driver specified
	 * in Preferences::m_sAudioDriver and created via createDriver() or
	 * the NullDriver, in case the former failed, will be assigned.
	 */	
	AudioOutput *			m_pAudioDriver;

	MidiInput* getMidiInput()
	MidiOutput* getMidiOutput();
	// TODO: more descriptive name since it is able to both delete and
	// add a pattern. Possibly without the sequencer_ prefix for
	// consistency.
	/**
	 * Adding and removing a Pattern from #m_pNextPatterns.
	 *
	 * After locking the AudioEngine the function retrieves the
	 * particular pattern @a pos from the Song::__pattern_list and
	 * either deletes it from #m_pNextPatterns if already present or
	 * add it to the same pattern list if not present yet.
	 *
	 * If the Song is not in Song::PATTERN_MODE or @a pos is not
	 * within the range of Song::__pattern_list, #m_pNextPatterns will
	 * be cleared instead.
	 *
	 * \param pos Index of a particular pattern in
	 * Song::__pattern_list, which should be added to
	 * #m_pNextPatterns.
	 */
	void			sequencer_setNextPattern( int pos );
	// TODO: Possibly without the sequencer_ prefix for consistency.
	/**
	 * Clear #m_pNextPatterns and add one Pattern.
	 *
	 * After locking the AudioEngine the function clears
	 * #m_pNextPatterns, fills it with all currently played one in
	 * #m_pPlayingPatterns, and appends the particular pattern @a pos
	 * from the Song::__pattern_list.
	 *
	 * If the Song is not in Song::PATTERN_MODE or @a pos is not
	 * within the range of Song::__pattern_list, #m_pNextPatterns will
	 * be just cleared.
	 *
	 * \param pos Index of a particular pattern in
	 * Song::__pattern_list, which should be added to
	 * #m_pNextPatterns.
	 */
	void			sequencer_setOnlyNextPattern( int pos );
	/** \return #m_pPlayingPatterns*/
	PatternList*		getCurrentPatternList();
	void		setCurrentPatternList( PatternList* pPatternList );
	/**
	 * Switches playback to focused pattern.
	 *
	 * If the current Song is in Song::PATTERN_MODE, the AudioEngine
	 * will be locked and Preferences::m_bPatternModePlaysSelected
	 * negated. If the latter was true before calling this function,
	 * #m_pPlayingPatterns will be cleared and replaced by the
	 * Pattern indexed with #m_nSelectedPatternNumber.
	 *
	 * This function will be called either by MainForm::eventFilter()
	 * when pressing Qt::Key_L or by
	 * SongEditorPanel::modeActionBtnPressed().
	 */
	void			togglePlaysSelected();
	/** Get the position of the current Pattern in the Song.
	 * \return #m_nSongPos */
	int			getPatternPos();
	void		setPatternPos( int nSongPos );
	/** 
	 * Same as getSelectedPatternNumber() without pushing an event.
	 * \param nPat Sets #m_nSelectedPatternNumber*/
	void			setSelectedPatternNumberWithoutGuiEvent( int nPat );
	int				getSelectedPatternNumber();
	/**
	 * Sets #m_nSelectedPatternNumber.
	 *
	 * If Preferences::m_pPatternModePlaysSelected is set to true, the
	 * AudioEngine is locked before @a nPat will be assigned. But in
	 * any case the function will push the
	 * #EVENT_SELECTED_PATTERN_CHANGED Event to the EventQueue.
	 *
	 * If @a nPat is equal to #m_nSelectedPatternNumber, the function
	 * will return right away.
	 *
	 *\param nPat Sets #m_nSelectedPatternNumber*/
	void			setSelectedPatternNumber( int nPat );

	int				getSelectedInstrumentNumber();
	void			setSelectedInstrumentNumber( int nInstrument );
	void setMainBuffer_L( float* pMainBuffer_L );
	void setMainBuffer_R( float* pMainBuffer_R );

		/** Returns the current state of the audio engine.
		 * \return #m_audioEngineState*/
	int getState();
	void setState( int nState );
	void			getLadspaFXPeak( int nFX, float *fL, float *fR );
	void			setLadspaFXPeak( int nFX, float fL, float fR );
	/** Move playback in Pattern mode to the beginning of the pattern.
	 *
	 * Resetting the global variable #m_nPatternStartTick to -1 if the
	 * current Song mode is Song::PATTERN_MODE.
	 */
	void			resetPatternStartTick();
	
	/** \return #m_nPatternTickPosition */
	unsigned long		getTickPosition();
	void setTickPosition( unsigned long nPatternTickPosition );
private:
	// info
	float				m_fMasterPeak_L;		///< Master peak (left channel)
	float				m_fMasterPeak_R;		///< Master peak (right channel)
	float				m_fProcessTime;		///< time used in process function
	float				m_fMaxProcessTime;	///< max ms usable in process with no xrun
	//~ info

	/**
	 * Fallback speed in beats per minute.
	 *
	 * It is set by Hydrogen::setNewBpmJTM() and accessed via
	 * Hydrogen::getNewBpmJTM().
	 */
	float				m_fNewBpmJTM;

	/**
	 * Mutex for locking the pointer to the audio output buffer, allowing
	 * multiple readers.
	 *
	 * When locking this __and__ the AudioEngine, always lock the
	 * AudioEngine first using AudioEngine::lock() or
	 * AudioEngine::tryLock(). Always use a QMutexLocker to lock this
	 * mutex.
	 */
	QMutex				mutex_OutputPointer;
	/**
	 * MIDI input
	 *
	 * In audioEngine_startAudioDrivers() it is assigned the midi driver
	 * specified in Preferences::m_sMidiDriver.
	 */
	MidiInput *			m_pMidiDriver;
	/**
	 * MIDI output
	 *
	 * In audioEngine_startAudioDrivers() it is assigned the midi driver
	 * specified in Preferences::m_sMidiDriver.
	 */
	MidiOutput *			m_pMidiDriverOut;

	// overload the > operator of Note objects for priority_queue
	struct compare_pNotes {
		bool operator() (Note* pNote1, Note* pNote2) {
			return (pNote1->get_humanize_delay()
					+ pNote1->get_position() * m_pAudioDriver->m_transport.m_fTickSize)
			    >
			    (pNote2->get_humanize_delay()
				 + pNote2->get_position() * m_pAudioDriver->m_transport.m_fTickSize);
		}
	};

	/// Song Note FIFO
	std::priority_queue<Note*, std::deque<Note*>, compare_pNotes > m_songNoteQueue;
	std::deque<Note*>		m_midiNoteQueue;	///< Midi Note FIFO

	/**
	 * Patterns to be played next in Song::PATTERN_MODE.
	 *
	 * In audioEngine_updateNoteQueue() whenever the end of the current
	 * pattern is reached the content of #m_pNextPatterns will be added to
	 * #m_pPlayingPatterns.
	 *
	 * Queried with Hydrogen::getNextPatterns(), set by
	 * Hydrogen::sequencer_setNextPattern() and
	 * Hydrogen::sequencer_setOnlyNextPattern(), initialized with an empty
	 * PatternList in audioEngine_init(), destroyed and set to NULL in
	 * audioEngine_destroy(), cleared in audioEngine_remove_Song(), and
	 * updated in audioEngine_updateNoteQueue(). Please note that ALL of
	 * these functions do access the variable directly!
	 */
	PatternList*		m_pNextPatterns;
	bool				m_bAppendNextPattern;		///< Add the next pattern to the list instead of replace.
	bool				m_bDeleteNextPattern;		///< Delete the next pattern from the list.
	/**
	 * PatternList containing all Patterns currently played back.
	 *
	 * Queried using Hydrogen::getCurrentPatternList(), set using
	 * Hydrogen::setCurrentPatternList(), initialized with an empty
	 * PatternList in audioEngine_init(), destroyed and set to NULL in
	 * audioEngine_destroy(), set to the first pattern list of the new
	 * song in audioEngine_setSong(), cleared in
	 * audioEngine_removeSong(), reset in Hydrogen::togglePlaysSelected()
	 * and processed in audioEngine_updateNoteQueue(). Please note that
	 * ALL of these functions do access the variable directly!
	 */
	PatternList*			m_pPlayingPatterns;
	/**
	 * Index of the current PatternList in the
	 * Song::__pattern_group_sequence.
	 *
	 * A value of -1 corresponds to "pattern list could not be found".
	 *
	 * Assigned using findPatternInTick() in
	 * audioEngine_updateNoteQueue(), queried using
	 * Hydrogen::getPatternPos() and set using Hydrogen::setPatternPos()
	 * if it AudioEngine is playing.
	 *
	 * It is initialized with -1 value in audioEngine_init(), and reset to
	 * the same value in audioEngine_start(), and
	 * Hydrogen::stopExportSong(). In Hydrogen::startExportSong() it will
	 * be set to 0. Please note that ALL of these functions do access the
	 * variable directly!
	 */
	int				m_nSongPos; // TODO: rename it to something more
	// accurate, like m_nPatternListNumber

	/**
	 * Index of the pattern selected in the GUI or by a MIDI event.
	 *
	 * If Preferences::m_bPatternModePlaysSelected is set to true and the
	 * playback is in Song::PATTERN_MODE, the corresponding pattern will
	 * be assigned to #m_pPlayingPatterns in
	 * audioEngine_updateNoteQueue(). This way the user can specify to
	 * play back the pattern she is currently viewing/editing.
	 *
	 * Queried using Hydrogen::getSelectedPatternNumber() and set by
	 * Hydrogen::setSelectedPatternNumber().
	 *
	 * Initialized to 0 in audioEngine_init().
	 */
	int				m_nSelectedPatternNumber;
	/**
	 * Instrument currently focused/selected in the GUI. 
	 *
	 * Within the core it is relevant for the MIDI input. Using
	 * Preferences::__playselectedinstrument incoming MIDI signals can be
	 * used to play back only the selected instrument or the whole
	 * drumkit.
	 *
	 * Queried using Hydrogen::getSelectedInstrumentNumber() and set by
	 * Hydrogen::setSelectedInstrumentNumber().
	 */
	int				m_nSelectedInstrumentNumber;
	/**
	 * Pointer to the metronome.
	 *
	 * Initialized in audioEngine_init().
	 */
	Instrument *			m_pMetronomeInstrument;

	// Buffers used in the process function
	unsigned			m_nBufferSize;
	/**
	 * Pointer to the audio buffer of the left stereo output returned by
	 * AudioOutput::getOut_L().
	 *
	 * Initialized to NULL in audioEngine_init(), assigned in
	 * audioEngine_startAudioDrivers(), reset in
	 * audioEngine_process_clearAudioBuffers(), and populated with the
	 * actual audio in audioEngine_process().
	 */
	float *				m_pMainBuffer_L;
	/**
	 * Pointer to the audio buffer of the right stereo output returned by
	 * AudioOutput::getOut_R().
	 *
	 * Initialized to NULL in audioEngine_init(), assigned in
	 * audioEngine_startAudioDrivers(), reset in
	 * audioEngine_process_clearAudioBuffers(), and populated with the
	 * actual audio in audioEngine_process().
	 */
	float *				m_pMainBuffer_R;
	/**
	 * Current state of the H2Core::AudioEngine. 
	 *
	 * It is supposed to take five different states:
	 *
	 * - #STATE_UNINITIALIZED:	1      Not even the constructors have been called.
	 * - #STATE_INITIALIZED:	2      Not ready, but most pointers are now valid or NULL
	 * - #STATE_PREPARED:		3      Drivers are set up, but not ready to process audio.
	 * - #STATE_READY:		4      Ready to process audio
	 * - #STATE_PLAYING:		5      Currently playing a sequence.
	 * 
	 * It gets initialized with #STATE_UNINITIALIZED.
	 */	
	int				m_audioEngineState;	

#if defined(H2CORE_HAVE_LADSPA) || _DOXYGEN_
	float				m_fFXPeak_L[MAX_FX];
	float				m_fFXPeak_R[MAX_FX];
#endif

	/**
	 * Beginning of the current pattern in ticks.
	 *
	 * It is set using finPatternInTick() and reset to -1 in
	 * audioEngine_start(), audioEngine_stop(),
	 * Hydrogen::startExportSong(), and
	 * Hydrogen::triggerRelocateDuringPlay() (if the playback it in
	 * Song::PATTERN_MODE).
	 */
	int				m_nPatternStartTick;
	/**
	 * Ticks passed since the beginning of the current pattern.
	 *
	 * Queried using Hydrogen::getTickPosition().
	 *
	 * Initialized to 0 in audioEngine_init() and reset to 0 in
	 * Hydrogen::setPatternPos(), if the AudioEngine is not playing, in
	 * audioEngine_start(), Hydrogen::startExportSong() and
	 * Hydrogen::stopExportSong(), which marks the beginning of a Song.
	 */
	unsigned int	m_nPatternTickPosition;

	/** Set to the total number of ticks in a Song in findPatternInTick()
		if Song::SONG_MODE is chosen and playback is at least in the
		second loop.*/
	int				m_nSongSizeInTicks;

	/** Updated in audioEngine_updateNoteQueue().*/
	struct timeval			m_currentTickTime;

	/**
	 * Variable keeping track of the transport position in realtime.
	 *
	 * Even if the audio engine is stopped, the variable will be
	 * incremented by #m_nBufferSize (as audioEngine_process() would do at
	 * the end of each cycle) to support realtime keyboard and MIDI event
	 * timing. It is set using Hydrogen::setRealtimeFrames(), accessed via
	 * Hydrogen::getRealtimeFrames(), and updated in
	 * audioEngine_process_transport() using the current transport
	 * position TransportInfo::m_nFrames.
	 */
	unsigned long			m_nRealtimeFrames;
	unsigned int			m_naddrealtimenotetickposition;
	
	/** Local instance of the Sampler. */
	std::shared_ptr<Sampler> m_pSampler;
	/** Local instance of the Synth. */
	std::shared_ptr<Synth> m_pSynth;

	/** Mutex for synchronizing the access to the Song object and
	    the AudioEngine. 
	  * 
	  * It can be used lock the access using either lock() or
	  * tryLock() and to unlock it via unlock(). It is
	  * initialized in AudioEngine() and not explicitly exited.
	  */
	std::timed_mutex m_engineMutex;

	/**
	 * This struct is most probably intended to be used for
	 * logging the locking of the AudioEngine. But neither it nor
	 * the Logger::AELockTracing state is ever used.
	 */
	struct _locker_struct {
		const char* file;
		unsigned int line;
		const char* function;
	} m_locker; ///< This struct is most probably intended to be
		    ///< used for logging the locking of the
		    ///< AudioEngine. But neither it nor the
		    ///< Logger::AELockTracing state is ever used.
};


inline float AudioEngine::getMasterPeak_L()
{
	return m_fMasterPeak_L;
}

inline float AudioEngine::getMasterPeak_R()
{
	return m_fMasterPeak_R;
}
inline void AudioEngine::setMasterPeak_L( float fValue )
{
	m_fMasterPeak_L = fValue;
}
inline void AudioEngine::setMasterPeak_R( float fValue )
{
	m_fMasterPeak_R = fValue;
}
inline float AudioEngine::getProcessTime()
{
	return m_fProcessTime;
}

inline float AudioEngine::getMaxProcessTime()
{
	return m_fMaxProcessTime;
}
inline float AudioEngine::getNewBpmJTM()
{
	return m_fNewBpmJTM;
}

inline void AudioEngine::setNewBpmJTM( float bpmJTM )
{
	m_fNewBpmJTM = bpmJTM;
}
inline MidiInput* AudioEngine::getMidiInput()
{
	return m_pMidiDriver;
}
inline MidiOutput* AudioEngine::getMidiOutput()
{
	return m_pMidiDriverOut;
}
inline PatternList* AudioEngine::getNextPatterns()
{
	return m_pNextPatterns;
}
inline PatternList* AudioEngine::getCurentPatternList()
{
	return m_pPlayingPatterns;
}
inline int AudioEngine::getPatternPos()
{
	return m_nSongPos;
}
inline void AudioEngine::setPatternPos( int nSongPos )
{
	m_nSongPos = nSongPos;
}
inline int AudioEngine::getSelectedInstrumentNumber()
{
	return m_nSelectedInstrumentNumber;
}
inline int AudioEngine::getSelectedPatternNumber()
{
	return m_nSelectedPatternNumber;
}
inline void AudioEngine::setMainBuffer_R( float* pMainBuffer_R )
{
	m_pMainBuffer_R = pMainBuffer_R;
}
inline void AudioEngine::setMainBuffer_L( float* pMainBuffer_L )
{
	m_pMainBuffer_L = pMainBuffer_L;
}
inline int AudioEngine::getState()
{
	return m_audioEngineState;
}
inline void AudioEngine::setState( int nState )
{
	m_audioEngineState = nState;
}
inline void AudioEngine::getLadspaFXPeak( int nFX, float *fL, float *fR )
{
#ifdef H2CORE_HAVE_LADSPA
	( *fL ) = m_fFXPeak_L[nFX];
	( *fR ) = m_fFXPeak_R[nFX];
#else
	( *fL ) = 0;
	( *fR ) = 0;
#endif
}

inline void AudioEngine::setLadspaFXPeak( int nFX, float fL, float fR )
{
#ifdef H2CORE_HAVE_LADSPA
	m_fFXPeak_L[nFX] = fL;
	m_fFXPeak_R[nFX] = fR;
#endif
}

inline unsigned long AudioEngine::getTickPosition()
{
	return m_nPatternTickPosition;
}
inline void AudioEngine::setTickPosition( unsigned long nPatternTickPosition )
{
	m_nPatternTickPosition = nPatternTickPosition;
}
};


#endif
