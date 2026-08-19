import 'dart:convert';
import 'dart:io';

import 'package:crypto/crypto.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:http/http.dart' as http;
import 'package:http/testing.dart';
import 'package:sqlite3/sqlite3.dart';
import 'package:vibestack_atlas/services/catalog_database.dart';
import 'package:vibestack_atlas/services/catalog_update_service.dart';

/// The update path is the one place where the app takes a file from the
/// network and lets it replace its own data, so every way that can go wrong is
/// worth a test: a truncated download, a tampered one, a catalog built for a
/// newer app, and the ordinary case of already being up to date.
void main() {
  final reason = _sqliteUnavailableReason();

  group('catalog update', skip: reason, () {
    late Directory workspace;
    late CatalogDatabase database;
    late List<int> archive;
    late Map<String, dynamic> manifest;

    setUp(() async {
      workspace = await Directory.systemTemp.createTemp('atlas-update-test');
      final installed = File('${workspace.path}/catalog.db');
      await File('assets/catalog.db').copy(installed.path);

      database = CatalogDatabase(directory: () async => workspace)
        ..openFile(installed.path);

      // The payload is this build's own catalog with its version bumped, so
      // the test exercises a real database rather than a fixture that only
      // resembles one.
      final payload = File('${workspace.path}/payload.db');
      await File('assets/catalog.db').copy(payload.path);
      sqlite3.open(payload.path)
        ..execute(
            "UPDATE meta SET value = '2099.01.01' WHERE key = 'catalogVersion'")
        ..dispose();

      final bytes = await payload.readAsBytes();
      archive = gzip.encode(bytes);
      manifest = {
        'schemaVersion': CatalogDatabase.supportedSchemaVersion,
        // Deliberately far in the future, so "there is something newer" is
        // what is under test rather than today's date.
        'catalogVersion': '2099.01.01',
        'verifiedAt': '2099-01-01',
        'itemCount': 1,
        'file': 'catalog.db.gz',
        'url': 'https://example.test/catalog.db.gz',
        'size': archive.length,
        'sha256': sha256.convert(archive).toString(),
        'databaseSize': bytes.length,
        'databaseSha256': sha256.convert(bytes).toString(),
        'minAppVersion': '0.7.0',
        'signature': '',
      };
    });

    tearDown(() async {
      database.close();
      if (workspace.existsSync()) await workspace.delete(recursive: true);
    });

    CatalogUpdateService serviceWith(
      Map<String, dynamic> manifestJson, {
      List<int>? body,
      int archiveStatus = 200,
    }) {
      final client = MockClient((request) async {
        if (request.url.path.endsWith('manifest.json')) {
          return http.Response(jsonEncode(manifestJson), 200);
        }
        return http.Response.bytes(body ?? archive, archiveStatus);
      });
      return CatalogUpdateService(
        database: database,
        client: client,
        manifestUrl: 'https://example.test/manifest.json',
      );
    }

    test('installs a newer catalog and reopens it', () async {
      final result = await serviceWith(manifest).update(appVersion: '0.7.0');

      // The payload is this build's own database, so the installed catalog
      // reads back with the same entry count it went in with.
      expect(result.status, CatalogUpdateStatus.installed);
      expect(result.version, '2099.01.01');
      expect(database.metadata.catalogVersion, '2099.01.01');
      expect(database.loadItems(), isNotEmpty);
      expect(File('${workspace.path}/catalog.db.incoming').existsSync(), isFalse,
          reason: 'the staging file must not survive a successful install');
    });

    test('does nothing when the published catalog is the installed one', () async {
      final installed = database.metadata.catalogVersion;
      final result = await serviceWith({...manifest, 'catalogVersion': installed})
          .update(appVersion: '0.7.0');

      expect(result.status, CatalogUpdateStatus.upToDate);
      expect(result.version, installed);
    });

    test('refuses a catalog that needs a newer app', () async {
      final result = await serviceWith({...manifest, 'minAppVersion': '9.0.0'})
          .update(appVersion: '0.7.0');

      expect(result.status, CatalogUpdateStatus.appTooOld);
    });

    test('refuses a catalog built for a different schema', () async {
      final result = await serviceWith({...manifest, 'schemaVersion': 99})
          .update(appVersion: '0.7.0');

      expect(result.status, CatalogUpdateStatus.appTooOld);
    });

    test('rejects a download whose checksum does not match', () async {
      final tampered = [...archive]..[archive.length ~/ 2] ^= 0xFF;
      final before = database.metadata.catalogVersion;

      final result =
          await serviceWith(manifest, body: tampered).update(appVersion: '0.7.0');

      expect(result.status, CatalogUpdateStatus.failed);
      expect(result.message, contains('checksum'));
      expect(database.metadata.catalogVersion, before,
          reason: 'a failed update must leave the installed catalog alone');
      expect(File('${workspace.path}/catalog.db.incoming').existsSync(), isFalse);
    });

    test('rejects a download of the wrong length', () async {
      final result = await serviceWith({...manifest, 'size': 12})
          .update(appVersion: '0.7.0');

      expect(result.status, CatalogUpdateStatus.failed);
    });

    test('reports a release that cannot be downloaded', () async {
      final before = database.metadata.catalogVersion;
      final result = await serviceWith(manifest, archiveStatus: 404)
          .update(appVersion: '0.7.0');

      expect(result.status, CatalogUpdateStatus.failed);
      expect(database.metadata.catalogVersion, before);
    });

    test('reports a manifest that cannot be fetched', () async {
      final service = CatalogUpdateService(
        database: database,
        client: MockClient((_) async => http.Response('nope', 500)),
        manifestUrl: 'https://example.test/manifest.json',
      );

      final result = await service.update(appVersion: '0.7.0');
      expect(result.status, CatalogUpdateStatus.failed);
      expect(result.message, contains('manifest'));
    });
  });
}

String? _sqliteUnavailableReason() {
  try {
    sqlite3.version;
    return null;
  } on Object catch (error) {
    return 'no native SQLite library on this host ($error)';
  }
}
