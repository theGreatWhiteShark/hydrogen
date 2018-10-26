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

#include <hydrogen/Preferences.h>
#include <hydrogen/hydrogen.h>
#include <hydrogen/basics/instrument.h>
#include <hydrogen/basics/instrument_list.h>
#include <hydrogen/basics/pattern.h>
#include <hydrogen/basics/pattern_list.h>
using namespace H2Core;

#include <cassert>
#include <chrono>

#include "../HydrogenApp.h"

#include "UndoActions.h"
#include "NotePropertiesRuler.h"
#include "PatternEditorPanel.h"
#include "DrumPatternEditor.h"
#include "PianoRollEditor.h"

const char* NotePropertiesRuler::__class_name = "NotePropertiesRuler";

NotePropertiesRuler::NotePropertiesRuler( QWidget *parent, PatternEditorPanel *pPatternEditorPanel, NotePropertiesMode mode )
 : QWidget( parent )
 , Object( __class_name )
 , m_Mode( mode )
 , m_pPatternEditorPanel( pPatternEditorPanel )
 , m_pPattern( NULL )
{
	//setAttribute(Qt::WA_NoBackground);

	m_nGridWidth = (Preferences::get_instance())->getPatternEditorGridWidth();
	m_nEditorWidth = 20 + m_nGridWidth * ( MAX_NOTES * 4 );

	if (m_Mode == VELOCITY ) {
		m_nEditorHeight = 100;
	}
	else if ( m_Mode == PAN ) {
		m_nEditorHeight = 100;
	}
	else if ( m_Mode == LEADLAG ) {
		m_nEditorHeight = 100;
	}
	else if ( m_Mode == NOTEKEY ) {
		m_nEditorHeight = 210;
	}
	if (m_Mode == PROBABILITY ) {
		m_nEditorHeight = 100;
	}

	resize( m_nEditorWidth, m_nEditorHeight );
	setMinimumSize( m_nEditorWidth, m_nEditorHeight );

	m_pBackground = new QPixmap( m_nEditorWidth, m_nEditorHeight );

	updateEditor();
	show();

	HydrogenApp::get_instance()->addEventListener( this );
	m_bMouseIsPressed = false;
}




NotePropertiesRuler::~NotePropertiesRuler()
{
	//infoLog("DESTROY");
}


