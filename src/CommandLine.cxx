#include "entity_rdf/CommandLine.hxx"
#include "entity_rdf/ArtifactBuilder.hxx"
#include "entity_rdf/ChangelogReader.hxx"
#include "entity_rdf/ChangelogWriter.hxx"
#include "entity_rdf/TurtleWriter.hxx"
#include "entity_rdf/instances/FeeModule.hxx"
#include <CLI/CLI.hpp>
#include <array>
#include <fstream>
#include <ostream>
#include <stdexcept>
namespace entity_rdf {
/** @brief Bound arguments for one selected offline command. */
struct CommandArguments {
  /// Root changelog XML input shared by all subcommands.
  std::string root;
  /// New output file or directory, as defined by the selected subcommand.
  std::string output;
  /// Changeset XML file to append to the root changelog.
  std::string change;
  /// Turtle input file used to author a new changeset.
  std::string input;
  /// FEE instance configuration XML input.
  std::string config;
  /// Identifier of a newly authored or generated changeset.
  std::string id;
  /// Prefix used when creating instance-generation configuration.
  std::string prefix;
  /// Absolute Turtle URI visible to the external Neo4j importer.
  std::string turtleUri;
};
CommandLine::CommandLine(std::ostream &output, std::ostream &error)
    : output_(output), error_(error) {}
int CommandLine::run(int argc, const char *const *argv) const {
  CommandArguments arguments;
  CLI::App app{"Offline RDF versioning and Neo4j artifact generation",
               "kgedit"};
  app.require_subcommand(1, 1);

  // Subcommand pointers are borrowed; the root CLI11 application owns them.
  CLI::App *validate =
      app.add_subcommand("validate", "Validate and summarize a changelog");
  CLI::App *compose = app.add_subcommand(
      "compose", "Compose a complete standalone Turtle file");
  CLI::App *build = app.add_subcommand(
      "build", "Generate a complete TTL/Cypher import bundle");
  CLI::App *create =
      app.add_subcommand("new-change", "Create a changeset from Turtle input");
  CLI::App *append =
      app.add_subcommand("append", "Append a validated changeset include");
  CLI::App *generate = app.add_subcommand("generate-instances",
                                          "Generate a FEE instance changeset");
  CLI::App *configure = app.add_subcommand(
      "create-instance-config", "Derive FEE configuration from core topology");
  for (CLI::App *command : std::array<CLI::App *, 7>{
           validate, compose, build, create, append, generate, configure})
    command->add_option("ROOT", arguments.root, "Root changelog XML path")
        ->required();

  compose->add_option("OUTPUT", arguments.output, "New Turtle output file")
      ->required();
  build
      ->add_option("OUTPUT_DIR", arguments.output,
                   "New artifact bundle directory")
      ->required();
  build
      ->add_option("--ttl-uri", arguments.turtleUri,
                   "Absolute Turtle URI visible to Neo4j")
      ->required();
  create->add_option("NEW_DIR", arguments.output, "New changeset directory")
      ->required();
  create->add_option("ID", arguments.id, "Changeset identifier")->required();
  create->add_option("INPUT", arguments.input, "Turtle input file")->required();
  append->add_option("CHANGE", arguments.change, "Changeset XML to include")
      ->required();
  generate->add_option("CONFIG", arguments.config, "FEE configuration XML path")
      ->required();
  generate
      ->add_option("OUTPUT_DIR", arguments.output,
                   "New instance changeset directory")
      ->required();
  generate->add_option("ID", arguments.id, "Generated changeset identifier")
      ->required();
  configure
      ->add_option("OUTPUT", arguments.output, "New FEE configuration XML file")
      ->required();
  configure
      ->add_option("IRI_PREFIX", arguments.prefix, "Prefix for instance IRIs")
      ->required();

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError &error) {
    return app.exit(error, output_, error_);
  }

  // Domain operations run only after CLI11 accepts the complete command line.
  ChangeLog log = ChangelogReader{}.read(arguments.root);
  if (*validate)
    output_ << log.changes.size() << " changesets, "
            << log.composed.nodes.size() << " nodes, "
            << log.composed.relationships.size() << " relationships\n";
  else if (*compose)
    writeNew(arguments.output, TurtleWriter{}.write(log.composed));
  else if (*build)
    ArtifactBuilder{}.build(log, arguments.output, arguments.turtleUri);
  else if (*create)
    ChangelogWriter{}.create(arguments.output, arguments.id, arguments.input);
  else if (*append)
    ChangelogWriter{}.append(arguments.root, arguments.change);
  else if (*generate)
    instances::FeeModule{}.generate(log.composed, arguments.config,
                                    arguments.output, arguments.id);
  else if (*configure)
    instances::FeeModule{}.createConfig(log.composed, arguments.output,
                                        arguments.prefix);
  return 0;
}
void CommandLine::writeNew(const std::filesystem::path &path,
                           const std::string &content) {
  if (std::filesystem::exists(path))
    throw std::runtime_error("Refusing to overwrite existing output: " +
                             path.string());
  std::ofstream output(path, std::ios::binary | std::ios::noreplace);
  output << content;
  if (!output)
    throw std::runtime_error("Cannot write " + path.string());
}
} // namespace entity_rdf
