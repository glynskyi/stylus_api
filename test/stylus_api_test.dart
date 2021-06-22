import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:stylus_api/stylus_api.dart';

void main() {
  const MethodChannel channel = MethodChannel('stylus_api');

  TestWidgetsFlutterBinding.ensureInitialized();

  setUp(() {
    channel.setMockMethodCallHandler((MethodCall methodCall) async {
      return '42';
    });
  });

  tearDown(() {
    channel.setMockMethodCallHandler(null);
  });

  test('getPlatformVersion', () async {
    expect(await StylusApi.platformVersion, '42');
  });
}