void NotePropertiesRuler::wheelEvent(QWheelEvent *ev )
{

	if (m_pPattern == NULL) return;

	chrono::high_resolution_clock::time_point t1_wheel = chrono::high_resolution_clock::now();
	
	pressAction( ev->x(), ev->y() ); //get all old values

	float delta;
	// Using '&' instead of '==' the finer control will be used
	// regardless of how many other modifier keys are pressed
	// along with the Control key.
	if (ev->modifiers() & Qt::ControlModifier) {
		delta = 0.01; // fine control
	} else {
		delta = 0.05; // course control
	}
		
	if ( ev->delta() < 0 ) {
		delta = (delta * -1.0);
	}

	DrumPatternEditor *pPatternEditor = m_pPatternEditorPanel->getDrumPatternEditor();
	int nBase;
	if (pPatternEditor->isUsingTriplets()) {
		nBase = 3;
	}
	else {
		nBase = 4;
	}
	// The note, which was chosen by the position of the cursor,
	// will be accessed via its x coordinate along the
	// one-dimensional pattern editor.
	int width = (m_nGridWidth * 4 *  MAX_NOTES) / ( nBase * pPatternEditor->getResolution());
	int x_pos = ev->x();
	int column;
	column = (x_pos - 20) + (width / 2);
	column = column / width;
	column = (column * 4 * MAX_NOTES) / ( nBase * pPatternEditor->getResolution() );

	int nSelectedInstrument = Hydrogen::get_instance()->getSelectedInstrumentNumber();
	Song *pSong = (Hydrogen::get_instance())->getSong();

	// Create a list, which will contain the changes applied to a
	// single notes. All changes will be inserted into the
	// `QUndoStack' at once when handed over to the
	// `pushUndoAction' function. This way they can all be
	// reverted upon pressed Ctrl-Z just once.
	std::list<NotePropertiesChanges> propertyChangesStack;
	
	const Pattern::notes_t* notes = m_pPattern->get_notes();

	// Whenever the Shift key is pressed, apply the current action
	// to all notes instead to just the one positioned right below
	// the cursor.
	if ( ev->modifiers() & Qt::ShiftModifier ){
	       
		FOREACH_NOTE_CST_IT_BEGIN_END(notes,it) {
			Note *pNote = it->second;
			assert( pNote );
			if ( pNote->get_instrument() != pSong->get_instrument_list()->get( nSelectedInstrument ) ) {
				continue;
			}
			// Create a H2Core::NoteProperties struct,
			// which will contain all relevant properties
			// for the following lines of code.
			noteProperties = pNote->get_note_properties();
			noteProperties.pattern_idx = __nSelectedPatternNumber;
			if ( m_Mode == VELOCITY && !pNote->get_note_off() ) {
				float val = noteProperties.velocity + delta;
				if (val > 1.0) {
					val = 1.0;
				}
				else if (val < 0.0) {
					val = 0.0;
				}

				pNote->set_velocity(val);

				char valueChar[100];
				sprintf( valueChar, "%#.2f",  val);
				( HydrogenApp::get_instance() )->setStatusBarMessage( QString("[%1] Set all note velocities").arg( valueChar ), 2000 );
			}
			else if ( m_Mode == PAN && !pNote->get_note_off() ){

				float pan_delta, old_pan_r, old_pan_l, pan_r, pan_l;

				// Access the current state of the
				// panning.
				old_pan_r = noteProperties.pan_r;
				old_pan_l = noteProperties.pan_l;
				
				// A positive delta corresponds to a
				// panning to the right and negative
				// ones to the left.
				// In mid position the panning volumes
				// of both sides are set to 0.5. As
				// the panning towards the right
				// proceeds, the value of pan_r stays
				// at 0.5 and the one of pan_l is
				// successively lowered.
				if ( delta > 0.0 ){
					// If already panned to the
					// left, move to the center first.
					if ( old_pan_r < 0.5 ){
						if ( ( old_pan_r + delta ) > 0.5 ){
							pan_r = 0.5;
							pan_delta = delta - 0.5 + old_pan_r;
						} else {
							pan_r = old_pan_r + delta;
							pan_delta = 0.0;
						}
					} else {
						pan_r = old_pan_r;
						pan_delta = delta;
					}
					// Panning to the right.
					if ( ( old_pan_l - pan_delta ) < 0.0 ){
						pan_l = 0.0;
					} else {
					        pan_l = old_pan_l - pan_delta;
					}
				} else if ( delta < 0.0 ) {
					// If already panned to the
					// right, move to the center first.
					if ( old_pan_l < 0.5 ){
						if ( ( old_pan_l - delta ) > 0.5 ){
							pan_l = 0.5;
							pan_delta = old_pan_l - delta - 0.5;
						} else {
							pan_l = old_pan_l - delta;
							pan_delta = 0.0;
						}
					} else {
						pan_l = old_pan_l;
						pan_delta = -delta;
					}
					// Panning to the left.
					// Note that `delta' will be
					// negative in the case of
					// left panning but
					// `pan_delta` is always positive.
					if ( ( old_pan_r - pan_delta ) < 0.0 ){
						pan_r = 0.0;
					} else {
						pan_r = old_pan_r - pan_delta;
					}
				} else {
					// delta equals zero. This
					// cause should not happen.
					pan_r = old_pan_r;
					pan_l = old_pan_l;
				}

				pNote->set_pan_l( pan_l );
				pNote->set_pan_r( pan_r );

				char valueChar[100];
				float val = pan_r - pan_l + 0.5;
				sprintf( valueChar, "%#.2f",  val);
				( HydrogenApp::get_instance() )->setStatusBarMessage( QString("[%1] Set all note pannings").arg( valueChar ), 2000 );
			}
			else if ( m_Mode == LEADLAG ){
				float leadLag;
				float val = ( noteProperties.leadLag - 1.0)/-2.0 + delta;
				if (val > 1.0) {
					val = 1.0;
				}
				else if (val < 0.0) {
					val = 0.0;
				}
				leadLag = val * -2.0 + 1.0;
				pNote->set_lead_lag( leadLag );
				char valueChar[100];
				if ( leadLag < 0.0 ) {
					sprintf( valueChar, "%.2f",  ( leadLag * -5 )); // FIXME: '5' taken from fLeadLagFactor calculation in hydrogen.cpp
					HydrogenApp::get_instance()->setStatusBarMessage( QString("Leading beat by: %1 ticks").arg( valueChar ), 2000 );
				} else if ( leadLag > 0.0 ) {
					sprintf( valueChar, "%.2f",  ( leadLag * 5 )); // FIXME: '5' taken from fLeadLagFactor calculation in hydrogen.cpp
					HydrogenApp::get_instance()->setStatusBarMessage( QString("Lagging beat by: %1 ticks").arg( valueChar ), 2000 );
				} else {
					HydrogenApp::get_instance()->setStatusBarMessage( QString("Note on beat"), 2000 );
				}
			}
			else if ( m_Mode == PROBABILITY && !pNote->get_note_off() ) {
				float val = noteProperties.probability + delta;
				if (val > 1.0) {
					val = 1.0;
				}
				else if (val < 0.0) {
					val = 0.0;
				}

				pNote->set_probability(val);

				char valueChar[100];
				sprintf( valueChar, "%#.2f",  val);
				( HydrogenApp::get_instance() )->setStatusBarMessage( QString("[%1] Set all note probabilities").arg( valueChar ), 2000 );

			}
			// The pattern id is not properly set by the
			// pattern editor and has to be adjusted
			// manually.
			notePropertiesNew = pNote->get_note_properties();
			notePropertiesNew.pattern_idx = __nSelectedPatternNumber;
			// Create a H2Core::NotePropertiesChanges
			// struct specifying what did change during
			// the last action.
			notePropertiesChanges =
				{ m_Mode, noteProperties, notePropertiesNew };
			// Push the changes onto the list keeping
			// track of all changes during the current
			// action.
			propertyChangesStack.push_front( notePropertiesChanges );
		}
		// Push the changes onto the `QUndoStack'.
		pushUndoAction( propertyChangesStack );
		pSong->set_is_modified( true );
		updateEditor();
		
	} else {
		// Only the properties of the note below the cursor
		// are altered.
		FOREACH_NOTE_CST_IT_BOUND(notes,it,column) {
			Note *pNote = it->second;
			assert( pNote );
			assert( (int)pNote->get_position() == column );
			if ( pNote->get_instrument() != pSong->get_instrument_list()->get( nSelectedInstrument ) ) {
				continue;
			}
			// Create a H2Core::NoteProperties struct,
			// which will contain all relevant properties
			// for the following lines of code.
			noteProperties = pNote->get_note_properties();
			noteProperties.pattern_idx = __nSelectedPatternNumber;
			if ( m_Mode == VELOCITY && !pNote->get_note_off() ) {
				float val = pNote->get_velocity() + delta;
				if (val > 1.0) {
					val = 1.0;
				}
				else if (val < 0.0) {
					val = 0.0;
				}

				pNote->set_velocity(val);

				char valueChar[100];
				sprintf( valueChar, "%#.2f",  val);
				( HydrogenApp::get_instance() )->setStatusBarMessage( QString("[%1] Set note velocity").arg( valueChar ), 2000 );
			}
			else if ( m_Mode == PAN && !pNote->get_note_off() ){

				float pan_delta, old_pan_r, old_pan_l, pan_r, pan_l;

				// Access the current state of the
				// panning.
				old_pan_r = noteProperties.pan_r;
				old_pan_l = noteProperties.pan_l;
				
				// A positive delta corresponds to a
				// panning to the right and negative
				// ones to the left.
				// In mid position the panning volumes
				// of both sides are set to 0.5. As
				// the panning towards the right
				// proceeds, the value of pan_r stays
				// at 0.5 and the one of pan_l is
				// successively lowered.
				if ( delta > 0.0 ){
					// If already panned to the
					// left, move to the center first.
					if ( old_pan_r < 0.5 ){
						if ( ( old_pan_r + delta ) > 0.5 ){
							pan_r = 0.5;
							pan_delta = delta - 0.5 + old_pan_r;
						} else {
							pan_r = old_pan_r + delta;
							pan_delta = 0.0;
						}
					} else {
						pan_r = old_pan_r;
						pan_delta = delta;
					}
					// Panning to the right.
					if ( ( old_pan_l - pan_delta ) < 0.0 ){
						pan_l = 0.0;
					} else {
						pan_l = old_pan_l - pan_delta;
					}
				} else if ( delta < 0.0 ) {
					// If already panned to the
					// right, move to the center first.
					if ( old_pan_l < 0.5 ){
						if ( ( old_pan_l - delta ) > 0.5 ){
							pan_l = 0.5;
							pan_delta = old_pan_l - delta - 0.5;
						} else {
							pan_l = old_pan_l - delta;
							pan_delta = 0.0;
						}
					} else {
						pan_l = old_pan_l;
						pan_delta = -delta;
					}
					// Panning to the left.
					// Note that `delta' will be
					// negative in the case of
					// left panning but
					// `pan_delta` is always positive.
					if ( ( old_pan_r - pan_delta ) < 0.0 ){
						pan_r = 0.0;
					} else {
						pan_r = old_pan_r - pan_delta;
					}
				} else {
					// delta equals zero. This
					// cause should not happen.
					pan_r = old_pan_r;
					pan_l = old_pan_l;
				}

				pNote->set_pan_l( pan_l );
				pNote->set_pan_r( pan_r );

				char valueChar[100];
				float val = pan_r - pan_l + 0.5;
				sprintf( valueChar, "%#.2f",  val);
				( HydrogenApp::get_instance() )->setStatusBarMessage( QString("[%1] Set note panning").arg( valueChar ), 2000 );
			}
			else if ( m_Mode == LEADLAG ){
				float leadLag;
				float val = ( noteProperties.leadLag - 1.0 )/-2.0 + delta;
				if (val > 1.0) {
					val = 1.0;
				}
				else if (val < 0.0) {
					val = 0.0;
				}
				leadLag = val * -2.0 + 1.0;
				pNote->set_lead_lag( leadLag );
				char valueChar[100];
				if ( leadLag < 0.0) {
					sprintf( valueChar, "%.2f",  ( leadLag * -5 )); // FIXME: '5' taken from fLeadLagFactor calculation in hydrogen.cpp
					HydrogenApp::get_instance()->setStatusBarMessage( QString("Leading beat by: %1 ticks").arg( valueChar ), 2000 );
				} else if ( leadLag > 0.0) {
					sprintf( valueChar, "%.2f",  ( leadLag * 5 )); // FIXME: '5' taken from fLeadLagFactor calculation in hydrogen.cpp
					HydrogenApp::get_instance()->setStatusBarMessage( QString("Lagging beat by: %1 ticks").arg( valueChar ), 2000 );
				} else {
					HydrogenApp::get_instance()->setStatusBarMessage( QString("Note on beat"), 2000 );
				}
			}
			else if ( m_Mode == PROBABILITY && !pNote->get_note_off() ) {
				float val = noteProperties.probability + delta;
				if (val > 1.0) {
					val = 1.0;
				}
				else if (val < 0.0) {
					val = 0.0;
				}

				pNote->set_probability(val);

				char valueChar[100];
				sprintf( valueChar, "%#.2f",  val);
				( HydrogenApp::get_instance() )->setStatusBarMessage( QString("[%1] Set note probability").arg( valueChar ), 2000 );

			}
			// The pattern id is not properly set by the
			// pattern editor and has to be adjusted
			// manually.
			notePropertiesNew = pNote->get_note_properties();
			notePropertiesNew.pattern_idx = __nSelectedPatternNumber;
			// Create a H2Core::NotePropertiesChanges
			// struct specifying what did change during
			// the last action.
			notePropertiesChanges =
				{ m_Mode, noteProperties, notePropertiesNew };
			// Push the changes onto the list keeping
			// track of all changes during the current
			// action.
			propertyChangesStack.push_front( notePropertiesChanges );
			// Push the changes onto the `QUndoStack'.
			pushUndoAction( propertyChangesStack );
			
			pSong->set_is_modified( true );
			updateEditor();
			break;
		}
	}
	chrono::high_resolution_clock::time_point t2_wheel = chrono::high_resolution_clock::now();
	chrono::duration<double> tWheelSpan = chrono::duration_cast<chrono::duration<double>>(t2_wheel - t1_wheel);
	// INFOLOG( QString( "Duration mouse wheel [seconds]: %1" ).arg( tWheelSpan.count() ) );
}


