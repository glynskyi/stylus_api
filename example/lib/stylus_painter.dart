import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';

class StylusPainter extends CustomPainter {
  var x = 0.0;
  var y = 0.0;

  final _paint = Paint()..color = Colors.redAccent;

  StylusPainter({required Listenable repaint}) : super(repaint: repaint);

  @override
  void paint(Canvas canvas, Size size) {
    canvas.drawCircle(Offset(x, y), 2, _paint);
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) {
    return true;
  }
}
