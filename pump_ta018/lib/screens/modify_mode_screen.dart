// modify_mode_screen.dart
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:pump_ta018/utils/font_size.dart';
import 'package:pump_ta018/utils/bluetooth_service.dart';
import 'package:flutter_bluetooth_serial/flutter_bluetooth_serial.dart';
import 'package:permission_handler/permission_handler.dart';
import 'dart:math' as math;

class ModifyModeScreen extends StatefulWidget {
  const ModifyModeScreen({super.key});

  @override
  State<ModifyModeScreen> createState() => _ModifyModeScreenState();
}

class _ModifyModeScreenState extends State<ModifyModeScreen> {
  // --- STATE ---------------------------------------------------------------
  String? _selectedMode;
  bool _isConnected = false;
  bool _isOpenLoop(String? mode) {
    final bluetoothService = Provider.of<BluetoothService>(context, listen: false);
    if (bluetoothService.parameters.closeloop == 0) {
      return true;
    }
    return false; // Default to closed loop if unknown
  }
  
  // Track which fields are being edited to prevent overwriting user input
  final Set<String> _activelyEditingFields = {};
  
  // Controllers for parameter editing
  final Map<String, TextEditingController> _controllers = {
    'heartRate': TextEditingController(),
    'systolicPressure': TextEditingController(),
    'diastolicPressure': TextEditingController(),
    'systolicPeriod': TextEditingController(),
    'diastolicPeriod': TextEditingController(),
    'notchPressure': TextEditingController(),
    'systolicPeakTime': TextEditingController(),
    'diastolicPeakTime': TextEditingController(),
    'flowRate': TextEditingController(),
    'basePressure': TextEditingController(),
    // New open loop parameters
    'sysPWM': TextEditingController(),
    'disPWM': TextEditingController(),
    'sysPeriod': TextEditingController(),
    'disPeriod': TextEditingController(),
    'sysHighPercent': TextEditingController(),
    'disHighPercent': TextEditingController(),
  };

  // Focus nodes to track when fields are being edited
  final Map<String, FocusNode> _focusNodes = {
    'heartRate': FocusNode(),
    'systolicPressure': FocusNode(),
    'diastolicPressure': FocusNode(),
    'systolicPeriod': FocusNode(),
    'diastolicPeriod': FocusNode(),
    'notchPressure': FocusNode(),
    'systolicPeakTime': FocusNode(),
    'diastolicPeakTime': FocusNode(),
    'flowRate': FocusNode(),
    'basePressure': FocusNode(),
    // New open loop focus nodes
    'sysPWM': FocusNode(),
    'disPWM': FocusNode(),
    'sysPeriod': FocusNode(),
    'disPeriod': FocusNode(),
    'sysHighPercent': FocusNode(),
    'disHighPercent': FocusNode(),
  };

  @override
  void initState() {
    super.initState();
    
    // Add listeners to controllers for real-time graph updates
    _controllers.forEach((key, controller) {
      controller.addListener(() {
        if (mounted) {
          setState(() {}); // Trigger rebuild to update graph
        }
      });
    });

    // Add focus listeners to track actively editing fields
    _focusNodes.forEach((key, focusNode) {
      focusNode.addListener(() {
        if (focusNode.hasFocus) {
          _activelyEditingFields.add(key);
        } else {
          // Small delay to ensure user has finished editing
          Future.delayed(Duration(milliseconds: 500), () {
            _activelyEditingFields.remove(key);
          });
        }
      });
    });
    
    WidgetsBinding.instance.addPostFrameCallback((_) {
      final bluetoothService = Provider.of<BluetoothService>(context, listen: false);
      
      // Initialize connection state
      _isConnected = bluetoothService.isConnected;
      
      // Listen to available modes stream
      bluetoothService.availableModesStream.listen((modes) {
        if (mounted) {
          setState(() {
            // Update selected mode if current one is not in the new list
            if (modes.isNotEmpty && !modes.contains(_selectedMode)) {
              _selectedMode = modes.first;
              _loadParametersForMode(_selectedMode!);
            }
          });
        }
      });
      
      // Listen to parameters stream - but only update if user is not actively editing
      bluetoothService.parametersStream.listen((parameters) {
        if (mounted && _selectedMode == parameters.pumpMode) {
          _updateControllersFromParameters(parameters);
        }
      });
      
      // Load initial data if connected
      if (_isConnected) {
        bluetoothService.getAvailableModes();
        if (bluetoothService.availableModes.isNotEmpty) {
          _selectedMode = bluetoothService.availableModes.first;
          _loadParametersForMode(_selectedMode!);
        }
      }
    });
  }

  @override
  void dispose() {
    // Dispose all controllers and focus nodes
    _controllers.values.forEach((controller) => controller.dispose());
    _focusNodes.values.forEach((focusNode) => focusNode.dispose());
    super.dispose();
  }