void NotePropertiesRuler::mousePressEvent(QMouseEvent *ev)
{
	m_bMouseIsPressed = true;
	pressAction( ev->x(), ev->y() );
	mouseMoveEvent( ev );
}

void NotePropertiesRuler::pressAction( int x, int y)
{

	if (m_pPattern == NULL) return;

	DrumPatternEditor *pPatternEditor = m_pPatternEditorPanel->getDrumPatternEditor();
	int nBase;
	if (pPatternEditor->isUsingTriplets()) {
		nBase = 3;
	}
	else {
		nBase = 4;
	}
	int width = (m_nGridWidth * 4 *  MAX_NOTES) / ( nBase * pPatternEditor->getResolution());
	int x_pos = x;
	int column;
	column = (x_pos - 20) + (width / 2);
	column = column / width;
	column = (column * 4 * MAX_NOTES) / ( nBase * pPatternEditor->getResolution() );
	float val = height() - y;
	if (val > height()) {
		val = height();
	}
	else if (val < 0.0) {
		val = 0.0;
	}
	int keyval = val;
	val = val / height();

	int nSelectedInstrument = Hydrogen::get_instance()->getSelectedInstrumentNumber();
	Song *pSong = (Hydrogen::get_instance())->getSong();

	__undoColumn = 	column;

	const Pattern::notes_t* notes = m_pPattern->get_notes();
	FOREACH_NOTE_CST_IT_BOUND(notes,it,column) {
		Note *pNote = it->second;
		assert( pNote );
		assert( (int)pNote->get_position() == column );
		if ( pNote->get_instrument() != pSong->get_instrument_list()->get( nSelectedInstrument ) ) {
			continue;
		}
		// In order to be able to group all connected move
		// actions into one undo/redo action, we will conserve
		// the state of the note property the user is clicking
		// on in a global variable.
		notePropertiesOld = pNote->get_note_properties();
		notePropertiesOld.pattern_idx = __nSelectedPatternNumber;
	}
}

 void NotePropertiesRuler::mouseMoveEvent( QMouseEvent *ev )
{
	chrono::high_resolution_clock::time_point t1_press = chrono::high_resolution_clock::now();
	if( m_bMouseIsPressed ){

		if (m_pPattern == NULL) return;
	
		DrumPatternEditor *pPatternEditor = m_pPatternEditorPanel->getDrumPatternEditor();
		int nBase;
		if (pPatternEditor->isUsingTriplets()) {
			nBase = 3;
		}
		else {
			nBase = 4;
		}
		int width = (m_nGridWidth * 4 *  MAX_NOTES) / ( nBase * pPatternEditor->getResolution());
		int x_pos = ev->x();
		int column;
		column = (x_pos - 20) + (width / 2);
		column = column / width;
		column = (column * 4 * MAX_NOTES) / ( nBase * pPatternEditor->getResolution() );

		bool columnChange = false;
		if( __columnCheckOnXmouseMouve != column ){
			__undoColumn = column;
			columnChange = true;
		}

		float val = height() - ev->y();
		if (val > height()) {
			val = height();
		}
		else if (val < 0.0) {
			val = 0.0;
		}
		int keyval = val;
		val = val / height();

		int nSelectedInstrument = Hydrogen::get_instance()->getSelectedInstrumentNumber();
		Song *pSong = (Hydrogen::get_instance())->getSong();

		// Create a list, which will contain the changes
		// applied to a single notes. All changes will be
		// inserted into the `QUndoStack' at once when handed
		// over to the `pushUndoAction' function. This way
		// they can all be reverted upon pressed Ctrl-Z just
		// once.
		std::list<NotePropertiesChanges> propertyChangesStack;
		const Pattern::notes_t* notes = m_pPattern->get_notes();

		// Whenever the Shift key is pressed, apply the current action
		// to all notes instead to just the one positioned right below
		// the cursor.
		if ( ev->modifiers() & Qt::ShiftModifier ){
			FOREACH_NOTE_CST_IT_BEGIN_END(notes,it) {
				Note *pNote = it->second;
						
				assert( pNote );
				if ( pNote->get_instrument() != pSong->get_instrument_list()->get( nSelectedInstrument ) ) {
					continue;
				}
				// Create a H2Core::NoteProperties
				// struct, which will contain all
				// relevant properties for the
				// following lines of code.
				noteProperties = pNote->get_note_properties();
				noteProperties.pattern_idx = __nSelectedPatternNumber;
				
				if ( m_Mode == VELOCITY && !pNote->get_note_off() ) {
					pNote->set_velocity( val );
					char valueChar[100];
					sprintf( valueChar, "%#.2f",  val);
					HydrogenApp::get_instance()->setStatusBarMessage( QString("[%1] Set note velocity").arg( valueChar ), 2000 );
				}
				else if ( m_Mode == PAN && !pNote->get_note_off() ){
					float pan_l, pan_r;
					if ( (ev->button() == Qt::MidButton) || (ev->modifiers() == Qt::ControlModifier && ev->button() == Qt::LeftButton) ) {
						val = 0.5;
					}
					if ( val > 0.5 ) {
						pan_l = 1.0 - val;
						pan_r = 0.5;
					}
					else {
						pan_l = 0.5;
						pan_r = val;
					}

					pNote->set_pan_l( pan_l );
					pNote->set_pan_r( pan_r );

					char valueChar[100];
					float val = pan_r - pan_l + 0.5;
					sprintf( valueChar, "%#.2f",  val);
					( HydrogenApp::get_instance() )->setStatusBarMessage( QString("[%1] Set all note pannings").arg( valueChar ), 2000 );
				}
				else if ( m_Mode == LEADLAG ){
					float leadLag;
					if ( (ev->button() == Qt::MidButton) || (ev->modifiers() == Qt::ControlModifier && ev->button() == Qt::LeftButton) ) {
						pNote->set_lead_lag(0.0);
						leadLag = 0.0;
					} else {
						leadLag = val * -2.0 + 1.0;
						pNote->set_lead_lag( leadLag );
						char valueChar[100];
						if ( leadLag < 0.0 ) {
							sprintf( valueChar, "%.2f",  ( leadLag * -5)); // FIXME: '5' taken from fLeadLagFactor calculation in hydrogen.cpp
							HydrogenApp::get_instance()->setStatusBarMessage( QString("Leading beat by: %1 ticks").arg( valueChar ), 2000 );
						} else if ( leadLag > 0.0 ) {
							sprintf( valueChar, "%.2f",  ( leadLag * 5)); // FIXME: '5' taken from fLeadLagFactor calculation in hydrogen.cpp
							HydrogenApp::get_instance()->setStatusBarMessage( QString("Lagging beat by: %1 ticks").arg( valueChar ), 2000 );
						} else {
							HydrogenApp::get_instance()->setStatusBarMessage( QString("Note on beat"), 2000 );
						}
	
					}
				}
	
				else if ( m_Mode == NOTEKEY ){
					if ( (ev->button() == Qt::MidButton) || (ev->modifiers() == Qt::ControlModifier && ev->button() == Qt::LeftButton) ) {
						;
					} else {
						//set the note hight
						//QMessageBox::information ( this, "Hydrogen", trUtf8( "val: %1" ).arg(keyval)  );
						int k = 666;
						int o = 666;
						if(keyval >=6 && keyval<=125) {
							k = (keyval-6)/10;
						} else if(keyval>=135 && keyval<=205) {
							o = (keyval-166)/10;
							if(o==-4) o=-3; // 135
						}
						pNote->set_key_octave((Note::Key)k,(Note::Octave)o); // won't set wrong values see Note::set_key_octave
					}
				}
				else if ( m_Mode == PROBABILITY && !pNote->get_note_off() ) {
					pNote->set_probability( val );
					char valueChar[100];
					sprintf( valueChar, "%#.2f",  val);
					HydrogenApp::get_instance()->setStatusBarMessage( QString("[%1] Set note probability").arg( valueChar ), 2000 );
				}

	
				if( columnChange ){
					__columnCheckOnXmouseMouve = column;
					return;
				}
				// The pattern id is not properly set
				// by the pattern editor and has to be
				// adjusted manually.
				notePropertiesNew = pNote->get_note_properties();
				notePropertiesNew.pattern_idx = __nSelectedPatternNumber;
				// For now we have to do a dirty trick
				// to make the undo working. The
				// notePropertiesOld struct is
				// associated with the note the user
				// is hovering over and there is not a
				// global old state for all
				// notes. Therefore, we will just take
				// this one and adjust the position by
				// hand.
				notePropertiesOld.position = it->first;
				// Create a H2Core::NotePropertiesChanges
				// struct specifying what did
				// change during the last action.
				notePropertiesChanges =
					{ m_Mode, notePropertiesOld, notePropertiesNew };
				// Push the changes onto the list
				// keeping track of all changes during
				// the current action.
				propertyChangesStack.push_front( notePropertiesChanges );
				__columnCheckOnXmouseMouve = column;
			}
			// Push the changes onto the `QUndoStack'.
			pushUndoAction( propertyChangesStack );

			pSong->set_is_modified( true );
			updateEditor();
		} else {
			// Only the properties of the note below the cursor
			// are altered.
			FOREACH_NOTE_CST_IT_BOUND(notes,it,column) {
				Note *pNote = it->second;
				assert( pNote );
				assert( (int)pNote->get_position() == column );
				if ( pNote->get_instrument() != pSong->get_instrument_list()->get( nSelectedInstrument ) ) {
					continue;
				}
				// Create a H2Core::NoteProperties
				// struct, which will contain all
				// relevant properties for the
				// following lines of code.
				noteProperties = pNote->get_note_properties();
				noteProperties.pattern_idx = __nSelectedPatternNumber;
				
				// Keep track of a global, old state
				// of the properties and update it
				// only if the cursor did move over
				// another note. This way all actions
				// performed with the same note
				// pointed at will be grouped together
				// and reverted using undo/redo at the
				// same time.
				if ( columnChange ){
					notePropertiesOld = noteProperties;
				}
				if ( m_Mode == VELOCITY && !pNote->get_note_off() ) {
					pNote->set_velocity( val );
					char valueChar[100];
					sprintf( valueChar, "%#.2f",  val);
					HydrogenApp::get_instance()->setStatusBarMessage( QString("[%1] Set note velocity").arg( valueChar ), 2000 );
				}
				else if ( m_Mode == PAN && !pNote->get_note_off() ){
					float pan_l, pan_r;
					if ( (ev->button() == Qt::MidButton) || (ev->modifiers() == Qt::ControlModifier && ev->button() == Qt::LeftButton) ) {
						val = 0.5;
					}
					if ( val > 0.5 ) {
						pan_l = 1.0 - val;
						pan_r = 0.5;
					}
					else {
						pan_l = 0.5;
						pan_r = val;
					}
					pNote->set_pan_l( pan_l );
					pNote->set_pan_r( pan_r );

					char valueChar[100];
					float val = pan_r - pan_l + 0.5;
					sprintf( valueChar, "%#.2f",  val);
					( HydrogenApp::get_instance() )->setStatusBarMessage( QString("[%1] Set note panning").arg( valueChar ), 2000 );
				}
				else if ( m_Mode == LEADLAG ){
					float leadLag;
					if ( (ev->button() == Qt::MidButton) || (ev->modifiers() == Qt::ControlModifier && ev->button() == Qt::LeftButton) ) {
						pNote->set_lead_lag(0.0);
						leadLag = 0.0;
					} else {
						leadLag = val * -2.0 + 1.0;
						pNote->set_lead_lag( leadLag );
						char valueChar[100];
						if ( leadLag < 0.0 ) {
							sprintf( valueChar, "%.2f",  ( leadLag * -5)); // FIXME: '5' taken from fLeadLagFactor calculation in hydrogen.cpp
							HydrogenApp::get_instance()->setStatusBarMessage( QString("Leading beat by: %1 ticks").arg( valueChar ), 2000 );
						} else if ( leadLag > 0.0 ) {
							sprintf( valueChar, "%.2f",  ( leadLag * 5)); // FIXME: '5' taken from fLeadLagFactor calculation in hydrogen.cpp
							HydrogenApp::get_instance()->setStatusBarMessage( QString("Lagging beat by: %1 ticks").arg( valueChar ), 2000 );
						} else {
							HydrogenApp::get_instance()->setStatusBarMessage( QString("Note on beat"), 2000 );
						}
	
					}
				}
	
				else if ( m_Mode == NOTEKEY ){
					if ( (ev->button() == Qt::MidButton) || (ev->modifiers() == Qt::ControlModifier && ev->button() == Qt::LeftButton) ) {
						;
					} else {
						//set the note hight
						//QMessageBox::information ( this, "Hydrogen", trUtf8( "val: %1" ).arg(keyval)  );
						int k = 666;
						int o = 666;
						if(keyval >=6 && keyval<=125) {
							k = (keyval-6)/10;
						} else if(keyval>=135 && keyval<=205) {
							o = (keyval-166)/10;
							if(o==-4) o=-3; // 135
						}
						pNote->set_key_octave((Note::Key)k,(Note::Octave)o); // won't set wrong values see Note::set_key_octave
					}
				}
				else if ( m_Mode == PROBABILITY && !pNote->get_note_off() ) {
					pNote->set_probability( val );
					char valueChar[100];
					sprintf( valueChar, "%#.2f",  val);
					HydrogenApp::get_instance()->setStatusBarMessage( QString("[%1] Set note probability").arg( valueChar ), 2000 );
				}

	
				if( columnChange ){
					__columnCheckOnXmouseMouve = column;
					return;
				}
				// The pattern id is not properly set
				// by the pattern editor and has to be
				// adjusted manually.
				notePropertiesNew = pNote->get_note_properties();
				notePropertiesNew.pattern_idx = __nSelectedPatternNumber;
				// Create a H2Core::NotePropertiesChanges
				// struct specifying what did
				// change during the last action.
				notePropertiesChanges =
					{ m_Mode, notePropertiesOld, notePropertiesNew };
				// Push the changes onto the list
				// keeping track of all changes during
				// the current action.
				propertyChangesStack.push_front( notePropertiesChanges );
				// Push the changes onto the `QUndoStack'.
				pushUndoAction( propertyChangesStack );

				pSong->set_is_modified( true );
				updateEditor();
				break;
			}
		}
		m_pPatternEditorPanel->getPianoRollEditor()->updateEditor();
		pPatternEditor->updateEditor();
	}
	chrono::high_resolution_clock::time_point t2_press = chrono::high_resolution_clock::now();
	chrono::duration<double> tPressSpan = chrono::duration_cast<chrono::duration<double>>(t2_press - t1_press);
	// INFOLOG( QString( "Duration mouse press [seconds]: %1" ).arg( tPressSpan.count() ) );
}

