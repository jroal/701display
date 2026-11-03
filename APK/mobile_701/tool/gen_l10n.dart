// Generates lib/l10n/app_<locale>.arb from android/app/src/main/res/values*/strings.xml
// and runs `flutter gen-l10n`. Android's strings.xml is the single source of truth.
//
// Usage (from the project root): dart run tool/gen_l10n.dart
import 'dart:convert';
import 'dart:io';

final _resDir = Directory('android/app/src/main/res');
final _outDir = Directory('lib/l10n');

void main() async {
  final dirs = _resDir
      .listSync()
      .whereType<Directory>()
      .where((d) => RegExp(r'[/\\]values(-[A-Za-z0-9-]+)?$').hasMatch(d.path))
      .where((d) => File('${d.path}/strings.xml').existsSync());

  for (final stale in _outDir.listSync().whereType<File>().where(
    (f) => RegExp(r'app_.*\.arb$').hasMatch(f.path),
  )) {
    stale.deleteSync();
  }

  for (final dir in dirs) {
    final locale = _localeFor(dir.path.split(RegExp(r'[/\\]')).last);
    if (locale == null) continue; // values-night, values-v21, ...
    final arb = _convert(File('${dir.path}/strings.xml').readAsStringSync(), locale);
    File('${_outDir.path}/app_$locale.arb').writeAsStringSync(
      '${const JsonEncoder.withIndent('  ').convert(arb)}\n',
    );
    stdout.writeln('app_$locale.arb: ${arb.keys.where((k) => !k.startsWith('@')).length} strings');
  }

  final r = await Process.run('flutter', ['gen-l10n'], runInShell: true);
  stdout.write(r.stdout);
  stderr.write(r.stderr);
  exit(r.exitCode);
}

/// values -> en (default), values-fr -> fr, values-pt-rBR -> pt_BR.
String? _localeFor(String dirName) {
  if (dirName == 'values') return 'en';
  final m = RegExp(r'^values-([a-z]{2})(?:-r([A-Z]{2}))?$').firstMatch(dirName);
  if (m == null) return null;
  return m.group(2) == null ? m.group(1)! : '${m.group(1)}_${m.group(2)}';
}

String _camel(String snake) {
  final p = snake.split('_');
  return p.first + p.skip(1).map((s) => s[0].toUpperCase() + s.substring(1)).join();
}

String _unescape(String s) => s
    .replaceAll('&lt;', '<')
    .replaceAll('&gt;', '>')
    .replaceAll("\\'", "'")
    .replaceAll('\\"', '"')
    .replaceAll('\\@', '@')
    .replaceAll('\\n', '\n')
    .replaceAll('&amp;', '&');

final _fmt = RegExp(r'%(\d+)\$([sd])');

/// Converts Android format args (%1$s, %2$d, %%) to ARB placeholders.
(String, Map<String, Object>) _args(String text) {
  final placeholders = <String, Object>{};
  final out = text.replaceAllMapped(_fmt, (m) {
    final name = 'arg${m.group(1)}';
    placeholders[name] = {'type': m.group(2) == 'd' ? 'int' : 'String'};
    return '{$name}';
  }).replaceAll('%%', '%');
  final sorted = Map.fromEntries(
    placeholders.entries.toList()..sort((a, b) => a.key.compareTo(b.key)),
  );
  return (out, sorted);
}

Map<String, Object> _convert(String xml, String locale) {
  final arb = <String, Object>{'@@locale': locale};
  final entry = RegExp(
    r'<(string|plurals)\s+name="([^"]+)"([^>]*)>(.*?)</\1>',
    dotAll: true,
  );
  final noComments = xml.replaceAll(RegExp(r'<!--.*?-->', dotAll: true), '');
  for (final m in entry.allMatches(noComments)) {
    if (m.group(3)!.contains('translatable="false"') && locale != 'en') continue;
    final key = _camel(m.group(2)!);
    if (m.group(1) == 'string') {
      final (text, ph) = _args(_unescape(m.group(4)!.trim()));
      arb[key] = text;
      if (ph.isNotEmpty) arb['@$key'] = {'placeholders': ph};
    } else {
      final cases = <String>[];
      var placeholders = <String, Object>{};
      for (final i in RegExp(r'<item quantity="(\w+)">(.*?)</item>', dotAll: true)
          .allMatches(m.group(4)!)) {
        final (text, ph) = _args(_unescape(i.group(2)!.trim()));
        placeholders = {...placeholders, ...ph};
        final q = i.group(1)!;
        cases.add('${q == 'zero' ? '=0' : q}{$text}');
      }
      // The first %N$d placeholder drives the plural selection.
      final count = placeholders.keys.first;
      arb[key] = '{$count, plural, ${cases.join(' ')}}';
      arb['@$key'] = {'placeholders': placeholders};
    }
  }
  return arb;
}
