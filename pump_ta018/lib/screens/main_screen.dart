import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:pump_ta018/utils/font_size.dart';
import 'package:pump_ta018/utils/bluetooth_service.dart';
import 'package:flutter_bluetooth_serial/flutter_bluetooth_serial.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:fl_chart/fl_chart.dart';

class MainScreen extends StatefulWidget {
  const MainScreen({super.key});

  @override
  State<MainScreen> createState() => _MainScreenState();
}

class _MainScreenState extends State<MainScreen> {
  // --- STATE VARIABLES ------------------------------------------------------
  String _selectedMode = 'Otomatis';
  bool _isStarted = false;
  
  // Data untuk grafik pressure
  List<FlSpot> _pressureData = [];
  double _timeCounter = 0;
  static const int maxDataPoints = 100; // Maksimum titik data yang ditampilkan

  @override
  void initState() {
    super.initState();
    requestPermissions();
    
    WidgetsBinding.instance.addPostFrameCallback((_) {
      final bluetoothService = Provider.of<BluetoothService>(context, listen: false);
      
      // Listen to available modes stream
      bluetoothService.availableModesStream.listen((modes) {
        if (modes.isNotEmpty && mounted) {
          setState(() {
            // Update selected mode if current one is not in the new list
            if (!modes.contains(_selectedMode)) {
              _selectedMode = modes.first;
            }
          });
        }
      });
      
      if (bluetoothService.isConnected) {
        bluetoothService.getParameters();
        bluetoothService.getAvailableModes();
      }
    });
  }

  Future<void> requestPermissions() async {
    if (await Permission.bluetoothScan.request().isGranted &&
        await Permission.bluetoothConnect.request().isGranted &&
        await Permission.location.request().isGranted) {
      print("All permissions granted");
    } else {
      print("Permission denied");
    }
  }

  void _updatePressureData(double pressure) {
    setState(() {
      _pressureData.add(FlSpot(_timeCounter, pressure));
      _timeCounter += 0.1; // Increment waktu (misalnya setiap 100ms)
      
      // Hapus data lama jika melebihi batas maksimum
      if (_pressureData.length > maxDataPoints) {
        _pressureData.removeAt(0);
        // Sesuaikan ulang x-axis agar tetap berurutan
        for (int i = 0; i < _pressureData.length; i++) {
          _pressureData[i] = FlSpot(i * 0.1, _pressureData[i].y);
        }
        _timeCounter = _pressureData.length * 0.1;
      }
    });
  }

  Future<void> _connectToBluetooth() async {
    final bluetoothService = Provider.of<BluetoothService>(context, listen: false);
    
    // Get available devices
    final devices = await bluetoothService.scanDevices();
    
    if (devices.isEmpty) {
      _showMessage('No Bluetooth devices found');
      return;
    }

    // Show device selection dialog
    final selectedDevice = await _showDeviceSelectionDialog(devices);
    if (selectedDevice != null) {
      final success = await bluetoothService.connectToDevice(selectedDevice);
      if (success) {
        _showMessage('Connected successfully');
        // Request available modes after successful connection
        await bluetoothService.getAvailableModes();
      } else {
        _showMessage('Connection failed');
      }
    }
  }

