#ifndef _TUTTLE_HOST_PROCESSGRAPH_HPP_
#define _TUTTLE_HOST_PROCESSGRAPH_HPP_

#include <tuttle/host/Graph.hpp>

#include <string>

#define PROCESSGRAPH_USE_LINK

namespace tuttle {
namespace host {
namespace graph {

class ProcessGraph
{
public:
	typedef Graph::Node Node; /// @todo tuttle ProcessNode...
	typedef Graph::Vertex Vertex;
	typedef Graph::Edge Edge;
	typedef Graph::Attribute Attribute;
	typedef InternalGraph<Vertex, Edge, boost::vecS, boost::vecS> InternalGraphImpl;
	typedef Graph::vertex_descriptor vertex_descriptor;
	typedef Graph::edge_descriptor edge_descriptor;
#ifdef PROCESSGRAPH_USE_LINK
	typedef std::map<std::string, Node*> NodeMap;
#else
	typedef Graph::NodeMap NodeMap;
#endif
	typedef Graph::InstanceCountMap InstanceCountMap;

public:
	ProcessGraph( Graph& graph, const std::list<std::string>& nodes ); ///@ todo: const Graph, no ?
	~ProcessGraph();

	memory::MemoryCache process( const int tBegin, const int tEnd );

private:
	void relink();

	InternalGraphImpl _graph;
	NodeMap _nodes;
	InstanceCountMap _instanceCount;

	static const std::string _outputId;
};

}
}
}

#endif

