import 'dart:async';

import 'package:flutter/gestures.dart';
import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';

typedef PacketsListener = void Function(int x, int y);

class StylusApi {
  static const MethodChannel _channel = MethodChannel('stylus_api');
  static final _listers = <PacketsListener>[];

  static Future<String?> get platformVersion async {
    final String? version = await _channel.invokeMethod('getPlatformVersion');
    return version;
  }

  static void createRts(BuildContext context) {
    _channel.setMethodCallHandler((call) async {
      if (call.method != "InAirPackets") {
        print(call);
        if (call.method == "StylusDown") {
          final args = (call.arguments as String).split(",");
          final x = double.parse(args[0]);
          final y = double.parse(args[1]);

          final mediaQuery = MediaQuery.of(context);
          final event = PointerDownEvent(
            kind: PointerDeviceKind.stylus,
            buttons: 0,
            position: Offset(x / mediaQuery.devicePixelRatio, y / mediaQuery.devicePixelRatio),
          );
          WidgetsBinding.instance?.handlePointerEvent(event);
        }
        if (call.method == "StylusUp") {
          final args = (call.arguments as String).split(",");
          final x = double.parse(args[0]);
          final y = double.parse(args[1]);

          final mediaQuery = MediaQuery.of(context);
          final event = PointerUpEvent(
            kind: PointerDeviceKind.stylus,
            buttons: 0,
            position: Offset(x / mediaQuery.devicePixelRatio, y / mediaQuery.devicePixelRatio),
          );
          WidgetsBinding.instance?.handlePointerEvent(event);
        }
        if (call.method == "Packets") {
          final args = (call.arguments as String).split(",");
          // for (final listener in _listers) {
          final x = double.parse(args[0]);
          final y = double.parse(args[1]);
          // final scaleX = double.parse(args[2]);
          // final scaleY = double.parse(args[3]);
          // listener(x.toInt(), y.toInt());

          final mediaQuery = MediaQuery.of(context);
          final event = PointerMoveEvent(
            kind: PointerDeviceKind.stylus,
            buttons: 0,
            position: Offset(x / mediaQuery.devicePixelRatio, y / mediaQuery.devicePixelRatio),
          );
          WidgetsBinding.instance?.handlePointerEvent(event);
          // }
        }
      }
    });
    _channel.invokeMethod('createRTS');
  }

  static void releaseRts(BuildContext context) {
    _channel.invokeMethod('releaseRTS');
  }

  static void addListener(PacketsListener listener) {
    _listers.add(listener);
  }

  static void removeListener(PacketsListener listener) {
    _listers.remove(listener);
  }
}
