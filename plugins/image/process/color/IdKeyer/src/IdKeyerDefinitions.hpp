#ifndef _TUTTLE_PLUGIN_IDKEYER_DEFINITIONS_HPP_
#define _TUTTLE_PLUGIN_IDKEYER_DEFINITIONS_HPP_

#include <tuttle/plugin/global.hpp>

#include <boost/lexical_cast.hpp>

namespace tuttle {
namespace plugin {
namespace idKeyer {

static const std::string kParamNbPoints = "nbPoints";
static const size_t      kMaxNbPoints   = 5;
static const std::string kParamColor    = "color";

inline std::string getColorParamName( const unsigned int i )
{
	return kParamColor + boost::lexical_cast<std::string>( i );
}

}
}
}

#endif