void NotePropertiesRuler::mouseReleaseEvent(QMouseEvent *ev)
{
	// Create a list, which will contain the changes applied to a
	// single notes. All changes will be inserted into the
	// `QUndoStack' at once when handed over to the
	// `pushUndoAction' function. This way they can all be
	// reverted upon pressed Ctrl-Z just once.
	// std::list<NotePropertiesChanges> propertyChangesStack;
	m_bMouseIsPressed = false;
	// // Grab all global variables specifying the past and current
	// // state of the note and instantiate structs using them.
	// NoteProperties oldNoteProperties =
	// 	{ __undoColumn, __nSelectedPatternNumber,
	// 	  __nSelectedInstrument, __oldVelocity, old_pan_l,
	// 	  old_pan_r, __oldLeadLag, __oldNoteKeyVal,
	// 	  __oldOctaveKeyVal, __oldProbability };
	// NoteProperties currentNoteProperties =
	// 	{ __undoColumn, __nSelectedPatternNumber,
	// 	  __nSelectedInstrument, __velocity, __pan_L, __pan_R,
	// 	  __leadLag, __noteKeyVal, __octaveKeyVal,
	// 	  __probability };
	// // Create a struct specifying what did change during the last
	// // action.
	// NotePropertiesChanges notePropertiesChanges =
	// 	{ m_Mode, oldNoteProperties,
	// 	  currentNoteProperties };
	// // Push the changes onto the list keeping track of all changes
	// // during the current action.
	// propertyChangesStack.push_front( notePropertiesChanges );
	// // Push the changes onto the `QUndoStack'.
	// pushUndoAction( propertyChangesStack );
}

