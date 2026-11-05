#pragma once
#include <filesystem>
#include <string>
namespace entity_rdf {
/**
 * @brief Publishes immutable changesets and appends validated root includes.
 */
class ChangelogWriter {
public:
  /**
   * @brief Create a new checksummed changeset from validated Turtle records.
   *
   * @throws std::runtime_error If the output exists, the ID is empty, or
   * decoding/publication fails.
   * @param output New changeset directory.
   * @param id Nonempty changeset identifier.
   * @param turtle Input Turtle file; references may target records in earlier
   * changesets.
   */
  void create(const std::filesystem::path &output, const std::string &id,
              const std::filesystem::path &turtle) const;
  /**
   * @brief Append an include only after validating the candidate complete
   * changelog.
   *
   * Existing includes retain their order. The candidate is validated before
   * replacing the root.
   * @throws std::runtime_error If composition fails or the root changes during
   * publication.
   * @param root Existing root changelog XML path.
   * @param change New changeset XML inside the root directory tree.
   */
  void append(const std::filesystem::path &root,
              const std::filesystem::path &change) const;

private:
  /**
   * @brief Write a new file without overwriting an existing destination.
   *
   * @throws std::runtime_error If the file exists or writing fails.
   * @param path Destination file path; its parent must exist.
   * @param content Exact bytes to write.
   */
  static void writeNew(const std::filesystem::path &path,
                       const std::string &content);
};
} // namespace entity_rdf
