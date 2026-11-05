#include "entity_rdf/History.hxx"
#include <stdexcept>
namespace entity_rdf {
std::size_t History::pending(const ChangeLog &log,
                             const AppliedHistory &history) {
  if (history.ids.size() != history.checksums.size() ||
      history.ids.size() > log.changes.size())
    throw std::runtime_error(
        "Applied history is corrupt or changelog was truncated");
  for (std::size_t i = 0; i < history.ids.size(); ++i)
    if (log.changes[i].id != history.ids[i] ||
        log.changes[i].checksum != history.checksums[i])
      throw std::runtime_error("Applied changeset was modified or reordered: " +
                               history.ids[i]);
  return history.ids.size();
}
} // namespace entity_rdf