// Create an action, which is capable of reverting the most recent
// change(s) to the note properties and pushing it onto the
// `QUndoStack'.
void NotePropertiesRuler::pushUndoAction( std::list<NotePropertiesChanges> propertyChangesStack )
{
	SE_editNotePropertiesAction *action = new SE_editNotePropertiesAction( propertyChangesStack );

	HydrogenApp::get_instance()->m_pUndoStack->push( action );
}

void NotePropertiesRuler::paintEvent( QPaintEvent *ev)
{
	QPainter painter(this);
	painter.drawPixmap( ev->rect(), *m_pBackground, ev->rect() );
}



void NotePropertiesRuler::createVelocityBackground(QPixmap *pixmap)
{
	if ( !isVisible() ) {
		return;
	}

	UIStyle *pStyle = Preferences::get_instance()->getDefaultUIStyle();

	H2RGBColor valueColor(
			(int)( pStyle->m_patternEditor_backgroundColor.getRed() * ( 1 - 0.3 ) ),
			(int)( pStyle->m_patternEditor_backgroundColor.getGreen() * ( 1 - 0.3 ) ),
			(int)( pStyle->m_patternEditor_backgroundColor.getBlue() * ( 1 - 0.3 ) )
	);

	QColor res_1( pStyle->m_patternEditor_line1Color.getRed(), pStyle->m_patternEditor_line1Color.getGreen(), pStyle->m_patternEditor_line1Color.getBlue() );
	QColor res_2( pStyle->m_patternEditor_line2Color.getRed(), pStyle->m_patternEditor_line2Color.getGreen(), pStyle->m_patternEditor_line2Color.getBlue() );
	QColor res_3( pStyle->m_patternEditor_line3Color.getRed(), pStyle->m_patternEditor_line3Color.getGreen(), pStyle->m_patternEditor_line3Color.getBlue() );
	QColor res_4( pStyle->m_patternEditor_line4Color.getRed(), pStyle->m_patternEditor_line4Color.getGreen(), pStyle->m_patternEditor_line4Color.getBlue() );
	QColor res_5( pStyle->m_patternEditor_line5Color.getRed(), pStyle->m_patternEditor_line5Color.getGreen(), pStyle->m_patternEditor_line5Color.getBlue() );

	QColor backgroundColor( pStyle->m_patternEditor_backgroundColor.getRed(), pStyle->m_patternEditor_backgroundColor.getGreen(), pStyle->m_patternEditor_backgroundColor.getBlue() );
	QColor horizLinesColor(
			pStyle->m_patternEditor_backgroundColor.getRed() - 20,
			pStyle->m_patternEditor_backgroundColor.getGreen() - 20,
			pStyle->m_patternEditor_backgroundColor.getBlue() - 20
	);

	unsigned nNotes = MAX_NOTES;
	if (m_pPattern) {
		nNotes = m_pPattern->get_length();
	}


	QPainter p( pixmap );

	p.fillRect( 0, 0, width(), height(), QColor(0,0,0) );
	p.fillRect( 0, 0, 20 + nNotes * m_nGridWidth, height(), backgroundColor );


	// vertical lines

	DrumPatternEditor *pPatternEditor = m_pPatternEditorPanel->getDrumPatternEditor();
	int nBase;
	if (pPatternEditor->isUsingTriplets()) {
		nBase = 3;
	}
	else {
		nBase = 4;
	}

	int n4th = 4 * MAX_NOTES / (nBase * 4);
	int n8th = 4 * MAX_NOTES / (nBase * 8);
	int n16th = 4 * MAX_NOTES / (nBase * 16);
	int n32th = 4 * MAX_NOTES / (nBase * 32);
	int n64th = 4 * MAX_NOTES / (nBase * 64);
	int nResolution = pPatternEditor->getResolution();


	if ( !pPatternEditor->isUsingTriplets() ) {

		for (uint i = 0; i < nNotes + 1; i++) {
			uint x = 20 + i * m_nGridWidth;

			if ( (i % n4th) == 0 ) {
				if (nResolution >= 4) {
					p.setPen( QPen( res_1, 0, Qt::DotLine ) );
					p.drawLine(x, 0, x, m_nEditorHeight);
				}
			}
			else if ( (i % n8th) == 0 ) {
				if (nResolution >= 8) {
					p.setPen( QPen( res_2, 0, Qt::DotLine ) );
					p.drawLine(x, 0, x, m_nEditorHeight);
				}
			}
			else if ( (i % n16th) == 0 ) {
				if (nResolution >= 16) {
					p.setPen( QPen( res_3, 0, Qt::DotLine ) );
					p.drawLine(x, 0, x, m_nEditorHeight);
				}
			}
			else if ( (i % n32th) == 0 ) {
				if (nResolution >= 32) {
					p.setPen( QPen( res_4, 0, Qt::DotLine ) );
					p.drawLine(x, 0, x, m_nEditorHeight);
				}
			}
			else if ( (i % n64th) == 0 ) {
				if (nResolution >= 64) {
					p.setPen( QPen( res_5, 0, Qt::DotLine ) );
					p.drawLine(x, 0, x, m_nEditorHeight);
				}
			}
		}
	}
	else {	// Triplets
		uint nCounter = 0;
		int nSize = 4 * MAX_NOTES / (nBase * nResolution);

		for (uint i = 0; i < nNotes + 1; i++) {
			uint x = 20 + i * m_nGridWidth;

			if ( (i % nSize) == 0) {
				if ((nCounter % 3) == 0) {
					p.setPen( QPen( res_1, 0, Qt::DotLine ) );
				}
				else {
					p.setPen( QPen( res_3, 0, Qt::DotLine ) );
				}
				p.drawLine(x, 0, x, m_nEditorHeight);
				nCounter++;
			}
		}
	}

	p.setPen( horizLinesColor );
	for (unsigned y = 0; y < m_nEditorHeight; y = y + (m_nEditorHeight / 10)) {
		p.drawLine(20, y, 20 + nNotes * m_nGridWidth, y);
	}

	// draw velocity lines
	if (m_pPattern != NULL) {
		int nSelectedInstrument = Hydrogen::get_instance()->getSelectedInstrumentNumber();
		Song *pSong = Hydrogen::get_instance()->getSong();

		const Pattern::notes_t* notes = m_pPattern->get_notes();
		FOREACH_NOTE_CST_IT_BEGIN_END(notes,it) {
			Note *pposNote = it->second;
			assert( pposNote );
			uint pos = pposNote->get_position();
			int xoffset = 0;
			FOREACH_NOTE_CST_IT_BOUND(notes,coit,pos) {
				Note *pNote = coit->second;
				assert( pNote );
				if ( pNote->get_instrument() != pSong->get_instrument_list()->get( nSelectedInstrument ) ) {
					continue;
				}
				uint x_pos = 20 + pos * m_nGridWidth;
				uint line_end = height();


				uint value = 0;
				if ( m_Mode == VELOCITY ) {
					value = (uint)(pNote->get_velocity() * height());
				}
				else if ( m_Mode == PROBABILITY ) {
					value = (uint)(pNote->get_probability() * height());
				}
				uint line_start = line_end - value;
				QColor centerColor = DrumPatternEditor::computeNoteColor( pNote->get_velocity() );
				int nLineWidth = 3;
				p.fillRect( x_pos - 1 + xoffset, line_start, nLineWidth,  line_end - line_start , centerColor );
				xoffset++;
			}
		}
	}
	p.setPen(res_1);
	p.drawLine(0, 0, m_nEditorWidth, 0);
	p.drawLine(0, m_nEditorHeight - 1, m_nEditorWidth, m_nEditorHeight - 1);
}