  void _updateControllersFromParameters(PumpParameters parameters) {
    // Only update fields that are not currently being edited
    if (!_activelyEditingFields.contains('heartRate')) {
      _controllers['heartRate']?.text = parameters.heartRate.toString();
    }
    if (!_activelyEditingFields.contains('systolicPressure')) {
      _controllers['systolicPressure']?.text = parameters.systolicPressure.toString();
    }
    if (!_activelyEditingFields.contains('diastolicPressure')) {
      _controllers['diastolicPressure']?.text = parameters.diastolicPressure.toString();
    }
    if (!_activelyEditingFields.contains('systolicPeriod')) {
      _controllers['systolicPeriod']?.text = parameters.systolicPeriod.toString();
    }
    if (!_activelyEditingFields.contains('diastolicPeriod')) {
      _controllers['diastolicPeriod']?.text = parameters.diastolicPeriod.toString();
    }
    if (!_activelyEditingFields.contains('notchPressure')) {
      _controllers['notchPressure']?.text = parameters.notchPressure.toString();
    }
    if (!_activelyEditingFields.contains('systolicPeakTime')) {
      _controllers['systolicPeakTime']?.text = parameters.systolicPeakTime.toString();
    }
    if (!_activelyEditingFields.contains('diastolicPeakTime')) {
      _controllers['diastolicPeakTime']?.text = parameters.diastolicPeakTime.toString();
    }
    if (!_activelyEditingFields.contains('flowRate')) {
      _controllers['flowRate']?.text = parameters.flowRate.toString();
    }
    if (!_activelyEditingFields.contains('basePressure')) {
      _controllers['basePressure']?.text = parameters.basePressure.toString();
    }
    
    // Update new open loop parameters
    if (!_activelyEditingFields.contains('sysPWM')) {
      _controllers['sysPWM']?.text = parameters.sysPWM.toString();
    }
    if (!_activelyEditingFields.contains('disPWM')) {
      _controllers['disPWM']?.text = parameters.disPWM.toString();
    }
    if (!_activelyEditingFields.contains('sysPeriod')) {
      _controllers['sysPeriod']?.text = parameters.sysPeriod.toString();
    }
    if (!_activelyEditingFields.contains('disPeriod')) {
      _controllers['disPeriod']?.text = parameters.disPeriod.toString();
    }
    if (!_activelyEditingFields.contains('sysHighPercent')) {
      _controllers['sysHighPercent']?.text = parameters.sysHighPercent.toString();
    }
    if (!_activelyEditingFields.contains('disHighPercent')) {
      _controllers['disHighPercent']?.text = parameters.disHighPercent.toString();
    }
  }

