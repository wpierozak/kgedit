#pragma once
#include "entity_rdf/ChangelogDto.hxx"
namespace entity_rdf {
/**
 * @brief Carries the retired database ledger entries in their applied order.
 * @deprecated Retained for historical reference; excluded from the offline tool
 * build.
 */
struct AppliedHistory {
  /// Applied changeset IDs in commit order.
  std::vector<std::string> ids;
  /// Corresponding immutable changeset checksums.
  std::vector<std::string> checksums;
};
/**
 * @brief Validates the retired applied ledger against an immutable changelog
 * prefix.
 * @deprecated Retained for historical reference; excluded from the offline tool
 * build.
 */
class History {
public:
  /**
   * @brief Validate the applied ledger and find the first unapplied changeset.
   *
   * @throws std::runtime_error If ledger lengths, order, IDs or checksums
   * disagree.
   * @param log Complete expected changelog.
   * @param history Applied ID/checksum prefix.
   * @return Index of the first unapplied changeset, or the changelog size.
   */
  static std::size_t pending(const ChangeLog &log,
                             const AppliedHistory &history);
};
} // namespace entity_rdf
