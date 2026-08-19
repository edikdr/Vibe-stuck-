import 'dart:convert';

import 'package:cryptography/cryptography.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:vibestack_atlas/services/catalog_update_service.dart';

/// A signature produced by `tool/catalog/signing.py`, verified here.
///
/// The signing code and the verifying code are in different languages, and a
/// test that signs and verifies in the same one proves only that it agrees
/// with itself. The vector below was generated once by the Python side; if
/// either implementation ever changes what it considers the signed payload,
/// this fails — which is the only warning anyone would get before shipping an
/// app that rejects every real release.
///
/// The key is a throwaway generated for this file. Its private half was never
/// written down.
void main() {
  const publicKey = 'IPnwauvnrCU+AQq1N28D7Ioxdz1R9kNYzEFMnQ4SPp0=';
  const signature = 'ed25519:9WgPWDJGYuz9KKw/fRb2SX5VmhDiz6UM0bXUPFt+gflZ'
      'EPM2JmvVABqtemaUgSPgmmc63Ffy1MPRZqk7AN+5Ag==';

  final manifestJson = <String, dynamic>{
    'schemaVersion': 4,
    'catalogVersion': '2026.08.19',
    'verifiedAt': '2026-08-19',
    'itemCount': 2889,
    'file': 'catalog.db.gz',
    'url': 'https://example.test/catalog.db.gz',
    'size': 828250,
    'sha256': '41176461f44d5f76e63c59586425db954c8ba6fb3fc03469a60dd7abc884a4cb',
    'databaseSize': 3633152,
    'databaseSha256':
        '5df5aaa333c81ac6e091a2e0046543f72b253868160547686fd1124fbdfacc7b',
    'minAppVersion': '0.7.0',
    'signature': signature,
  };

  Future<bool> verify(Map<String, dynamic> json, {String key = publicKey}) async {
    final manifest = CatalogManifest.fromJson(json);
    final raw = manifest.signature
        .substring(CatalogManifest.signaturePrefix.length);
    return Ed25519().verify(
      manifest.signingPayload,
      signature: Signature(
        base64.decode(raw),
        publicKey: SimplePublicKey(base64.decode(key), type: KeyPairType.ed25519),
      ),
    );
  }

  test('the signed payload is byte-identical to the Python one', () {
    expect(
      utf8.decode(CatalogManifest.fromJson(manifestJson).signingPayload),
      'vibestack-atlas-catalog-v1\n'
      '4\n'
      '2026.08.19\n'
      '828250\n'
      '41176461f44d5f76e63c59586425db954c8ba6fb3fc03469a60dd7abc884a4cb\n'
      '3633152\n'
      '5df5aaa333c81ac6e091a2e0046543f72b253868160547686fd1124fbdfacc7b',
    );
  });

  test('a signature made by the release tooling verifies here', () async {
    expect(await verify(manifestJson), isTrue);
  });

  test('every field the payload covers is actually covered', () async {
    const tampered = <String, Object>{
      'schemaVersion': 5,
      'catalogVersion': '2026.08.20',
      'size': 828251,
      'sha256': 'f0',
      'databaseSize': 3633153,
      'databaseSha256': 'f0',
    };
    for (final entry in tampered.entries) {
      expect(await verify({...manifestJson, entry.key: entry.value}), isFalse,
          reason: 'changing ${entry.key} left the signature valid');
    }
  });

  test('a field outside the payload does not invalidate it', () async {
    // Manifests have to be able to grow. `url` is deliberately not signed:
    // a mirror serves the same catalog from a different address.
    expect(await verify({...manifestJson, 'url': 'https://mirror.test/x.gz'}),
        isTrue);
    expect(await verify({...manifestJson, 'addedInSomeFutureRelease': 1}), isTrue);
  });
}