  Future<void> _loadParametersForMode(String mode) async {
    final bluetoothService = Provider.of<BluetoothService>(context, listen: false);
    if (bluetoothService.isConnected) {
      await bluetoothService.setMode(mode);
      await bluetoothService.getParameters();
    }
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
        setState(() => _isConnected = true);
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

  // --- DIALOG UNTUK MENAMBAH MODE -----------------------------------------
  Future<void> _showAddModeDialog() async {
    final bluetoothService = Provider.of<BluetoothService>(context, listen: false);
    
    if (!bluetoothService.isConnected) {
      _showMessage('Please connect to device first');
      return;
    }

    final nameCtrl = TextEditingController();
    int selectedModeType = 1; // Default to close loop

final result = await showDialog<Map<String, dynamic>>(
  context: context,
  builder: (ctx) => StatefulBuilder(
    builder: (context, setState) => AlertDialog(
      title: const Text('Add New Mode'),
      content: SingleChildScrollView(
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              TextField(
                controller: nameCtrl,
                decoration: const InputDecoration(hintText: 'Mode name'),
                autofocus: true,
              ),
              const SizedBox(height: 16),
              const Text('Mode Type:', style: TextStyle(fontWeight: FontWeight.bold)),
              RadioListTile<int>(
                title: const Text('Heart Pulse Simulator'),
                subtitle: const Text('Closed loop pressure control'),
                value: 1,
                groupValue: selectedModeType,
                onChanged: (value) => setState(() => selectedModeType = value!),
              ),
              RadioListTile<int>(
                title: const Text('Manual Pump Power Configuration'),
                subtitle: const Text('Open loop power control'),
                value: 0,
                groupValue: selectedModeType,
                onChanged: (value) => setState(() => selectedModeType = value!),
              ),
            ],
          ),
        
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(ctx),
          child: const Text('Cancel'),
        ),
        TextButton(
          onPressed: () => Navigator.pop(ctx, {
            'name': nameCtrl.text.trim(),
            'closeloop': selectedModeType,
          }),
          child: const Text('Add'),
        ),
      ],
    ),
  ),
);
    if (result != null && result['name'].isNotEmpty) {
      await _sendAddModeCommand(result['name'], result['closeloop']);
    }
  }

  Future<void> _sendAddModeCommand(String modeName, int closeloop) async {
    final bluetoothService = Provider.of<BluetoothService>(context, listen: false);
    
    try {
      await bluetoothService.addMode(modeName, closeloop);
      _showMessage('Mode "$modeName" "$closeloop" added successfully');
      // Refresh available modes
      await bluetoothService.getAvailableModes();
    } catch (e) {
      _showMessage('Failed to add mode: $e');
    }
  }

  Future<void> _sendDeleteModeCommand(String modeName) async {
    final bluetoothService = Provider.of<BluetoothService>(context, listen: false);
    
    final message = modeName;

    try {
      await bluetoothService.deleteMode(message);
      _showMessage('Mode "$modeName" deleted successfully');
      // Refresh available modes
      await bluetoothService.getAvailableModes();
    } catch (e) {
      _showMessage('Failed to delete mode: $e');
    }
  }

  Future<void> _saveParameters() async {
    final bluetoothService = Provider.of<BluetoothService>(context, listen: false);
    
    if (!bluetoothService.isConnected || _selectedMode == null) {
      _showMessage('Please connect to device and select a mode');
      return;
    }

    try {
      // Create parameters from controllers
      final parameters = PumpParameters(
        heartRate: int.tryParse(_controllers['heartRate']?.text ?? '80') ?? 80,
        systolicPressure: int.tryParse(_controllers['systolicPressure']?.text ?? '120') ?? 120,
        diastolicPressure: int.tryParse(_controllers['diastolicPressure']?.text ?? '80') ?? 80,
        systolicPeriod: int.tryParse(_controllers['systolicPeriod']?.text ?? '60') ?? 60,
        diastolicPeriod: int.tryParse(_controllers['diastolicPeriod']?.text ?? '40') ?? 40,
        notchPressure: int.tryParse(_controllers['notchPressure']?.text ?? '60') ?? 60,
        systolicPeakTime: int.tryParse(_controllers['systolicPeakTime']?.text ?? '0') ?? 0,
        diastolicPeakTime: int.tryParse(_controllers['diastolicPeakTime']?.text ?? '0') ?? 0,
        flowRate: int.tryParse(_controllers['flowRate']?.text ?? '80') ?? 80,
        basePressure: int.tryParse(_controllers['basePressure']?.text ?? '60') ?? 60,
        pumpMode: _selectedMode!,
        startPump: bluetoothService.parameters.startPump,
        pressureActual: bluetoothService.parameters.pressureActual,
        closeloop: bluetoothService.parameters.closeloop,
        // New open loop parameters
        sysPWM: int.tryParse(_controllers['sysPWM']?.text ?? '50') ?? 50,
        disPWM: int.tryParse(_controllers['disPWM']?.text ?? '30') ?? 30,
        sysPeriod: int.tryParse(_controllers['sysPeriod']?.text ?? '60') ?? 60,
        disPeriod: 100 - (int.tryParse(_controllers['sysPeriod']?.text ?? '60') ?? 60),
        sysHighPercent: int.tryParse(_controllers['sysHighPercent']?.text ?? '50') ?? 50,
        disHighPercent: int.tryParse(_controllers['disHighPercent']?.text ?? '30') ?? 30,
      );

      await bluetoothService.setParameters(parameters);
      _showMessage('Parameters saved successfully');
    } catch (e) {
      _showMessage('Failed to save parameters: $e');
    }
  }

  Widget _buildEditableParameter(String label, String unit, String controllerKey) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          '$label:',
          style: TextStyle( fontSize: FontSizes.medium(context), fontWeight: FontWeight.w600, fontFamily: 'Inter',
          ),
        ),
        const SizedBox(height: 4),
        Row(
          crossAxisAlignment: CrossAxisAlignment.center,
          children: [
            Expanded(
              child: TextField(
                controller: _controllers[controllerKey],
                focusNode: _focusNodes[controllerKey],
                keyboardType: TextInputType.number,
                style: TextStyle(
                  fontSize: FontSizes.big(context),
                  fontWeight: FontWeight.w600,
                  fontFamily: 'Inter',
                ),
                decoration: InputDecoration(
                  border: OutlineInputBorder(
                    borderRadius: BorderRadius.circular(4),
                  ),
                  contentPadding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
                  isDense: true,
                ),
              ),
            ),
            const SizedBox(width: 10),
            Text(
              unit,
              style: TextStyle(
                fontSize: FontSizes.small(context),
                fontWeight: FontWeight.w500,
                fontFamily: 'Inter',
                color: Colors.grey[700],
              ),
            ),
          ],
        ),
        const SizedBox(height: 10),
      ],
    );
  }

  // Helper method to get mode type color
  Color _getModeTypeColor(String mode) {
    final bluetoothService = Provider.of<BluetoothService>(context, listen: false);
    // Check if this mode is open loop (closeloop == 0) or closed loop (closeloop == 1)
    // You would need to get this information from the bluetooth service
    // For now, assuming you can get the closeloop value for the mode
    if (bluetoothService.parameters.pumpMode == mode) {
      return bluetoothService.parameters.closeloop == 1 ? Colors.blue : Colors.green;
    }
    return Colors.black; // Default color
  }

  @override
  Widget build(BuildContext context) {
    final size = MediaQuery.of(context).size;

    return Scaffold(
      body: Stack(
        children: [
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
          Padding(
            padding: const EdgeInsets.all(30),
            child: Column(
              children: [
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

                // ====================== BODY ======================
                Expanded(
                  child: Consumer<BluetoothService>(
                    builder: (context, bluetoothService, child) {
                      final availableModes = bluetoothService.availableModes;
                      _isConnected = bluetoothService.isConnected;
                      final isOpenLoop = bluetoothService.parameters.closeloop == 0;

                      return Row(
                        children: [
                          // =========================================================
                          // PANEL KIRI (pilih mode)
                          // =========================================================
                          Expanded(
                            flex: 2,
                            child: Column(
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                // Connection status
                                Container(
                                  padding: const EdgeInsets.all(8),
                                  decoration: BoxDecoration(
                                    color: _isConnected 
                                      ? Colors.green.withOpacity(0.1)
                                      : Colors.red.withOpacity(0.1),
                                    borderRadius: BorderRadius.circular(4),
                                    border: Border.all(
                                      color: _isConnected ? Colors.green : Colors.red,
                                    ),
                                  ),
                                  child: Row(
                                    children: [
                                      Icon(
                                        _isConnected ? Icons.bluetooth_connected : Icons.bluetooth_disabled,
                                        color: _isConnected ? Colors.green : Colors.red,
                                        size: 16,
                                      ),
                                      const SizedBox(width: 8),
                                      Text(
                                        bluetoothService.connectionStatus,
                                        style: TextStyle(
                                          fontSize: FontSizes.small(context),
                                          color: _isConnected ? Colors.green[700] : Colors.red[700],
                                        ),
                                      ),
                                    ],
                                  ),
                                ),
                                const SizedBox(height: 12),

                                // Modes list
                                Expanded(
                                  child: Container(
                                    decoration: BoxDecoration(
                                      border: Border.all(color: Colors.grey),
                                      borderRadius: BorderRadius.circular(6),
                                    ),
                                    child: availableModes.isEmpty
                                        ? const Center(
                                            child: Text(
                                              'No modes available\nConnect to device first',
                                              textAlign: TextAlign.center,
                                              style: TextStyle(color: Colors.grey),
                                            ),
                                          )
                                        : ListView.builder(
                                            itemCount: availableModes.length,
                                            itemBuilder: (ctx, i) {
                                              final mode = availableModes[i];
                                              final selected = mode == _selectedMode;
                                              // Get color based on mode type (you'll need to implement this logic)
                                              final textColor = _getModeTypeColor(mode);

                                              return GestureDetector(
                                                onLongPress: () {
                                                  if (_isConnected) {
                                                    _showDeleteConfirmationDialog(mode);
                                                  }
                                                },
                                                child: ListTile(
                                                  title: Text(
                                                    mode,
                                                    style: TextStyle(
                                                      fontSize: FontSizes.small(context),
                                                      fontWeight: selected ? FontWeight.w700 : FontWeight.w400,
                                                      color: textColor,
                                                    ),
                                                  ),
                                                  selected: selected,
                                                  selectedTileColor: _getModeTypeColor(mode).withOpacity(0.1),
                                                  onTap: () {
                                                    setState(() => _selectedMode = mode);
                                                    if (_isConnected) {
                                                      _loadParametersForMode(mode);
                                                    }
                                                  },
                                                ),
                                              );
                                            },
                                          ),
                                  ),
                                ),

                                const SizedBox(height: 12),

                                // ---------- TOMBOL TAMBAH MODE ----------
                                SizedBox(
                                  width: double.infinity,
                                  child: OutlinedButton.icon(
                                    icon: const Icon(Icons.add, color: Colors.blue),
                                    label: Text(
                                      'Add Mode',
                                      style: TextStyle(
                                        fontSize: FontSizes.small(context),
                                        fontWeight: FontWeight.w400,
                                        fontFamily: 'Inter',
                                        color: Colors.blue,
                                      ),
                                    ),
                                    onPressed: _isConnected ? _showAddModeDialog : null,
                                    style: OutlinedButton.styleFrom(
                                      shape: RoundedRectangleBorder(
                                        borderRadius: BorderRadius.circular(4),
                                      ),
                                      side: const BorderSide(color: Colors.grey),
                                    ),
                                  ),
                                ),

                                const SizedBox(height: 12),

                                // ---------- CONNECT/BACK BUTTONS ----------
                                if (!_isConnected)
                                  SizedBox(
                                    width: double.infinity,
                                    child: ElevatedButton(
                                      onPressed: _connectToBluetooth,
                                      style: ElevatedButton.styleFrom(
                                        backgroundColor: Colors.blue,
                                        foregroundColor: Colors.white,
                                        shape: RoundedRectangleBorder(
                                          borderRadius: BorderRadius.circular(4),
                                        ),
                                        padding: const EdgeInsets.symmetric(
                                          horizontal: 24, vertical: 12,
                                        ),
                                      ),
                                      child: Text(
                                        'Connect',
                                        style: TextStyle(
                                          fontSize: FontSizes.small(context),
                                          fontWeight: FontWeight.w400,
                                          fontFamily: 'Inter',
                                        ),
                                      ),
                                    ),
                                  ),
                                const SizedBox(height: 8),
                                ElevatedButton(
                                  onPressed: () => Navigator.pop(context),
                                  style: ElevatedButton.styleFrom(
                                    foregroundColor: Colors.blue,
                                    shape: RoundedRectangleBorder(
                                      borderRadius: BorderRadius.circular(4),
                                    ),
                                    padding: const EdgeInsets.symmetric(
                                      horizontal: 24, vertical: 12,
                                    ),
                                  ),
                                  child: Text(
                                    'Back',
                                    style: TextStyle(
                                      fontSize: FontSizes.small(context),
                                      fontWeight: FontWeight.w400,
                                      fontFamily: 'Inter',
                                    ),
                                  ),
                                ),
                              ],
                            ),
                          ),
                          const SizedBox(width: 20),

                          // =========================================================
                          // PANEL TENGAH (Parameters)
                          // =========================================================
                          Expanded(
                            flex: 4,
                            child: Column(
                              mainAxisAlignment: MainAxisAlignment.start,
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                Column(
                                  crossAxisAlignment: CrossAxisAlignment.start,
                                  children: [
                                    if (isOpenLoop) ...[
                                      Text( 'Manual Pump Power Configuration', style: TextStyle( fontSize: FontSizes.medium(context), fontWeight: FontWeight.w700, fontFamily: 'Inter',),),
                                      Text( 'Open loop power control', style: TextStyle( fontSize: FontSizes.small(context), fontWeight: FontWeight.w500, fontFamily: 'Inter', color: Colors.grey[700],), ),
                                    ] else ...[
                                      Text( 'Heart Pulse Simulator', style: TextStyle( fontSize: FontSizes.medium(context), fontWeight: FontWeight.w700, fontFamily: 'Inter',),),
                                      Text( 'Closed loop pressure control', style: TextStyle( fontSize: FontSizes.small(context), fontWeight: FontWeight.w500, fontFamily: 'Inter', color: Colors.grey[700],), ),
                                    ],
                                  ],
                                ),
                                const SizedBox(height:  20),
                                Expanded(
                                  child: SingleChildScrollView(
                                    child: Row(
                                      children: [
                                        Expanded(
                                          flex: 1,
                                          child: Column(
                                            crossAxisAlignment: CrossAxisAlignment.start,
                                            children: [
                                              if (isOpenLoop) ...[
                                                _buildEditableParameter('Systole Pump Power', '% pump power', 'sysPWM'),
                                                _buildEditableParameter('Systole High Period', '% of systole Period', 'sysHighPercent'),
                                                _buildEditableParameter('Systole Period', '% of pulse period', 'sysPeriod'),
                                                Text(
                                                  'Diastole Period: ${100 - (int.tryParse(_controllers['sysPeriod']?.text ?? '60') ?? 60)}%',
                                                  style: TextStyle(
                                                    fontSize: FontSizes.small(context),
                                                    fontWeight: FontWeight.w500,
                                                    fontFamily: 'Inter',
                                                    color: Colors.grey[700],
                                                  ),
                                                ),
                                              ] else ...[
                                                _buildEditableParameter('Heart Rate', 'BPM', 'heartRate'),
                                                _buildEditableParameter('Systolic Peak Pressure', 'mmHg', 'systolicPressure'),
                                                _buildEditableParameter('Diastolic Peak Pressure', 'mmHg', 'diastolicPressure'),
                                                _buildEditableParameter('Diastolic Base Pressure', 'mmHg', 'basePressure'),
                                                const SizedBox(height: 20),
                                              ],
                                            ],
                                          ),
                                        ),
                                        const SizedBox(width: 20),
                                        Expanded(
                                          flex: 1,
                                          child: Column(
                                            crossAxisAlignment: CrossAxisAlignment.start,
                                            children: [
                                              if (isOpenLoop) ...[
                                                _buildEditableParameter('Diastole Pump Power', '% pump power', 'disPWM'),
                                                _buildEditableParameter('Diastole High Period', '% of diastole period', 'disHighPercent'),
                                                _buildEditableParameter('Heart Rate', 'BPM', 'heartRate'),
                                                const SizedBox(height: 20),
                                              ] else ...[
                                                _buildEditableParameter('Notch Pressure', 'mmHg', 'notchPressure'),
                                                _buildEditableParameter('Systolic Peak Time', 'ms', 'systolicPeakTime'),
                                                _buildEditableParameter('Diastolic Peak Time', 'ms', 'diastolicPeakTime'),
                                                _buildEditableParameter('Systolic Period', '%', 'systolicPeriod'),
                                                Text(
                                                  'Diastolic Period: ${100 - (int.tryParse(_controllers['systolicPeriod']?.text ?? '60') ?? 60)}%',
                                                  style: TextStyle(
                                                    fontSize: FontSizes.small(context),
                                                    fontWeight: FontWeight.w500,
                                                    fontFamily: 'Inter',
                                                    color: Colors.grey[700],
                                                  ),
                                                ),
                                                
                                              ],
                                            ],
                                          ),
                                        ),
                                      ],
                                    ),
                                  ),
                                ),
                                const SizedBox(height: 20),
                                ElevatedButton(
                                  onPressed: (_isConnected && _selectedMode != null) ? _saveParameters : null,
                                  style: ElevatedButton.styleFrom(
                                    backgroundColor: Colors.blue,
                                    foregroundColor: Colors.white,
                                    shape: RoundedRectangleBorder(
                                      borderRadius: BorderRadius.circular(4),
                                    ),
                                    padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 12),
                                  ),
                                  child: Text(
                                    'Save Parameters',
                                    style: TextStyle(
                                      fontSize: FontSizes.small(context),
                                      fontWeight: FontWeight.w400,
                                      fontFamily: 'Inter',
                                    ),
                                  ),
                                ),
                              ],
                            ),
                          ),
                          const SizedBox(width: 20),
                          // =========================================================
                          // PANEL KANAN (Dynamic Graph)
                          // =========================================================
                          Expanded(
                            flex: 4,
                            child: Column(
                              mainAxisAlignment: MainAxisAlignment.spaceBetween,
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                Expanded(
                                  flex: 1,
                                  child: Container(
                                    decoration: BoxDecoration(
                                      border: Border.all(color: Colors.grey),
                                      borderRadius: BorderRadius.circular(8),
                                    ),
                                    child: Padding(
                                      padding: const EdgeInsets.all(40.0),
                                      child: Column(
                                        crossAxisAlignment: CrossAxisAlignment.start,
                                        children: [
                                          const SizedBox(height: 8),
                                          Expanded(
                                            child: CustomPaint(
                                              size: Size.infinite,
                                              painter: _isOpenLoop(_selectedMode) 
                                                ? PumpPowerWaveformPainter(
                                                    sysPWM: int.tryParse(_controllers['sysPWM']?.text ?? '80') ?? 80,
                                                    disPWM: int.tryParse(_controllers['disPWM']?.text ?? '60') ?? 60,
                                                    sysPeriod: int.tryParse(_controllers['sysPeriod']?.text ?? '60') ?? 60,
                                                    heartRate: int.tryParse(_controllers['heartRate']?.text ?? '80') ?? 80,
                                                    sysHighPercent: int.tryParse(_controllers['sysHighPercent']?.text ?? '50') ?? 50,
                                                    disHighPercent: int.tryParse(_controllers['disHighPercent']?.text ?? '50') ?? 50,
                                                  )
                                                : PressureWaveformPainter(
                                                    systolicPressure: int.tryParse(_controllers['systolicPressure']?.text ?? '120') ?? 120,
                                                    diastolicPressure: int.tryParse(_controllers['diastolicPressure']?.text ?? '80') ?? 80,
                                                    notchPressure: int.tryParse(_controllers['notchPressure']?.text ?? '60') ?? 60,
                                                    systolicPeriod: int.tryParse(_controllers['systolicPeriod']?.text ?? '60') ?? 60,
                                                    systolicPeakTime: int.tryParse(_controllers['systolicPeakTime']?.text ?? '0') ?? 0,
                                                    diastolicPeakTime: int.tryParse(_controllers['diastolicPeakTime']?.text ?? '0') ?? 0,
                                                    heartRate: int.tryParse(_controllers['heartRate']?.text ?? '80') ?? 80,
                                                    basePressure: int.tryParse(_controllers['basePressure']?.text ?? '80') ?? 80,
                                                  ),
                                            ),
                                          ),
                                        ],
                                      ),
                                    ),
                                  ),
                                ),
                                const SizedBox(height: 20),
                                Expanded(
                                  flex: 1,
                                  child: Container(
                                    decoration: BoxDecoration(
                                      border: Border.all(color: Colors.grey),
                                      borderRadius: BorderRadius.circular(8),
                                    ),
                                    child: Padding(
                                      padding: EdgeInsets.all(8.0),
                                      child: Image.asset(
                                        _isOpenLoop(_selectedMode) 
                                          ? 'assets/image/pumpPowerGraph.png'
                                          : 'assets/image/pressureGraph.png',
                                        fit: BoxFit.contain,
                                      ),
                                    ),
                                  ),
                                ),
                              ],
                            ),
                          ),
                        ],
                      );
                    },
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  void _showDeleteConfirmationDialog(String modeToDelete) {
    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Delete Mode'),
        content: Text('Are you sure you want to delete mode "$modeToDelete"?'),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(),
            child: const Text('Cancel'),
          ),
          TextButton(
            onPressed: () {
              Navigator.of(ctx).pop();
              _sendDeleteModeCommand(modeToDelete);
            },
            child: const Text('Delete', style: TextStyle(color: Colors.red)),
          ),
        ],
      ),
    );
  }
}

