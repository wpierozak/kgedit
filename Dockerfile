FROM gcc:14.3-bookworm AS dependencies
RUN apt-get update && apt-get install -y --no-install-recommends cmake ninja-build python3-venv curl ca-certificates nlohmann-json3-dev libgtest-dev clang-format && rm -rf /var/lib/apt/lists/*
RUN python3 -m venv /opt/conan && /opt/conan/bin/pip install --no-cache-dir conan==2.15.1
ENV PATH="/opt/conan/bin:${PATH}"
RUN git clone https://github.com/tentris/rdf4cpp.git /deps/rdf4cpp && git -C /deps/rdf4cpp checkout f6b22963c44672ef1493c5b5759e649ff30235d2
RUN conan profile detect && conan remote add dice-group https://conan.dice-research.org/artifactory/api/conan/tentris && conan install /deps/rdf4cpp --output-folder=/deps/build --build=missing -s compiler.cppstd=23 -o 'boost/*:header_only=True'
RUN cmake -S /deps/rdf4cpp -B /deps/build-native -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=/deps/build/build/Release/generators/conan_toolchain.cmake -DBUILD_TESTING=OFF -DCMAKE_INSTALL_PREFIX=/usr/local && cmake --build /deps/build-native --parallel 3 && cmake --install /deps/build-native

FROM dependencies AS cli11
ADD https://codeload.github.com/CLIUtils/CLI11/tar.gz/refs/tags/v2.7.2 /deps/cli11.tar.gz
RUN echo "46eef3101da70852ec7af026e09d485ccee81813331c8c6052d39344443b83da  /deps/cli11.tar.gz" | sha256sum -c - && mkdir /deps/cli11 && tar -xzf /deps/cli11.tar.gz --strip-components=1 -C /deps/cli11 && cmake -S /deps/cli11 -B /deps/cli11-build -DCLI11_BUILD_TESTS=OFF -DCLI11_BUILD_EXAMPLES=OFF -DCLI11_BUILD_DOCS=OFF -DCLI11_INSTALL=ON -DCMAKE_INSTALL_PREFIX=/usr/local && cmake --install /deps/cli11-build

FROM cli11 AS build
RUN test "$(dpkg-query -W -f='${Version}' nlohmann-json3-dev)" = "3.11.2-2" && test "$(dpkg-query -W -f='${Version}' libgtest-dev)" = "1.12.1-0.2"
WORKDIR /source
COPY CMakeLists.txt /source/CMakeLists.txt
COPY src /source/src
COPY include /source/include
COPY tests /source/tests
COPY examples /source/examples
RUN cmake -S . -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=/deps/build/build/Release/generators/conan_toolchain.cmake && cmake --build /build --parallel 3
RUN ctest --test-dir /build --output-on-failure

FROM build AS runtime
ENTRYPOINT ["/build/kgedit"]
CMD ["--help"]

FROM neo4j:5.26-community AS neo4j
ADD https://github.com/neo4j-labs/neosemantics/releases/download/5.26.0/neosemantics-5.26.0.jar /plugins/neosemantics-5.26.0.jar
USER root
RUN echo "509bec192befce6224a7e3bf78a0e6b74aeb8bf61eb7a1e009b7b62e0737900b  /plugins/neosemantics-5.26.0.jar" | sha256sum -c - && chmod 644 /plugins/neosemantics-5.26.0.jar
USER neo4j
