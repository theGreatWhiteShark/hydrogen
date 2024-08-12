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

#include "XmlTest.h"

#include <unistd.h>

#include <core/Basics/Drumkit.h>
#include <core/Basics/Pattern.h>
#include <core/Basics/Instrument.h>
#include <core/Basics/InstrumentList.h>
#include <core/Basics/InstrumentLayer.h>
#include <core/Basics/InstrumentComponent.h>
#include <core/Basics/Sample.h>
#include <core/Basics/Playlist.h>
#include <core/CoreActionController.h>
#include <core/Helpers/Filesystem.h>
#include <core/Hydrogen.h>
#include <core/Helpers/Xml.h>
#include <core/Preferences/Preferences.h>

#include <QDir>
#include <QTemporaryDir>
#include <QTime>

#include "TestHelper.h"
#include "assertions/File.h"

void XmlTest::setUp() {
	// Test for possible side effects by comparing serializations
	m_sPrefPre =
		H2Core::Preferences::get_instance()->toQString( "", true );
	m_sHydrogenPre = H2Core::Hydrogen::get_instance()->toQString( "", true );

}

void XmlTest::tearDown() {

	QDirIterator it( TestHelper::get_instance()->getTestDataDir(),
					 QDirIterator::Subdirectories);
	QStringList filters;
	filters << "*.bak*";

	while ( it.hasNext() ) {
		it.next();
		const QDir testFolder( it.next() );
		const QStringList backupFiles = testFolder.entryList( filters, QDir::NoFilter, QDir::NoSort );

		for ( auto& bbackupFile : backupFiles ) {

			H2Core::Filesystem::rm( testFolder.absolutePath()
									.append( "/" )
									.append( bbackupFile ), false );
		}
	}

	CPPUNIT_ASSERT( m_sPrefPre ==
					H2Core::Preferences::get_instance()->toQString( "", true ) );
	// CPPUNIT_ASSERT( m_sHydrogenPre ==
	// 				H2Core::Hydrogen::get_instance()->toQString( "", true ) );
}

////////////////////////////////////////////////////////////////////////////////

void XmlTest::testDrumkitFormatIntegrity() {
	___INFOLOG( "" );
	const QString sTestFolder = H2TEST_FILE( "/drumkits/format-integrity/");
	const auto pDrumkit = H2Core::Drumkit::load( sTestFolder );
	CPPUNIT_ASSERT( pDrumkit != nullptr );

	const QString sTmpDrumkitXml =
		H2Core::Filesystem::tmp_file_path( "drumkit-format-integrity.xml" );

	// We just store the definition. Saving the whole kit is tested in another
	// function.
	H2Core::XMLDoc doc;
	H2Core::XMLNode root = doc.set_root( "drumkit_info", "drumkit" );
	pDrumkit->saveTo( root, -1, true, false, false );

	CPPUNIT_ASSERT( doc.write( sTmpDrumkitXml ) );

	H2TEST_ASSERT_XML_FILES_EQUAL(
		H2Core::Filesystem::drumkit_file( sTestFolder ), sTmpDrumkitXml );

	// Cleanup
	CPPUNIT_ASSERT( H2Core::Filesystem::rm( sTmpDrumkitXml ) );
	___INFOLOG( "passed" );
}