void NotePropertiesRuler::createPanBackground(QPixmap *pixmap)
{
	if ( !isVisible() ) {
		return;
	}

	UIStyle *pStyle = Preferences::get_instance()->getDefaultUIStyle();

	QColor backgroundColor( pStyle->m_patternEditor_backgroundColor.getRed(), pStyle->m_patternEditor_backgroundColor.getGreen(), pStyle->m_patternEditor_backgroundColor.getBlue() );

	//QColor backgroundColor( 255, 255, 255 );
	QColor blackKeysColor( 240, 240, 240 );
	QColor horizLinesColor(
			pStyle->m_patternEditor_backgroundColor.getRed() - 20,
			pStyle->m_patternEditor_backgroundColor.getGreen() - 20,
			pStyle->m_patternEditor_backgroundColor.getBlue() - 20
	);
	H2RGBColor valueColor(
			(int)( pStyle->m_patternEditor_backgroundColor.getRed() * ( 1 - 0.3 ) ),
			(int)( pStyle->m_patternEditor_backgroundColor.getGreen() * ( 1 - 0.3 ) ),
			(int)( pStyle->m_patternEditor_backgroundColor.getBlue() * ( 1 - 0.3 ) )
	);

	QColor res_1( pStyle->m_patternEditor_line1Color.getRed(), pStyle->m_patternEditor_line1Color.getGreen(), pStyle->m_patternEditor_line1Color.getBlue() );
	QColor res_2( pStyle->m_patternEditor_line2Color.getRed(), pStyle->m_patternEditor_line2Color.getGreen(), pStyle->m_patternEditor_line2Color.getBlue() );
	QColor res_3( pStyle->m_patternEditor_line3Color.getRed(), pStyle->m_patternEditor_line3Color.getGreen(), pStyle->m_patternEditor_line3Color.getBlue() );
	QColor res_4( pStyle->m_patternEditor_line4Color.getRed(), pStyle->m_patternEditor_line4Color.getGreen(), pStyle->m_patternEditor_line4Color.getBlue() );
	QColor res_5( pStyle->m_patternEditor_line5Color.getRed(), pStyle->m_patternEditor_line5Color.getGreen(), pStyle->m_patternEditor_line5Color.getBlue() );

	QPainter p( pixmap );

	p.fillRect( 0, 0, width(), height(), QColor(0, 0, 0) );

	unsigned nNotes = MAX_NOTES;
	if (m_pPattern) {
		nNotes = m_pPattern->get_length();
	}
	p.fillRect( 0, 0, 20 + nNotes * m_nGridWidth, height(), backgroundColor );


	// central line
	p.setPen( horizLinesColor );
	p.drawLine(0, height() / 2.0, m_nEditorWidth, height() / 2.0);



	// vertical lines
	DrumPatternEditor *pPatternEditor = m_pPatternEditorPanel->getDrumPatternEditor();
	int nBase;
	if (pPatternEditor->isUsingTriplets()) {
		nBase = 3;
	}
	else {
		nBase = 4;
	}

	int n4th = 4 * MAX_NOTES / (nBase * 4);
	int n8th = 4 * MAX_NOTES / (nBase * 8);
	int n16th = 4 * MAX_NOTES / (nBase * 16);
	int n32th = 4 * MAX_NOTES / (nBase * 32);
	int n64th = 4 * MAX_NOTES / (nBase * 64);

	int nResolution = pPatternEditor->getResolution();

	if ( !pPatternEditor->isUsingTriplets() ) {

		for (uint i = 0; i < nNotes +1 ; i++) {
			uint x = 20 + i * m_nGridWidth;

			if ( (i % n4th) == 0 ) {
				if (nResolution >= 4) {
					p.setPen( QPen( res_1, 0, Qt::DotLine ) );
					p.drawLine(x, 0, x, m_nEditorHeight);
				}
			}
			else if ( (i % n8th) == 0 ) {
				if (nResolution >= 8) {
					p.setPen( QPen( res_2, 0, Qt::DotLine ) );
					p.drawLine(x, 0, x, m_nEditorHeight);
				}
			}
			else if ( (i % n16th) == 0 ) {
				if (nResolution >= 16) {
					p.setPen( QPen( res_3, 0, Qt::DotLine ) );
					p.drawLine(x, 0, x, m_nEditorHeight);
				}
			}
			else if ( (i % n32th) == 0 ) {
				if (nResolution >= 32) {
					p.setPen( QPen( res_4, 0, Qt::DotLine ) );
					p.drawLine(x, 0, x, m_nEditorHeight);
				}
			}
			else if ( (i % n64th) == 0 ) {
				if (nResolution >= 64) {
					p.setPen( QPen( res_5, 0, Qt::DotLine ) );
					p.drawLine(x, 0, x, m_nEditorHeight);
				}
			}
		}
	}
	else {	// Triplets
		uint nCounter = 0;
		int nSize = 4 * MAX_NOTES / (nBase * nResolution);

		for (uint i = 0; i < nNotes +1; i++) {
			uint x = 20 + i * m_nGridWidth;

			if ( (i % nSize) == 0) {
				if ((nCounter % 3) == 0) {
					p.setPen( QPen( res_1, 0, Qt::DotLine ) );
				}
				else {
					p.setPen( QPen( res_3, 0, Qt::DotLine ) );
				}
				p.drawLine(x, 0, x, m_nEditorHeight);
				nCounter++;
			}
		}
	}

	if ( m_pPattern ) {
		int nSelectedInstrument = Hydrogen::get_instance()->getSelectedInstrumentNumber();
		Song *pSong = Hydrogen::get_instance()->getSong();

		const Pattern::notes_t* notes = m_pPattern->get_notes();
		FOREACH_NOTE_CST_IT_BEGIN_END(notes,it) {
			Note *pposNote = it->second;
			assert( pposNote );
			uint pos = pposNote->get_position();
			int xoffset = 0;
			FOREACH_NOTE_CST_IT_BOUND(notes,coit,pos) {
				Note *pNote = coit->second;
				assert( pNote );
				if ( pNote->get_note_off() || pNote->get_instrument() != pSong->get_instrument_list()->get( nSelectedInstrument ) ) {
					continue;
				}
				uint x_pos = 20 + pNote->get_position() * m_nGridWidth;
				QColor centerColor = DrumPatternEditor::computeNoteColor( pNote->get_velocity() );
				
				if (pNote->get_pan_r() == pNote->get_pan_l()) {
					// pan value is centered - draw circle
					int y_pos = (int)( height() * 0.5 );
					p.setBrush(QColor( centerColor ));
					p.drawEllipse( x_pos-4 + xoffset, y_pos-4, 8, 8);
				} else {
					int y_start = (int)( pNote->get_pan_l() * height() );
					int y_end = (int)( height() - pNote->get_pan_r() * height() );
					int nLineWidth = 3;
					p.fillRect( x_pos - 1 + xoffset, y_start, nLineWidth, y_end - y_start, QColor(  centerColor) );
					p.fillRect( x_pos - 1 + xoffset, ( height() / 2.0 ) - 2 , nLineWidth, 5, QColor(  centerColor ) );
				}
				xoffset++;
			}
		}
	}

	p.setPen(res_1);
	p.drawLine(0, 0, m_nEditorWidth, 0);
	p.drawLine(0, m_nEditorHeight - 1, m_nEditorWidth, m_nEditorHeight - 1);
}

