"""Verify offline bundles using external tooling against an owned disposable Neo4j.

Uses only Python's standard library, Docker and fixtures bundled with kgedit.
Never accepts an existing database endpoint or container as input.
"""
from __future__ import annotations

import base64
import hashlib
import json
import os
import shutil
import subprocess
import tempfile
import time
import urllib.error
import urllib.request
import uuid
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Any


class ArtifactVerification:
    PASSWORD: str = 'entity-rdf-disposable-verification'
    XSD: str = 'http://www.w3.org/2001/XMLSchema#'

    def __init__(self, directory: Path) -> None:
        self.directory: Path = directory
        self.container: str = 'kgedit-artifacts-' + uuid.uuid4().hex
        self.endpoint: str = ''
        self.started: bool = False
        directory.chmod(0o755)
        shutil.copytree('examples/changelog', directory / 'changes')

    @staticmethod
    def command(arguments: list[str], success: bool = True) -> str:
        result: subprocess.CompletedProcess[str] = subprocess.run(arguments, capture_output=True, text=True, timeout=600)
        if (result.returncode == 0) != success:
            raise AssertionError(f'{arguments[:5]}: {result.stdout[-5000:]}\n{result.stderr[-5000:]}')
        return result.stdout + result.stderr

    def cli(self, *arguments: str, success: bool = True) -> str:
        return self.command(['docker', 'run', '--rm', '--network', 'none', '--user', f'{os.getuid()}:{os.getgid()}',
                             '-v', f'{self.directory}:/test', '--entrypoint', '/build/kgedit',
                             'kgedit-build:verification', *arguments], success)

    def query(self, statement: str) -> list[list[Any]]:
        request: urllib.request.Request = urllib.request.Request(
            self.endpoint + '/db/neo4j/tx/commit',
            data=json.dumps({'statements': [{'statement': statement}]}).encode(),
            headers={'Content-Type': 'application/json', 'Authorization': 'Basic ' +
                     base64.b64encode(('neo4j:' + self.PASSWORD).encode()).decode()})
        with urllib.request.urlopen(request, timeout=300) as response:
            result: dict[str, Any] = json.load(response)
        if result['errors']:
            raise AssertionError(result['errors'])
        return [entry['row'] for entry in result['results'][0]['data']]

    def start(self) -> None:
        self.command(['docker', 'run', '-d', '--name', self.container,
                      '-p', '127.0.0.1::7474', '-v', f'{self.directory}:/import:ro',
                      '-e', 'NEO4J_AUTH=neo4j/' + self.PASSWORD,
                      '-e', 'NEO4J_dbms_security_procedures_unrestricted=n10s.*',
                      '-e', 'NEO4J_dbms_security_allow__csv__import__from__file__urls=true',
                      '-e', 'NEO4J_server_memory_heap_initial__size=256m',
                      '-e', 'NEO4J_server_memory_heap_max__size=2G',
                      'kgedit-neo4j:verification'])
        self.started = True
        port: str = self.command(['docker', 'port', self.container, '7474/tcp']).strip()
        self.endpoint = 'http://' + port
        for attempt in range(90):
            try:
                self.query('RETURN 1')
                return
            except (urllib.error.URLError, ConnectionError):
                time.sleep(1)
        raise AssertionError('Disposable Neo4j did not become ready')

    def import_bundle(self, name: str, success: bool = True, filename: str = 'import.cypher') -> str:
        return self.command(['docker', 'exec', self.container, 'cypher-shell', '-a', 'bolt://localhost:7687',
                             '-u', 'neo4j', '-p', self.PASSWORD, '--fail-fast', '--format', 'plain',
                             '-f', f'/import/{name}/{filename}'], success)

    def reset(self) -> None:
        # This endpoint is obtained exclusively from the container created above.
        assert self.started and self.container.startswith('kgedit-artifacts-')
        self.query('MATCH (n) DETACH DELETE n')

    @staticmethod
    def typed(value: Any) -> Any:
        if isinstance(value, dict):
            return {key: ArtifactVerification.typed(item) for key, item in value.items()}
        if isinstance(value, list):
            return [ArtifactVerification.typed(item) for item in value]
        return type(value).__name__, value

    def baseline(self) -> None:
        root: str = '/test/changes/root.xml'
        self.cli('validate', root)
        self.cli('build', root, '/test/baseline', '--ttl-uri', 'file:///import/baseline/combined.ttl')
        manifest: ET.Element = ET.parse(self.directory / 'baseline/manifest.xml').getroot()
        for artifact in manifest.findall('artifact'):
            content: bytes = (self.directory / 'baseline' / artifact.attrib['file']).read_bytes()
            assert hashlib.sha256(content).hexdigest() == artifact.attrib['sha256']
        self.import_bundle('baseline')
        actual_nodes: dict[str, Any] = {identity: (set(labels), properties) for identity, labels, properties in self.query('MATCH (n:_DATASET_) RETURN n._dataset_id,labels(n),properties(n)')}
        expected_nodes: dict[str, Any] = {
            'board': ({'_DATASET_', 'Board'}, {'_dataset_id': 'board', 'iri': 'urn:example:board', 'name': 'Example board', 'entity_identity': 'preserved-board-id'}),
            'parameter': ({'_DATASET_', 'Parameter'}, {'_dataset_id': 'parameter', 'iri': 'urn:example:parameter', 'name': 'Power'}),
        }
        assert actual_nodes == expected_nodes, actual_nodes
        edges: dict[str, Any] = {identity: (start, end, kind, properties) for identity, start, end, kind, properties in self.query('MATCH (a:_DATASET_)-[r]->(b:_DATASET_) RETURN r._dataset_id,a._dataset_id,b._dataset_id,type(r),properties(r)')}
        assert edges == {
            'link-a': ('board', 'parameter', 'has_parameter', {'_dataset_id': 'link-a', 'iri': 'urn:example:link-a', 'rank': 1}),
            'link-b': ('board', 'parameter', 'has_parameter', {'_dataset_id': 'link-b', 'iri': 'urn:example:link-b', 'rank': 2}),
            'loop': ('board', 'board', 'self', {'_dataset_id': 'loop', 'iri': 'urn:example:loop'}),
        }, edges
        assert self.query('MATCH (n:Resource) RETURN count(n)') == [[0]]
        self.import_bundle('baseline', success=False)
        assert self.query('MATCH (n:_DATASET_) RETURN count(n)') == [[2]]
        self.cli('apply', root, '/test/unused.xml', success=False)
        print('PASS independent snapshot: preserved identities, canonical IRIs, topology, properties, manifest and populated-target refusal', flush=True)

    @staticmethod
    def write_xml(path: Path, element: ET.Element) -> None:
        ET.ElementTree(element).write(path, encoding='utf-8', xml_declaration=True)

    def fixture(self) -> str:
        directory: Path = self.directory / 'fixture'
        directory.mkdir()
        timestamp: str = json.dumps(json.dumps({'@type': 'DateTime', 'value': '2026-01-02T03:04:05.123456789Z'}))
        data: str = f'''@prefix g: <urn:dcs:graph:> .
@prefix p: <urn:dcs:property:> .
@prefix xsd: <{self.XSD}> .
<urn:test:board> a g:Node; g:recordId "board"; g:label "Board`$Label"; p:name "quote '\\\"; MATCH (n) DELETE n; \\\\"; p:rank "42"^^xsd:long; p:enabled true; p:ratio "1.5"^^xsd:double; p:timestamp {timestamp}^^<urn:dcs:neo4j:DateTime>; p:ports <urn:dcs:value:ports>; p:empty <urn:dcs:value:empty> .
<urn:test:parameter> a g:Node; g:recordId "parameter"; g:label "Parameter" .
<urn:dcs:value:ports> a g:Array; g:length "3"^^xsd:long; <urn:dcs:item:0> "B"; <urn:dcs:item:1> "A"; <urn:dcs:item:2> "B" .
<urn:dcs:value:empty> a g:Array; g:length "0"^^xsd:long .
<urn:test:link-a> a g:Relationship; g:recordId "link-a"; g:source <urn:test:board>; g:target <urn:test:parameter>; g:relationshipType "connects"; p:rank "1"^^xsd:long .
<urn:test:link-b> a g:Relationship; g:recordId "link-b"; g:source <urn:test:board>; g:target <urn:test:parameter>; g:relationshipType "connects"; p:rank "2"^^xsd:long .
<urn:test:loop> a g:Relationship; g:recordId "loop"; g:source <urn:test:board>; g:target <urn:test:board>; g:relationshipType "self" .
'''
        (directory / 'data.ttl').write_text(data)
        change: ET.Element = ET.Element('graphChangeSet', id='fixture')
        ET.SubElement(change, 'rdf', file='data.ttl', sha256=hashlib.sha256(data.encode()).hexdigest())
        self.write_xml(directory / 'change.xml', change)
        root: ET.Element = ET.Element('graphChangeLog', format='rdf-changelog-v1')
        ET.SubElement(root, 'include', file='change.xml', sha256=hashlib.sha256((directory / 'change.xml').read_bytes()).hexdigest())
        self.write_xml(directory / 'root.xml', root)
        return '/test/fixture/root.xml'

    def scenarios(self) -> None:
        root: str = self.fixture()
        self.cli('build', root, '/test/typed', '--ttl-uri', 'file:///import/typed/combined.ttl')
        self.reset()
        self.import_bundle('typed')
        actual: list[list[Any]] = self.query("MATCH (n:_DATASET_ {_dataset_id:'board'}) RETURN n.ports,n.empty,n.rank,n.enabled,n.ratio,toString(n.timestamp),valueType(n.rank),valueType(n.ratio),labels(n),n.name")
        assert actual == [[['B', 'A', 'B'], [], 42, True, 1.5, '2026-01-02T03:04:05.123456789Z', 'INTEGER NOT NULL', 'FLOAT NOT NULL', ['_DATASET_', 'Board`$Label'], 'quote \'"; MATCH (n) DELETE n; \\']], actual
        assert self.query('MATCH (a:_DATASET_)-[r:connects]->(b:_DATASET_) RETURN count(r),collect(r.rank)') == [[2, [1, 2]]]
        assert self.query('MATCH (a:_DATASET_)-[r:self]->(a) RETURN count(r)') == [[1]]
        print('PASS native types, nanoseconds, ordered/empty arrays, escaping, parallel edges and self-loops', flush=True)
        self.reset()
        self.cli('build', root, '/test/missing', '--ttl-uri', 'file:///import/absent/combined.ttl')
        self.import_bundle('missing', success=False)
        assert self.query('MATCH (n:_DATASET_) RETURN count(n)') == [[0]]
        self.reset()
        # A valid RDF file with the same triple count but different identity must
        # fail projection verification and roll back every application record.
        turtle_path: Path = self.directory / 'typed/combined.ttl'
        turtle_path.write_text(turtle_path.read_text().replace('urn:test:board', 'urn:test:unexpected'))
        self.import_bundle('typed', success=False)
        assert self.query('MATCH (n:_DATASET_) RETURN count(n)') == [[0]]
        assert self.query('MATCH (n:Resource) RETURN count(n)')[0][0] > 0
        print('PASS invalid import abort and projection rollback; staging retained for diagnosis', flush=True)
        original: str = (self.directory / 'fixture/data.ttl').read_text()
        (self.directory / 'fixture/data.ttl').write_text(original + '\n# tampered\n')
        self.cli('build', root, '/test/rejected', '--ttl-uri', 'file:///import/rejected/combined.ttl', success=False)
        assert not (self.directory / 'rejected').exists()
        print('PASS source checksum rejection before artifact publication', flush=True)

    def instances(self) -> None:
        directory: Path = self.directory / 'fee'
        directory.mkdir()
        data: str = """@prefix g: <urn:dcs:graph:> .
@prefix p: <urn:dcs:property:> .
<urn:example:board:PMA0> a g:Node; g:recordId "board"; g:label "FrontEndElectronic"; p:name "PMA0" .
<urn:example:detector:FT0> a g:Node; g:recordId "detector"; g:label "DCSDetector"; p:name "FT0" .
<urn:example:type:PM> a g:Node; g:recordId "type"; g:label "FrontEndElectronicType"; p:name "PM" .
<urn:example:parameter:threshold> a g:Node; g:recordId "parameter"; g:label "HardwareParameter"; p:name "threshold" .
<urn:example:hardware> a g:Relationship; g:recordId "hardware"; g:source <urn:example:detector:FT0>; g:target <urn:example:board:PMA0>; g:relationshipType "has_hardware" .
<urn:example:device-type> a g:Relationship; g:recordId "device-type"; g:source <urn:example:board:PMA0>; g:target <urn:example:type:PM>; g:relationshipType "is_of_device_type" .
<urn:example:parameter-link> a g:Relationship; g:recordId "parameter-link"; g:source <urn:example:type:PM>; g:target <urn:example:parameter:threshold>; g:relationshipType "has_parameter" .
"""
        (directory / 'data.ttl').write_text(data)
        change: ET.Element = ET.Element('graphChangeSet', id='fee-core')
        ET.SubElement(change, 'rdf', file='data.ttl', sha256=hashlib.sha256(data.encode()).hexdigest())
        self.write_xml(directory / 'change.xml', change)
        root: ET.Element = ET.Element('graphChangeLog', format='rdf-changelog-v1')
        ET.SubElement(root, 'include', file='change.xml', sha256=hashlib.sha256((directory / 'change.xml').read_bytes()).hexdigest())
        self.write_xml(directory / 'root.xml', root)
        self.cli('create-instance-config', '/test/fee/root.xml', '/test/fee/config.xml', 'urn:example:instance:')
        self.cli('generate-instances', '/test/fee/root.xml', '/test/fee/config.xml', '/test/fee/instances', 'fee-instances')
        generated: str = (directory / 'instances/instances.ttl').read_text()
        assert generated.count(' a g:Node') == 1
        assert generated.count(' a g:Relationship') == 2
        assert '792c4aa7-0a22-58da-9d28-4310d811ccc8' in generated
        assert '<urn:example:instance:PMA0/threshold>' in generated
        self.cli('append', '/test/fee/root.xml', '/test/fee/instances/changelog.xml')
        assert '5 nodes, 5 relationships' in self.cli('validate', '/test/fee/root.xml')
        self.cli('build', '/test/fee/root.xml', '/test/fee/bundle', '--ttl-uri', 'file:///import/fee/bundle/combined.ttl')
        print('PASS independent FEE configuration, historical UUID, instance generation and appended composition', flush=True)

    def run(self) -> None:
        try:
            self.start()
            self.baseline()
            self.scenarios()
            self.instances()
        finally:
            if self.started:
                self.command(['docker', 'rm', '-f', self.container])


if __name__ == '__main__':
    with tempfile.TemporaryDirectory(prefix='kgedit-artifacts-') as name:
        ArtifactVerification(Path(name)).run()