class PumpPowerWaveformPainter extends CustomPainter {
  final int sysPWM;
  final int disPWM;
  final int sysPeriod;
  final int heartRate;
  final int sysHighPercent;
  final int disHighPercent;

  PumpPowerWaveformPainter({
    required this.sysPWM,
    required this.disPWM,
    required this.sysPeriod,
    required this.heartRate,
    required this.sysHighPercent,
    required this.disHighPercent,
  });


  @override
  void paint(Canvas canvas, Size size) {
    final paint = Paint()
      ..color = Colors.green
      ..strokeWidth = 2.0
      ..style = PaintingStyle.stroke;

    final gridPaint = Paint()
      ..color = Colors.grey.withOpacity(0.3)
      ..strokeWidth = 1.0;

    final labelPaint = TextStyle(
      color: Colors.black54,
      fontSize: 10,
      fontFamily: 'monospace',
    );

    // Draw grid
    // Vertical grid lines (time)
    for (int i = 0; i <= 10; i++) {
      final x = (i / 10.0) * size.width;
      canvas.drawLine(Offset(x, 0), Offset(x, size.height), gridPaint);
    }
    // Horizontal grid lines (pressure)
    for (int i = 0; i <= 8; i++) {
      final y = (i / 8.0) * size.height;
      canvas.drawLine(Offset(0, y), Offset(size.width, y), gridPaint);
    }
    
    // Draw labels
    _drawLabels(canvas, size, labelPaint);

    // Calculate waveform parameters
    final cycleTime = 60.0 / heartRate; // seconds per cycle
    final systolicDuration = (sysPeriod / 100.0) * cycleTime;
    final diastolicDuration = cycleTime - systolicDuration;
    
    // Number of cycles to show
    const numCycles = 2.3;
    final totalTime = numCycles * cycleTime;
    
    final path = Path();
    final pointsPerSecond = 100;
    final totalPoints = (totalTime * pointsPerSecond).round();
    // Power range for scaling (0-100%)
    const minPower = 0;
    const maxPower = 100;
    const powerRange = maxPower - minPower;

    bool firstPoint = true;
    
    for (int i = 0; i <= totalPoints; i++) {
      final t = (i / pointsPerSecond) % cycleTime;
      final x = (i / totalPoints.toDouble()) * size.width;
      
      double power;
      
      if (t <= systolicDuration) {
        // Systolic phase
        final systolicProgress = t / systolicDuration;
        power = _calculateSystolicPower(systolicProgress);
      } else {
        // Diastolic phase
        final diastolicProgress = (t - systolicDuration) / diastolicDuration;
        power = _calculateDiastolicPower(diastolicProgress);
      }
      
      final y = size.height - ((power - minPower) / powerRange) * size.height;
      
      if (firstPoint) {
        path.moveTo(x, y);
        firstPoint = false;
      } else {
        path.lineTo(x, y);
      }
    }
    
    canvas.drawPath(path, paint);
  }

