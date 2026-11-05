cmake_minimum_required(VERSION 3.22)
project(kgedit VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(CLI11 2.7.2 EXACT CONFIG REQUIRED)
find_package(nlohmann_json 3.11 REQUIRED)
find_package(GTest REQUIRED)
find_package(expected-lite REQUIRED)
find_package(Boost REQUIRED)
find_package(re2 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(uni-algo REQUIRED)
find_package(highway REQUIRED)
find_package(dice-hash REQUIRED)
find_package(dice-sparse-map REQUIRED)
find_package(dice-template-library REQUIRED)
find_package(libxml2 REQUIRED)
find_package(rdf4cpp REQUIRED)
add_library(entity_rdf_core
  src/ArtifactBuilder.cxx
  src/TurtleReader.cxx
  src/Encoding.cxx
  src/GraphMapper.cxx
  src/TurtleWriter.cxx
  src/XmlReader.cxx
  src/ChangelogReader.cxx
  src/CypherBuilder.cxx
  src/XmlWriter.cxx
  src/ChangelogWriter.cxx
  src/PropertyCodec.cxx
)
target_include_directories(entity_rdf_core PUBLIC include)
target_link_libraries(entity_rdf_core PUBLIC
  nlohmann_json::nlohmann_json
  rdf4cpp::rdf4cpp
  LibXml2::LibXml2
  OpenSSL::Crypto
)
target_compile_options(entity_rdf_core PRIVATE -Wall -Wextra -Wpedantic)
enable_testing()
add_executable(entity_rdf_tests
  tests/TurtleReaderTest.cxx
  tests/GraphMapperTest.cxx
  tests/ChangelogReaderTest.cxx
  tests/CypherBuilderTest.cxx
  tests/ChangelogWriterTest.cxx
  tests/CanonicalIriTest.cxx
  tests/ArtifactBuilderTest.cxx
  tests/PropertyCodecTest.cxx
)
target_link_libraries(entity_rdf_tests PRIVATE entity_rdf_core GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(entity_rdf_tests)

add_library(entity_rdf_cli src/CommandLine.cxx)
target_include_directories(entity_rdf_cli PUBLIC include)
target_link_libraries(entity_rdf_cli PRIVATE entity_rdf_core CLI11::CLI11)
target_compile_options(entity_rdf_cli PRIVATE -Wall -Wextra -Wpedantic)
add_executable(kgedit src/main.cxx)
target_link_libraries(kgedit PRIVATE entity_rdf_cli)

add_executable(entity_rdf_cli_tests tests/CoreCommandLineTest.cxx)
target_link_libraries(entity_rdf_cli_tests PRIVATE entity_rdf_cli GTest::gtest_main)
gtest_discover_tests(entity_rdf_cli_tests)

include(GNUInstallDirs)
install(TARGETS kgedit RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
add_test(NAME CliHelp COMMAND kgedit --help)
add_test(NAME CliValidateExample COMMAND kgedit validate ${CMAKE_CURRENT_SOURCE_DIR}/examples/changelog/root.xml)
add_test(NAME CliRejectUnknownCommand COMMAND kgedit unsupported ${CMAKE_CURRENT_SOURCE_DIR}/examples/changelog/root.xml)
set_tests_properties(CliRejectUnknownCommand PROPERTIES WILL_FAIL TRUE)
