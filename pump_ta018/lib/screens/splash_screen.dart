import 'package:flutter/material.dart';
import 'dart:async';

class SplashScreen extends StatelessWidget {
  const SplashScreen({super.key});

  @override
  Widget build(BuildContext context) {
    Timer(const Duration(seconds: 3), () {
      Navigator.pushReplacementNamed(context, '/main');
    });

    return Scaffold(
      body: Stack(
        children: [
          // Border atas (normal)
          Align(
            alignment: Alignment.topCenter,
            child: Opacity(
              opacity: 0.55,
              child: Transform(
                alignment: Alignment.center,
                transform: Matrix4.identity()..scale(-1.0, 1.0), // Flip vertikal
                child: Image.asset(
                  'assets/image/border.png',
                  fit: BoxFit.fill,
                  width: double.infinity,
                  height: 300,
                ),
              ),
            ),
          ),
          // Border bawah (flip vertikal)
          Align(
            alignment: Alignment.bottomCenter,
            child: Opacity(
              opacity: 0.55,
              child: Transform(
                alignment: Alignment.center,
                transform: Matrix4.identity()..scale(1.0, -1.0), // Flip vertikal
                child: Image.asset(
                  'assets/image/border.png',
                  fit: BoxFit.fill,
                  width: double.infinity,
                  height: 300,
                ),
              ),
            ),
          ),
          // Logo di tengah
          Center(
            child: Image.asset('assets/image/logo_itb.png', width: 350),
          ),
        ],
      ),
    );
  }
}
