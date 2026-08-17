import 'dart:async';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:http/http.dart' as http;
import 'package:path_provider/path_provider.dart';

/// Tiny disk cache for showcase previews.
///
/// The app is offline-first, so a preview must survive the next launch with no
/// network. Written by hand instead of pulling a cache package in: the whole
/// contract is four methods and it behaves identically on Android, Windows and
/// Linux.
class ImageCacheService {
  ImageCacheService({http.Client? client}) : _client = client ?? http.Client();

  final http.Client _client;
  final Map<String, Uint8List> _memory = {};
  final Map<String, Future<Uint8List?>> _inFlight = {};
  Directory? _directory;

  static const _maxMemoryEntries = 60;
  static const _maxBytes = 3 * 1024 * 1024;

  Future<Directory?> _cacheDirectory() async {
    if (kIsWeb) return null;
    if (_directory != null) return _directory;
    try {
      final base = await getApplicationSupportDirectory();
      final directory = Directory('${base.path}${Platform.pathSeparator}showcase_cache');
      if (!directory.existsSync()) directory.createSync(recursive: true);
      _directory = directory;
      return directory;
    } catch (_) {
      return null;
    }
  }

  /// Stable, filesystem-safe cache key. FNV-1a is enough here: this is a cache
  /// name, not a security primitive.
  static String keyFor(String url) {
    var hash = 0xcbf29ce484222325;
    for (final unit in url.codeUnits) {
      hash ^= unit;
      hash = (hash * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF;
    }
    return hash.toRadixString(16).padLeft(16, '0');
  }

  Uint8List? peek(String url) => _memory[keyFor(url)];

  /// Returns bytes from memory, then disk, then the network.
  Future<Uint8List?> load(String url, {bool allowNetwork = true}) {
    final key = keyFor(url);
    final cached = _memory[key];
    if (cached != null) return Future.value(cached);
    return _inFlight[key] ??= _load(url, key, allowNetwork).whenComplete(() => _inFlight.remove(key));
  }

  Future<Uint8List?> _load(String url, String key, bool allowNetwork) async {
    final directory = await _cacheDirectory();
    final file = directory == null ? null : File('${directory.path}${Platform.pathSeparator}$key');
    if (file != null && file.existsSync()) {
      try {
        final bytes = await file.readAsBytes();
        if (bytes.isNotEmpty) {
          _remember(key, bytes);
          return bytes;
        }
      } catch (_) {
        // A corrupt cache entry should simply be refetched.
      }
    }
    if (!allowNetwork) return null;

    try {
      final uri = Uri.tryParse(url);
      if (uri == null || uri.scheme != 'https') return null;
      final response = await _client.get(uri).timeout(const Duration(seconds: 20));
      if (response.statusCode != 200) return null;
      final bytes = response.bodyBytes;
      if (bytes.isEmpty || bytes.lengthInBytes > _maxBytes) return null;
      _remember(key, bytes);
      if (file != null) unawaited(file.writeAsBytes(bytes, flush: false).catchError((_) => file));
      return bytes;
    } catch (_) {
      return null;
    }
  }

  void _remember(String key, Uint8List bytes) {
    if (_memory.length >= _maxMemoryEntries) {
      _memory.remove(_memory.keys.first);
    }
    _memory[key] = bytes;
  }

  Future<int> cachedCount() async {
    final directory = await _cacheDirectory();
    if (directory == null) return _memory.length;
    try {
      return directory.listSync().whereType<File>().length;
    } catch (_) {
      return 0;
    }
  }

  Future<void> clear() async {
    _memory.clear();
    final directory = await _cacheDirectory();
    if (directory == null) return;
    try {
      for (final entity in directory.listSync().whereType<File>()) {
        entity.deleteSync();
      }
    } catch (_) {
      // Nothing to do: a locked file simply stays cached.
    }
  }
}