  double _calculateSystolicPower(double progress) {
    final highDuration = sysHighPercent / 100.0; // High period as fraction of systolic period
    
    if (progress <= highDuration) {
      // High power period
      return sysPWM.toDouble();
    } else {
      return 0;
    }
  }

  double _calculateDiastolicPower(double progress) {
    final highDuration = disHighPercent / 100.0; // High period as fraction of diastolic period
    
    if (progress <= highDuration) {
      return disPWM.toDouble();
    } else {
      return 0;
    }
  }

  void _drawLabels(Canvas canvas, Size size, TextStyle labelStyle) {
    // Y-axis labels (power percentage)
    for (int i = 0; i <= 10; i++) {
      final power = 100 - (i * 10); // 100%, 90%, 80%, ... 0%
      final y = (i / 10.0) * size.height;
      
      final textSpan = TextSpan(
        text: '$power%',
        style: labelStyle,
      );
      final textPainter = TextPainter(
        text: textSpan,
        textDirection: TextDirection.ltr,
      );
      textPainter.layout();
      
      // Draw label to the left of the graph
      textPainter.paint(canvas, Offset(-35, y - textPainter.height / 2));
    }
    
    // X-axis labels (time)
    final cycleTime = 60.0 / heartRate;
    const numCycles = 2.3;
    final totalTime = numCycles * cycleTime;
    
    for (int i = 0; i <= 5; i++) {
      final time = (i / 5.0) * totalTime;
      final x = (i / 5.0) * size.width;
      
      final textSpan = TextSpan(
        text: '${time.toStringAsFixed(1)}s',
        style: labelStyle,
      );
      final textPainter = TextPainter(
        text: textSpan,
        textDirection: TextDirection.ltr,
      );
      textPainter.layout();
      
      // Draw label below the graph
      textPainter.paint(canvas, Offset(x - textPainter.width / 2, size.height + 5));
    }
    
    // Title
    final titleSpan = TextSpan(
      text: 'Pump Power Waveform Preview (%)',
      style: labelStyle.copyWith(
        fontSize: 12,
        fontWeight: FontWeight.bold,
      ),
    );
    final titlePainter = TextPainter(
      text: titleSpan,
      textDirection: TextDirection.ltr,
    );
    titlePainter.layout();
    titlePainter.paint(canvas, Offset((size.width - titlePainter.width) / 2, -25));
  }