void XmlTest::testDrumkit()
{
	___INFOLOG( "" );
	QString sDrumkitPath = H2Core::Filesystem::tmp_dir()+"dk0";

	std::shared_ptr<H2Core::Drumkit> pDrumkitLoaded = nullptr;
	std::shared_ptr<H2Core::Drumkit> pDrumkitReloaded = nullptr;
	std::shared_ptr<H2Core::Drumkit> pDrumkitCopied = nullptr;
	std::shared_ptr<H2Core::Drumkit> pDrumkitNew = nullptr;
	H2Core::XMLDoc doc;

	// load without samples
	pDrumkitLoaded = H2Core::Drumkit::load( H2TEST_FILE( "/drumkits/baseKit") );
	CPPUNIT_ASSERT( pDrumkitLoaded!=nullptr );
	CPPUNIT_ASSERT( pDrumkitLoaded->areSamplesLoaded()==false );
	CPPUNIT_ASSERT( checkSampleData( pDrumkitLoaded, false ) );
	CPPUNIT_ASSERT_EQUAL( 4, pDrumkitLoaded->getInstruments()->size() );

	// Check if drumkit was valid (what we assume in this test)
	CPPUNIT_ASSERT( TestHelper::get_instance()->findDrumkitBackupFiles( "drumkits/baseKit/" )
					.size() == 0 );
	
	// manually load samples
	pDrumkitLoaded->loadSamples();
	CPPUNIT_ASSERT( pDrumkitLoaded->areSamplesLoaded()==true );
	CPPUNIT_ASSERT( checkSampleData( pDrumkitLoaded, true ) );

	pDrumkitLoaded = nullptr;
	
	// load with samples
	pDrumkitLoaded = H2Core::Drumkit::load( H2TEST_FILE( "/drumkits/baseKit" ) );
	CPPUNIT_ASSERT( pDrumkitLoaded!=nullptr );

	pDrumkitLoaded->loadSamples();
	CPPUNIT_ASSERT( pDrumkitLoaded->areSamplesLoaded()==true );
	CPPUNIT_ASSERT( checkSampleData( pDrumkitLoaded, true ) );
	
	// unload samples
	pDrumkitLoaded->unloadSamples();
	CPPUNIT_ASSERT( pDrumkitLoaded->areSamplesLoaded()==false );
	CPPUNIT_ASSERT( checkSampleData( pDrumkitLoaded, false ) );
	
	// save drumkit elsewhere
	pDrumkitLoaded->setName( "pDrumkitLoaded" );
	CPPUNIT_ASSERT( pDrumkitLoaded->save( sDrumkitPath, true ) );
	CPPUNIT_ASSERT( H2Core::Filesystem::file_readable( sDrumkitPath+"/drumkit.xml" ) );
	CPPUNIT_ASSERT( H2Core::Filesystem::file_readable( sDrumkitPath+"/crash.wav" ) );
	CPPUNIT_ASSERT( H2Core::Filesystem::file_readable( sDrumkitPath+"/hh.wav" ) );
	CPPUNIT_ASSERT( H2Core::Filesystem::file_readable( sDrumkitPath+"/kick.wav" ) );
	CPPUNIT_ASSERT( H2Core::Filesystem::file_readable( sDrumkitPath+"/snare.wav" ) );

	// Check whether the generated drumkit is valid.
	CPPUNIT_ASSERT( doc.read( H2Core::Filesystem::drumkit_file( sDrumkitPath ),
							  H2Core::Filesystem::drumkit_xsd_path() ) );
	
	// load file
	pDrumkitReloaded = H2Core::Drumkit::load( sDrumkitPath );
	CPPUNIT_ASSERT( pDrumkitReloaded!=nullptr );
	
	// copy constructor
	pDrumkitCopied = std::make_shared<H2Core::Drumkit>( pDrumkitReloaded );
	CPPUNIT_ASSERT( pDrumkitCopied!=nullptr );
	// save file
	pDrumkitCopied->setName( "COPY" );
	CPPUNIT_ASSERT( pDrumkitCopied->save( sDrumkitPath ) );

	pDrumkitReloaded = nullptr;

	// Check whether blank drumkits are valid.
	pDrumkitNew = std::make_shared<H2Core::Drumkit>();
	CPPUNIT_ASSERT( pDrumkitNew != nullptr );
	CPPUNIT_ASSERT( pDrumkitNew->save( sDrumkitPath ) );
	CPPUNIT_ASSERT( doc.read( H2Core::Filesystem::drumkit_file( sDrumkitPath ),
							  H2Core::Filesystem::drumkit_xsd_path() ) );
	pDrumkitReloaded = H2Core::Drumkit::load( sDrumkitPath );
	CPPUNIT_ASSERT( pDrumkitReloaded != nullptr );

	// Cleanup
	H2Core::Filesystem::rm( sDrumkitPath, true );
	___INFOLOG( "passed" );
}

//Load drumkit which includes instrument with invalid ADSR values.
// Expected behavior: The drumkit will be loaded successfully. 
//					  In addition, the drumkit file will be saved with 
//					  correct ADSR values.
void XmlTest::testDrumkit_UpgradeInvalidADSRValues()
{
	___INFOLOG( "" );
	auto pTestHelper = TestHelper::get_instance();
	std::shared_ptr<H2Core::Drumkit> pDrumkit = nullptr;

	//1. Check, if the drumkit has been loaded
	pDrumkit = H2Core::Drumkit::load( H2TEST_FILE( "drumkits/invAdsrKit") );
	CPPUNIT_ASSERT( pDrumkit != nullptr );
	
	//2. Make sure that the instruments of the drumkit have been loaded correctly (see GH issue #839)
	auto pInstruments = pDrumkit->getInstruments();
	CPPUNIT_ASSERT( pInstruments != nullptr );
	
	auto pFirstInstrument = pInstruments->get(0);
	CPPUNIT_ASSERT( pFirstInstrument != nullptr );
	
	auto pLayer = pFirstInstrument->get_components()->front()->get_layer(0);
	CPPUNIT_ASSERT( pLayer != nullptr );
	
	auto pSample = pLayer->get_sample();
	CPPUNIT_ASSERT( pSample != nullptr );
	
	CPPUNIT_ASSERT( pSample->get_filename() == QString("snare.wav"));
	
	// 3. Make sure that the original (invalid) file has been saved as
	// a backup.
	if ( H2Core::Filesystem::dir_writable( H2TEST_FILE( "drumkits/invAdsrKit" ), true ) ) {
		QStringList backupFiles = pTestHelper->findDrumkitBackupFiles( "drumkits/invAdsrKit" );
		CPPUNIT_ASSERT( backupFiles.size() == 1 );
		CPPUNIT_ASSERT( H2Core::Filesystem::file_exists( backupFiles[ 0 ] ) );
	}

	//4. Load the drumkit again to assure updated file is valid
	pDrumkit = H2Core::Drumkit::load( H2TEST_FILE( "drumkits/invAdsrKit") );
	QStringList backupFiles = pTestHelper->findDrumkitBackupFiles( "drumkits/invAdsrKit" );
	CPPUNIT_ASSERT( pDrumkit != nullptr );
	CPPUNIT_ASSERT( backupFiles.size() == 1 );
	
	// Cleanup
	CPPUNIT_ASSERT( H2Core::Filesystem::file_copy( backupFiles[ 0 ],
												   H2TEST_FILE( "/drumkits/invAdsrKit/drumkit.xml" ),
												   true ) );
	CPPUNIT_ASSERT( H2Core::Filesystem::rm( backupFiles[ 0 ], false ) );
	___INFOLOG( "passed" );
}

