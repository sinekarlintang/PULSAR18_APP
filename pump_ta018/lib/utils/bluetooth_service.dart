import 'dart:async';
import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:flutter_bluetooth_serial/flutter_bluetooth_serial.dart';

class PumpParameters {
  int heartRate;
  int systolicPressure;
  int diastolicPressure;
  int systolicPeriod;
  int diastolicPeriod;
  int notchPressure;
  int systolicPeakTime;
  int diastolicPeakTime;
  int flowRate;
  int pressureActual;
  String pumpMode;
  int startPump; // 1 for start, 0 for stop
  int basePressure;
  
  // === PARAMETER BARU UNTUK OPEN LOOP ===
  int closeloop; // 1 for close loop, 0 for open loop
  int sysPWM; // PWM percentage untuk systole high
  int disPWM; // PWM percentage untuk diastole high
  int sysPeriod; // Percentage durasi systole dalam cycle
  int disPeriod; // Percentage durasi diastole dalam cycle
  int sysHighPercent; // Percentage high phase dalam systole
  int disHighPercent; // Percentage high phase dalam diastole

  PumpParameters({
    this.heartRate = 80,
    this.systolicPressure = 120,
    this.diastolicPressure = 90,
    this.systolicPeriod = 60,
    this.diastolicPeriod = 40,
    this.notchPressure = 60,
    this.systolicPeakTime = 0,
    this.diastolicPeakTime = 0,
    this.flowRate = 80,
    this.pressureActual = 80,
    this.pumpMode = "Otomatis",
    this.startPump = 0, // Default to stopped
    this.basePressure = 80,
    // === DEFAULT VALUES UNTUK PARAMETER OPEN LOOP ===
    this.closeloop = 1, // Default to close loop
    this.sysPWM = 100, // Default systole PWM 100%
    this.disPWM = 50, // Default diastole PWM 50%
    this.sysPeriod = 50, // Default systole period 50%
    this.disPeriod = 50, // Default diastole period 50%
    this.sysHighPercent = 20, // Default systole high 20%
    this.disHighPercent = 20, // Default diastole high 20%
  });

  Map<String, dynamic> toJson() {
    return {
      'heartRate': heartRate,
      'systolicPressure': systolicPressure,
      'diastolicPressure': diastolicPressure,
      'systolicPeriod': systolicPeriod,
      'diastolicPeriod': diastolicPeriod,
      'notchPressure': notchPressure,
      'systolicPeakTime': systolicPeakTime,
      'diastolicPeakTime': diastolicPeakTime,
      'flowRate': flowRate,
      'pressureActual': pressureActual,
      'pumpMode': pumpMode,
      'startPump': startPump,
      'basePressure': basePressure,
      // === TAMBAHAN UNTUK OPEN LOOP ===
      'closeloop': closeloop,
      'sysPWM': sysPWM,
      'disPWM': disPWM,
      'sysPeriod': sysPeriod,
      'disPeriod': disPeriod,
      'sysHighPercent': sysHighPercent,
      'disHighPercent': disHighPercent,
    };
  }

  factory PumpParameters.fromJson(Map<String, dynamic> json) {
    return PumpParameters(
      heartRate: json['heartRate'] ?? 80,
      systolicPressure: json['systolicPressure'] ?? 120,
      diastolicPressure: json['diastolicPressure'] ?? 80,
      systolicPeriod: json['systolicPeriod'] ?? 60,
      diastolicPeriod: json['diastolicPeriod'] ?? 40,
      notchPressure: json['notchPressure'] ?? 60,
      systolicPeakTime: json['systolicPeakTime'] ?? 0,
      diastolicPeakTime: json['diastolicPeakTime'] ?? 0,
      flowRate: json['flowRate'] ?? 80,
      pressureActual: json['pressureActual'] ?? 80,
      pumpMode: json['pumpMode'] ?? "Otomatis",
      startPump: json['startPump'] ?? 0,
      basePressure: json['basePressure'] ?? 0,
      // === TAMBAHAN UNTUK OPEN LOOP ===
      closeloop: json['closeloop'] ?? 1,
      sysPWM: json['sysPWM'] ?? 100,
      disPWM: json['disPWM'] ?? 50,
      sysPeriod: json['sysPeriod'] ?? 50,
      disPeriod: json['disPeriod'] ?? 50,
      sysHighPercent: json['sysHighPercent'] ?? 20,
      disHighPercent: json['disHighPercent'] ?? 20,
    );
  }
}

class BluetoothService extends ChangeNotifier {
  static final BluetoothService _instance = BluetoothService._internal();
  factory BluetoothService() => _instance;
  BluetoothService._internal();

