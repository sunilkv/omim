#pragma once

#include "base/cancellable.hpp"

#include "routing/road_graph.hpp"
#include "routing/router.hpp"

#include "routing/base/astar_progress.hpp"

#include "std/functional.hpp"
#include "std/string.hpp"
#include "std/vector.hpp"

namespace routing
{

// IRoutingAlgorithm is an abstract interface of a routing algorithm,
// which searches the optimal way between two junctions on the graph
class IRoutingAlgorithm
{
public:
  enum class Result
  {
    OK,
    NoPath,
    Cancelled
  };

  virtual Result CalculateRoute(IRoadGraph const & graph, Junction const & startPos,
                                Junction const & finalPos, IRouterObserver const & observer,
                                vector<Junction> & path) = 0;
};

string DebugPrint(IRoutingAlgorithm::Result const & result);

// Base class for AStar-based routing algorithms
class AStarRoutingAlgorithmBase : public IRoutingAlgorithm
{
protected:
  AStarRoutingAlgorithmBase();

  AStarProgress m_progress;
};

// AStar routing algorithm implementation
class AStarRoutingAlgorithm : public AStarRoutingAlgorithmBase
{
public:
  // IRoutingAlgorithm overrides:
  Result CalculateRoute(IRoadGraph const & graph, Junction const & startPos,
                        Junction const & finalPos, IRouterObserver const & observer,
                        vector<Junction> & path) override;
};

// AStar-bidirectional routing algorithm implementation
class AStarBidirectionalRoutingAlgorithm : public AStarRoutingAlgorithmBase
{
public:
  // IRoutingAlgorithm overrides:
  Result CalculateRoute(IRoadGraph const & graph, Junction const & startPos,
                        Junction const & finalPos, IRouterObserver const & observer,
                        vector<Junction> & path) override;
};

}  // namespace routing