void XmlTest::testDrumkitUpgrade() {
	___INFOLOG( "" );

	// `CoreActionController::validateDrumkit()` will be called on invalid kits
	// in this unit test. This will cause the routine to _not_ clean up
	// extracted artifacts. We have to do ourselves. Else they will pile up in
	// the tmp folder.
	QDir tmpDir( H2Core::Filesystem::tmp_dir() );
	const auto tmpDirContentPre = tmpDir.entryList(
		QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files );

	// For all drumkits in the legacy folder, check whether there are
	// invalid. Then, we upgrade them to the most recent version and
	// check whether there are valid and if a second upgrade is yields
	// the same result.
	QDir legacyDir( H2TEST_FILE( "drumkits/legacyKits" ) );
	QStringList nameFilters;
	nameFilters << "*" + H2Core::Filesystem::drumkit_ext;

	QString sDrumkitPath;

	for ( const auto& ssFile : legacyDir.entryList( nameFilters, QDir::Files ) ) {

		sDrumkitPath = H2TEST_FILE( "drumkits/legacyKits" ) + "/" + ssFile;

		CPPUNIT_ASSERT( ! H2Core::CoreActionController::validateDrumkit(
							sDrumkitPath, false ) );

		// The number of files within the drumkit has to be constant.
		QTemporaryDir contentOriginal( H2Core::Filesystem::tmp_dir() +
									   "testDrumkitUpgrade_orig-" +
									   QTime::currentTime().toString( "hh-mm-ss-zzz" ) +
									   "-XXXXXX" );
		contentOriginal.setAutoRemove( false );
		CPPUNIT_ASSERT( H2Core::CoreActionController::extractDrumkit(
							sDrumkitPath, contentOriginal.path() ) );
		QDir contentDirOriginal( contentOriginal.path() );
		int nFilesOriginal = contentDirOriginal.entryList(
			QDir::AllEntries | QDir::NoDotAndDotDot ).size();

		// Upgrade the legacy kit and store the result in a temporary
		// folder (they will be automatically removed by Qt as soon as
		// the variable gets out of scope)
		QTemporaryDir firstUpgrade( H2Core::Filesystem::tmp_dir() +
									"testDrumkitUpgrade_firstUpgrade-" +
									QTime::currentTime().toString( "hh-mm-ss-zzz" ) +
									"-XXXXXX" );
		firstUpgrade.setAutoRemove( false );
		CPPUNIT_ASSERT( H2Core::CoreActionController::upgradeDrumkit(
							sDrumkitPath, firstUpgrade.path() ) );
		// The upgrade should have yielded a single .h2drumkit file.
		QDir upgradeFolder( firstUpgrade.path() );
		CPPUNIT_ASSERT( upgradeFolder.entryList(
							QDir::AllEntries | QDir::NoDotAndDotDot ).size() == 1 );
		
		QString sUpgradedKit( firstUpgrade.path() + "/" +
							  upgradeFolder.entryList( QDir::AllEntries |
													   QDir::NoDotAndDotDot )[ 0 ] );
		CPPUNIT_ASSERT( H2Core::CoreActionController::validateDrumkit(
							sUpgradedKit, false ) );

		// Check whether the drumkit call be loaded properly.
		bool b;
		QString s1, s2;
		auto pDrumkit = H2Core::CoreActionController::retrieveDrumkit(
			firstUpgrade.path() + "/" + ssFile, &b, &s1, &s2 );
		CPPUNIT_ASSERT( pDrumkit != nullptr );
		if ( pDrumkit->getName() == "Boss DR-110" ) {
			// For our default kit we put in some prior knowledge to
			// check whether the upgrade process produce the expected
			// results.
			auto pInstrumentList = pDrumkit->getInstruments();
			CPPUNIT_ASSERT( pInstrumentList != nullptr );
			CPPUNIT_ASSERT( pInstrumentList->size() == 6 );

			auto pInstrument = pInstrumentList->get( 0 );
			CPPUNIT_ASSERT( pInstrument != nullptr );

			auto pComponents = pInstrument->get_components();
			CPPUNIT_ASSERT( pComponents != nullptr );
			CPPUNIT_ASSERT( pComponents->size() == 1 );

			auto pComponent = pComponents->at( 0 );
			CPPUNIT_ASSERT( pComponent != nullptr );
			
			auto pLayers = pComponent->get_layers();
			CPPUNIT_ASSERT( pLayers.size() == 2 );
		}
		
		QTemporaryDir contentUpgraded( H2Core::Filesystem::tmp_dir() +
									"testDrumkitUpgrade_contentUpgraded-" +
									QTime::currentTime().toString( "hh-mm-ss-zzz" ) +
									"-XXXXXX" );
		contentUpgraded.setAutoRemove( false );
		CPPUNIT_ASSERT( H2Core::CoreActionController::extractDrumkit(
							sUpgradedKit, contentUpgraded.path() ) );
		QDir contentDirUpgraded( contentUpgraded.path() );
		int nFilesUpgraded =
			contentDirUpgraded.entryList( QDir::AllEntries |
										  QDir::NoDotAndDotDot ).size();
		___INFOLOG( nFilesUpgraded );
		if ( nFilesOriginal != nFilesUpgraded ) {
			___ERRORLOG( "Mismatching content of original and upgraded drumkit." );
			___ERRORLOG( QString( "original [%1]:" ).arg( contentOriginal.path() ) );
			for ( const auto& ssFile : contentDirOriginal.entryList( QDir::AllEntries |
																	 QDir::NoDotAndDotDot ) ) {
				___ERRORLOG( "   " + ssFile );
			}
			___ERRORLOG( QString( "upgraded [%1]:" ).arg( contentUpgraded.path() ) );
			for ( const auto& ssFile : contentDirUpgraded.entryList( QDir::AllEntries |
																	 QDir::NoDotAndDotDot ) ) {
				___ERRORLOG( "   " + ssFile );
			}
		}
		CPPUNIT_ASSERT( nFilesOriginal == nFilesUpgraded );

		// Now we upgrade the upgraded drumkit again and bit-compare
		// the results.
		QTemporaryDir secondUpgrade( H2Core::Filesystem::tmp_dir() +
									"testDrumkitUpgrade_secondUpgrade-" +
									QTime::currentTime().toString( "hh-mm-ss-zzz" ) +
									 "-XXXXXX" );
		secondUpgrade.setAutoRemove( false );
		CPPUNIT_ASSERT( H2Core::CoreActionController::upgradeDrumkit(
							sUpgradedKit, secondUpgrade.path() ) );
		upgradeFolder = QDir( secondUpgrade.path() );
		CPPUNIT_ASSERT( upgradeFolder.entryList( QDir::AllEntries |
												 QDir::NoDotAndDotDot ).size() == 1 );
		
		QString sValidationKit( secondUpgrade.path() + "/" +
								upgradeFolder.entryList( QDir::AllEntries |
														 QDir::NoDotAndDotDot )[ 0 ] );

		QTemporaryDir contentValidation( H2Core::Filesystem::tmp_dir() +
										 "testDrumkitUpgrade_contentValidation-" +
										 QTime::currentTime().toString( "hh-mm-ss-zzz" ) +
										 "-XXXXXX" );
		contentValidation.setAutoRemove( false );
		CPPUNIT_ASSERT( H2Core::CoreActionController::extractDrumkit(
							sUpgradedKit, contentValidation.path() ) );

		// Compare the extracted folders. Attention: in the toplevel
		// temporary folder there is a single directory named
		// according to the drumkit. These ones have to be compared.
		H2TEST_ASSERT_DIRS_EQUAL(
			QDir( contentUpgraded.path() )
			.entryList( QDir::Dirs | QDir::NoDotAndDotDot )[ 0 ],
			QDir( contentValidation.path() )
			.entryList( QDir::Dirs | QDir::NoDotAndDotDot )[ 0 ] );

		// Only clean up if all checks passed.
		H2Core::Filesystem::rm( contentOriginal.path(), true, true );
		H2Core::Filesystem::rm( contentUpgraded.path(), true, true );
		H2Core::Filesystem::rm( contentValidation.path(), true, true );
		H2Core::Filesystem::rm( firstUpgrade.path(), true, true );
		H2Core::Filesystem::rm( secondUpgrade.path(), true, true );
	}

	// Check whether there is new content in the tmp dir.
	const auto tmpDirContentPost = tmpDir.entryList(
		QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files );

	for ( const auto& ssEntry : tmpDirContentPost ) {
		if ( ! tmpDirContentPre.contains( ssEntry ) ) {
			H2Core::Filesystem::rm( tmpDir.filePath( ssEntry ), true, true );
		}
	}

	___INFOLOG( "passed" );
}