void NotePropertiesRuler::createLeadLagBackground(QPixmap *pixmap)
{
	if ( !isVisible() ) {
		return;
	}


	UIStyle *pStyle = Preferences::get_instance()->getDefaultUIStyle();
	
	QColor backgroundColor( pStyle->m_patternEditor_backgroundColor.getRed(), pStyle->m_patternEditor_backgroundColor.getGreen(), pStyle->m_patternEditor_backgroundColor.getBlue() );
	QColor blackKeysColor( 240, 240, 240 );
	QColor horizLinesColor(
			pStyle->m_patternEditor_backgroundColor.getRed() - 20,
			pStyle->m_patternEditor_backgroundColor.getGreen() - 20,
			pStyle->m_patternEditor_backgroundColor.getBlue() - 20
	);
	H2RGBColor valueColor(
			(int)( pStyle->m_patternEditor_backgroundColor.getRed() * ( 1 - 0.3 ) ),
			(int)( pStyle->m_patternEditor_backgroundColor.getGreen() * ( 1 - 0.3 ) ),
			(int)( pStyle->m_patternEditor_backgroundColor.getBlue() * ( 1 - 0.3 ) )
	);

	QColor res_1( pStyle->m_patternEditor_line1Color.getRed(), pStyle->m_patternEditor_line1Color.getGreen(), pStyle->m_patternEditor_line1Color.getBlue() );
	QColor res_2( pStyle->m_patternEditor_line2Color.getRed(), pStyle->m_patternEditor_line2Color.getGreen(), pStyle->m_patternEditor_line2Color.getBlue() );
	QColor res_3( pStyle->m_patternEditor_line3Color.getRed(), pStyle->m_patternEditor_line3Color.getGreen(), pStyle->m_patternEditor_line3Color.getBlue() );
	QColor res_4( pStyle->m_patternEditor_line4Color.getRed(), pStyle->m_patternEditor_line4Color.getGreen(), pStyle->m_patternEditor_line4Color.getBlue() );
	QColor res_5( pStyle->m_patternEditor_line5Color.getRed(), pStyle->m_patternEditor_line5Color.getGreen(), pStyle->m_patternEditor_line5Color.getBlue() );

	QPainter p( pixmap );

	p.fillRect( 0, 0, width(), height(), QColor(0, 0, 0) );

	unsigned nNotes = MAX_NOTES;
	if (m_pPattern) {
		nNotes = m_pPattern->get_length();
	}
	p.fillRect( 0, 0, 20 + nNotes * m_nGridWidth, height(), backgroundColor );


	// central line
	p.setPen( horizLinesColor );
	p.drawLine(0, height() / 2.0, m_nEditorWidth, height() / 2.0);



	// vertical lines
	DrumPatternEditor *pPatternEditor = m_pPatternEditorPanel->getDrumPatternEditor();
	int nBase;
	if (pPatternEditor->isUsingTriplets()) {
		nBase = 3;
	}
	else {
		nBase = 4;
	}

	int n4th = 4 * MAX_NOTES / (nBase * 4);
	int n8th = 4 * MAX_NOTES / (nBase * 8);
	int n16th = 4 * MAX_NOTES / (nBase * 16);
	int n32th = 4 * MAX_NOTES / (nBase * 32);
	int n64th = 4 * MAX_NOTES / (nBase * 64);

	int nResolution = pPatternEditor->getResolution();

	if ( !pPatternEditor->isUsingTriplets() ) {

		for (uint i = 0; i < nNotes + 1; i++) {
			uint x = 20 + i * m_nGridWidth;

			if ( (i % n4th) == 0 ) {
				if (nResolution >= 4) {
					p.setPen( QPen( res_1, 0, Qt::DotLine ) );
					p.drawLine(x, 0, x, m_nEditorHeight);
				}
			}
			else if ( (i % n8th) == 0 ) {
				if (nResolution >= 8) {
					p.setPen( QPen( res_2, 0, Qt::DotLine ) );
					p.drawLine(x, 0, x, m_nEditorHeight);
				}
			}
			else if ( (i % n16th) == 0 ) {
				if (nResolution >= 16) {
					p.setPen( QPen( res_3, 0, Qt::DotLine ) );
					p.drawLine(x, 0, x, m_nEditorHeight);
				}
			}
			else if ( (i % n32th) == 0 ) {
				if (nResolution >= 32) {
					p.setPen( QPen( res_4, 0, Qt::DotLine ) );
					p.drawLine(x, 0, x, m_nEditorHeight);
				}
			}
			else if ( (i % n64th) == 0 ) {
				if (nResolution >= 64) {
					p.setPen( QPen( res_5, 0, Qt::DotLine ) );
					p.drawLine(x, 0, x, m_nEditorHeight);
				}
			}
		}
	}
	else {  // Triplets
		uint nCounter = 0;
		int nSize = 4 * MAX_NOTES / (nBase * nResolution);

		for (uint i = 0; i < nNotes + 1; i++) {
			uint x = 20 + i * m_nGridWidth;

			if ( (i % nSize) == 0) {
				if ((nCounter % 3) == 0) {
					p.setPen( QPen( res_1, 0, Qt::DotLine ) );
				}
				else {
					p.setPen( QPen( res_3, 0, Qt::DotLine ) );
				}
				p.drawLine(x, 0, x, m_nEditorHeight);
				nCounter++;
			}
		}
	}

	if ( m_pPattern ) {
		int nSelectedInstrument = Hydrogen::get_instance()->getSelectedInstrumentNumber();
		Song *pSong = Hydrogen::get_instance()->getSong();

		const Pattern::notes_t* notes = m_pPattern->get_notes();
		FOREACH_NOTE_CST_IT_BEGIN_END(notes,it) {
			Note *pposNote = it->second;
			assert( pposNote );
			uint pos = pposNote->get_position();
			int xoffset = 0;
			FOREACH_NOTE_CST_IT_BOUND(notes,coit,pos) {
				Note *pNote = coit->second;
				assert( pNote );
				if ( pNote->get_instrument() != pSong->get_instrument_list()->get( nSelectedInstrument ) ) {
					continue;
				}

				uint x_pos = 20 + pNote->get_position() * m_nGridWidth;

				int red1 = (int) (pNote->get_velocity() * 255);
				int green1;
				int blue1;
				blue1 = ( 255 - (int) red1 )* .33;
				green1 =  ( 255 - (int) red1 );
	
				if (pNote->get_lead_lag() == 0) {
				
					// leadlag value is centered - draw circle
					int y_pos = (int)( height() * 0.5 );
					p.setBrush(QColor( 0 , 0 , 0 ));
					p.drawEllipse( x_pos-4 + xoffset, y_pos-4, 8, 8);
				} else {
					int y_start = (int)( height() * 0.5 );
					int y_end = y_start + ((pNote->get_lead_lag()/2) * height());
		
					int nLineWidth = 3;
					int red;
					int green;
					int blue = (int) (pNote->get_lead_lag() * 255);
					if (blue < 0)  {
						red = blue *-1;
						blue = (int) red * .33;
						green = (int) red * .33;
					} else {
						red = (int) blue * .33;
						green = (int) blue * .33;
					}
					p.fillRect( x_pos - 1 + xoffset, y_start, nLineWidth, y_end - y_start, QColor( red, green ,blue ) );
		
					p.fillRect( x_pos - 1 + xoffset, ( height() / 2.0 ) - 2 , nLineWidth, 5, QColor( red1, green1 ,blue1 ) );
				}
			xoffset++;
 			}
		}
	}

	p.setPen(res_1);
	p.drawLine(0, 0, m_nEditorWidth, 0);
	p.drawLine(0, m_nEditorHeight - 1, m_nEditorWidth, m_nEditorHeight - 1);
}



