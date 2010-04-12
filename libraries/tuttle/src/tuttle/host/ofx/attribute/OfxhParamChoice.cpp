#include "OfxhParamChoice.hpp"

#include <boost/numeric/conversion/cast.hpp>

namespace tuttle {
namespace host {
namespace ofx {
namespace attribute {


int OfxhParamChoice::getIndexFor( const std::string& key ) const
{
	typedef std::vector<std::string> StringVector;
	const StringVector& values = this->getProperties().fetchStringProperty(kOfxParamPropChoiceOption).getValues();
	StringVector::const_iterator itValue = std::find( values.begin(), values.end(), key );
	if( itValue == values.end() )
	{
	    std::string errorMsg( std::string("The key \"") + key + "\" doesn't exist for choice param \"" + this->getName() + "\".\n" );
	    errorMsg += "Correct values are : [";
	    for( StringVector::const_iterator it = values.begin(), itEnd = values.end();
		     it != itEnd;
		     ++it )
		{
	        errorMsg +=  *it + ", ";
		}
	    errorMsg += "]";	    
		throw( OfxhException( errorMsg ) ); // @todo tuttle: use boost::exception to allows to easily write error message !
	}
	return boost::numeric_cast<int>( std::distance( values.begin(), itValue ) );
}

/**
 * implementation of var args function
 */
void OfxhParamChoice::getV( va_list arg ) const OFX_EXCEPTION_SPEC
{
	int* value = va_arg( arg, int* );

	return get( *value );
}

/**
 * implementation of var args function
 */
void OfxhParamChoice::getV( const OfxTime time, va_list arg ) const OFX_EXCEPTION_SPEC
{
	int* value = va_arg( arg, int* );

	return get( time, *value );
}

/**
 * implementation of var args function
 */
void OfxhParamChoice::setV( va_list arg ) OFX_EXCEPTION_SPEC
{
	int value = va_arg( arg, int );

	return set( value );
}

/**
 * implementation of var args function
 */
void OfxhParamChoice::setV( const OfxTime time, va_list arg ) OFX_EXCEPTION_SPEC
{
	int value = va_arg( arg, int );

	return set( time, value );
}



}
}
}
}
