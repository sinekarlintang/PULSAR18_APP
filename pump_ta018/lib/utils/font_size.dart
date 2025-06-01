import 'package:flutter/material.dart';

class FontSizes {
  /// Ukuran font besar, cocok untuk angka utama (misal: heart rate)
  static double big(BuildContext context) {
    return MediaQuery.of(context).size.longestSide * 0.025; // sekitar 24 pada lebar 400
  }

  /// Ukuran font sedang, cocok untuk label utama
  static double medium(BuildContext context) {
    return MediaQuery.of(context).size.longestSide * 0.016; // sekitar 18
  }

  /// Ukuran font kecil, cocok untuk satuan seperti "bpm" atau teks sekunder
  static double small(BuildContext context) {
    return MediaQuery.of(context).size.longestSide * 0.014; // sekitar 14
  }

  /// Ukuran font tombol
  static double button(BuildContext context) {
    return MediaQuery.of(context).size.longestSide * 0.014; // sekitar 16
  }
}