  @override
  bool shouldRepaint(PumpPowerWaveformPainter oldDelegate) {
    return sysPWM != oldDelegate.sysPWM ||
           disPWM != oldDelegate.disPWM ||
           sysPeriod != oldDelegate.sysPeriod ||
           heartRate != oldDelegate.heartRate ||
           sysHighPercent != oldDelegate.sysHighPercent ||
           disHighPercent != oldDelegate.disHighPercent;
  }
}

// Custom painter for dynamic pressure waveform
class PressureWaveformPainter extends CustomPainter {
  final int systolicPressure;
  final int diastolicPressure;
  final int notchPressure;
  final int systolicPeriod;
  final int systolicPeakTime;
  final int diastolicPeakTime;
  final int heartRate;
  final int basePressure;

  PressureWaveformPainter({
    required this.systolicPressure,
    required this.diastolicPressure,
    required this.notchPressure,
    required this.systolicPeriod,
    required this.systolicPeakTime,
    required this.diastolicPeakTime,
    required this.heartRate,
    required this.basePressure,
  });

  @override
  void paint(Canvas canvas, Size size) {
    final paint = Paint()
      ..color = Colors.blue
      ..strokeWidth = 2.0
      ..style = PaintingStyle.stroke;

    final gridPaint = Paint()
      ..color = Colors.grey.withOpacity(0.3)
      ..strokeWidth = 1.0;

    final labelPaint = TextStyle(
      color: Colors.black54,
      fontSize: 10,
      fontFamily: 'monospace',
    );

    // Draw grid
    // Vertical grid lines (time)
    for (int i = 0; i <= 10; i++) {
      final x = (i / 10.0) * size.width;
      canvas.drawLine(Offset(x, 0), Offset(x, size.height), gridPaint);
    }
    // Horizontal grid lines (pressure)
    for (int i = 0; i <= 8; i++) {
      final y = (i / 8.0) * size.height;
      canvas.drawLine(Offset(0, y), Offset(size.width, y), gridPaint);
    }
    
    // Draw labels
    _drawLabels(canvas, size, labelPaint);

    // Calculate waveform parameters
    final cycleTime = 60.0 / heartRate; // seconds per cycle
    final systolicDuration = (systolicPeriod / 100.0) * cycleTime;
    final diastolicDuration = cycleTime - systolicDuration;

    final sysPeak = (systolicPeakTime/1000) /systolicDuration;
    
    // Number of cycles to show
    const numCycles = 2.3;
    final totalTime = numCycles * cycleTime;
    
    final path = Path();
    final pointsPerSecond = 100;
    final totalPoints = (totalTime * pointsPerSecond).round();
    
    // Pressure range for scaling
    final minPressure = basePressure -10;
    final maxPressure = systolicPressure + 10;
    final pressureRange = maxPressure - minPressure;
    
    bool firstPoint = true;
    
    for (int i = 0; i <= totalPoints; i++) {
      final t = (i / pointsPerSecond) % cycleTime;
      final x = (i / totalPoints.toDouble()) * size.width;
      
      double pressure;
      
      if (t <= systolicDuration) {
        // Systolic phase
        final systolicProgress = t / systolicDuration;
        pressure = _calculateSystolicPressure(systolicProgress, sysPeak);
      } else {
        // Diastolic phase
        final diastolicProgress = (t - systolicDuration) / diastolicDuration;
        pressure = _calculateDiastolicPressure(diastolicProgress, diastolicDuration);
      }
      
      final y = size.height - ((pressure - minPressure) / pressureRange) * size.height;
      
      if (firstPoint) {
        path.moveTo(x, y);
        firstPoint = false;
      } else {
        path.lineTo(x, y);
      }
    }
    
    canvas.drawPath(path, paint);
  }

