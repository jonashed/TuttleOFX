#include "AVWriterPlugin.hpp"
#include "AVWriterProcess.hpp"
#include "AVWriterDefinitions.hpp"

#include <AvTranscoder/progress/NoDisplayProgress.hpp>

#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>

#include <cctype>
#include <sstream>
#include <utility>

namespace tuttle {
namespace plugin {
namespace av {
namespace writer {

AVWriterPlugin::AVWriterPlugin( OfxImageEffectHandle handle )
	: WriterPlugin( handle )
	, _paramAudioSubGroup()
	, _paramAudioSilent()
	, _paramAudioFilePath()
	, _paramAudioFileInfo()
	, _paramAudioStreamIndex()
	, _paramAudioPreset()
	, _paramFormatCustom()
	, _paramVideoCustom()
	, _paramAudioCustom()
	, _paramFormatDetailCustom()
	, _paramVideoDetailCustom()
	, _paramAudioDetailCustom()
	, _paramMetadatas()
	, _outputFile( NULL )
	, _transcoder( NULL )
	, _presets( true ) 
	, _lastOutputFilePath()
	, _outputFps( 0 )
	, _initVideo( false )
	, _initWrap( false )
{
	// We want to render a sequence
	setSequentialRender( true );

	// format
	_paramFormatCustomGroup = fetchGroupParam( kParamFormatCustomGroup );
	_paramFormat = fetchChoiceParam( kParamFormat );
	
	// video
	_paramVideoCustomGroup = fetchGroupParam( kParamVideoCustomGroup );
	_paramVideoCodec = fetchChoiceParam( kParamVideoCodec );
	
	// audio
	_paramAudioCustomGroup = fetchGroupParam( kParamAudioCustomGroup );
	_paramAudioCodec = fetchChoiceParam( kParamAudioCodec );
	
	_paramAudioNbInput = fetchIntParam( kParamAudioNbInputs );
	for( size_t indexAudioInput = 0; indexAudioInput < maxNbAudioInput; ++indexAudioInput )
	{
		std::ostringstream audioSubGroupName( kParamAudioSubGroup, std::ios_base::in | std::ios_base::ate );
		audioSubGroupName << "_" << indexAudioInput;
		_paramAudioSubGroup.push_back( fetchGroupParam( audioSubGroupName.str() ) );
		_paramAudioSubGroup.back()->setIsSecretAndDisabled( true );
		
		std::ostringstream audioSilentName( kParamAudioSilent, std::ios_base::in | std::ios_base::ate );
		audioSilentName << "_" << indexAudioInput;
		_paramAudioSilent.push_back( fetchBooleanParam( audioSilentName.str() ) );
		_paramAudioSilent.back()->setIsSecretAndDisabled( true );
		
		std::ostringstream audioFilePathName( kParamAudioFilePath, std::ios_base::in | std::ios_base::ate );
		audioFilePathName << "_" << indexAudioInput;
		_paramAudioFilePath.push_back( fetchStringParam( audioFilePathName.str() ) );
		_paramAudioFilePath.back()->setIsSecretAndDisabled( true );
		
		std::ostringstream audioFileNbStreamName( kParamAudioFileInfo, std::ios_base::in | std::ios_base::ate );
		audioFileNbStreamName << "_" << indexAudioInput;
		_paramAudioFileInfo.push_back( fetchStringParam( audioFileNbStreamName.str() ) );
		_paramAudioFileInfo.back()->setIsSecret( true );

		std::ostringstream audioSelectStreamName( kParamAudioSelectStream, std::ios_base::in | std::ios_base::ate );
		audioSelectStreamName << "_" << indexAudioInput;
		_paramAudioSelectStream.push_back( fetchBooleanParam( audioSelectStreamName.str() ) );
		_paramAudioSelectStream.back()->setIsSecretAndDisabled( true );

		std::ostringstream audioStreamIndexName( kParamAudioStreamIndex, std::ios_base::in | std::ios_base::ate );
		audioStreamIndexName << "_" << indexAudioInput;
		_paramAudioStreamIndex.push_back( fetchIntParam( audioStreamIndexName.str() ) );
		_paramAudioStreamIndex.back()->setIsSecretAndDisabled( true );
		
		std::ostringstream audioCodecPresetName( kParamAudioPreset, std::ios_base::in | std::ios_base::ate );
		audioCodecPresetName << "_" << indexAudioInput;
		_paramAudioPreset.push_back( fetchChoiceParam( audioCodecPresetName.str() ) );
		_paramAudioPreset.back()->setIsSecretAndDisabled( true );

		std::ostringstream audioOffsetName( kParamAudioOffset, std::ios_base::in | std::ios_base::ate );
		audioOffsetName << "_" << indexAudioInput;
		_paramAudioOffset.push_back( fetchIntParam( audioOffsetName.str() ) );
		_paramAudioOffset.back()->setIsSecretAndDisabled( true );
	}
	updateAudioParams();
	
	// our custom params
	_paramUseCustomFps = fetchBooleanParam( kParamUseCustomFps );
	_paramCustomFps = fetchDoubleParam( kParamCustomFps );
	_paramVideoPixelFormat = fetchChoiceParam( kParamVideoCodecPixelFmt );
	_paramAudioSampleFormat = fetchChoiceParam( kParamAudioCodecSampleFmt );
	
	// all custom params
	avtranscoder::OptionLoader::OptionArray optionsFormatArray = _optionLoader.loadFormatContextOptions( AV_OPT_FLAG_ENCODING_PARAM );
	_paramFormatCustom.fetchCustomParams( *this, optionsFormatArray, common::kPrefixFormat );
	
	avtranscoder::OptionLoader::OptionArray optionsVideoArray = _optionLoader.loadCodecContextOptions( AV_OPT_FLAG_ENCODING_PARAM | AV_OPT_FLAG_VIDEO_PARAM );
	_paramVideoCustom.fetchCustomParams( *this, optionsVideoArray, common::kPrefixVideo );
	
	avtranscoder::OptionLoader::OptionArray optionsAudioArray = _optionLoader.loadCodecContextOptions( AV_OPT_FLAG_ENCODING_PARAM | AV_OPT_FLAG_AUDIO_PARAM );
	_paramAudioCustom.fetchCustomParams( *this, optionsAudioArray, common::kPrefixAudio );
	
	avtranscoder::OptionLoader::OptionMap optionsFormatDetailMap = _optionLoader.loadOutputFormatOptions();
	_paramFormatDetailCustom.fetchCustomParams( *this, optionsFormatDetailMap, common::kPrefixFormat );
	const std::string formatName = _optionLoader.getFormatsShortNames().at( _paramFormat->getValue() );
	common::disableOFXParamsForFormatOrCodec( *this, optionsFormatDetailMap, formatName, common::kPrefixFormat );
	
	avtranscoder::OptionLoader::OptionMap optionsVideoCodecMap = _optionLoader.loadVideoCodecOptions();
	_paramVideoDetailCustom.fetchCustomParams( *this, optionsVideoCodecMap, common::kPrefixVideo );
	const std::string videoCodecName = _optionLoader.getVideoCodecsShortNames().at( _paramVideoCodec->getValue() );
	common::disableOFXParamsForFormatOrCodec( *this, optionsVideoCodecMap, videoCodecName, common::kPrefixVideo );
	
	avtranscoder::OptionLoader::OptionMap optionsAudioCodecMap = _optionLoader.loadAudioCodecOptions();
	_paramAudioDetailCustom.fetchCustomParams( *this, optionsAudioCodecMap, common::kPrefixAudio );
	const std::string audioCodecName = _optionLoader.getAudioCodecsShortNames().at( _paramAudioCodec->getValue() );
	common::disableOFXParamsForFormatOrCodec( *this, optionsAudioCodecMap, audioCodecName, common::kPrefixAudio );
	
	updatePixelFormats( videoCodecName );
	updateSampleFormats( audioCodecName );
	
	// preset
	_paramMainPreset = fetchChoiceParam( kParamMainPreset );
	_paramFormatPreset = fetchChoiceParam( kParamFormatPreset );
	_paramVideoPreset = fetchChoiceParam( kParamVideoPreset );
	_paramAudioMainPreset = fetchChoiceParam( kParamAudioMainPreset );
	
	// metadata
	_paramMetadatas.push_back( fetchStringParam( kParamMetaAlbum           ) );
	_paramMetadatas.push_back( fetchStringParam( kParamMetaAlbumArtist     ) );
	_paramMetadatas.push_back( fetchStringParam( kParamMetaArtist          ) );
	_paramMetadatas.push_back( fetchStringParam( kParamMetaComment         ) );
	_paramMetadatas.push_back( fetchStringParam( kParamMetaComposer        ) );
	_paramMetadatas.push_back( fetchStringParam( kParamMetaCopyright       ) );
	_paramMetadatas.push_back( fetchStringParam( kParamMetaCreationTime    ) );
	_paramMetadatas.push_back( fetchStringParam( kParamMetaDate            ) );
	_paramMetadatas.push_back( fetchStringParam( kParamMetaDisc            ) );
	_paramMetadatas.push_back( fetchStringParam( kParamMetaEncodedBy       ) );
	_paramMetadatas.push_back( fetchStringParam( kParamMetaFilename        ) );
	_paramMetadatas.push_back( fetchStringParam( kParamMetaGenre           ) );
	_paramMetadatas.push_back( fetchStringParam( kParamMetaLanguage        ) );
	_paramMetadatas.push_back( fetchStringParam( kParamMetaPerformer       ) );
	_paramMetadatas.push_back( fetchStringParam( kParamMetaPublisher       ) );
	_paramMetadatas.push_back( fetchStringParam( kParamMetaServiceName     ) );
	_paramMetadatas.push_back( fetchStringParam( kParamMetaServiceProvider ) );
	_paramMetadatas.push_back( fetchStringParam( kParamMetaTitle           ) );
	_paramMetadatas.push_back( fetchStringParam( kParamMetaTrack           ) );
	_paramMetadatas.push_back( fetchStringParam( kParamMetaVariantBitrate  ) );

	updateVisibleTools();
}

void AVWriterPlugin::updateVisibleTools()
{
	OFX::InstanceChangedArgs args( this->timeLineGetTime() );
	changedParam( args, kParamUseCustomFps );
}

AVProcessParams AVWriterPlugin::getProcessParams()
{
	AVProcessParams params;

	params._outputFilePath = _paramFilepath->getValue();
	
	params._format = _paramFormat->getValue();
	params._formatName = _optionLoader.getFormatsShortNames().at( params._format );
	
	params._videoCodec = _paramVideoCodec->getValue();
	params._videoCodecName = _optionLoader.getVideoCodecsShortNames().at( params._videoCodec );
	
	params._audioCodec = _paramAudioCodec->getValue();
	params._audioCodecName = _optionLoader.getAudioCodecsShortNames().at( params._audioCodec );

	try
	{
		std::vector<std::string> supportedPixelFormats( avtranscoder::OptionLoader::getPixelFormats( params._videoCodecName ) );
		params._videoPixelFormatName = supportedPixelFormats.at( _paramVideoPixelFormat->getValue() );
	}
	catch( const std::exception& e )
	{
		BOOST_THROW_EXCEPTION( exception::Failed()
		    << exception::user() + "unable to get supported pixel format choosen " + e.what() );
	}

	try
	{
		std::vector<std::string> supportedSampleFormats( avtranscoder::OptionLoader::getSampleFormats( params._audioCodecName ) );
		params._audioSampleFormatName = supportedSampleFormats.at( _paramAudioSampleFormat->getValue() );
	}
	catch( const std::exception& e )
	{
		BOOST_THROW_EXCEPTION( exception::Failed()
		    << exception::user() + "unable to get supported sample format choosen " + e.what() );
	}

	BOOST_FOREACH( OFX::StringParam* parameter, _paramMetadatas )
	{
		if( parameter->getValue().size() > 0 )
		{
			std::string ffmpegKey = parameter->getName();
			ffmpegKey.erase( 0, common::prefixSize );
			params._metadatas.push_back( std::pair<std::string, std::string>( ffmpegKey, parameter->getValue() ) );
		}
	}
	return params;
}

/**
 * @brief Update the list of pixel format supported depending on the video codec.
 * Warning: the function does not update the list correctly in Nuke.
 * @param videoCodecName
 */
void AVWriterPlugin::updatePixelFormats( const std::string& videoCodecName )
{
	_paramVideoPixelFormat->resetOptions();
	
	std::vector<std::string> pixelFormats( avtranscoder::OptionLoader::getPixelFormats( videoCodecName ) );
	// get all pixel formats if the list is empty with the video codec indicated
	if( pixelFormats.empty() )
	{
		pixelFormats = avtranscoder::OptionLoader::getPixelFormats();
	}
	
	for( std::vector<std::string>::iterator it = pixelFormats.begin(); it != pixelFormats.end(); ++it )
	{
		_paramVideoPixelFormat->appendOption( *it );
	}
}

/**
 * @brief Update the list of sample format supported depending on the audio codec.
 * Warning: the function does not update the list correctly in Nuke.
 * @param audioCodecName
 */
void AVWriterPlugin::updateSampleFormats( const std::string& audioCodecName )
{
	_paramAudioSampleFormat->resetOptions();
	
	std::vector<std::string> sampleFormats( _optionLoader.getSampleFormats( audioCodecName ) );
	// get all sample formats if the list is empty with the audio codec indicated
	if( sampleFormats.empty() )
	{
		sampleFormats = _optionLoader.getSampleFormats();
	}
	
	for( std::vector<std::string>::iterator it = sampleFormats.begin(); it != sampleFormats.end(); ++it )
	{
		_paramAudioSampleFormat->appendOption( *it );
	}
}

void AVWriterPlugin::updateAudioParams()
{
	for( size_t indexAudioOutput = 0; indexAudioOutput < maxNbAudioInput; ++indexAudioOutput )
	{
		bool isStreamConcerned = indexAudioOutput < (size_t)_paramAudioNbInput->getValue();
		_paramAudioSubGroup.at( indexAudioOutput )->setIsSecretAndDisabled( ! isStreamConcerned );
		_paramAudioSilent.at( indexAudioOutput )->setIsSecretAndDisabled( ! isStreamConcerned );
		_paramAudioFilePath.at( indexAudioOutput )->setIsSecretAndDisabled( ! isStreamConcerned );
		_paramAudioFileInfo.at( indexAudioOutput )->setIsSecret( ! isStreamConcerned );
		_paramAudioSelectStream.at( indexAudioOutput )->setIsSecretAndDisabled( ! isStreamConcerned );
		_paramAudioStreamIndex.at( indexAudioOutput )->setIsSecretAndDisabled( ! isStreamConcerned );
		_paramAudioPreset.at( indexAudioOutput )->setIsSecretAndDisabled( ! isStreamConcerned );
		_paramAudioOffset.at( indexAudioOutput )->setIsSecretAndDisabled( ! isStreamConcerned );

		updateAudioSilent( indexAudioOutput );
	}
}

void AVWriterPlugin::updateAudioSilent( size_t indexAudioOutput )
{
	if( _paramAudioSubGroup.at( indexAudioOutput )->getIsEnable() &&
		! _paramAudioSubGroup.at( indexAudioOutput )->getIsSecret() )
	{
		bool isSilent = _paramAudioSilent.at( indexAudioOutput )->getValue();
		_paramAudioFilePath.at( indexAudioOutput )->setIsSecretAndDisabled( isSilent );
		_paramAudioFileInfo.at( indexAudioOutput )->setIsSecret( isSilent );
		_paramAudioSelectStream.at( indexAudioOutput )->setIsSecretAndDisabled( isSilent );
		_paramAudioStreamIndex.at( indexAudioOutput )->setIsSecretAndDisabled( isSilent );
		_paramAudioOffset.at( indexAudioOutput )->setIsSecretAndDisabled( isSilent );
		_paramAudioPreset.at( indexAudioOutput )->setIsSecretAndDisabled( false );
	}
	updateAudioSelectStream( indexAudioOutput );
}


void AVWriterPlugin::updateAudioSelectStream( size_t indexAudioOutput )
{
	if( _paramAudioSubGroup.at( indexAudioOutput )->getIsEnable() &&
		! _paramAudioSubGroup.at( indexAudioOutput )->getIsSecret() &&
		! _paramAudioSilent.at( indexAudioOutput )->getValue() )
	{
		bool isSelectStream = _paramAudioSelectStream.at( indexAudioOutput )->getValue();
		_paramAudioStreamIndex.at( indexAudioOutput )->setIsSecretAndDisabled( ! isSelectStream );
		if( isSelectStream )
		{
			std::string inputFilePath = _paramAudioFilePath.at( indexAudioOutput )->getValue();
			if( boost::filesystem::exists( inputFilePath ) )
			{
				avtranscoder::NoDisplayProgress progress;
				avtranscoder::Properties properties = avtranscoder::InputFile::analyseFile( inputFilePath, progress, avtranscoder::InputFile::eAnalyseLevelFast );
				size_t nbAudioStream = properties.audioStreams.size();
				_paramAudioStreamIndex.at( indexAudioOutput )->setRange( 0, nbAudioStream );
				_paramAudioStreamIndex.at( indexAudioOutput )->setDisplayRange( 0, nbAudioStream );
			}
		}
	}
	updateAudioRewrap( indexAudioOutput );
}

void AVWriterPlugin::updateAudioRewrap( size_t indexAudioOutput )
{
	if( _paramAudioSubGroup.at( indexAudioOutput )->getIsEnable() &&
		! _paramAudioSubGroup.at( indexAudioOutput )->getIsSecret() &&
		! _paramAudioSilent.at( indexAudioOutput )->getValue() )
	{
		bool isRewrap = _paramAudioPreset.at( indexAudioOutput )->getValue() == 1;
		_paramAudioOffset.at( indexAudioOutput )->setIsSecretAndDisabled( isRewrap );
	}
}


void AVWriterPlugin::updateAudioFileInfo( size_t indexAudioOutput )
{
	if( _paramAudioSubGroup.at( indexAudioOutput )->getIsEnable() &&
		! _paramAudioSubGroup.at( indexAudioOutput )->getIsSecret() &&
		! _paramAudioSilent.at( indexAudioOutput )->getValue() )
	{
		std::string inputFilePath = _paramAudioFilePath.at( indexAudioOutput )->getValue();
		if( boost::filesystem::exists( inputFilePath ) )
		{
			avtranscoder::NoDisplayProgress progress;
			avtranscoder::Properties properties = avtranscoder::InputFile::analyseFile( inputFilePath, progress, avtranscoder::InputFile::eAnalyseLevelFast );
			size_t nbAudioStream = properties.audioStreams.size();
			std::string audioInfo = "Audio streams: ";
			audioInfo += boost::lexical_cast<std::string>( nbAudioStream );
			audioInfo += "\n";
			for( size_t audioStreamIndex = 0; audioStreamIndex < nbAudioStream; ++audioStreamIndex )
			{
				audioInfo += "Stream ";
				audioInfo += boost::lexical_cast<std::string>( audioStreamIndex );
				audioInfo += ": ";
				audioInfo += boost::lexical_cast<std::string>( properties.audioStreams.at( audioStreamIndex ).channels );
				audioInfo += " channels";
				audioInfo += "\n";
			}
			_paramAudioFileInfo.at( indexAudioOutput )->setValue( audioInfo );
		}
		else
		{
			_paramAudioFileInfo.at( indexAudioOutput )->setValue( "" );
		}
	}
}
	

void AVWriterPlugin::changedParam( const OFX::InstanceChangedArgs& args, const std::string& paramName )
{
	WriterPlugin::changedParam( args, paramName );
	
	if( paramName == kTuttlePluginFilename )
	{
		const std::string& extension = avtranscoder::getFormat( _paramFilepath->getValue() );
		std::vector<std::string>::iterator itFormat = std::find( _optionLoader.getFormatsShortNames().begin(), _optionLoader.getFormatsShortNames().end(), extension );
		if( itFormat != _optionLoader.getFormatsShortNames().end() )
		{
			size_t indexFormat = itFormat - _optionLoader.getFormatsShortNames().begin();
			_paramFormat->setValue( indexFormat );
		}
		cleanVideoAndAudio();
	}
	else if( paramName == kParamFormat )
	{
		avtranscoder::OptionLoader::OptionMap optionsFormatMap = _optionLoader.loadOutputFormatOptions();
		const std::string formatName = _optionLoader.getFormatsShortNames().at( _paramFormat->getValue() );
		common::disableOFXParamsForFormatOrCodec( *this, optionsFormatMap, formatName, common::kPrefixFormat );
	}
	else if( paramName == kParamVideoCodec )
	{
		avtranscoder::OptionLoader::OptionMap optionsVideoCodecMap = _optionLoader.loadVideoCodecOptions();
		const std::string videoCodecName = _optionLoader.getVideoCodecsShortNames().at( _paramVideoCodec->getValue() );
		common::disableOFXParamsForFormatOrCodec( *this, optionsVideoCodecMap, videoCodecName, common::kPrefixVideo );
		
		updatePixelFormats( videoCodecName );
	}
	else if( paramName == kParamAudioCodec )
	{
		avtranscoder::OptionLoader::OptionMap optionsAudioCodecMap = _optionLoader.loadAudioCodecOptions();
		const std::string audioCodecName = _optionLoader.getAudioCodecsShortNames().at(_paramAudioCodec->getValue() );
		common::disableOFXParamsForFormatOrCodec( *this, optionsAudioCodecMap, audioCodecName, common::kPrefixAudio );
		
		updateSampleFormats( audioCodecName );
	}
	else if( paramName == kParamMainPreset )
	{
		if( _paramMainPreset->getValue() == 0 )
			return;
	}
	else if( paramName == kParamFormatPreset )
	{
		// if custom format preset
		if( _paramFormatPreset->getValue() == 0 )
		{
			int defaultFormatIndex;
			_paramFormat->getDefault( defaultFormatIndex );
			_paramFormat->setValue( defaultFormatIndex );
		}
		else
		{
			updateFormatFromExistingProfile();
		}
	}
	else if( paramName == kParamVideoPreset )
	{
		// if custom video preset
		if( _paramVideoPreset->getValue() == 0 )
		{	
			int defaultVideoCodecIndex;
			_paramVideoCodec->getDefault( defaultVideoCodecIndex );
			_paramVideoCodec->setValue( defaultVideoCodecIndex );
		}
		else
		{
			updateVideoFromExistingProfile();
		}
	}
	else if( paramName == kParamAudioMainPreset )
	{
		// if custom audio preset
		if( _paramAudioMainPreset->getValue() == 0 )
		{
			int defaultAudioCodecIndex;
			_paramAudioCodec->getDefault( defaultAudioCodecIndex );
			_paramAudioCodec->setValue( defaultAudioCodecIndex );
		}
		else
		{
			updateAudiotFromExistingProfile();
		}
	}
	else if( paramName == kParamUseCustomFps )
	{
		_paramCustomFps->setIsSecretAndDisabled( ! _paramUseCustomFps->getValue() );
	}
	else if( paramName == kParamCustomFps )
	{
		if( ! _paramUseCustomFps->getValue() )
		{
			_paramUseCustomFps->setValue(true);
		}
	}
	else if( paramName == kParamAudioNbInputs )
	{
		updateAudioParams();
	}
	else if( paramName.find( kParamAudioSilent ) != std::string::npos )
	{
		const size_t indexPos = kParamAudioSilent.size() + 1; // add "_"
		size_t indexAudioOutput = boost::lexical_cast<size_t>( paramName.substr( indexPos ) );
		updateAudioSilent( indexAudioOutput );
	}
	else if( paramName.find( kParamAudioSelectStream ) != std::string::npos )
	{
		const size_t indexPos = kParamAudioSelectStream.size() + 1; // add "_"
		size_t indexAudioOutput = boost::lexical_cast<size_t>( paramName.substr( indexPos ) );
		updateAudioSelectStream( indexAudioOutput );
	}
	else if( paramName.find( kParamAudioPreset ) != std::string::npos )
	{
		const size_t indexPos = kParamAudioPreset.size() + 1; // add "_"
		size_t indexAudioOutput = boost::lexical_cast<size_t>( paramName.substr( indexPos ) );
		updateAudioRewrap( indexAudioOutput );
	}
	else if( paramName.find( kParamAudioFilePath ) != std::string::npos )
	{
		const size_t indexPos = kParamAudioFilePath.size() + 1; // add "_"
		const size_t indexAudioOutput = boost::lexical_cast<size_t>( paramName.substr( indexPos ) );
		_paramAudioSilent.at( indexAudioOutput )->setValue( false );
		
		updateAudioFileInfo( indexAudioOutput );
	}
	else if( paramName.find( kParamAudioStreamIndex ) != std::string::npos )
	{
		const size_t indexPos = kParamAudioStreamIndex.size() + 1; // add "_"
		const size_t indexAudioOutput = boost::lexical_cast<size_t>( paramName.substr( indexPos ) );
		_paramAudioSelectStream.at( indexAudioOutput )->setValue( true );
	}
}

void AVWriterPlugin::getClipPreferences( OFX::ClipPreferencesSetter& clipPreferences )
{
	// Need to be computed at each frame
	clipPreferences.setOutputFrameVarying( true );
}

void AVWriterPlugin::initOutput()
{
	AVProcessParams params = getProcessParams();
	
	// create output file
	try
	{
		_outputFile.reset( new avtranscoder::OutputFile( params._outputFilePath ) );

		// update format depending on the output file extension
		boost::filesystem::path path( params._outputFilePath );
		std::string formatFromFile = avtranscoder::getFormat( path.filename().string() );
		if( ! formatFromFile.empty() )
		{
			std::vector<std::string> formats = _optionLoader.getFormatsShortNames();
			std::vector<std::string>::iterator iterFormat = std::find( formats.begin(), formats.end(), formatFromFile );
			size_t formatIndex = std::distance( formats.begin(), iterFormat);
			if( formatIndex != formats.size() )
			{
				_paramFormat->setValue( formatIndex );
			}
		}

		// Get format profile
		avtranscoder::Profile::ProfileDesc profile;
		profile[ avtranscoder::Profile::avProfileIdentificator ] = "customFormatPreset";
		profile[ avtranscoder::Profile::avProfileIdentificatorHuman ] = "Custom format preset";
		profile[ avtranscoder::Profile::avProfileType ] = avtranscoder::Profile::avProfileTypeFormat;

		AVProcessParams params = getProcessParams();
		profile[ avtranscoder::Profile::avProfileFormat ] = params._formatName;

		avtranscoder::Profile::ProfileDesc formatDesc = _paramFormatCustom.getCorrespondingProfileDesc();
		profile.insert( formatDesc.begin(), formatDesc.end() );

		avtranscoder::Profile::ProfileDesc formatDetailDesc = _paramFormatDetailCustom.getCorrespondingProfileDesc( params._formatName );
		profile.insert( formatDetailDesc.begin(), formatDetailDesc.end() );

		_outputFile->setProfile( profile );

		_outputFile->addMetadata( params._metadatas );

		_transcoder.reset( new avtranscoder::Transcoder( *_outputFile ) );
	}
	catch( std::exception& e )
	{
		_initVideo = false;
		BOOST_THROW_EXCEPTION( exception::Failed()
		    << exception::user() + "unable to open output file: " + e.what() );
	}
}

void AVWriterPlugin::ensureVideoIsInit( const OFX::RenderArguments& args )
{
	AVProcessParams params = getProcessParams();
	
	// ouput file path already set
	if( _lastOutputFilePath != "" && _lastOutputFilePath == params._outputFilePath )
		return;
	
	if( params._outputFilePath == "" ) // no output file indicated
	{
		_initVideo = false;
		BOOST_THROW_EXCEPTION( exception::Failed()
		    << exception::user() + "no output file indicated"
		    << exception::filename( params._outputFilePath ) );
	}
	
	if( ! isOutputInit() )
	{
		_initVideo = false;
		BOOST_THROW_EXCEPTION( exception::Failed()
		    << exception::user() + "the output is not init. Can't process." );
	}
	
	_lastOutputFilePath = params._outputFilePath;
	
	// create video stream
	try
	{
		// Get video profile
		avtranscoder::Profile::ProfileDesc profile;
		profile[ avtranscoder::Profile::avProfileIdentificator ] = "customVideoPreset";
		profile[ avtranscoder::Profile::avProfileIdentificatorHuman ] = "Custom video preset";
		profile[ avtranscoder::Profile::avProfileType ] = avtranscoder::Profile::avProfileTypeVideo;

		AVProcessParams params = getProcessParams();
		profile[ avtranscoder::Profile::avProfileCodec ] = params._videoCodecName;
		profile[ avtranscoder::Profile::avProfilePixelFormat ] = params._videoPixelFormatName;

		if( _paramUseCustomFps->getValue() )
			profile[ avtranscoder::Profile::avProfileFrameRate ] = boost::to_string( _paramCustomFps->getValue() );
		else
			profile[ avtranscoder::Profile::avProfileFrameRate ] = boost::to_string( _clipSrc->getFrameRate() );

		profile[ "aspect" ] = boost::to_string( _clipSrc->getPixelAspectRatio() );

		avtranscoder::Profile::ProfileDesc videoDesc = _paramVideoCustom.getCorrespondingProfileDesc();
		profile.insert( videoDesc.begin(), videoDesc.end() );

		avtranscoder::Profile::ProfileDesc videoDetailDesc = _paramVideoDetailCustom.getCorrespondingProfileDesc( params._videoCodecName );
		profile.insert( videoDetailDesc.begin(), videoDetailDesc.end() );

		const OfxRectI bounds = _clipSrc->getPixelRod( args.time, args.renderScale );
		int width = bounds.x2 - bounds.x1;
		int height = bounds.y2 - bounds.y1;

		// describe the input of transcode
		avtranscoder::VideoFrameDesc imageDesc;
		imageDesc.setWidth( width );
		imageDesc.setHeight( height );
		imageDesc.setDar( width, height );

		avtranscoder::Pixel inputPixel;
		inputPixel.setColorComponents( avtranscoder::eComponentRgb );
		inputPixel.setPlanar( false );

		imageDesc.setPixel( inputPixel );

		avtranscoder::VideoDesc inputVideoDesc = avtranscoder::VideoDesc( profile[ avtranscoder::Profile::avProfileCodec ] );
		inputVideoDesc.setImageParameters( imageDesc );

		_generatorVideo.setVideoDesc( inputVideoDesc );

		// the streamTranscoder is deleted by avTranscoder
		avtranscoder::StreamTranscoder* stream = new avtranscoder::StreamTranscoder( _generatorVideo, *_outputFile, profile );
		_transcoder->add( *stream );

		_outputFps = boost::lexical_cast<double>( profile[ avtranscoder::Profile::avProfileFrameRate ] );
	}	
	catch( std::exception& e )
	{
		_initVideo = false;
		BOOST_THROW_EXCEPTION( exception::Failed()
		    << exception::user() + "unable to create video stream: " + e.what() );
	}
	_initVideo = true;
}

void AVWriterPlugin::initAudio()
{
	// no audio stream specified
	if( ! _paramAudioNbInput->getValue() )
		return;

	if( ! isOutputInit() )
	{
		_initVideo = false;
		BOOST_THROW_EXCEPTION( exception::Failed()
		    << exception::user() + "the output is not init. Can't process." );
	}
	
	// create audio streams
	try
	{
		size_t mainPresetIndex = _paramAudioMainPreset->getValue();
		avtranscoder::Profile::ProfileDesc profile;
		std::string presetName( "" );
		
		for( int i = 0; i < _paramAudioNbInput->getValue(); ++i )
		{
			std::string inputFileName( _paramAudioFilePath.at( i )->getValue() );
			size_t presetIndex = _paramAudioPreset.at( i )->getValue();
			bool isSilent = _paramAudioSilent.at( i )->getValue();
			
			// No effect if file path is empty
			if( ! isSilent && inputFileName.empty() )
				continue;

			// main audio preset
			if( presetIndex == 0 && mainPresetIndex == 0 )
			{
				profile[ avtranscoder::Profile::avProfileIdentificator ] = "customAudioPreset";
				profile[ avtranscoder::Profile::avProfileIdentificatorHuman ] = "Custom audio preset";
				profile[ avtranscoder::Profile::avProfileType ] = avtranscoder::Profile::avProfileTypeAudio;

				AVProcessParams params = getProcessParams();
				profile[ avtranscoder::Profile::avProfileCodec ] = params._audioCodecName;
				profile[ avtranscoder::Profile::avProfileSampleFormat ] = params._audioSampleFormatName;

				avtranscoder::Profile::ProfileDesc audioDesc = _paramAudioCustom.getCorrespondingProfileDesc();
				profile.insert( audioDesc.begin(), audioDesc.end() );

				avtranscoder::Profile::ProfileDesc audioDetailDesc = _paramAudioDetailCustom.getCorrespondingProfileDesc( params._audioCodecName );
				profile.insert( audioDetailDesc.begin(), audioDetailDesc.end() );
			}
			// Rewrap
			else if( presetIndex == 1 )
			{
				presetName = "";
			}
			// specific audio preset
			else
			{
				// at( presetIndex - 2 ): subtract the index of main preset and rewrap
				profile = _presets.getAudioProfiles().at( presetIndex - 2 );
				presetName = profile.find( avtranscoder::Profile::avProfileIdentificator )->second;
			}
			
			// generator
			if( isSilent )
			{	
				avtranscoder::AudioDesc audioDesc( profile[ avtranscoder::Profile::avProfileCodec ] );
				const size_t sampleRate = boost::lexical_cast<size_t>( profile[ avtranscoder::Profile::avProfileSampleRate ] );
				const size_t channels = boost::lexical_cast<size_t>( profile[ avtranscoder::Profile::avProfileChannel ] );
				audioDesc.setAudioParameters( 
					sampleRate, 
					channels, 
					avtranscoder::OptionLoader::getAVSampleFormat( profile[ avtranscoder::Profile::avProfileSampleFormat ] ) );
				
				_transcoder->add( "", 1, profile, audioDesc );
			}
			else
			{
				bool selectOneStream = _paramAudioSelectStream.at( i )->getValue();
				int inputStreamIndex = selectOneStream ? _paramAudioStreamIndex.at( i )->getValue() : -1;				

				// Get number of audio stream
				size_t nbAudioStream = 1;
				if( inputStreamIndex == -1 )
				{
					avtranscoder::NoDisplayProgress progress;
					avtranscoder::Properties properties = avtranscoder::InputFile::analyseFile( inputFileName, progress, avtranscoder::InputFile::eAnalyseLevelFast );
					nbAudioStream = properties.audioStreams.size();
				}
				
				// rewrap
				if( ! isSilent && presetIndex == 1 )
				{
					if( inputStreamIndex != -1 )
					{
						_transcoder->add( inputFileName, inputStreamIndex, presetName );
					}
					else
					{
						for( size_t streamIndex = 0; streamIndex < nbAudioStream; ++streamIndex )
						{
							_transcoder->add( inputFileName, streamIndex, presetName );
						}
					}
				}
				// transcode
				else
				{
					// select all channels of the stream
					size_t subStream = -1;
					size_t offsetMillisecond = _paramAudioOffset.at( i )->getValue();
					size_t offsetFrame = ( offsetMillisecond * _outputFps ) / 1000;

					// custom audio preset
					if( presetIndex == 0 && mainPresetIndex == 0 )
					{
						if( inputStreamIndex != -1 )
						{
							_transcoder->add( inputFileName, inputStreamIndex, subStream, profile, offsetFrame );
						}
						else
						{
							for( size_t streamIndex = 0; streamIndex < nbAudioStream; ++streamIndex )
							{
								_transcoder->add( inputFileName, streamIndex, subStream, profile, offsetFrame );
							}
						}
					}
					// existing audio preset
					else
					{
						if( inputStreamIndex != -1 )
						{
							_transcoder->add( inputFileName, inputStreamIndex, subStream, presetName, offsetFrame );
						}
						else
						{
							for( size_t streamIndex = 0; streamIndex < nbAudioStream; ++streamIndex )
							{
								_transcoder->add( inputFileName, streamIndex, subStream, presetName, offsetFrame );
							}
						}
					}
				}
			}
		}
	}
	catch( std::exception& e )
	{
		BOOST_THROW_EXCEPTION( exception::Failed()
		    << exception::user() + "unable to create audio stream: " + e.what() );
	}
}

bool AVWriterPlugin::isOutputInit()
{
	if( _outputFile.get() == NULL ||
		_transcoder.get() == NULL )
		return false;
	return true;
}

void AVWriterPlugin::updateFormatFromExistingProfile()
{
	size_t presetIndex = _paramFormatPreset->getValue();
	
	// existing format preset
	if( presetIndex != 0 )
	{
		avtranscoder::Profile::ProfileDesc existingProfile = _presets.getFormatProfiles().at( presetIndex - 1 );

		// format
		std::vector<std::string> formats = _optionLoader.getFormatsShortNames();
		std::vector<std::string>::iterator iterFormat = std::find( formats.begin(), formats.end(), existingProfile[ avtranscoder::Profile::avProfileFormat ] );
		size_t fomatIndex = std::distance( formats.begin(), iterFormat );
		if( fomatIndex != formats.size() )
		{
			_paramFormat->setValue( fomatIndex );
		}

		// other options
		BOOST_FOREACH( avtranscoder::Profile::ProfileDesc::value_type& option, existingProfile )
		{
			if( option.first == avtranscoder::Profile::avProfileIdentificator ||
				option.first == avtranscoder::Profile::avProfileIdentificatorHuman ||
				option.first == avtranscoder::Profile::avProfileType )
				continue;

			_paramFormatCustom.setOption( option.first, option.second );
		}
	}
}

void AVWriterPlugin::updateVideoFromExistingProfile()
{
	size_t presetIndex = _paramVideoPreset->getValue();
	
	// existing video preset
	if( presetIndex != 0 )
	{
		avtranscoder::Profile::ProfileDesc existingProfile = _presets.getVideoProfiles().at( presetIndex - 1 );

		// video codec
		std::vector<std::string> codecs = _optionLoader.getVideoCodecsShortNames();
		std::vector<std::string>::iterator iterCodec = std::find( codecs.begin(), codecs.end(), existingProfile[ avtranscoder::Profile::avProfileCodec ] );
		size_t codecIndex = std::distance( codecs.begin(), iterCodec);
		if( codecIndex != codecs.size() )
		{
			_paramVideoCodec->setValue( codecIndex );
		}

		// pixel format
		std::vector<std::string> pixelFormats = _optionLoader.getPixelFormats( *iterCodec );
		std::vector<std::string>::iterator iterPixelFormat = std::find( pixelFormats.begin(), pixelFormats.end(), existingProfile[ avtranscoder::Profile::avProfilePixelFormat ] );
		size_t pixelFomatIndex = std::distance( pixelFormats.begin(), iterPixelFormat);
		if( pixelFomatIndex != pixelFormats.size() )
		{
			_paramVideoPixelFormat->setValue( pixelFomatIndex );
		}

		// frame rate
		_paramCustomFps->setValue( boost::lexical_cast<double>( existingProfile[ avtranscoder::Profile::avProfileFrameRate ] ) );

		// other options
		BOOST_FOREACH( avtranscoder::Profile::ProfileDesc::value_type& option, existingProfile )
		{
			if( option.first == avtranscoder::Profile::avProfileIdentificator ||
				option.first == avtranscoder::Profile::avProfileIdentificatorHuman ||
				option.first == avtranscoder::Profile::avProfileType ||
				option.first == avtranscoder::Profile::avProfileCodec ||
				option.first == avtranscoder::Profile::avProfilePixelFormat ||
				option.first == avtranscoder::Profile::avProfileFrameRate )
				continue;

			_paramVideoCustom.setOption( option.first, option.second );
		}
	}
}

void AVWriterPlugin::updateAudiotFromExistingProfile()
{
	size_t presetIndex = _paramAudioMainPreset->getValue();
	
	// existing audio preset
	if( presetIndex != 0 )
	{
		avtranscoder::Profile::ProfileDesc existingProfile = _presets.getAudioProfiles().at( presetIndex - 1 );

		// audio codec
		std::vector<std::string> codecs = _optionLoader.getAudioCodecsShortNames();
		std::vector<std::string>::iterator iterCodec = std::find( codecs.begin(), codecs.end(), existingProfile[ avtranscoder::Profile::avProfileCodec ] );
		size_t codecIndex = std::distance( codecs.begin(), iterCodec);
		if( codecIndex != codecs.size() )
		{
			_paramAudioCodec->setValue( codecIndex );
		}

		// sample format
		std::vector<std::string> sampleFormats = _optionLoader.getSampleFormats( *iterCodec );
		std::vector<std::string>::iterator iterSampleFormat = std::find( sampleFormats.begin(), sampleFormats.end(), existingProfile[ avtranscoder::Profile::avProfileSampleFormat ] );
		size_t sampleFomatIndex = std::distance( sampleFormats.begin(), iterSampleFormat);
		if( sampleFomatIndex != sampleFormats.size() )
		{
			_paramAudioSampleFormat->setValue( sampleFomatIndex );
		}

		// other options
		BOOST_FOREACH( avtranscoder::Profile::ProfileDesc::value_type& option, existingProfile )
		{
			if( option.first == avtranscoder::Profile::avProfileIdentificator ||
				option.first == avtranscoder::Profile::avProfileIdentificatorHuman ||
				option.first == avtranscoder::Profile::avProfileType ||
				option.first == avtranscoder::Profile::avProfileCodec ||
				option.first == avtranscoder::Profile::avProfileSampleFormat )
				continue;

			_paramAudioCustom.setOption( option.first, option.second );
		}
	}
}

void AVWriterPlugin::cleanVideoAndAudio()
{
	_outputFile.reset();
	_transcoder.reset();
	
	_initVideo = false;
	_initWrap = false;
	
	_lastOutputFilePath = "";
}

/**
 * @brief The overridden begin render function
 * @param[in]   args     Begin Rendering parameters
 */
void AVWriterPlugin::beginSequenceRender( const OFX::BeginSequenceRenderArguments& args )
{
	// Before new render
	cleanVideoAndAudio();
	
	initOutput();
}

/**
 * @brief The overridden render function
 * @param[in]   args     Rendering parameters
 */
void AVWriterPlugin::render( const OFX::RenderArguments& args )
{
	WriterPlugin::render( args );

	ensureVideoIsInit( args );

	if( ! _initWrap )
	{
		initAudio();
		_transcoder->setProcessMethod( avtranscoder::eProcessMethodInfinity );
		_outputFile->beginWrap();
		_initWrap = true;
	}
	
	doGilRender<AVWriterProcess>( *this, args );
}

void AVWriterPlugin::endSequenceRender( const OFX::EndSequenceRenderArguments& args )
{	
	if( ! _initWrap || ! _initVideo )
		return;
	
	_outputFile->endWrap();
}

}
}
}
}
