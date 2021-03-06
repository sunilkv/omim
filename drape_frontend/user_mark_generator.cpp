#include "drape_frontend/user_mark_generator.hpp"
#include "drape_frontend/tile_utils.hpp"

#include "drape/batcher.hpp"

#include "geometry/mercator.hpp"
#include "geometry/rect_intersect.hpp"

#include "indexer/scales.hpp"

#include <algorithm>

namespace df
{

std::vector<int> const kLineIndexingLevels = {1, 7, 11};

UserMarkGenerator::UserMarkGenerator(TFlushFn const & flushFn)
  : m_flushFn(flushFn)
{
  ASSERT(m_flushFn != nullptr, ());
}

void UserMarkGenerator::RemoveGroup(MarkGroupID groupId)
{
  m_groupsVisibility.erase(groupId);
  m_groups.erase(groupId);
  UpdateIndex(groupId);
}

void UserMarkGenerator::SetGroup(MarkGroupID groupId, drape_ptr<MarkIDCollection> && ids)
{
  m_groups[groupId] = std::move(ids);
  UpdateIndex(groupId);
}

void UserMarkGenerator::SetRemovedUserMarks(drape_ptr<MarkIDCollection> && ids)
{
  if (ids == nullptr)
    return;
  for (auto const & id : ids->m_marksID)
    m_marks.erase(id);
  for (auto const & id : ids->m_linesID)
    m_lines.erase(id);
}

void UserMarkGenerator::SetCreatedUserMarks(drape_ptr<MarkIDCollection> && ids)
{
  if (ids == nullptr)
    return;
  for (auto const & id : ids->m_marksID)
    m_marks[id].get()->m_justCreated = true;
}

void UserMarkGenerator::SetUserMarks(drape_ptr<UserMarksRenderCollection> && marks)
{
  for (auto & pair : *marks.get())
  {
    auto it = m_marks.find(pair.first);
    if (it != m_marks.end())
      it->second = std::move(pair.second);
    else
      m_marks.emplace(pair.first, std::move(pair.second));
  }
}

void UserMarkGenerator::SetUserLines(drape_ptr<UserLinesRenderCollection> && lines)
{
  for (auto & pair : *lines.get())
  {
    auto it = m_lines.find(pair.first);
    if (it != m_lines.end())
      it->second = std::move(pair.second);
    else
      m_lines.emplace(pair.first, std::move(pair.second));
  }
}


void UserMarkGenerator::UpdateIndex(MarkGroupID groupId)
{
  for (auto & tileGroups : m_index)
  {
    auto itGroupIndexes = tileGroups.second->find(groupId);
    if (itGroupIndexes != tileGroups.second->end())
    {
      itGroupIndexes->second->m_marksID.clear();
      itGroupIndexes->second->m_linesID.clear();
    }
  }

  auto const groupIt = m_groups.find(groupId);
  if (groupIt == m_groups.end())
    return;

  MarkIDCollection & idCollection = *groupIt->second.get();

  for (auto markId : idCollection.m_marksID)
  {
    UserMarkRenderParams const & params = *m_marks[markId].get();
    for (int zoomLevel = params.m_minZoom; zoomLevel <= scales::GetUpperScale(); ++zoomLevel)
    {
      TileKey const tileKey = GetTileKeyByPoint(params.m_pivot, zoomLevel);
      ref_ptr<MarkIDCollection> groupIDs = GetIdCollection(tileKey, groupId);
      groupIDs->m_marksID.push_back(static_cast<uint32_t>(markId));
    }
  }

  for (auto lineId : idCollection.m_linesID)
  {
    UserLineRenderParams const & params = *m_lines[lineId].get();

    int const startZoom = GetNearestLineIndexZoom(params.m_minZoom);
    for (int zoomLevel : kLineIndexingLevels)
    {
      if (zoomLevel < startZoom)
        continue;
      // Process spline by segments that no longer than tile size.
      double const range = MercatorBounds::maxX - MercatorBounds::minX;
      double const maxLength = range / (1 << (zoomLevel - 1));

      df::ProcessSplineSegmentRects(params.m_spline, maxLength,
                                    [&](m2::RectD const & segmentRect)
      {
        CalcTilesCoverage(segmentRect, zoomLevel, [&](int tileX, int tileY)
        {
          TileKey const tileKey(tileX, tileY, zoomLevel);
          ref_ptr<MarkIDCollection> groupIDs = GetIdCollection(tileKey, groupId);
          groupIDs->m_linesID.push_back(static_cast<uint32_t>(lineId));
        });
        return true;
      });
    }
  }

  CleanIndex();
}

ref_ptr<MarkIDCollection> UserMarkGenerator::GetIdCollection(TileKey const & tileKey, MarkGroupID groupId)
{
  ref_ptr<MarksIDGroups> tileGroups;
  auto itTileGroups = m_index.find(tileKey);
  if (itTileGroups == m_index.end())
  {
    auto tileIDGroups = make_unique_dp<MarksIDGroups>();
    tileGroups = make_ref(tileIDGroups);
    m_index.insert(make_pair(tileKey, std::move(tileIDGroups)));
  }
  else
  {
    tileGroups = make_ref(itTileGroups->second);
  }

  ref_ptr<MarkIDCollection> groupIDs;
  auto itGroupIDs = tileGroups->find(groupId);
  if (itGroupIDs == tileGroups->end())
  {
    auto groupMarkIndexes = make_unique_dp<MarkIDCollection>();
    groupIDs = make_ref(groupMarkIndexes);
    tileGroups->insert(make_pair(groupId, std::move(groupMarkIndexes)));
  }
  else
  {
    groupIDs = make_ref(itGroupIDs->second);
  }

  return groupIDs;
}

void UserMarkGenerator::CleanIndex()
{
  for (auto tileIt = m_index.begin(); tileIt != m_index.end();)
  {
    if (tileIt->second->empty())
      tileIt = m_index.erase(tileIt);
    else
      ++tileIt;
  }

  for (auto & tileGroups : m_index)
  {
    for (auto groupIt = tileGroups.second->begin(); groupIt != tileGroups.second->end();)
    {
      if (groupIt->second->m_marksID.empty() && groupIt->second->m_linesID.empty())
        groupIt = tileGroups.second->erase(groupIt);
      else
        ++groupIt;
    }
  }
}

void UserMarkGenerator::SetGroupVisibility(MarkGroupID groupId, bool isVisible)
{
  if (isVisible)
    m_groupsVisibility.insert(groupId);
  else
    m_groupsVisibility.erase(groupId);
}

ref_ptr<MarksIDGroups> UserMarkGenerator::GetUserMarksGroups(TileKey const & tileKey)
{
  auto const itTile = m_index.find(tileKey);
  if (itTile != m_index.end())
    return make_ref(itTile->second);
  return nullptr;
}

ref_ptr<MarksIDGroups> UserMarkGenerator::GetUserLinesGroups(TileKey const & tileKey)
{
  auto itTile = m_index.end();
  int const lineZoom = GetNearestLineIndexZoom(tileKey.m_zoomLevel);
  CalcTilesCoverage(tileKey.GetGlobalRect(), lineZoom,
                    [this, &itTile, lineZoom](int tileX, int tileY)
  {
    itTile = m_index.find(TileKey(tileX, tileY, lineZoom));
  });
  if (itTile != m_index.end())
    return make_ref(itTile->second);
  return nullptr;
}

void UserMarkGenerator::GenerateUserMarksGeometry(TileKey const & tileKey, ref_ptr<dp::TextureManager> textures)
{
  auto const clippedTileKey = TileKey(tileKey.m_x, tileKey.m_y, ClipTileZoomByMaxDataZoom(tileKey.m_zoomLevel));
  auto marksGroups = GetUserMarksGroups(clippedTileKey);
  auto linesGroups = GetUserLinesGroups(clippedTileKey);

  if (marksGroups == nullptr && linesGroups == nullptr)
    return;

  uint32_t const kMaxSize = 65000;
  dp::Batcher batcher(kMaxSize, kMaxSize);
  TUserMarksRenderData renderData;
  {
    dp::SessionGuard guard(batcher, [&tileKey, &renderData](dp::GLState const & state,
                                                           drape_ptr<dp::RenderBucket> && b)
    {
      renderData.emplace_back(state, std::move(b), tileKey);
    });

    if (marksGroups != nullptr)
      CacheUserMarks(tileKey, *marksGroups.get(), textures, batcher);
    if (linesGroups != nullptr)
      CacheUserLines(tileKey, *linesGroups.get(), textures, batcher);
  }
  m_flushFn(std::move(renderData));
}

void UserMarkGenerator::CacheUserLines(TileKey const & tileKey, MarksIDGroups const & indexesGroups,
                                       ref_ptr<dp::TextureManager> textures, dp::Batcher & batcher)
{
  for (auto & groupPair : indexesGroups)
  {
    MarkGroupID groupId = groupPair.first;
    if (m_groupsVisibility.find(groupId) == m_groupsVisibility.end())
      continue;

    df::CacheUserLines(tileKey, textures, groupPair.second->m_linesID, m_lines, batcher);
  }
}

void UserMarkGenerator::CacheUserMarks(TileKey const & tileKey, MarksIDGroups const & indexesGroups,
                                       ref_ptr<dp::TextureManager> textures, dp::Batcher & batcher)
{
  for (auto & groupPair : indexesGroups)
  {
    MarkGroupID groupId = groupPair.first;
    if (m_groupsVisibility.find(groupId) == m_groupsVisibility.end())
      continue;
    df::CacheUserMarks(tileKey, textures, groupPair.second->m_marksID, m_marks, batcher);
  }
}

int UserMarkGenerator::GetNearestLineIndexZoom(int zoom) const
{
  int nearestZoom = kLineIndexingLevels[0];
  for (size_t i = 1; i < kLineIndexingLevels.size(); ++i)
  {
    if (kLineIndexingLevels[i] <= zoom)
      nearestZoom = kLineIndexingLevels[i];
    else
      break;
  }
  return nearestZoom;
}

}  // namespace df