void XmlTest::testDrumkitInstrumentTypeUniqueness()
{
	___INFOLOG( "" );

	// Test resilience against loading duplicate type and key. They should both
	// be dropped.
	const QString sRefFolder = H2TEST_FILE( "drumkits/instrument-type-ref" );
	const QString sDuplicateFolder =
		H2TEST_FILE( "drumkits/instrument-type-ref-duplicate" );
	const auto pDrumkitRef = H2Core::Drumkit::load( sRefFolder );
	CPPUNIT_ASSERT( pDrumkitRef != nullptr );
	const auto pDrumkitDuplicates = H2Core::Drumkit::load( sDuplicateFolder );
	CPPUNIT_ASSERT( pDrumkitDuplicates != nullptr );

	H2TEST_ASSERT_XML_FILES_UNEQUAL( sRefFolder + "/drumkit.xml",
								   sDuplicateFolder + "/drumkit.xml" );

	const QString sTmpRef = H2Core::Filesystem::tmp_dir() + "ref-saved";
	const QString sTmpDuplicate =
		H2Core::Filesystem::tmp_dir() + "duplicate-saved";

	CPPUNIT_ASSERT( pDrumkitRef->save( sTmpRef ) );
	CPPUNIT_ASSERT( pDrumkitDuplicates->save( sTmpDuplicate ) );

	H2TEST_ASSERT_XML_FILES_EQUAL( sTmpRef + "/drumkit.xml",
								   sTmpDuplicate + "/drumkit.xml" );
	H2TEST_ASSERT_DIRS_EQUAL( sTmpRef, sTmpDuplicate );

	H2Core::Filesystem::rm( sTmpRef, true );
	H2Core::Filesystem::rm( sTmpDuplicate, true );
	___INFOLOG( "passed" );
}

