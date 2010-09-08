#include "ProcessGraph.hpp"
#include "ProcessVisitors.hpp"
#include <tuttle/host/graph/GraphExporter.hpp>

#include <boost/foreach.hpp>

namespace tuttle {
namespace host {
namespace graph {

const std::string ProcessGraph::_outputId( "TUTTLE_FAKE_OUTPUT" );

ProcessGraph::ProcessGraph( Graph& graph, const std::list<std::string>& outputNodes )
	: _instanceCount( graph.getInstanceCount() )
{
	_graph.copyTransposed( graph.getGraph() );

	Vertex outputVertex( _outputId );
	_graph.addVertex( outputVertex );

	BOOST_FOREACH( const std::string & s, outputNodes )
	{
		_graph.connect( _outputId, s, "Output" );
		COUT_DEBUG( "MY OUTPUT: " << s );
	}
	relink();
}

ProcessGraph::~ProcessGraph()
{}

/**
 * @brief After copying Vertices, we need to duplicate Nodes and relink Vertices with new Nodes.
 */
void ProcessGraph::relink()
{
	_graph.removeUnconnectedVertices( _graph.getVertexDescriptor( _outputId ) );

	InternalGraphImpl::vertex_range_t vrange = _graph.getVertices();
	for( InternalGraphImpl::vertex_iterator it = vrange.first; it != vrange.second; ++it )
	{
		graph::Vertex& v = _graph.instance( *it );

		// fake node has no ProcessNode
		if( !v.isFake() )
		{
			const tuttle::host::INode* const origNode = v.getProcessNode(); // pointer of the copied graph, we don't owns it !
			std::string key( origNode->getName() );
			NodeMap::iterator it = _nodes.find( key );
			tuttle::host::INode* newNode;
			if( it != _nodes.end() )
			{
				newNode = it->second;
			}
			else
			{
				newNode = origNode->clone();
				/// @todo tuttle: no dynamic_cast here, _nodes must use tuttle::host::Node
				_nodes.insert( key, dynamic_cast<Node*>( newNode ) ); // owns the new pointer
				// our vertices have a link to our Nodes
			}
			v.setProcessNode( newNode );
		}
	}
}

/*
   void removeVertexAndReconnectTo( const VertexDescriptor& v, const VertexDescriptor& other )
   {
    InternalGraph::out_edge_iterator oe, oeEnd;
    tie(oe, oeEnd) = out_edges(v, g);
    // InternalGraph::in_edge_iterator ie, ieEnd;
    // tie(ie, ieEnd) = in_edges(v, g);

    for( ; oe != oeEnd; ++oe )
        source( oe )

    _graph.removeVertex( v );
   }
 */

/*
   // May be interesting for process function.
   typedef std::vector< Vertex > container;
   container c;
   topological_sort( G, std::back_inserter(c) );

   cout << "A topological ordering: ";
   for( container::reverse_iterator ii=c.rbegin(); ii!=c.rend(); ++ii )
   cout << index(*ii) << " ";
   cout << endl;
*/

template<class TGraph>
class OrderEdgeByMemorySize
{
public:
	typedef typename TGraph::GraphContainer GraphContainer;
	typedef typename TGraph::Vertex Vertex;
	typedef typename TGraph::Edge Edge;
	typedef typename TGraph::edge_descriptor edge_descriptor;

	OrderEdgeByMemorySize( const TGraph& graph )
		: _graph( graph )
	{}