  BluetoothConnection? _connection;
  PumpParameters _parameters = PumpParameters();
  bool _isConnected = false;
  String _connectionStatus = "Disconnected";
  List<String> _availableModes = [];
  
  // Stream controllers
  final StreamController<PumpParameters> _parametersController = 
      StreamController<PumpParameters>.broadcast();
  final StreamController<String> _statusController = 
      StreamController<String>.broadcast();
  final StreamController<List<String>> _availableModesController = 
      StreamController<List<String>>.broadcast();
  final StreamController<String> _messageController = 
      StreamController<String>.broadcast();

  // Getters
  PumpParameters get parameters => _parameters;
  bool get isConnected => _isConnected;
  String get connectionStatus => _connectionStatus;
  List<String> get availableModes => _availableModes;
  Stream<PumpParameters> get parametersStream => _parametersController.stream;
  Stream<String> get statusStream => _statusController.stream;
  Stream<List<String>> get availableModesStream => _availableModesController.stream;
  Stream<String> get messageStream => _messageController.stream;

  // Scan for available Bluetooth devices
  Future<List<BluetoothDevice>> scanDevices() async {
    try {
      return await FlutterBluetoothSerial.instance.getBondedDevices();
    } catch (e) {
      debugPrint('Error scanning devices: $e');
      return [];
    }
  }

  // Connect to ESP32
  Future<bool> connectToDevice(BluetoothDevice device) async {
    try {
      _updateConnectionStatus("Connecting...");
      
      _connection = await BluetoothConnection.toAddress(device.address);
      _isConnected = true;
      _updateConnectionStatus("Connected");
      
      // Start listening for incoming data
      _startListening();
      
      // Request initial parameters
      await getParameters();
      await getAvailableModes();
      
      notifyListeners();
      return true;
    } catch (e) {
      debugPrint('Connection failed: $e');
      _updateConnectionStatus("Connection Failed");
      return false;
    }
  }

  // Disconnect from ESP32
  Future<void> disconnect() async {
    try {
      await _connection?.close();
      _connection = null;
      _isConnected = false;
      _updateConnectionStatus("Disconnected");
      notifyListeners();
    } catch (e) {
      debugPrint('Disconnect error: $e');
    }
  }

  // Send mode to ESP32
  Future<void> setMode(String mode) async {
    if (!_isConnected || _connection == null) return;

    final message = {
      'type': 'SET_MODE',
      'mode': mode,
      'timestamp': DateTime.now().millisecondsSinceEpoch,
    };

    await _sendMessage(message);
  }

  // Send start/stop command to ESP32
  Future<void> setStartStop(int startPump) async {
    if (!_isConnected || _connection == null) return;

    _parameters.startPump = startPump;

    final message = {
      'type': 'SET_START_STOP',
      'startPump': startPump,
      'timestamp': DateTime.now().millisecondsSinceEpoch,
    };

    await _sendMessage(message);
    notifyListeners();
  }

  // Get parameters from ESP32
  Future<void> getParameters() async {
    if (!_isConnected || _connection == null) return;

    final message = {
      'type': 'GET_PARAMETERS',
      'timestamp': DateTime.now().millisecondsSinceEpoch,
    };

    await _sendMessage(message);
  }

  // Get available modes from ESP32
  Future<void> getAvailableModes() async {
    if (!_isConnected || _connection == null) return;

    final message = {
      'type': 'GET_AVAILABLE_MODES',
      'timestamp': DateTime.now().millisecondsSinceEpoch,
    };

    await _sendMessage(message);
  }

  // Add new mode to ESP32
  Future<void> addMode(String modeName, int closeloop) async {
    if (!_isConnected || _connection == null) return;

    final message = {
      'type': 'ADD_MODE',
      'modeName': modeName,
      'closeloop': closeloop,
      'timestamp': DateTime.now().millisecondsSinceEpoch,
    };

    await _sendMessage(message);
  }

  // Delete mode from ESP32
  Future<void> deleteMode(String modeName) async {
    if (!_isConnected || _connection == null) return;

    final message = {
      'type': 'DELETE_MODE',
      'modeName': modeName,
      'timestamp': DateTime.now().millisecondsSinceEpoch,
    };

    await _sendMessage(message);
  }