void XmlTest::testShippedDrumkits()
{
	___INFOLOG( "" );
	H2Core::XMLDoc doc;
	for ( const auto& ssKit : H2Core::Filesystem::sys_drumkit_list() ) {
		CPPUNIT_ASSERT( doc.read( QString( "%1%2/drumkit.xml" )
								  .arg( H2Core::Filesystem::sys_drumkits_dir() )
								  .arg( ssKit ),
								  H2Core::Filesystem::drumkit_xsd_path() ) );

	}
	___INFOLOG( "passed" );
}

////////////////////////////////////////////////////////////////////////////////

void XmlTest::testDrumkitMapFormatIntegrity() {
	___INFOLOG( "" );
	const QString sTestFile = H2TEST_FILE( "/drumkit_map/ref.h2map");
	const auto pDrumkitMap = H2Core::DrumkitMap::load( sTestFile );
	CPPUNIT_ASSERT( pDrumkitMap != nullptr );

	const QString sTmpDrumkitMap =
		H2Core::Filesystem::tmp_file_path( "drumkit-map-format-integrity.h2map" );
	CPPUNIT_ASSERT( pDrumkitMap->save( sTmpDrumkitMap ) );

	H2TEST_ASSERT_XML_FILES_EQUAL( sTestFile, sTmpDrumkitMap );

	// Cleanup
	CPPUNIT_ASSERT( H2Core::Filesystem::rm( sTmpDrumkitMap ) );
	___INFOLOG( "passed" );
}

void XmlTest::testDrumkitMap()
{
	___INFOLOG( "" );

	// Test resilience against loading duplicate type and key. They should both
	// be dropped.
	const QString sRefFile = H2TEST_FILE( "drumkit_map/ref.h2map" );
	const auto pDrumkitMapRef = H2Core::DrumkitMap::load( sRefFile );
	CPPUNIT_ASSERT( pDrumkitMapRef != nullptr );
	const auto pDrumkitMapDuplicates = H2Core::DrumkitMap::load(
		H2TEST_FILE( "drumkit_map/ref-duplicates.h2map" ) );
	CPPUNIT_ASSERT( pDrumkitMapDuplicates != nullptr );

	const QString sTmpFile = H2Core::Filesystem::tmp_dir() + "ref-saved.h2map";

	CPPUNIT_ASSERT( pDrumkitMapDuplicates->save( sTmpFile, false ) );
	H2TEST_ASSERT_XML_FILES_EQUAL( sRefFile, sTmpFile );

	H2Core::Filesystem::rm( sTmpFile );
	___INFOLOG( "passed" );
}

void XmlTest::testShippedDrumkitMaps()
{
	___INFOLOG( "" );

	QDir mapDir( H2Core::Filesystem::sys_drumkit_maps_dir() );
	H2Core::XMLDoc doc;
	const auto shippedMaps = mapDir.entryList(
		QStringList( QString( "*%1" ).arg( H2Core::Filesystem::drumkit_map_ext ) ),
		QDir::Files | QDir::NoDotAndDotDot );

	CPPUNIT_ASSERT( shippedMaps.size() > 0 );

	for ( const auto& ssMap : shippedMaps ) {
		CPPUNIT_ASSERT( doc.read( mapDir.filePath( ssMap ),
								  H2Core::Filesystem::drumkit_map_xsd_path() ) );
	}
	___INFOLOG( "passed" );
}

////////////////////////////////////////////////////////////////////////////////

void XmlTest::testPatternFormatIntegrity() {
	___INFOLOG( "" );
	const QString sTestFile = H2TEST_FILE( "/pattern/pattern.h2pattern" );
	const auto pPattern = H2Core::Pattern::load_file( sTestFile );
	CPPUNIT_ASSERT( pPattern != nullptr );

	const QString sTmpPattern =
		H2Core::Filesystem::tmp_file_path( "pattern-format-integrity.h2pattern" );
	CPPUNIT_ASSERT( pPattern->save_file( "GMRockKit", sTmpPattern, true ) );

	H2TEST_ASSERT_XML_FILES_EQUAL( sTestFile, sTmpPattern );

	// Cleanup
	CPPUNIT_ASSERT( H2Core::Filesystem::rm( sTmpPattern ) );
	___INFOLOG( "passed" );
}

