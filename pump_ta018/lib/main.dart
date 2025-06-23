import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'screens/splash_screen.dart';
import 'screens/main_screen.dart';
import 'screens/modify_mode_screen.dart';
import 'utils/bluetooth_service.dart';
import 'utils/font_size.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MultiProvider(
      providers: [
        ChangeNotifierProvider(
          create: (context) => BluetoothService(),
        ),
      ],
      child: MaterialApp(
        title: 'Pump Controller',
        debugShowCheckedModeBanner: false,
        theme: ThemeData(
          primarySwatch: Colors.blue,
          fontFamily: 'Inter',
          useMaterial3: true, 
        ),
        initialRoute: '/',
        routes: {
          '/': (context) => const SplashScreen(),
          '/main': (context) => const MainScreen(),
          '/modify': (context) => const ModifyModeScreen(),
        },
      ),
    );
  }
}