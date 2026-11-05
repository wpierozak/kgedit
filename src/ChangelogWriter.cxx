#include "entity_rdf/ChangelogWriter.hxx"
#include "entity_rdf/ChangelogReader.hxx"
#include "entity_rdf/GraphMapper.hxx"
#include "entity_rdf/TurtleReader.hxx"
#include "entity_rdf/Vocabulary.hxx"
#include "entity_rdf/XmlReader.hxx"
#include "entity_rdf/XmlWriter.hxx"
#include <fstream>
#include <stdexcept>
namespace entity_rdf {
void ChangelogWriter::writeNew(const std::filesystem::path &path,
                               const std::string &content) {
  if (std::filesystem::exists(path))
    throw std::runtime_error("Refusing to overwrite published output");
  std::ofstream stream(path, std::ios::binary | std::ios::noreplace);
  stream << content;
  if (!stream)
    throw std::runtime_error("Cannot write " + path.string());
}
void ChangelogWriter::create(const std::filesystem::path &output,
                             const std::string &id,
                             const std::filesystem::path &turtle) const {
  if (id.empty() || std::filesystem::exists(output))
    throw std::runtime_error(
        "New changeset requires a new directory and nonempty ID");
  GraphMapper{}.decode(TurtleReader{}.read(turtle));
  std::string content = Encoding::readFile(turtle.string());
  std::filesystem::create_directories(output);
  writeNew(output / "data.ttl", content);
  XmlElement root{
      "graphChangeSet",
      {{"id", id}},
      {{"rdf",
        {{"file", "data.ttl"}, {"sha256", Encoding::sha256(content)}},
        {}}}};
  writeNew(output / "changelog.xml", XmlWriter{}.write(root));
}
void ChangelogWriter::append(const std::filesystem::path &rootPath,
                             const std::filesystem::path &change) const {
  ChangelogReader{}.read(rootPath);
  std::string original = Encoding::readFile(rootPath.string());
  XmlElement root = XmlReader{}.read(rootPath);
  std::filesystem::path relative =
      std::filesystem::canonical(change).lexically_relative(
          std::filesystem::canonical(rootPath).parent_path());
  if (relative.empty() || relative.is_absolute() || *relative.begin() == "..")
    throw std::runtime_error(
        "Changeset must be inside the root changelog directory");
  root.children.push_back(
      {"include",
       {{"file", relative.generic_string()},
        {"sha256", Encoding::sha256(Encoding::readFile(change.string()))}},
       {}});
  std::filesystem::path candidate = rootPath;
  candidate += ".pending";
  writeNew(candidate, XmlWriter{}.write(root));
  try {
    ChangelogReader{}.read(candidate);
    if (Encoding::readFile(rootPath.string()) != original)
      throw std::runtime_error("Root changed during publication; retry append");
    std::filesystem::rename(candidate, rootPath);
  } catch (...) {
    std::filesystem::remove(candidate);
    throw;
  }
}
} // namespace entity_rdf