void XmlTest::testPattern()
{
	___INFOLOG( "" );

	QString sPatternPath =
		H2Core::Filesystem::tmp_dir() + "pattern.h2pattern";

	H2Core::Pattern* pPatternLoaded = nullptr;
	H2Core::Pattern* pPatternCopied = nullptr;
	H2Core::Pattern* pPatternNew = nullptr;
	std::shared_ptr<H2Core::Drumkit> pDrumkit = nullptr;
	std::shared_ptr<H2Core::InstrumentList> pInstrumentList = nullptr;
	H2Core::XMLDoc doc;

	pDrumkit = H2Core::Drumkit::load( H2TEST_FILE( "/drumkits/baseKit" ) );
	CPPUNIT_ASSERT( pDrumkit!=nullptr );
	pInstrumentList = pDrumkit->getInstruments();
	CPPUNIT_ASSERT( pInstrumentList->size()==4 );

	pPatternLoaded = H2Core::Pattern::load_file(
		H2TEST_FILE( "/pattern/pattern.h2pattern" ) );
	CPPUNIT_ASSERT( pPatternLoaded != nullptr );
	CPPUNIT_ASSERT( pPatternLoaded->save_file( "GMRockKit", sPatternPath, true ) );

	H2TEST_ASSERT_XML_FILES_EQUAL( H2TEST_FILE( "pattern/pattern.h2pattern" ),
								   sPatternPath );

	// Check for double freeing when destructing both copy and original.
	pPatternCopied = new H2Core::Pattern( pPatternLoaded );

	// Check whether the constructor produces valid patterns.
	QString sEmptyPatternPath =
		H2Core::Filesystem::tmp_dir() + "empty.h2pattern";
	pPatternNew = new H2Core::Pattern( "test", "ladida", "", 1, 1 );
	CPPUNIT_ASSERT( pPatternNew->save_file( "GMRockKit", sPatternPath, true ) );
	CPPUNIT_ASSERT( doc.read( sPatternPath,
							  H2Core::Filesystem::pattern_xsd_path() ) );
	H2TEST_ASSERT_XML_FILES_EQUAL( H2TEST_FILE( "pattern/empty.h2pattern" ),
								   sPatternPath );

	// Cleanup
	H2Core::Filesystem::rm( sPatternPath );
	H2Core::Filesystem::rm( sEmptyPatternPath );
	delete pPatternLoaded;
	delete pPatternCopied;
	delete pPatternNew;
	___INFOLOG( "passed" );
}

void XmlTest::testPatternLegacy() {
	___INFOLOG( "" );

	QStringList legacyPatterns;
	legacyPatterns << H2TEST_FILE( "pattern/legacy/pattern-1.X.X.h2pattern" )
				   << H2TEST_FILE( "pattern/legacy/legacy_pattern.h2pattern" );

	H2Core::Pattern* pPattern;
	for ( const auto& ssPattern : legacyPatterns ) {
		pPattern = H2Core::Pattern::load_file( ssPattern );
		CPPUNIT_ASSERT( pPattern );
	}
	delete pPattern;

	___INFOLOG( "passed" );
}

void XmlTest::testPatternInstrumentTypes()
{
	___INFOLOG( "" );

	const QString sTmpWithoutTypes =
		H2Core::Filesystem::tmp_dir() + "pattern-without-types.h2pattern";
	const QString sTmpMismatch =
		H2Core::Filesystem::tmp_dir() + "pattern-with-mismatch.h2pattern";
	// Be sure to remove past artifacts or saving the patterns will fail.
	if ( H2Core::Filesystem::file_exists( sTmpWithoutTypes, true ) ) {
		H2Core::Filesystem::rm( sTmpWithoutTypes );
	}
	if ( H2Core::Filesystem::file_exists( sTmpMismatch, true ) ) {
		H2Core::Filesystem::rm( sTmpMismatch );
	}

	// Check whether the reference pattern is valid.
	const auto pPatternRef = H2Core::Pattern::load_file(
		H2TEST_FILE( "pattern/pattern.h2pattern") );
	CPPUNIT_ASSERT( pPatternRef != nullptr );

	// The version of the reference without any type information should be
	// filled with those obtained from the shipped .h2map file.
	const auto pPatternWithoutTypes = H2Core::Pattern::load_file(
		H2TEST_FILE( "pattern/pattern-without-types.h2pattern") );
	CPPUNIT_ASSERT( pPatternWithoutTypes != nullptr );
	CPPUNIT_ASSERT( pPatternWithoutTypes->save_file(
						"GMRockKit", sTmpWithoutTypes ) );
	H2TEST_ASSERT_XML_FILES_EQUAL(
		H2TEST_FILE( "pattern/pattern.h2pattern" ), sTmpWithoutTypes );

	// In this file an instrument id is off. But this should heal itself when
	// switching to another kit and back (as only instrument types are used
	// during switching and the ids are reassigned).
	const auto pPatternMismatch = H2Core::Pattern::load_file(
		H2TEST_FILE( "pattern/pattern-with-mismatch.h2pattern") );
	CPPUNIT_ASSERT( pPatternMismatch != nullptr );
	// TODO switch back and forth
	// CPPUNIT_ASSERT( pPatternMismatch->save_file( "GMRockKit", sTmpMismatch ) );
	// H2TEST_ASSERT_XML_FILES_EQUAL(
	// 	H2TEST_FILE( "pattern/pattern.h2pattern" ), sTmpMismatch );

	delete pPatternRef;
	delete pPatternWithoutTypes;
	delete pPatternMismatch;

	H2Core::Filesystem::rm( sTmpWithoutTypes );
	H2Core::Filesystem::rm( sTmpMismatch );
	___INFOLOG( "passed" );
}

