#include "ParamRGBA.hpp"

namespace tuttle {
namespace host {
namespace core {

ParamRGBA::ParamRGBA( ImageEffectNode& effect,
                              const std::string& name,
                              const ofx::attribute::OfxhParamDescriptor& descriptor )
	: ofx::attribute::OfxhMultiDimParam<ParamDouble, 4>( descriptor, name, effect ),
	_effect( effect )

{
	_controls.push_back( new ParamDouble( effect, name + ".r", descriptor, 0 ) );
	_controls.push_back( new ParamDouble( effect, name + ".g", descriptor, 1 ) );
	_controls.push_back( new ParamDouble( effect, name + ".b", descriptor, 2 ) );
	_controls.push_back( new ParamDouble( effect, name + ".a", descriptor, 3 ) );
}

OfxRGBAColourD ParamRGBA::getDefault() const
{
	OfxRGBAColourD rgb;
	rgb.r = _controls[0]->getDefault();
	rgb.g = _controls[1]->getDefault();
	rgb.b = _controls[2]->getDefault();
	rgb.a = _controls[3]->getDefault();
	return rgb;
}

void ParamRGBA::get( double& r, double& g, double& b, double& a ) const OFX_EXCEPTION_SPEC
{
	_controls[0]->get(r);
	_controls[1]->get(g);
	_controls[2]->get(b);
	_controls[3]->get(a);
}

void ParamRGBA::get( const OfxTime time, double& r, double& g, double& b, double& a ) const OFX_EXCEPTION_SPEC
{
	_controls[0]->get(time, r);
	_controls[1]->get(time, g);
	_controls[2]->get(time, b);
	_controls[3]->get(time, a);
}

void ParamRGBA::set( const double &r, const double &g, const double &b, const double &a, const ofx::attribute::EChange change ) OFX_EXCEPTION_SPEC
{
	_controls[0]->set(r, ofx::attribute::eChangeNone);
	_controls[1]->set(g, ofx::attribute::eChangeNone);
	_controls[2]->set(b, ofx::attribute::eChangeNone);
	_controls[3]->set(a, ofx::attribute::eChangeNone);
	this->paramChanged( change );
}

void ParamRGBA::set( const OfxTime time, const double &r, const double &g, const double &b, const double &a, const ofx::attribute::EChange change ) OFX_EXCEPTION_SPEC
{
	_controls[0]->set(time, r, ofx::attribute::eChangeNone);
	_controls[1]->set(time, g, ofx::attribute::eChangeNone);
	_controls[2]->set(time, b, ofx::attribute::eChangeNone);
	_controls[3]->set(time, a, ofx::attribute::eChangeNone);
	this->paramChanged( change );
}

}
}
}

