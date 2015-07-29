#include "cross_mwm_router.hpp"
#include "cross_mwm_road_graph.hpp"
#include "base/astar_algorithm.hpp"
#include "base/timer.hpp"

namespace routing
{

namespace
{
/// Function to run AStar Algorithm from the base.
IRouter::ResultCode CalculateRoute(BorderCross const & startPos, BorderCross const & finalPos,
                                   CrossMwmGraph const & roadGraph, vector<BorderCross> & route,
                                   IRouterObserver const & observer)
{
  using TAlgorithm = AStarAlgorithm<CrossMwmGraph>;

  TAlgorithm::TOnVisitedVertexCallback onVisitedVertex =
      [&observer](BorderCross const & cross, BorderCross const & /* target */)
  {
    observer.OnPointCheck(cross.fromNode.point);
  };

  my::HighResTimer timer(true);
  TAlgorithm::Result const result =
      TAlgorithm().FindPath(roadGraph, startPos, finalPos, route, observer, onVisitedVertex);
  LOG(LINFO, ("Duration of the cross MWM path finding", timer.ElapsedNano()));
  switch (result)
  {
    case TAlgorithm::Result::OK:
      ASSERT_EQUAL(route.front(), startPos, ());
      ASSERT_EQUAL(route.back(), finalPos, ());
      return IRouter::NoError;
    case TAlgorithm::Result::NoPath:
      return IRouter::RouteNotFound;
    case TAlgorithm::Result::Cancelled:
      return IRouter::Cancelled;
  }
  return IRouter::RouteNotFound;
}
}  // namespace

IRouter::ResultCode CalculateCrossMwmPath(TRoutingNodes const & startGraphNodes,
                                          TRoutingNodes const & finalGraphNodes,
                                          RoutingIndexManager & indexManager,
                                          IRouterObserver const & observer, TCheckedPath & route)
{
  CrossMwmGraph roadGraph(indexManager);
  FeatureGraphNode startGraphNode, finalGraphNode;
  CrossNode startNode, finalNode;

  // Finding start node.
  IRouter::ResultCode code = IRouter::StartPointNotFound;
  for (FeatureGraphNode const & start : startGraphNodes)
  {
    startNode = CrossNode(start.node.forward_node_id, start.mwmName, start.segmentPoint);
    code = roadGraph.SetStartNode(startNode);
    if (code == IRouter::NoError)
    {
      startGraphNode = start;
      break;
    }
  }
  if (code != IRouter::NoError)
    return IRouter::StartPointNotFound;

  // Finding final node.
  code = IRouter::EndPointNotFound;
  for (FeatureGraphNode const & final : finalGraphNodes)
  {
    finalNode = CrossNode(final.node.reverse_node_id, final.mwmName, final.segmentPoint);
    finalNode.isVirtual = true;
    code = roadGraph.SetFinalNode(finalNode);
    if (code == IRouter::NoError)
    {
      finalGraphNode = final;
      break;
    }
  }
  if (code != IRouter::NoError)
    return IRouter::EndPointNotFound;

  // Finding path through maps.
  vector<BorderCross> tempRoad;
  code = CalculateRoute({startNode, startNode}, {finalNode, finalNode}, roadGraph, tempRoad,
                        observer);
  if (code != IRouter::NoError)
    return code;

  // Final path conversion to output type.
  ConvertToSingleRouterTasks(tempRoad, startGraphNode, finalGraphNode, route);

  return route.empty() ? IRouter::RouteNotFound : IRouter::NoError;
}

}  // namespace routing