void NotePropertiesRuler::createNoteKeyBackground(QPixmap *pixmap)
{
	if ( !isVisible() ) {
		return;
	}

	UIStyle *pStyle = Preferences::get_instance()->getDefaultUIStyle();

	H2RGBColor valueColor(
			(int)( pStyle->m_patternEditor_backgroundColor.getRed() * ( 1 - 0.3 ) ),
			(int)( pStyle->m_patternEditor_backgroundColor.getGreen() * ( 1 - 0.3 ) ),
			(int)( pStyle->m_patternEditor_backgroundColor.getBlue() * ( 1 - 0.3 ) )
	);

	QColor res_1( pStyle->m_patternEditor_line1Color.getRed(), pStyle->m_patternEditor_line1Color.getGreen(), pStyle->m_patternEditor_line1Color.getBlue() );
	QColor res_2( pStyle->m_patternEditor_line2Color.getRed(), pStyle->m_patternEditor_line2Color.getGreen(), pStyle->m_patternEditor_line2Color.getBlue() );
	QColor res_3( pStyle->m_patternEditor_line3Color.getRed(), pStyle->m_patternEditor_line3Color.getGreen(), pStyle->m_patternEditor_line3Color.getBlue() );
	QColor res_4( pStyle->m_patternEditor_line4Color.getRed(), pStyle->m_patternEditor_line4Color.getGreen(), pStyle->m_patternEditor_line4Color.getBlue() );
	QColor res_5( pStyle->m_patternEditor_line5Color.getRed(), pStyle->m_patternEditor_line5Color.getGreen(), pStyle->m_patternEditor_line5Color.getBlue() );

	QColor backgroundColor( pStyle->m_patternEditor_backgroundColor.getRed(), pStyle->m_patternEditor_backgroundColor.getGreen(), pStyle->m_patternEditor_backgroundColor.getBlue() );
	QColor horizLinesColor(
			pStyle->m_patternEditor_backgroundColor.getRed() - 100,
			pStyle->m_patternEditor_backgroundColor.getGreen() - 100,
			pStyle->m_patternEditor_backgroundColor.getBlue() - 100
	);

	unsigned nNotes = MAX_NOTES;
	if (m_pPattern) {
		nNotes = m_pPattern->get_length();
	}
	QPainter p( pixmap );


	p.fillRect( 0, 0, width(), height(), QColor(0,0,0) );
	p.fillRect( 0, 0, 20 + nNotes * m_nGridWidth, height(), backgroundColor );


	p.setPen( horizLinesColor );
	for (unsigned y = 10; y < 80; y = y + 10 ) {
		p.setPen( QPen( res_1, 1, Qt::DashLine ) );
		if (y == 40) p.setPen( QPen( QColor(0,0,0), 1, Qt::SolidLine ) );
		p.drawLine(20, y, 20 + nNotes * m_nGridWidth, y);
	}

	for (unsigned y = 90; y < 210; y = y + 10 ) {
		p.setPen( QPen( QColor( 255, 255, 255 ), 9, Qt::SolidLine, Qt::FlatCap) );
		if ( y == 100 ||y == 120 ||y == 140 ||y == 170 ||y == 190)
			p.setPen( QPen( QColor( 0, 0, 0 ), 7, Qt::SolidLine, Qt::FlatCap ) );
		p.drawLine(20, y, 20 + nNotes * m_nGridWidth, y);
	}

	// vertical lines
	DrumPatternEditor *pPatternEditor = m_pPatternEditorPanel->getDrumPatternEditor();
	int nBase;
	if (pPatternEditor->isUsingTriplets()) {
		nBase = 3;
	}
	else {
		nBase = 4;
	}

	int n4th = 4 * MAX_NOTES / (nBase * 4);
	int n8th = 4 * MAX_NOTES / (nBase * 8);
	int n16th = 4 * MAX_NOTES / (nBase * 16);
	int n32th = 4 * MAX_NOTES / (nBase * 32);
	int n64th = 4 * MAX_NOTES / (nBase * 64);
	int nResolution = pPatternEditor->getResolution();


	if ( !pPatternEditor->isUsingTriplets() ) {

		for (uint i = 0; i < nNotes + 1; i++) {
			uint x = 20 + i * m_nGridWidth;

			if ( (i % n4th) == 0 ) {
				if (nResolution >= 4) {
					p.setPen( QPen( res_1, 0, Qt::DotLine ) );
					p.drawLine(x, 0, x, m_nEditorHeight);
				}
			}
			else if ( (i % n8th) == 0 ) {
				if (nResolution >= 8) {
					p.setPen( QPen( res_2, 0, Qt::DotLine ) );
					p.drawLine(x, 0, x, m_nEditorHeight);
				}
			}
			else if ( (i % n16th) == 0 ) {
				if (nResolution >= 16) {
					p.setPen( QPen( res_3, 0, Qt::DotLine ) );
					p.drawLine(x, 0, x, m_nEditorHeight);
				}
			}
			else if ( (i % n32th) == 0 ) {
				if (nResolution >= 32) {
					p.setPen( QPen( res_4, 0, Qt::DotLine ) );
					p.drawLine(x, 0, x, m_nEditorHeight);
				}
			}
			else if ( (i % n64th) == 0 ) {
				if (nResolution >= 64) {
					p.setPen( QPen( res_5, 0, Qt::DotLine ) );
					p.drawLine(x, 0, x, m_nEditorHeight);
				}
			}
		}
	}
	else {	// Triplets
		uint nCounter = 0;
		int nSize = 4 * MAX_NOTES / (nBase * nResolution);

		for (uint i = 0; i < nNotes + 1; i++) {
			uint x = 20 + i * m_nGridWidth;

			if ( (i % nSize) == 0) {
				if ((nCounter % 3) == 0) {
					p.setPen( QPen( res_1, 0, Qt::DotLine ) );
				}
				else {
					p.setPen( QPen( res_3, 0, Qt::DotLine ) );
				}
				p.drawLine(x, 0, x, m_nEditorHeight);
				nCounter++;
			}
		}
	}


	p.setPen(res_1);
	p.drawLine(0, 0, m_nEditorWidth, 0);
	p.drawLine(0, m_nEditorHeight - 1, m_nEditorWidth, m_nEditorHeight - 1);

//paint the oktave	
	if ( m_pPattern ) {
		int nSelectedInstrument = Hydrogen::get_instance()->getSelectedInstrumentNumber();
		Song *pSong = Hydrogen::get_instance()->getSong();
		const Pattern::notes_t* notes = m_pPattern->get_notes();
		FOREACH_NOTE_CST_IT_BEGIN_END(notes,it) {
			Note *pNote = it->second;
			assert( pNote );
			if ( pNote->get_instrument() != pSong->get_instrument_list()->get( nSelectedInstrument ) ) {
				continue;
			}
			if ( !pNote->get_note_off() ) {
				uint x_pos = 17 + pNote->get_position() * m_nGridWidth;
				uint y_pos = (4-pNote->get_octave())*10-3;
				p.setBrush(QColor( 99, 160, 233 ));
				p.drawEllipse( x_pos, y_pos, 6, 6);
			}
		}
	}

//paint the note
	if ( m_pPattern ) {
		int nSelectedInstrument = Hydrogen::get_instance()->getSelectedInstrumentNumber();
		Song *pSong = Hydrogen::get_instance()->getSong();
		const Pattern::notes_t* notes = m_pPattern->get_notes();
		FOREACH_NOTE_CST_IT_BEGIN_END(notes,it) {
			Note *pNote = it->second;
			assert( pNote );
			if ( pNote->get_instrument() != pSong->get_instrument_list()->get( nSelectedInstrument ) ) {
				continue;
			}

			if ( !pNote->get_note_off() ) {
				int d = 6;
				int k = pNote->get_key();
				uint x_pos = 17 + pNote->get_position() * m_nGridWidth;
				uint y_pos = 200-(k*10)-3;
				if(k<5) {
					if(!(k&0x01)) {
						x_pos-=1;
						y_pos-=1;
						d+=2;
					}
				} else {
					if(k&0x01) {
						x_pos-=1;
						y_pos-=1;
						d+=2;
					}
				}
				p.setBrush(QColor( 0, 0, 0));
				p.drawEllipse( x_pos, y_pos, d, d);
			}
		}
	}
}




void NotePropertiesRuler::updateEditor()
{
	Hydrogen *pEngine = Hydrogen::get_instance();
	PatternList *pPatternList = pEngine->getSong()->get_pattern_list();
	int nSelectedPatternNumber = pEngine->getSelectedPatternNumber();
	if ( (nSelectedPatternNumber != -1) && ( (uint)nSelectedPatternNumber < pPatternList->size() ) ) {
		m_pPattern = pPatternList->get( nSelectedPatternNumber );
	}
	else {
		m_pPattern = NULL;
	}
	__nSelectedPatternNumber = nSelectedPatternNumber;

	// update editor width
	int editorWidth;
	if ( m_pPattern ) {
		editorWidth = 20 + m_pPattern->get_length() * m_nGridWidth;
	}
	else {
		editorWidth =  20 + MAX_NOTES * m_nGridWidth;
	}
	resize( editorWidth, height() );
		
	delete m_pBackground;
	m_pBackground = new QPixmap( editorWidth, m_nEditorHeight );

	if ( m_Mode == VELOCITY || m_Mode == PROBABILITY ) {
		createVelocityBackground( m_pBackground );
	}
	else if ( m_Mode == PAN ) {
		createPanBackground( m_pBackground );
	}
	else if ( m_Mode == LEADLAG ) {
		createLeadLagBackground( m_pBackground );
	}
	else if ( m_Mode == NOTEKEY ) {
		createNoteKeyBackground( m_pBackground );
	}

	// redraw all
	update();
}



void NotePropertiesRuler::zoomIn()
{
	if (m_nGridWidth >= 3){
		m_nGridWidth *= 2;
	}else
	{
		m_nGridWidth *= 1.5;
	}
	updateEditor();
}



void NotePropertiesRuler::zoomOut()
{
	if ( m_nGridWidth > 1.5 ) {
		if (m_nGridWidth > 3){
			m_nGridWidth /=  2;
		}else
		{
			m_nGridWidth /= 1.5;
		}
	updateEditor();
	}
}


void NotePropertiesRuler::selectedPatternChangedEvent()
{
	updateEditor();
}



void NotePropertiesRuler::selectedInstrumentChangedEvent()
{
	updateEditor();
}