void XmlTest::checkTestPatterns()
{
	___INFOLOG( "" );
	H2Core::XMLDoc doc;
	CPPUNIT_ASSERT( doc.read( H2TEST_FILE( "/pattern/empty.h2pattern" ),
							  H2Core::Filesystem::pattern_xsd_path() ) );
	CPPUNIT_ASSERT( doc.read( H2TEST_FILE( "/pattern/pattern.h2pattern" ),
							  H2Core::Filesystem::pattern_xsd_path() ) );
	CPPUNIT_ASSERT( doc.read(
						H2TEST_FILE( "/pattern/pattern-with-mismatch.h2pattern" ),
						H2Core::Filesystem::pattern_xsd_path() ) );
	CPPUNIT_ASSERT( doc.read(
						H2TEST_FILE( "/pattern/pattern-without-types.h2pattern" ),
						H2Core::Filesystem::pattern_xsd_path() ) );

	___INFOLOG( "passed" );
}

void XmlTest::testPlaylistFormatIntegrity() {
	___INFOLOG( "" );
	const QString sTestFile = H2TEST_FILE( "/playlist/test.h2playlist" );
	const auto pPlaylist = H2Core::Playlist::load( sTestFile );
	CPPUNIT_ASSERT( pPlaylist != nullptr );

	// As we are using relative paths to the song files, we have to create the
	// test artifact within the same folder as the original playlist.
	const QString sTmpPlaylist =
		H2TEST_FILE( "/playlist/tmp-duplicate-test.h2playlist" );
	CPPUNIT_ASSERT( pPlaylist->saveAs( sTmpPlaylist, false ) );

	H2TEST_ASSERT_XML_FILES_EQUAL( sTestFile, sTmpPlaylist );

	// Cleanup
	CPPUNIT_ASSERT( H2Core::Filesystem::rm( sTmpPlaylist ) );
	___INFOLOG( "passed" );
}

////////////////////////////////////////////////////////////////////////////////

void XmlTest::testPlaylist()
{
	___INFOLOG( "" );

	const QString sTmpPath = H2Core::Filesystem::tmp_dir() +
		"playlist.h2playlist";
	const QString sTmpPathEmpty = H2Core::Filesystem::tmp_dir() +
		"empty.h2playlist";

	// Test constructor
	auto pPlaylist = H2Core::Playlist::load(
		H2TEST_FILE( "playlist/test.h2playlist" ) );
	H2Core::XMLDoc doc;

	CPPUNIT_ASSERT( pPlaylist != nullptr );
	CPPUNIT_ASSERT( pPlaylist->saveAs( sTmpPath ) );
	CPPUNIT_ASSERT( doc.read( sTmpPath,
							  H2Core::Filesystem::playlist_xsd_path() ) );
	const auto pPlaylistLoaded = H2Core::Playlist::load( sTmpPath );
	CPPUNIT_ASSERT( pPlaylistLoaded != nullptr );

	// TODO Fails since it does not seem to be clear what relative does actually
	// mean? Relative to the playlist user dir? To the playlist itself?
	//
	// H2TEST_ASSERT_XML_FILES_EQUAL(
	// 	sTmpPath, H2TEST_FILE( "playlist/test.h2playlist" ));

	// Test constructor
	auto pPlaylistEmpty = std::make_shared<H2Core::Playlist>();
	H2Core::XMLDoc docEmpty;

	CPPUNIT_ASSERT( pPlaylistEmpty->saveAs( sTmpPathEmpty ) );
	CPPUNIT_ASSERT( docEmpty.read( sTmpPathEmpty,
							  H2Core::Filesystem::playlist_xsd_path() ) );
	const auto pPlaylistEmptyLoaded = H2Core::Playlist::load( sTmpPathEmpty );
	CPPUNIT_ASSERT( pPlaylistEmptyLoaded != nullptr );

	H2TEST_ASSERT_XML_FILES_EQUAL(
		sTmpPathEmpty, H2TEST_FILE( "playlist/empty.h2playlist" ));

	// Cleanup
	H2Core::Filesystem::rm( sTmpPath );
	H2Core::Filesystem::rm( sTmpPathEmpty );

	___INFOLOG( "passed" );
}

////////////////////////////////////////////////////////////////////////////////

void XmlTest::testSongFormatIntegrity() {
	___INFOLOG( "" );
	const QString sTestFile = H2TEST_FILE( "song/current.h2song" );
	const auto pSong = H2Core::Song::load( sTestFile );
	CPPUNIT_ASSERT( pSong != nullptr );

	const QString sTmpSong =
		H2Core::Filesystem::tmp_file_path( "current-format-integrity.h2song" );
	CPPUNIT_ASSERT( pSong->save( sTmpSong ) );

	H2TEST_ASSERT_H2SONG_FILES_EQUAL( sTestFile, sTmpSong );

	// Cleanup
	CPPUNIT_ASSERT( H2Core::Filesystem::rm( sTmpSong ) );
	___INFOLOG( "passed" );
}

