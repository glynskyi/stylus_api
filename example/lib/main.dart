import 'dart:async';

import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:stylus_api/stylus_api.dart';
import 'package:stylus_api_example/stylus_painter.dart';

void main() {
  runApp(MaterialApp(home: const MyApp()));
}

class MyApp extends StatefulWidget {
  const MyApp({Key? key}) : super(key: key);

  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  String _platformVersion = 'Unknown';
  final _stylusRepaint = ChangeNotifier();
  late StylusPainter _stylusPainter;

  @override
  void initState() {
    super.initState();
    initPlatformState();
    _stylusPainter = StylusPainter(repaint: _stylusRepaint);
    StylusApi.addListener(_onPackets);
  }

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    final mediaQuery = MediaQuery.of(context);
    print("devicePixelRatio = ${mediaQuery.devicePixelRatio}");
  }

  // Platform messages are asynchronous, so we initialize in an async method.
  Future<void> initPlatformState() async {
    String platformVersion;
    // Platform messages may fail, so we use a try/catch PlatformException.
    // We also handle the message potentially returning null.
    try {
      platformVersion = await StylusApi.platformVersion ?? 'Unknown platform version';
    } on PlatformException {
      platformVersion = 'Failed to get platform version.';
    }

    // If the widget was removed from the tree while the asynchronous platform
    // message was in flight, we want to discard the reply rather than calling
    // setState to update our non-existent appearance.
    if (!mounted) return;

    setState(() {
      _platformVersion = platformVersion;
    });
  }

  void _onPackets(int x, int y) async {
    final mediaQuery = MediaQuery.of(context);
    setState(() {
      // _x = (x / windowSize.width * mediaQuery.size.width).toInt();
      // _y = (y / windowSize.height * mediaQuery.size.height).toInt();
      _stylusPainter.x = x / mediaQuery.devicePixelRatio;
      _stylusPainter.y = y / mediaQuery.devicePixelRatio;
      _stylusRepaint.notifyListeners();
    });

    final event = PointerMoveEvent(
      kind: PointerDeviceKind.stylus,
      position: Offset(x / mediaQuery.devicePixelRatio, y / mediaQuery.devicePixelRatio),
    );
    WidgetsBinding.instance?.handlePointerEvent(event);
  }

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onPanDown: (event) {
        print("onPanDown(kind: ${event}");
      },
      onPanUpdate: (event) {
        print("onPanUpdate(kind: ${event}");
      },
      child: Scaffold(
        // appBar: AppBar(
        //   title: const Text('Plugin example app'),
        // ),
        body: Stack(
          fit: StackFit.expand,
          children: [
            CustomPaint(painter: _stylusPainter),
            Center(
              child: Text('Running on: $_platformVersion\n'),
            ),
            Column(
              children: [
                ElevatedButton(
                  onPressed: () {
                    StylusApi.createRts(context);
                  },
                  child: Text("createRTS"),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}