	inline bool operator()( const edge_descriptor& ed1, const edge_descriptor& ed2 ) const
	{
		const Vertex& v1 = _graph.targetInstance( ed1 );
		const Vertex& v2 = _graph.targetInstance( ed2 );

		bool res= v1.getProcessOptions()._globalInfos._memory < v2.getProcessOptions()._globalInfos._memory;
		COUT_VAR2(v1.getName(), v1.getProcessOptions()._globalInfos._memory);
		COUT_VAR2(v2.getName(), v2.getProcessOptions()._globalInfos._memory);
		COUT_VAR(res);
		return res;
	}
private:
	const TGraph& _graph;
};

void ProcessGraph::process( const int tBegin, const int tEnd )
{
	using namespace boost;
	using namespace boost::graph;
	COUT( "process" );
	graph::exportAsDOT( "graphprocess.dot", _graph );

	// Initialize variables
	OfxPointD renderScale = { 1.0, 1.0 };
	OfxRectD renderWindow = { 0, 0, 0, 0 };

	//--- BEGIN RENDER
	ProcessOptions defaultOptions;
	defaultOptions._time        = tBegin;
	defaultOptions._startFrame  = tBegin;
	defaultOptions._endFrame    = tEnd;
	defaultOptions._step        = 1;
	defaultOptions._interactive = false;
	// imageEffect specific...
	defaultOptions._field       = kOfxImageFieldBoth;
	defaultOptions._renderScale = renderScale;
	defaultOptions._renderRoI   = renderWindow;

	COUT( "process begin" );
	BOOST_FOREACH( NodeMap::value_type p, _nodes )
	{
		p.second->begin( defaultOptions );
	}

	COUT( "process render..." );
	//--- RENDER
	// at each frame
	for( int t = tBegin; t <= tEnd; ++t )
	{
		defaultOptions._time = t;
		COUT( "________________________________________ frame: " << t );
		InternalGraphImpl renderGraph                = _graph;
		InternalGraphImpl::vertex_descriptor& output = renderGraph.getVertexDescriptor( _outputId );

		COUT( "________________________________________ output node : " << renderGraph.getVertex( _outputId ).getName() );

		COUT( "---------------------------------------- set default options" );
		BOOST_FOREACH( InternalGraphImpl::vertex_descriptor vd, renderGraph.getVertices() )
		{
			Vertex& v = renderGraph.instance( vd );

			v.setProcessOptions( defaultOptions );
			v.getProcessOptions()._inDegree  = renderGraph.getInDegree( vd );
			v.getProcessOptions()._outDegree = renderGraph.getOutDegree( vd );
		}
		// for each final nodes
		BOOST_FOREACH( InternalGraphImpl::edge_descriptor ed, boost::out_edges( output, renderGraph.getGraph() ) )
		{
			Vertex& v = renderGraph.targetInstance( ed );
			v.getProcessOptions()._finalNode = true;
		}

		COUT( "---------------------------------------- connectClips" );
		graph::visitor::ConnectClips<InternalGraphImpl> connectClipsVisitor( renderGraph );
		renderGraph.dfs( connectClipsVisitor, output );
		graph::exportDebugAsDOT( "graphprocess_a.dot", renderGraph );

		COUT( "---------------------------------------- preprocess 1" );
		graph::visitor::PreProcess1<InternalGraphImpl> preProcess1Visitor( renderGraph );
		renderGraph.dfs( preProcess1Visitor, output );
		graph::exportDebugAsDOT( "graphprocess_b.dot", renderGraph );

		COUT( "---------------------------------------- preprocess 2" );
		graph::visitor::PreProcess2<InternalGraphImpl> preProcess2Visitor( renderGraph );
		renderGraph.dfs_reverse( preProcess2Visitor ); //, output
		graph::exportDebugAsDOT( "graphprocess_c.dot", renderGraph );

		COUT( "---------------------------------------- preprocess 3" );
		graph::visitor::PreProcess3<InternalGraphImpl> preProcess3Visitor( renderGraph );
		renderGraph.dfs( preProcess3Visitor, output );
		graph::exportDebugAsDOT( "graphprocess_d.dot", renderGraph );

		COUT( "---------------------------------------- optimize graph" );
		graph::visitor::OptimizeGraph<InternalGraphImpl> optimizeGraphVisitor( renderGraph );
		renderGraph.dfs( optimizeGraphVisitor, output );
		graph::exportDebugAsDOT( "graphprocess_e.dot", renderGraph );

		/*
		/// @todo tuttle: out_edges sort don't work...
		COUT( "---------------------------------------- sorting graph" );
		BOOST_FOREACH( InternalGraphImpl::vertex_descriptor vd, renderGraph.getVertices() )
		{
			Vertex& v = renderGraph.instance(vd);
			COUT_X( 30, "-" );
			std::size_t i = 0;
			COUT( "before sort edges of " << v.getName() );
			BOOST_FOREACH( InternalGraphImpl::edge_descriptor ed, boost::out_edges( vd, renderGraph.getGraph() ) )
			{
				Edge& e = renderGraph.instance(ed);
				e._localId = i++;
				e._name += " -- ";
				e._name += boost::lexical_cast<std::string>(e._localId); // tmp
				COUT( e.getName() << " - " <<  renderGraph.targetInstance(ed).getProcessOptions()._globalInfos._memory  );
			}
			InternalGraphImpl::out_edge_iterator oe_it, oe_itEnd;
			boost::tie( oe_it, oe_itEnd ) = boost::out_edges( vd, renderGraph.getGraph() );
			std::sort( oe_it, oe_itEnd, OrderEdgeByMemorySize<InternalGraphImpl>(renderGraph) );
			COUT( "after sort edges of " << v.getName() );
			BOOST_FOREACH( InternalGraphImpl::edge_descriptor ed, boost::out_edges( vd, renderGraph.getGraph() ) )
			{
				Edge& e = renderGraph.instance(ed);
				COUT( e.getName() << " - " <<  renderGraph.targetInstance(ed).getProcessOptions()._globalInfos._memory );
			}
			InternalGraphImpl::out_edge_iterator oe_it, oe_itEnd;
			boost::tie( oe_it, oe_itEnd ) = boost::out_edges( vd, renderGraph.getGraph() );
			for( ; oe_it != oe_itEnd; ++oe_it )
			{
				Edge& e = renderGraph.instance(*oe_it);
				COUT( e.getName() << " - " <<  renderGraph.targetInstance(*oe_it).getProcessOptions()._globalInfos._memory );
			}
		}
		graph::exportDebugAsDOT( "graphprocess_f.dot", renderGraph );
		*/
		// remove isIdentity nodes

		COUT( "---------------------------------------- process" );
		graph::visitor::Process<InternalGraphImpl> processVisitor( renderGraph );
		renderGraph.dfs( processVisitor, output );
		graph::exportDebugAsDOT( "graphprocess_g.dot", renderGraph );

		COUT( "---------------------------------------- postprocess" );
		graph::visitor::PostProcess<InternalGraphImpl> postProcessVisitor( renderGraph );
		renderGraph.dfs( postProcessVisitor, output );

		// end of one frame
		// do some clean: memory clean, as temporary solution...
		COUT( "---------------------------------------- clearUnused" );
		Core::instance().getMemoryCache().clearUnused();
		COUT_VAR( Core::instance().getMemoryCache().size() );
		COUT_VAR( Core::instance().getMemoryCache() );

	}

	//--- END RENDER
	BOOST_FOREACH( NodeMap::value_type p, _nodes )
	{
		p.second->end( defaultOptions ); // node option... or no option here ?
	}

}

}
}
}

