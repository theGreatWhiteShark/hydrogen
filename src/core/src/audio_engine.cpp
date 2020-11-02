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

#include <hydrogen/audio_engine.h>

#include <hydrogen/fx/Effects.h>
#include <hydrogen/basics/song.h>
#include <hydrogen/IO/AudioOutput.h>
#include <hydrogen/sampler/Sampler.h>

#include <hydrogen/hydrogen.h>	// TODO: remove this line as soon as possible
#include <cassert>

namespace H2Core
{


AudioEngine* AudioEngine::__instance = nullptr;
const char* AudioEngine::__class_name = "AudioEngine";


void AudioEngine::create_instance()
{
	if( __instance == nullptr ) {
		__instance = new AudioEngine;
	}
}

AudioEngine::AudioEngine()
		: Object( __class_name )
		, m_pSampler( nullptr )
		, m_pSynth( nullptr )
{
	__instance = this;
	INFOLOG( "INIT" );

	m_pSampler = new Sampler;
	m_pSynth = new Synth;

#ifdef H2CORE_HAVE_LADSPA
	Effects::create_instance();
#endif

}



AudioEngine::~AudioEngine()
{
	INFOLOG( "DESTROY" );
#ifdef H2CORE_HAVE_LADSPA
	delete Effects::get_instance();
#endif

//	delete Sequencer::get_instance();
	delete m_pSampler;
	delete m_pSynth;
}



Sampler* AudioEngine::getSampler()
{
	assert(m_pSampler);
	return m_pSampler;
}




Synth* AudioEngine::getSynth()
{
	assert(m_pSynth);
	return m_pSynth;
}

void AudioEngine::lock( const char* file, unsigned int line, const char* function )
{
	m_engineMutex.lock();
	m_locker.file = file;
	m_locker.line = line;
	m_locker.function = function;
}



bool AudioEngine::tryLock( const char* file, unsigned int line, const char* function )
{
	bool res = m_engineMutex.try_lock();
	if ( !res ) {
		// Lock not obtained
		return false;
	}
	m_locker.file = file;
	m_locker.line = line;
	m_locker.function = function;
	return true;
}

bool AudioEngine::tryLockFor( std::chrono::microseconds duration, const char* file, unsigned int line, const char* function )
{
	bool res = m_engineMutex.try_lock_for( duration );
	if ( !res ) {
		// Lock not obtained
		WARNINGLOG( QString( "Lock timeout: lock timeout %1:%2%3, lock held by %4:%5:%6" )
					.arg( file ).arg( function ).arg( line )
					.arg( m_locker.file ).arg( m_locker.function ).arg( m_locker.line ));
		return false;
	}
	m_locker.file = file;
	m_locker.line = line;
	m_locker.function = function;
	return true;
}

float AudioEngine::computeTickSize(int nSampleRate, float fBpm, int nResolution)
{
	float fTickSize = nSampleRate * 60.0 / fBpm / nResolution;
	
	return fTickSize;
}


void AudioEngine::unlock()
{
	// Leave "m_locker" dirty.
	m_engineMutex.unlock();
}


}; // namespace H2Core
