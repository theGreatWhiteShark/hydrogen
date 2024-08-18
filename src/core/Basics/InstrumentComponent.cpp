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

#include <core/Basics/InstrumentComponent.h>

#include <cassert>

#include <core/Basics/InstrumentLayer.h>
#include <core/Helpers/Xml.h>
#include <core/License.h>


namespace H2Core
{

int InstrumentComponent::m_nMaxLayers = 16;

InstrumentComponent::InstrumentComponent( int related_drumkit_componentID )
	: __related_drumkit_componentID( related_drumkit_componentID )
	, m_fGain( 1.0 )
{
	m_layers.resize( m_nMaxLayers );
	for ( int i = 0; i < m_nMaxLayers; i++ ) {
		m_layers[i] = nullptr;
	}
}

InstrumentComponent::InstrumentComponent( std::shared_ptr<InstrumentComponent> other )
	: __related_drumkit_componentID( other->__related_drumkit_componentID )
	, m_fGain( other->m_fGain )
{
	m_layers.resize( m_nMaxLayers );
	for ( int i = 0; i < m_nMaxLayers; i++ ) {
		std::shared_ptr<InstrumentLayer> other_layer = other->getLayer( i );
		if ( other_layer ) {
			m_layers[i] = std::make_shared<InstrumentLayer>( other_layer );
		} else {
			m_layers[i] = nullptr;
		}
	}
}

InstrumentComponent::~InstrumentComponent()
{
	for ( int i = 0; i < m_nMaxLayers; i++ ) {
		m_layers[i] = nullptr;
	}
}

void InstrumentComponent::setLayer( std::shared_ptr<InstrumentLayer> layer, int idx )
{
	assert( idx >= 0 && idx < m_nMaxLayers );
	m_layers[ idx ] = layer;
}

void InstrumentComponent::setMaxLayers( int nLayers )
{
	if ( nLayers <= 1 ) {
		ERRORLOG( QString( "Attempting to set a max layer [%1] smaller than 1. Aborting" )
				  .arg( nLayers ) );
		return;
	}
	m_nMaxLayers = nLayers;
}

int InstrumentComponent::getMaxLayers()
{
	return m_nMaxLayers;
}

std::shared_ptr<InstrumentComponent> InstrumentComponent::loadFrom(
	const XMLNode& node,
	const QString& sDrumkitPath,
	const QString& sSongPath,
	const License& drumkitLicense,
	bool bSilent )
{
	auto pInstrumentComponent = std::make_shared<InstrumentComponent>( 0 );
	pInstrumentComponent->setGain( node.read_float( "gain", 1.0f,
													true, false, bSilent ) );
	XMLNode layer_node = node.firstChildElement( "layer" );
	int nLayer = 0;
	while ( ! layer_node.isNull() ) {
		if ( nLayer >= m_nMaxLayers ) {
			ERRORLOG( QString( "Layer #%1 >= m_nMaxLayers (%2). This as well as all further layers will be omitted." )
					  .arg( nLayer ).arg( m_nMaxLayers ) );
			break;
		}

		auto pLayer = InstrumentLayer::load_from(
			layer_node, sDrumkitPath, sSongPath, drumkitLicense, bSilent );
		if ( pLayer != nullptr ) {
			pInstrumentComponent->setLayer( pLayer, nLayer );
			nLayer++;
		}
		layer_node = layer_node.nextSiblingElement( "layer" );
	}
	
	return pInstrumentComponent;
}

void InstrumentComponent::saveTo( XMLNode& node,
								  bool bRecentVersion,
								  bool bSongKit ) const
{
	XMLNode component_node;
	if ( bRecentVersion ) {
		component_node = node.createNode( "instrumentComponent" );
		component_node.write_int( "component_id", __related_drumkit_componentID );
		component_node.write_float( "gain", m_fGain );
	}
	for ( int n = 0; n < m_nMaxLayers; n++ ) {
		auto pLayer = getLayer( n );
		if ( pLayer != nullptr ) {
			if ( bRecentVersion ) {
				pLayer->save_to( component_node, bSongKit );
			} else {
				pLayer->save_to( node, bSongKit );
			}
		}
	}
}

QString InstrumentComponent::toQString( const QString& sPrefix, bool bShort ) const {
	QString s = Base::sPrintIndention;
	QString sOutput;
	if ( ! bShort ) {
		sOutput = QString( "%1[InstrumentComponent]\n" ).arg( sPrefix )
			.append( QString( "%1%2related_drumkit_componentID: %3\n" ).arg( sPrefix ).arg( s ).arg( __related_drumkit_componentID ) )
			.append( QString( "%1%2m_fGain: %3\n" ).arg( sPrefix ).arg( s ).arg( m_fGain ) )
			.append( QString( "%1%2m_nMaxLayers: %3\n" ).arg( sPrefix ).arg( s ).arg( m_nMaxLayers ) )
			.append( QString( "%1%2m_layers:\n" ).arg( sPrefix ).arg( s ) );
	
		for ( const auto& ll : m_layers ) {
			if ( ll != nullptr ) {
				sOutput.append( QString( "%1" ).arg( ll->toQString( sPrefix + s + s, bShort ) ) );
			}
		}
	} else {
		sOutput = QString( "[InstrumentComponent]" )
			.append( QString( " related_drumkit_componentID: %1" ).arg( __related_drumkit_componentID ) )
			.append( QString( ", m_fGain: %1" ).arg( m_fGain ) )
			.append( QString( ", m_nMaxLayers: %1" ).arg( m_nMaxLayers ) )
			.append( QString( ", m_layers: [" ) );
	
		for ( const auto& ll : m_layers ) {
			if ( ll != nullptr ) {
				sOutput.append( QString( " [%1" ).arg( ll->toQString( sPrefix + s + s, bShort ).replace( "\n", "]" ) ) );
			}
		}

		sOutput.append( "]\n" );

	}
	
	return sOutput;
}

const std::vector<std::shared_ptr<InstrumentLayer>> InstrumentComponent::getLayers() const {
	std::vector<std::shared_ptr<InstrumentLayer>> layersUsed;
	for ( const auto& layer : m_layers ) {
		if ( layer != nullptr ) {
			layersUsed.push_back( layer );
		}
	}
	
	return layersUsed;
}

std::vector<std::shared_ptr<InstrumentLayer>>::iterator InstrumentComponent::begin() {
	return m_layers.begin();
}

std::vector<std::shared_ptr<InstrumentLayer>>::iterator InstrumentComponent::end() {
	return m_layers.end();
}
};

/* vim: set softtabstop=4 noexpandtab: */