  // Send parameters to ESP32
  Future<void> setParameters(PumpParameters params) async {
  if (!_isConnected || _connection == null) return;

  final message = {
    'type': 'SET_PARAMETERS',
    'heartRate': params.heartRate,
    'systolicPressure': params.systolicPressure,
    'diastolicPressure': params.diastolicPressure,
    'systolicPeriod': params.systolicPeriod,
    'diastolicPeriod': params.diastolicPeriod,
    'notchPressure': params.notchPressure,
    'systolicPeakTime': params.systolicPeakTime,
    'diastolicPeakTime': params.diastolicPeakTime,
    'startPump': params.startPump,
    'basePressure': params.basePressure,
    // === PARAMETER BARU UNTUK OPEN LOOP ===
    'closeloop': params.closeloop,
    'sysPWM': params.sysPWM,
    'disPWM': params.disPWM,
    'sysPeriod': params.sysPeriod,
    'disPeriod': params.disPeriod,
    'sysHighPercent': params.sysHighPercent,
    'disHighPercent': params.disHighPercent,
    'timestamp': DateTime.now().millisecondsSinceEpoch,
  };

  await _sendMessage(message);
}

  // Send custom message to ESP32 (for flexibility)
  Future<void> sendCustomMessage(Map<String, dynamic> message) async {
    if (!_isConnected || _connection == null) return;
    
    // Add timestamp if not present
    if (!message.containsKey('timestamp')) {
      message['timestamp'] = DateTime.now().millisecondsSinceEpoch;
    }
    
    await _sendMessage(message);
  }

  // Send message to ESP32
  Future<void> _sendMessage(Map<String, dynamic> message) async {
    try {
      final jsonString = jsonEncode(message);
      _connection?.output.add(utf8.encode('$jsonString\n'));
      await _connection?.output.allSent;
      debugPrint('Sent: $jsonString');
    } catch (e) {
      debugPrint('Send error: $e');
    }
  }

  // Listen for incoming data
  void _startListening() {
    String buffer = '';
    
    _connection?.input?.listen(
      (data) {
        buffer += utf8.decode(data);
        
        // Process complete messages (lines ending with \n)
        while (buffer.contains('\n')) {
          final newlineIndex = buffer.indexOf('\n');
          final line = buffer.substring(0, newlineIndex).trim();
          buffer = buffer.substring(newlineIndex + 1);
          
          if (line.isNotEmpty) {
            _processReceivedMessage(line);
          }
        }
      },
      onDone: () {
        debugPrint('Connection closed');
        _isConnected = false;
        _availableModes.clear();
        _updateConnectionStatus("Disconnected");
        notifyListeners();
      },
      onError: (error) {
        debugPrint('Connection error: $error');
        _isConnected = false;
        _availableModes.clear();
        _updateConnectionStatus("Error");
        notifyListeners();
      },
    );
  }

  // Process received messages from ESP32
  void _processReceivedMessage(String message) {
    try {
      
      final data = jsonDecode(message) as Map<String, dynamic>;
      final messageType = data['type'] as String;

      switch (messageType) {
        case 'MODE_CONFIRMED':
          _handleModeConfirmed(data);
          debugPrint('Received: $message');
          break;
          
        case 'START_STOP_CONFIRMED':
          _handleStartStopConfirmed(data);
          debugPrint('Received: $message');
          break;
          
        case 'DESIRED_PARAMETERS':
          _handleDesiredParameters(data);
          debugPrint('Received: $message');
          break;
          
        case 'ACTUAL_PARAMETERS':
          _handleActualParameters(data);
          
          debugPrint('Received: $message');
          break;
          
        case 'PARAMETERS_SAVED':
          _handleParametersSaved(data);
          debugPrint('Received: $message');
          break;
        
        case 'AVAILABLE_MODES':
          _handleAvailableModes(data);
          debugPrint('Received: $message');
          break;

        case 'MODE_ADDED':
          _handleModeAdded(data);
          debugPrint('Received: $message');
          break;

        case 'MODE_DELETED':
          _handleModeDeleted(data);
          debugPrint('Received: $message');
          break;
          
        case 'ERROR':
          _handleError(data);
          debugPrint('Received: $message');
          break;
          
        default:
          debugPrint('Unknown message type: $messageType');
      }
    } catch (e) {
      debugPrint('Error processing message: $e');
    }
  }

  void _handleModeConfirmed(Map<String, dynamic> data) {
    final mode = data['mode'] as String;
    _parameters.pumpMode = mode;
    _parametersController.add(_parameters);
    notifyListeners();
    debugPrint('Mode confirmed: $mode');
  }

  void _handleStartStopConfirmed(Map<String, dynamic> data) {
    final startPump = data['startPump'] as int;
    _parameters.startPump = startPump;
    _parametersController.add(_parameters);
    notifyListeners();
    debugPrint('Start/Stop confirmed: $startPump');
  }

