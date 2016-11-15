#pragma once

#include "routing/routing_serialization.hpp"

#include "std/functional.hpp"
#include "std/limits.hpp"
#include "std/map.hpp"
#include "std/string.hpp"
#include "std/utility.hpp"
#include "std/vector.hpp"

namespace routing
{
/// This class collects all relations with type restriction and save feature ids of
/// their road feature in text file for using later.
class RestrictionCollector
{
public:
  /// \param restrictionPath full path to file with road restrictions in osm id terms.
  /// \param featureIdToOsmIdsPath full path to file with mapping from feature id to osm id.
  RestrictionCollector(string const & restrictionPath, string const & featureIdToOsmIdsPath);

  bool HasRestrictions() const { return !m_restrictions.empty(); }

  /// \returns true if all restrictions in |m_restrictions| are valid and false otherwise.
  /// \note Complexity of the method is linear in the size of |m_restrictions|.
  bool IsValid() const;

  /// \returns Sorted vector of restrictions.
  RestrictionVec const & GetRestrictions() const { return m_restrictions; }

private:
  friend void UnitTest_RestrictionTest_ValidCase();
  friend void UnitTest_RestrictionTest_InvalidCase();
  friend void UnitTest_RestrictionTest_ParseRestrictions();
  friend void UnitTest_RestrictionTest_ParseFeatureId2OsmIdsMapping();

  /// \brief Parses comma separated text file with line in following format:
  /// <feature id>, <osm id 1 corresponding feature id>, <osm id 2 corresponding feature id>, and so
  /// on
  /// For example:
  /// 137999, 5170186,
  /// 138000, 5170209, 5143342,
  /// 138001, 5170228,
  /// \param featureIdToOsmIdsPath path to the text file.
  /// \note Most restrictions consist of type and two linear(road) features.
  /// \note For the time being only line-point-line restrictions are supported.
  bool ParseFeatureId2OsmIdsMapping(string const & featureIdToOsmIdsPath);

  /// \brief Parses comma separated text file with line in following format:
  /// <type of restrictions>, <osm id 1 of the restriction>, <osm id 2>, and so on
  /// For example:
  /// Only, 335049632, 49356687,
  /// No, 157616940, 157616940,
  /// No, 157616940, 157617107,
  /// \param featureIdToOsmIdsPath path to the text file.
  bool ParseRestrictions(string const & path);

  /// \brief Adds feature id and corresponding vector of |osmIds| to |m_osmIdToFeatureId|.
  /// \note One feature id (|featureId|) may correspond to several osm ids (|osmIds|).
  void AddFeatureId(uint32_t featureId, vector<uint64_t> const & osmIds);

  /// \brief Adds a restriction (vector of osm id).
  /// \param type is a type of restriction
  /// \param osmIds is osm ids of restriction links
  /// \note This method should be called to add a restriction when feature ids of the restriction
  /// are unknown. The feature ids should be set later with a call of |SetFeatureId(...)| method.
  /// \returns true if restriction is add and false otherwise.
  bool AddRestriction(Restriction::Type type, vector<uint64_t> const & osmIds);

  RestrictionVec m_restrictions;
  map<uint64_t, uint32_t> m_osmIdToFeatureId;
};

bool FromString(string str, Restriction::Type & type);
}  // namespace routing