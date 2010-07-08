#include "OpenImageIOWriterDefinitions.hpp"
#include "OpenImageIOWriterPlugin.hpp"

#include <tuttle/plugin/image/gil/globals.hpp>
#include <tuttle/plugin/PluginException.hpp>

#include <OpenImageIO/imageio.h>

#include <boost/gil/gil_all.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/mpl/map.hpp>
#include <boost/mpl/integral_c.hpp>
#include <boost/mpl/at.hpp>

#include <cmath>
#include <vector>

namespace tuttle {
namespace plugin {
namespace openImageIO {
namespace writer {

template<class View>
OpenImageIOWriterProcess<View>::OpenImageIOWriterProcess( OpenImageIOWriterPlugin& instance )
	: ImageGilFilterProcessor<View>( instance ),
	_plugin( instance )
{
	this->setNoMultiThreading();
}

/**
 * @brief Function called by rendering thread each time a process must be done.
 * @param[in] procWindowRoW  Processing window in RoW
 * @warning no multithread here !
 */
template<class View>
void OpenImageIOWriterProcess<View>::multiThreadProcessImages( const OfxRectI& procWindowRoW )
{
	BOOST_ASSERT( procWindowRoW == this->_srcPixelRod );
	using namespace boost::gil;
	try
	{
		OpenImageIOWriterProcessParams params = _plugin.getParams(this->_renderArgs.time);
		writeImage( this->_srcView, params._filepath, params._bitDepth );
		copy_pixels( this->_srcView, this->_dstView );
	}
	catch( tuttle::plugin::PluginException& e )
	{
		COUT_EXCEPTION( e );
	}
}

/**
 * 
 */
template<class View>
void OpenImageIOWriterProcess<View>::writeImage( const View& src, const std::string& filepath, const TypeDesc bitDepth )
{
	using namespace boost;
	using namespace OpenImageIO;
	boost::scoped_ptr<ImageOutput> out( ImageOutput::create( filepath ) );
	ImageSpec spec( src.width(), src.height(), gil::num_channels<View>::value, bitDepth );
	out->open( filepath, spec );

	typedef mpl::map<
      mpl::pair<gil::bits8, mpl::integral_c<TypeDesc::BASETYPE,TypeDesc::UINT8> >
    , mpl::pair<gil::bits16, mpl::integral_c<TypeDesc::BASETYPE,TypeDesc::UINT16> >
    , mpl::pair<gil::bits32, mpl::integral_c<TypeDesc::BASETYPE,TypeDesc::UINT32> >
    , mpl::pair<gil::bits32f, mpl::integral_c<TypeDesc::BASETYPE,TypeDesc::FLOAT> >
    > MapBits;
	typedef typename gil::channel_type<View>::type ChannelType;

	out->write_image( mpl::at<MapBits, ChannelType>::type::value, &((*src.begin())[0]) ); // get the adress of the first channel value from the first pixel
	out->close();
}

}
}
}
}