  void _handleDesiredParameters(Map<String, dynamic> data) {
    _parameters = PumpParameters(
      heartRate: data['heartRate'] ?? _parameters.heartRate,
      systolicPressure: data['systolicPressure'] ?? _parameters.systolicPressure,
      diastolicPressure: data['diastolicPressure'] ?? _parameters.diastolicPressure,
      systolicPeriod: data['systolicPeriod'] ?? _parameters.systolicPeriod,
      diastolicPeriod: data['diastolicPeriod'] ?? _parameters.diastolicPeriod,
      notchPressure: data['notchPressure'] ?? _parameters.notchPressure,
      systolicPeakTime: data['systolicPeakTime'] ?? _parameters.systolicPeakTime,
      diastolicPeakTime: data['diastolicPeakTime'] ?? _parameters.diastolicPeakTime,
      flowRate: _parameters.flowRate, // Keep current actual values
      pressureActual: _parameters.pressureActual,
      pumpMode: data['mode'] ?? _parameters.pumpMode,
      startPump: data['startPump'] ?? _parameters.startPump,
      basePressure: data['basePressure'] ?? _parameters.basePressure,

      // === PARAMETER BARU UNTUK OPEN LOOP ===
      closeloop: data['closeloop'] ?? _parameters.closeloop,                   // close loop 1 open loop 0
      sysPWM: data['sysPWM'] ?? _parameters.sysPWM,                            // percent pump power saat systole high
      disPWM: data['disPWM'] ?? _parameters.disPWM,                            // percent pump power saat diastole high
      sysPeriod: data['sysPeriod'] ?? _parameters.sysPeriod,                   // period systole dalam persen dari period hear pulse (diambil dari bpm)
      disPeriod: data['disPeriod'] ?? _parameters.disPeriod,                   // period diastole dalam persen dari period hear pulse (diambil dari bpm)
      sysHighPercent: data['sysHighPercent'] ?? _parameters.sysHighPercent,    // periode high sistol dalam persen dari period sistole
      disHighPercent: data['disHighPercent'] ?? _parameters.disHighPercent,    // perioda high diastol dalam persen dari period diastole
    );
  
    _parametersController.add(_parameters);
    notifyListeners();
    debugPrint('Desired parameters updated');
  }

  void _handleActualParameters(Map<String, dynamic> data) {
    _parameters.flowRate = data['flowRate'] ?? _parameters.flowRate;
    _parameters.pressureActual = data['pressureActual'] ?? _parameters.pressureActual;
    
    _parametersController.add(_parameters);
    notifyListeners();
    // debugPrint('Actual parameters updated: Flow=${_parameters.flowRate}, Pressure=${_parameters.pressureActual}');
  }

  void _handleAvailableModes(Map<String, dynamic> data) {
    if (data.containsKey('modes') && data['modes'] is List) {
      _availableModes = List<String>.from(data['modes']);
      _availableModesController.add(_availableModes);
      notifyListeners();
      debugPrint('Available modes updated: $_availableModes');
    }
  }

  void _handleModeAdded(Map<String, dynamic> data) {
    final modeName = data['modeName'] as String;
    final message = data['message'] as String? ?? 'Mode "$modeName" added successfully';
    
    // Add mode to local list if not already present
    if (!_availableModes.contains(modeName)) {
      _availableModes.add(modeName);
      _availableModesController.add(_availableModes);
      notifyListeners();
    }
    
    _messageController.add(message);
    debugPrint('Mode added: $modeName - $message');
  }

  void _handleModeDeleted(Map<String, dynamic> data) {
    final modeName = data['modeName'] as String;
    final message = data['message'] as String? ?? 'Mode "$modeName" deleted successfully';
    
    // Remove mode from local list
    _availableModes.remove(modeName);
    _availableModesController.add(_availableModes);
    
    // If current mode was deleted, switch to first available mode
    if (_parameters.pumpMode == modeName && _availableModes.isNotEmpty) {
      _parameters.pumpMode = _availableModes.first;
      _parametersController.add(_parameters);
    }
    
    notifyListeners();
    _messageController.add(message);
    debugPrint('Mode deleted: $modeName - $message');
  }

  void _handleParametersSaved(Map<String, dynamic> data) {
    final message = data['message'] as String;
    _messageController.add(message);
    debugPrint('Parameters saved: $message');
  }

  void _handleError(Map<String, dynamic> data) {
    final errorMessage = data['message'] as String;
    _messageController.add('Error: $errorMessage');
    debugPrint('ESP32 Error: $errorMessage');
  }

  void _updateConnectionStatus(String status) {
    _connectionStatus = status;
    _statusController.add(status);
  }

  @override
  void dispose() {
    _parametersController.close();
    _statusController.close();
    _availableModesController.close();
    _messageController.close();
    disconnect();
    super.dispose();
  }
}