  Future<BluetoothDevice?> _showDeviceSelectionDialog(List<BluetoothDevice> devices) async {
    return showDialog<BluetoothDevice>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Select Bluetooth Device'),
        content: SizedBox(
          width: double.maxFinite,
          child: ListView.builder(
            shrinkWrap: true,
            itemCount: devices.length,
            itemBuilder: (context, index) {
              final device = devices[index];
              return ListTile(
                title: Text(device.name ?? 'Unknown Device'),
                subtitle: Text(device.address),
                onTap: () => Navigator.pop(context, device),
              );
            },
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Cancel'),
          ),
        ],
      ),
    );
  }

  void _showMessage(String message) {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text(message)),
    );
  }

  Future<void> _onModeChanged(String newMode) async {
    final bluetoothService = Provider.of<BluetoothService>(context, listen: false);
    
    setState(() => _selectedMode = newMode);
    
    // Send mode to ESP32 if connected
    if (bluetoothService.isConnected) {
      await bluetoothService.setMode(newMode);
    }
  }

  Future<void> _onStartStopPressed() async {
    final bluetoothService = Provider.of<BluetoothService>(context, listen: false);
    
    if (!bluetoothService.isConnected) {
      await _connectToBluetooth();
      return;
    }

    setState(() {
      _isStarted = !_isStarted;
      if (!_isStarted) {
        // Reset data grafik ketika stop
        _pressureData.clear();
        _timeCounter = 0;
      }
    });
    
    // Send startPump parameter (1 for start, 0 for stop)
    int startPumpValue = _isStarted ? 1 : 0;
    await bluetoothService.setStartStop(startPumpValue);
    
    if (_isStarted) {
      // Send current mode and get parameters
      await bluetoothService.setMode(_selectedMode);
      await bluetoothService.getParameters();
    }
  }

  Widget _buildPressureChart(PumpParameters parameters) {
    // Always update data grafik dengan pressure terbaru (continuously monitor pressureActual)
    if (parameters.pressureActual > 0) {
      // Hanya update jika ada perubahan nilai yang signifikan
      if (_pressureData.isEmpty || 
          (_pressureData.last.y - parameters.pressureActual).abs() > 0.1) {
        WidgetsBinding.instance.addPostFrameCallback((_) {
          _updatePressureData(parameters.pressureActual.toDouble());
        });
      }
    }

    return Container(
      decoration: BoxDecoration(
        border: Border.all(color: Colors.grey),
        borderRadius: BorderRadius.circular(8),
      ),
      padding: const EdgeInsets.all(16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            'Real-time Pressure Monitor',
            style: TextStyle(
              fontSize: FontSizes.medium(context),
              fontWeight: FontWeight.w600,
              fontFamily: 'Inter',
            ),
          ),
          const SizedBox(height: 8),
          Text(
            'Current: ${parameters.pressureActual.toStringAsFixed(1)} mmHg',
            style: TextStyle(
              fontSize: FontSizes.small(context),
              fontWeight: FontWeight.w500,
              fontFamily: 'Inter',
              color: Colors.blue[700],
            ),
          ),
          const SizedBox(height: 16),
          Expanded(
            child: _pressureData.isEmpty
                ? const Center(
                    child: Text(
                      'Waiting for pressure data...',
                      style: TextStyle(
                        color: Colors.grey,
                        fontSize: 16,
                      ),
                    ),
                  )
                : LineChart(
                    LineChartData(
                      gridData: FlGridData(
                        show: true,
                        drawVerticalLine: true,
                        horizontalInterval: 20,
                        verticalInterval: 2,
                        getDrawingHorizontalLine: (value) {
                          return FlLine(
                            color: Colors.grey.withOpacity(0.3),
                            strokeWidth: 1,
                          );
                        },
                        getDrawingVerticalLine: (value) {
                          return FlLine(
                            color: Colors.grey.withOpacity(0.3),
                            strokeWidth: 1,
                          );
                        },
                      ),
                      titlesData: FlTitlesData(
                        show: true,
                        rightTitles: AxisTitles(
                          sideTitles: SideTitles(showTitles: false),
                        ),
                        topTitles: AxisTitles(
                          sideTitles: SideTitles(showTitles: false),
                        ),
                        bottomTitles: AxisTitles(
                          axisNameWidget: const Text(
                            'Time (s)',
                            style: TextStyle(fontSize: 12, color: Colors.grey),
                          ),
                          sideTitles: SideTitles(
                            showTitles: true,
                            reservedSize: 30,
                            interval: 2,
                            getTitlesWidget: (value, meta) {
                              return Text(
                                value.toStringAsFixed(1),
                                style: const TextStyle(
                                  color: Colors.grey,
                                  fontSize: 10,
                                ),
                              );
                            },
                          ),
                        ),
                        leftTitles: AxisTitles(
                          axisNameWidget: const Text(
                            'Pressure (mmHg)',
                            style: TextStyle(fontSize: 12, color: Colors.grey),
                          ),
                          sideTitles: SideTitles(
                            showTitles: true,
                            reservedSize: 50,
                            interval: 20,
                            getTitlesWidget: (value, meta) {
                              return Text(
                                value.toInt().toString(),
                                style: const TextStyle(
                                  color: Colors.grey,
                                  fontSize: 10,
                                ),
                              );
                            },
                          ),
                        ),
                      ),
                      borderData: FlBorderData(
                        show: true,
                        border: Border.all(color: Colors.grey.withOpacity(0.3)),
                      ),
                      minX: _pressureData.isNotEmpty ? _pressureData.first.x : 0,
                      maxX: _pressureData.isNotEmpty ? _pressureData.last.x : 10,
                      minY: 0,
                      maxY: 200, // Sesuaikan dengan range pressure yang diharapkan
                      lineBarsData: [
                        LineChartBarData(
                          spots: _pressureData,
                          isCurved: true,
                          color: Colors.blue,
                          barWidth: 2,
                          isStrokeCapRound: true,
                          dotData: FlDotData(show: false),
                          belowBarData: BarAreaData(
                            show: true,
                            color: Colors.blue.withOpacity(0.1),
                          ),
                        ),
                      ],
                      lineTouchData: LineTouchData(
                        enabled: true,
                        touchTooltipData: LineTouchTooltipData(
                          tooltipBgColor: Colors.black87,
                          getTooltipItems: (List<LineBarSpot> touchedBarSpots) {
                            return touchedBarSpots.map((barSpot) {
                              return LineTooltipItem(
                                '${barSpot.y.toStringAsFixed(1)} mmHg\n${barSpot.x.toStringAsFixed(1)}s',
                                const TextStyle(
                                  color: Colors.white,
                                  fontWeight: FontWeight.bold,
                                ),
                              );
                            }).toList();
                          },
                        ),
                      ),
                    ),
                  ),
          ),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final screenHeight = MediaQuery.of(context).size.height;
    final dropdownFontSize = screenHeight * 0.02;
    final dropdownItemHeight = screenHeight * 0.1;
    final size = MediaQuery.of(context).size;

    return Scaffold(
      body: Consumer<BluetoothService>(
        builder: (context, bluetoothService, child) {
          // Use available modes from bluetooth service, with fallback
          final availableModes = bluetoothService.availableModes.isNotEmpty 
              ? bluetoothService.availableModes 
              : ['Otomatis']; // Fallback mode if no modes available
          
          // Ensure selected mode is valid
          final currentSelectedMode = availableModes.contains(_selectedMode) 
              ? _selectedMode 
              : availableModes.first;

          return Stack(
            children: [
              // ===== BORDER GAMBAR DI BAGIAN ATAS =====
              Align(
                alignment: Alignment.topCenter,
                child: Opacity(
                  opacity: 0.55,
                  child: Transform(
                    alignment: Alignment.center,
                    transform: Matrix4.identity()..scale(-1.0, 1.0),
                    child: Image.asset(
                      'assets/image/border.png',
                      fit: BoxFit.fill,
                      width: double.infinity,
                      height: 300,
                    ),
                  ),
                ),
              ),

              // ===== KONTEN UTAMA =====
              Align(
                alignment: Alignment.topCenter,
                child: Padding(
                  padding: const EdgeInsets.all(30),
                  child: SizedBox(
                    height: size.height,
                    child: Column(
                      children: [
                        // ---------- LOGO DERET ----------
                        Row(
                          children: [
                            Image.asset(
                              'assets/image/logo_itb.png',
                              width: size.longestSide * 0.06,
                            ),
                            const SizedBox(width: 10),
                            Image.asset(
                              'assets/image/logo_stei.png',
                              width: size.longestSide * 0.04,
                            ),
                            const SizedBox(width: 10),
                            Image.asset(
                              'assets/image/PULSAR18.png',
                              width: size.longestSide * 0.1,
                            ),
                          ],
                        ),
                        const SizedBox(height: 20),

                        // ---------- BODY ----------
                        Expanded(
                          child: Row(
                            children: [
                              // ======== PANEL KIRI (40 %) ========
                              Expanded(
                                flex: 2,
                                child: Column(
                                  crossAxisAlignment: CrossAxisAlignment.start,
                                  children: [
                                    // --- Mode & Dropdown ---
                                    Text(
                                      'Mode:',
                                      style: TextStyle(
                                        fontSize: FontSizes.medium(context),
                                        fontWeight: FontWeight.w600,
                                        fontFamily: 'Inter',
                                      ),
                                    ),
                                    const SizedBox(height: 8),
                                    DropdownButton<String>(
                                      value: currentSelectedMode,
                                      isExpanded: true,
                                      borderRadius: BorderRadius.circular(8),
                                      icon: const Icon(Icons.arrow_drop_down),
                                      style: TextStyle(
                                        fontSize: FontSizes.medium(context),
                                        fontWeight: FontWeight.w600,
                                        fontFamily: 'Inter',
                                        color: Colors.black,
                                      ),
                                      items: availableModes.map((mode) {
                                        return DropdownMenuItem<String>(
                                          value: mode,
                                          child: SizedBox(
                                            height: dropdownItemHeight,
                                            child: Align(
                                              alignment: Alignment.centerLeft,
                                              child: Text(
                                                mode,
                                                style: TextStyle(fontSize: dropdownFontSize),
                                              ),
                                            ),
                                          ),
                                        );
                                      }).toList(),
                                      onChanged: (String? newValue) {
                                        if (newValue != null) {
                                          _onModeChanged(newValue);
                                        }
                                      },
                                    ),
                                    const SizedBox(height: 10),

                                    // Show connection status for modes
                                    if (!bluetoothService.isConnected)
                                      Container(
                                        padding: const EdgeInsets.all(8),
                                        decoration: BoxDecoration(
                                          color: Colors.orange.withOpacity(0.1),
                                          borderRadius: BorderRadius.circular(4),
                                          border: Border.all(color: Colors.orange),
                                        ),
                                        child: Row(
                                          children: [
                                            Icon(Icons.info_outline, 
                                                 color: Colors.orange, size: 16),
                                            const SizedBox(width: 8),
                                            Expanded(
                                              child: Text(
                                                'Connect to device to load available modes',
                                                style: TextStyle(
                                                  fontSize: FontSizes.small(context),
                                                  color: Colors.orange[700],
                                                ),
                                              ),
                                            ),
                                          ],
                                        ),
                                      ),
                                    const SizedBox(height: 10),

                                    // --- PARAMETER & NILAI ---
                                    _buildParameterPanels(context, bluetoothService.parameters),
                                    const SizedBox(height: 10),

                                    // --- TOMBOL START/STOP ---
                                    ElevatedButton(
                                      onPressed: _onStartStopPressed,
                                      style: ElevatedButton.styleFrom(
                                        backgroundColor: _isStarted ? Colors.red : Colors.blue,
                                        foregroundColor: Colors.white,
                                        shape: RoundedRectangleBorder(
                                          borderRadius: BorderRadius.circular(4),
                                        ),
                                        padding: const EdgeInsets.symmetric(
                                            horizontal: 24, vertical: 12),
                                      ),
                                      child: Text(
                                        bluetoothService.isConnected 
                                          ? (_isStarted ? 'STOP' : 'START')
                                          : 'CONNECT',
                                        style: TextStyle(
                                          fontSize: FontSizes.small(context),
                                          fontWeight: FontWeight.w600,
                                          fontFamily: 'Inter',
                                        ),
                                      ),
                                    ),
                                  ],
                                ),
                              ),
                              const SizedBox(width: 20),
                              // ======== PANEL KANAN (60 %) ========
                              Expanded(
                                flex: 3,
                                child: Column(
                                  crossAxisAlignment: CrossAxisAlignment.start,
                                  children: [
                                    // Grafik pressure real-time
                                    Expanded(
                                      child: _buildPressureChart(bluetoothService.parameters),
                                    ),
                                    const SizedBox(height: 10),
                                    // Tombol customise + status
                                    Row(
                                      mainAxisAlignment: MainAxisAlignment.spaceBetween,
                                      children: [
                                        ElevatedButton(
                                          onPressed: () => Navigator.pushNamed(context, '/modify'),
                                          style: ElevatedButton.styleFrom(
                                            foregroundColor: Colors.blue,
                                            shape: RoundedRectangleBorder(
                                              borderRadius: BorderRadius.circular(4),
                                            ),
                                            padding: const EdgeInsets.symmetric(
                                                horizontal: 24, vertical: 12),
                                          ),
                                          child: Text(
                                            'Customize Mode',
                                            style: TextStyle(
                                              fontSize: FontSizes.small(context),
                                              fontWeight: FontWeight.w400,
                                              fontFamily: 'Inter',
                                            ),
                                          ),
                                        ),
                                        Container(
                                          padding: const EdgeInsets.symmetric(
                                              horizontal: 12, vertical: 6),
                                          decoration: BoxDecoration(
                                            color: bluetoothService.isConnected 
                                              ? Colors.green.withOpacity(0.1)
                                              : Colors.red.withOpacity(0.1),
                                            borderRadius: BorderRadius.circular(12),
                                            border: Border.all(
                                              color: bluetoothService.isConnected 
                                                ? Colors.green
                                                : Colors.red,
                                            ),
                                          ),
                                          child: Row(
                                            mainAxisSize: MainAxisSize.min,
                                            children: [
                                              Container(
                                                width: 8,
                                                height: 8,
                                                decoration: BoxDecoration(
                                                  color: bluetoothService.isConnected 
                                                    ? Colors.green
                                                    : Colors.red,
                                                  shape: BoxShape.circle,
                                                ),
                                              ),
                                              const SizedBox(width: 6),
                                              Text(
                                                bluetoothService.connectionStatus,
                                                style: TextStyle(
                                                  color: bluetoothService.isConnected 
                                                    ? Colors.green[700]
                                                    : Colors.red[700],
                                                  fontWeight: FontWeight.w500,
                                                ),
                                              ),
                                            ],
                                          ),
                                        ),
                                      ],
                                    ),
                                  ],
                                ),
                              ),
                            ],
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
              ),
            ],
          );
        },
      ),
    );
  }

  /// Pisahkan bagian parameter‑parameter supaya rapih dibaca.
  Widget _buildParameterPanels(BuildContext context, PumpParameters parameters) {
    return Row(
      children: [
        // ---------- Kolom kiri ----------
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text('Heart Rate:', style: TextStyle(fontSize: FontSizes.medium(context),fontWeight: FontWeight.w600,fontFamily: 'Inter',),),
              Row(crossAxisAlignment: CrossAxisAlignment.center,children: [
                Text('${parameters.heartRate}',style: TextStyle(fontSize: FontSizes.big(context),fontWeight: FontWeight.w600, fontFamily: 'Inter', ), ),
                const SizedBox(width: 10),
                Text('BPM', style: TextStyle( fontSize: FontSizes.small(context), fontWeight: FontWeight.w500, fontFamily: 'Inter', color: Colors.grey[700], ), ),
              ],),
              Text('Systolic Peak:', style: TextStyle(fontSize: FontSizes.medium(context),fontWeight: FontWeight.w600,fontFamily: 'Inter',),),
              Text('Pressure', style: TextStyle( fontSize: FontSizes.small(context), fontWeight: FontWeight.w500, fontFamily: 'Inter', color: Colors.grey[700], ), ),
              Row(crossAxisAlignment: CrossAxisAlignment.center,children: [
                Text('${parameters.systolicPressure}',style: TextStyle(fontSize: FontSizes.big(context),fontWeight: FontWeight.w600, fontFamily: 'Inter', ), ),
                const SizedBox(width: 10),
                Text('mmhg', style: TextStyle( fontSize: FontSizes.small(context), fontWeight: FontWeight.w500, fontFamily: 'Inter', color: Colors.grey[700], ), ),
              ],),
              Text('Diastolic Peak:', style: TextStyle(fontSize: FontSizes.medium(context),fontWeight: FontWeight.w600,fontFamily: 'Inter',),),
              Text('Pressure', style: TextStyle( fontSize: FontSizes.small(context), fontWeight: FontWeight.w500, fontFamily: 'Inter', color: Colors.grey[700], ), ),
              Row(crossAxisAlignment: CrossAxisAlignment.center,children: [
                Text('${parameters.diastolicPressure}',style: TextStyle(fontSize: FontSizes.big(context),fontWeight: FontWeight.w600, fontFamily: 'Inter', ), ),
                const SizedBox(width: 10),
                Text('mmhg', style: TextStyle( fontSize: FontSizes.small(context), fontWeight: FontWeight.w500, fontFamily: 'Inter', color: Colors.grey[700], ), ),
              ],),
              Text('Diastolic Base Pressure', style: TextStyle( fontSize: FontSizes.small(context), fontWeight: FontWeight.w500, fontFamily: 'Inter', color: Colors.grey[700], ), ),
              Row(crossAxisAlignment: CrossAxisAlignment.center,children: [
                Text('${parameters.basePressure}',style: TextStyle(fontSize: FontSizes.big(context),fontWeight: FontWeight.w600, fontFamily: 'Inter', ), ),
                const SizedBox(width: 10),
                Text('mmhg', style: TextStyle( fontSize: FontSizes.small(context), fontWeight: FontWeight.w500, fontFamily: 'Inter', color: Colors.grey[700], ), ),
              ],),
              Text('Systolic/Diastolic Period:', style: TextStyle(fontSize: FontSizes.medium(context),fontWeight: FontWeight.w600,fontFamily: 'Inter',),),
              Text('Ratio', style: TextStyle( fontSize: FontSizes.small(context), fontWeight: FontWeight.w500, fontFamily: 'Inter', color: Colors.grey[700], ), ),
              Row(crossAxisAlignment: CrossAxisAlignment.center,children: [
                Text('${parameters.systolicPeriod}',style: TextStyle(fontSize: FontSizes.big(context),fontWeight: FontWeight.w600, fontFamily: 'Inter', ), ),
                const SizedBox(width: 10),
                Text('%', style: TextStyle( fontSize: FontSizes.small(context), fontWeight: FontWeight.w500, fontFamily: 'Inter', color: Colors.grey[700], ), ),
                const SizedBox(width: 10),
                Text('/',style: TextStyle(fontSize: FontSizes.big(context),fontWeight: FontWeight.w600, fontFamily: 'Inter', ), ),
                const SizedBox(width: 10),
                Text('${parameters.diastolicPeriod}',style: TextStyle(fontSize: FontSizes.big(context),fontWeight: FontWeight.w600, fontFamily: 'Inter', ), ),
                const SizedBox(width: 10),
                Text('%', style: TextStyle( fontSize: FontSizes.small(context), fontWeight: FontWeight.w500, fontFamily: 'Inter', color: Colors.grey[700], ), ),
                const SizedBox(width: 10),
              ],),
            ],
          ),
        ),

        // ---------- Kolom kanan ----------
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text('Flow Rate:', style: TextStyle(fontSize: FontSizes.medium(context),fontWeight: FontWeight.w600,fontFamily: 'Inter',),),
              Row(crossAxisAlignment: CrossAxisAlignment.center,children: [
                Text('${parameters.flowRate}',style: TextStyle(fontSize: FontSizes.big(context),fontWeight: FontWeight.w600, fontFamily: 'Inter', ), ),
                const SizedBox(width: 10),
                Text('ml/min', style: TextStyle( fontSize: FontSizes.small(context), fontWeight: FontWeight.w500, fontFamily: 'Inter', color: Colors.grey[700], ), ),
              ],),
              Text('Systolic Peak:', style: TextStyle(fontSize: FontSizes.medium(context),fontWeight: FontWeight.w600,fontFamily: 'Inter',color: Colors.white, ),),
              Text('Time', style: TextStyle( fontSize: FontSizes.small(context), fontWeight: FontWeight.w500, fontFamily: 'Inter', color: Colors.grey[700], ), ),
              Row(crossAxisAlignment: CrossAxisAlignment.center,children: [
                Text('${parameters.systolicPeakTime}',style: TextStyle(fontSize: FontSizes.big(context),fontWeight: FontWeight.w600, fontFamily: 'Inter', ), ),
                const SizedBox(width: 10),
                Text('ms', style: TextStyle( fontSize: FontSizes.small(context), fontWeight: FontWeight.w500, fontFamily: 'Inter', color: Colors.grey[700], ), ),
              ],),
              Text('Diastolic Peak:', style: TextStyle(fontSize: FontSizes.medium(context),fontWeight: FontWeight.w600,fontFamily: 'Inter',color: Colors.white, ),),
              Text('Time', style: TextStyle( fontSize: FontSizes.small(context), fontWeight: FontWeight.w500, fontFamily: 'Inter', color: Colors.grey[700], ), ),
              Row(crossAxisAlignment: CrossAxisAlignment.center,children: [
                Text('${parameters.diastolicPeakTime}',style: TextStyle(fontSize: FontSizes.big(context),fontWeight: FontWeight.w600, fontFamily: 'Inter', ), ),
                const SizedBox(width: 10),
                Text('ms', style: TextStyle( fontSize: FontSizes.small(context), fontWeight: FontWeight.w500, fontFamily: 'Inter', color: Colors.grey[700], ), ),
              ],),
              Text('Diastolic Base Pressure', style: TextStyle( fontSize: FontSizes.small(context), fontWeight: FontWeight.w500, fontFamily: 'Inter',color: Colors.white, ), ),
              Row(crossAxisAlignment: CrossAxisAlignment.center,children: [
                Text('${parameters.basePressure}',style: TextStyle(fontSize: FontSizes.big(context),fontWeight: FontWeight.w600, fontFamily: 'Inter', color: Colors.white,), ),
                const SizedBox(width: 10),
                Text('mmhg', style: TextStyle( fontSize: FontSizes.small(context), fontWeight: FontWeight.w500, fontFamily: 'Inter', color: Colors.white, ), ),
              ],),
              Text('Notch:', style: TextStyle(fontSize: FontSizes.medium(context),fontWeight: FontWeight.w600,fontFamily: 'Inter',),),
              Text('Pressure', style: TextStyle( fontSize: FontSizes.small(context), fontWeight: FontWeight.w500, fontFamily: 'Inter', color: Colors.grey[700], ), ),
              Row(crossAxisAlignment: CrossAxisAlignment.center,children: [
                Text('${parameters.notchPressure}',style: TextStyle(fontSize: FontSizes.big(context),fontWeight: FontWeight.w600, fontFamily: 'Inter', ), ),
                const SizedBox(width: 10),
                Text('mmhg', style: TextStyle( fontSize: FontSizes.small(context), fontWeight: FontWeight.w500, fontFamily: 'Inter', color: Colors.grey[700], ), ),
                
              ],),
            ],
          ),
        ),
      ],
    );
  }
}