void XmlTest::testSong()
{
	___INFOLOG( "" );
	const QString sTmpPath = H2Core::Filesystem::tmp_dir() +
		"song.h2song";
	const QString sTmpPathEmpty = H2Core::Filesystem::tmp_dir() +
		"empty.h2song";
	const QString sTmpPathConstructor = H2Core::Filesystem::tmp_dir() +
		"constructor.h2song";

	// Test constructor
	const auto pSongConstructor = std::make_shared<H2Core::Song>();
	CPPUNIT_ASSERT( pSongConstructor->save( sTmpPathConstructor ) );
	CPPUNIT_ASSERT( H2Core::Song::load( sTmpPathConstructor ) != nullptr );

	H2TEST_ASSERT_H2SONG_FILES_EQUAL(
		sTmpPathConstructor, H2TEST_FILE( "song/constructor.h2song" ));

	// Test empty song (which is using the default kit)
	const auto pSongEmpty = H2Core::Song::getEmptySong();
	CPPUNIT_ASSERT( pSongEmpty->save( sTmpPathEmpty ) );
	CPPUNIT_ASSERT( H2Core::Song::load( sTmpPathEmpty ) != nullptr );

	H2TEST_ASSERT_H2SONG_FILES_EQUAL(
		sTmpPathEmpty, H2TEST_FILE( "song/empty.h2song" ));

	// Cleanup
	H2Core::Filesystem::rm( sTmpPath );
	H2Core::Filesystem::rm( sTmpPathEmpty );
	H2Core::Filesystem::rm( sTmpPathConstructor );

	___INFOLOG( "passed" );
}

void XmlTest::testSongLegacy() {
	___INFOLOG( "" );
	QStringList testSongs;
	testSongs << H2TEST_FILE( "song/legacy/test_song_1.2.2.h2song" )
			  << H2TEST_FILE( "song/legacy/test_song_1.2.1.h2song" )
			  << H2TEST_FILE( "song/legacy/test_song_1.2.0.h2song" )
			  << H2TEST_FILE( "song/legacy/test_song_1.2.0-beta1.h2song" )
			  << H2TEST_FILE( "song/legacy/test_song_1.1.1.h2song" )
			  << H2TEST_FILE( "song/legacy/test_song_1.1.0.h2song" )
			  << H2TEST_FILE( "song/legacy/test_song_1.1.0-beta1.h2song" )
			  << H2TEST_FILE( "song/legacy/test_song_1.0.2.h2song" )
			  << H2TEST_FILE( "song/legacy/test_song_1.0.1.h2song" )
			  << H2TEST_FILE( "song/legacy/test_song_1.0.0.h2song" )
			  << H2TEST_FILE( "song/legacy/test_song_0.9.7.h2song" );

	for ( const auto& ssSong : testSongs ) {
		___INFOLOG(ssSong);
		auto pSong = H2Core::Song::load( ssSong, false );
		CPPUNIT_ASSERT( pSong != nullptr );
		CPPUNIT_ASSERT( ! pSong->hasMissingSamples() );
	}

	// Check that invalid paths and drumkit names could indeed result in missing
	// samples.
	testSongs.clear();
	testSongs << H2TEST_FILE( "song/legacy/test_song_invalid_drumkit_name.h2song" )
			  << H2TEST_FILE( "song/legacy/test_song_invalid_sample_path.h2song" );

	for ( const auto& ssSong : testSongs ) {
		___INFOLOG(ssSong);
		auto pSong = H2Core::Song::load( ssSong, false );
		CPPUNIT_ASSERT( pSong != nullptr );
		CPPUNIT_ASSERT( pSong->hasMissingSamples() );
	}
	___INFOLOG( "passed" );
}

////////////////////////////////////////////////////////////////////////////////

void XmlTest::testPreferencesFormatIntegrity() {
	___INFOLOG( "" );
	const QString sTestFile = H2TEST_FILE( "preferences/current.conf" );
	const auto pPreferences = H2Core::Preferences::load( sTestFile );
	CPPUNIT_ASSERT( pPreferences != nullptr );

	const QString sTmpPreferences =
		H2Core::Filesystem::tmp_file_path( "current-format-integrity.conf" );
	CPPUNIT_ASSERT( pPreferences->saveCopyAs( sTmpPreferences ) );

	H2TEST_ASSERT_PREFERENCES_FILES_EQUAL( sTestFile, sTmpPreferences );

	// Cleanup
	CPPUNIT_ASSERT( H2Core::Filesystem::rm( sTmpPreferences ) );
	___INFOLOG( "passed" );
}


bool XmlTest::checkSampleData( std::shared_ptr<H2Core::Drumkit> pKit, bool bLoaded )
{
	int count = 0;
	H2Core::InstrumentComponent::setMaxLayers( 16 );
	auto instruments = pKit->getInstruments();
	for( int i=0; i<instruments->size(); i++ ) {
		count++;
		auto pInstr = ( *instruments )[i];
		for ( const auto& pComponent : *pInstr->get_components() ) {
			for ( int nLayer = 0; nLayer < H2Core::InstrumentComponent::getMaxLayers(); nLayer++ ) {
				auto pLayer = pComponent->get_layer( nLayer );
				if( pLayer ) {
					auto pSample = pLayer->get_sample();
					if ( pSample == nullptr ) {
						return false;
					}
					if( bLoaded ) {
						if( pSample->get_data_l()==nullptr || pSample->get_data_r()==nullptr ) {
							return false;
						}
					} else {
						if( pSample->get_data_l() != nullptr || pSample->get_data_r() != nullptr ) {
							return false;
						}
					}
				}

			}
		}
	}
	return ( count==4 );
}