  double _calculateSystolicPressure(double progress, double sysPeak) {    //sysPeak : systolicPeriod yang uda diubah dari percent ke ms
    final tPeak   = sysPeak;                                            

    if (progress <= tPeak) {
      // Naik: baseline → systolicPressure  (cos setengah gelombang)
      final rise = progress / tPeak;               // 0 … 1
      final factor = (1 - math.cos(rise * math.pi)) / 2; // 0 … 1
      return basePressure + (systolicPressure - basePressure) * factor;
    } else {
      // Turun: systolicPressure → notchPressure (cos setengah gelombang)
      final fall = (progress - tPeak) / (1 - tPeak); // 0 … 1
      final factor = (1 + math.cos(fall * math.pi)) / 2;
      return notchPressure + (systolicPressure - notchPressure) * factor;
    } 
    // return baseline;
  }

  double _calculateDiastolicPressure(double progress,double diastolicDuration) {
    final tPeak   = 0.3;

    if (progress <= tPeak) {
      // Naik: baseline → diastolicPressure  (cos setengah gelombang)
      final rise = progress / tPeak;               // 0 … 1
      final factor = (1 - math.cos(rise * math.pi)) / 2; // 0 … 1
      return notchPressure + (diastolicPressure - notchPressure) * factor;
    } else {
      // Turun: diastolicPressure → notchPressure (cos setengah gelombang)
      final fall = (progress - tPeak) / (1 - tPeak); // 0 … 1
      final factor = (1 + math.cos(fall * math.pi)) / 2;   // 1 … 0
      return basePressure + (diastolicPressure - basePressure) * factor;
    } 
  }
  void _drawLabels(Canvas canvas, Size size, TextStyle labelStyle) {
    // Calculate pressure range
    final minPressure = basePressure - 10;
    final maxPressure = systolicPressure + 10;
    final pressureStep = (maxPressure - minPressure) / 8;
    
    // Y-axis labels (pressure)
    for (int i = 0; i <= 8; i++) {
      final pressure = maxPressure - (i * pressureStep);
      final y = (i / 8.0) * size.height;
      
      final textSpan = TextSpan(
        text: '${pressure.round()}',
        style: labelStyle,
      );
      final textPainter = TextPainter(
        text: textSpan,
        textDirection: TextDirection.ltr,
      );
      textPainter.layout();
      
      // Draw label to the left of the graph
      textPainter.paint(canvas, Offset(-35, y - textPainter.height / 2));
    }
    
    // X-axis labels (time)
    final cycleTime = 60.0 / heartRate;
    const numCycles = 2.3;
    final totalTime = numCycles * cycleTime;
    
    for (int i = 0; i <= 5; i++) {
      final time = (i / 5.0) * totalTime;
      final x = (i / 5.0) * size.width;
      
      final textSpan = TextSpan(
        text: '${time.toStringAsFixed(1)}s',
        style: labelStyle,
      );
      final textPainter = TextPainter(
        text: textSpan,
        textDirection: TextDirection.ltr,
      );
      textPainter.layout();
      
      // Draw label below the graph
      textPainter.paint(canvas, Offset(x - textPainter.width / 2, size.height + 5));
    }
    
    // Title and axis labels
    final titleSpan = TextSpan(
      text: 'Pressure Waveform Preview (mmHg)',
      style: labelStyle.copyWith(
        fontSize: 12,
        fontWeight: FontWeight.bold,
      ),
    );
    final titlePainter = TextPainter(
      text: titleSpan,
      textDirection: TextDirection.ltr,
    );
    titlePainter.layout();
    titlePainter.paint(canvas, Offset((size.width - titlePainter.width) / 2, -25));
  }

  @override
  bool shouldRepaint(PressureWaveformPainter oldDelegate) {
    return systolicPressure != oldDelegate.systolicPressure ||
           diastolicPressure != oldDelegate.diastolicPressure ||
           notchPressure != oldDelegate.notchPressure ||
           systolicPeriod != oldDelegate.systolicPeriod ||
           systolicPeakTime != oldDelegate.systolicPeakTime ||
           diastolicPeakTime != oldDelegate.diastolicPeakTime ||
           heartRate != oldDelegate.heartRate ||
           basePressure != oldDelegate.basePressure;
  }
}