#include <WiFi.h>
#include <WebServer.h>

// Pin definitions
const int TRIG_PIN = 21;
const int ECHO_PIN = 19;
const int VIBRATOR_PIN = 18;
const int IR_SENSOR_PIN = 22;
const int BUTTON_PIN = 4;
const int BUZZER_PIN = 25;

// Variables
unsigned long lastButtonPress = 0;
int buttonPressCount = 0;
bool isFollowingLine = false;
String locationData = "Location not available";
bool buzzerActive = false;
bool locationPermissionDenied = false;

// WiFi credentials
const char* ssid = "SmartGuide_Device";
const char* password = "12345678";

// Web server on port 80
WebServer server(80);

// Emergency melody for active buzzer (1 = ON, 0 = OFF)
int emergencyMelody[] = {1, 1, 0, 1, 1}; // ON, ON, OFF, ON, ON
int emergencyDurations[] = {300, 300, 200, 300, 300}; // Duration for each state
int melodyLength = 5;

// Staircase detection variables
int consecutiveIRChanges = 0;
int lastIRState = HIGH;
unsigned long lastIRChangeTime = 0;
bool staircaseDetected = false;

// Status variables for web interface
long lastDistance = 0;
int irSensorState = HIGH;
bool obstacleDetected = false;

void setup() {
  Serial.begin(115200);
  
  // Initialize pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(VIBRATOR_PIN, OUTPUT);
  pinMode(IR_SENSOR_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Ensure all outputs are off initially
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(VIBRATOR_PIN, LOW);
  digitalWrite(TRIG_PIN, LOW);
  
  // Set up WiFi access point
  Serial.println("=== Smart Navigation Device Starting ===");
  Serial.println("Creating WiFi Access Point...");
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("WiFi AP IP address: ");
  Serial.println(IP);
  
  // Set up web server endpoints
  server.on("/", handleRoot);
  server.on("/location", handleLocation);
  server.on("/updateLocation", handleUpdateLocation);
  server.on("/status", handleStatus);
  server.on("/control", handleControl);
  
  server.begin();
  Serial.println("HTTP server started successfully");
  Serial.println("WiFi Network: SmartGuide_Device");
  Serial.println("Password: 12345678");
  Serial.println("Open http://192.168.4.1 in your browser");
  Serial.println("=== System Ready ===\n");
  
  // Initial sensor test
  testAllSensors();
}

void loop() {
  server.handleClient();
  
  // === ULTRASONIC SENSOR (Obstacle Detection) ===
  handleUltrasonicSensor();
  
  // === IR SENSOR (Staircase Detection & Line Following) ===
  handleIRSensor();
  
  // === BUTTON HANDLING ===
  handleButtonPress();
  
  // Small delay to prevent overwhelming
  delay(50);
}

void handleUltrasonicSensor() {
  static unsigned long lastUltrasonicCheck = 0;
  
  if (millis() - lastUltrasonicCheck > 100) { // Check every 100ms
    long distance = measureDistance();
    lastDistance = distance;
    
    // Print distance only if it changed significantly
    static long lastReportedDistance = 0;
    if (abs(distance - lastReportedDistance) > 5 || millis() - lastUltrasonicCheck > 2000) {
      Serial.print("[ULTRASONIC] Distance: ");
      Serial.print(distance);
      Serial.println(" cm");
      lastReportedDistance = distance;
    }
    
    // Obstacle detection - vibrate when closer than 50cm
    obstacleDetected = (distance > 0 && distance < 50);
    if (obstacleDetected) {
      digitalWrite(VIBRATOR_PIN, HIGH);
      if (abs(distance - lastReportedDistance) > 10) {
        Serial.println("[ULTRASONIC] ⚠️  OBSTACLE DETECTED - Vibrating!");
      }
    } else {
      // Only turn off vibrator if not being used by other functions
      if (!isFollowingLine || digitalRead(IR_SENSOR_PIN) == HIGH) {
        digitalWrite(VIBRATOR_PIN, LOW);
      }
    }
    
    lastUltrasonicCheck = millis();
  }
}

void handleIRSensor() {
  static unsigned long lastIRCheck = 0;
  static int stableIRReadings = 0;
  
  if (millis() - lastIRCheck > 50) { // Check every 50ms
    int currentIRState = digitalRead(IR_SENSOR_PIN);
    irSensorState = currentIRState;
    
    // Staircase detection logic
    if (currentIRState != lastIRState) {
      consecutiveIRChanges++;
      lastIRChangeTime = millis();
      
      lastIRState = currentIRState;
      stableIRReadings = 0;
    } else {
      stableIRReadings++;
    }
    
    // Detect staircase if we have many rapid changes
    if (consecutiveIRChanges >= 6 && !staircaseDetected) {
      staircaseDetected = true;
      Serial.println("[IR SENSOR] 🚶 STAIRCASE DETECTED!");
      // Pattern vibration for staircase
      for (int i = 0; i < 3; i++) {
        digitalWrite(VIBRATOR_PIN, HIGH);
        delay(150);
        digitalWrite(VIBRATOR_PIN, LOW);
        delay(100);
      }
    }
    
    // Reset staircase detection after stable readings
    if (stableIRReadings > 20) { // About 1 second of stable readings
      if (staircaseDetected) {
        Serial.println("[IR SENSOR] Staircase detection reset");
      }
      consecutiveIRChanges = 0;
      staircaseDetected = false;
    }
    
    // White line following logic
    if (isFollowingLine) {
      static unsigned long lastLineCheck = 0;
      if (millis() - lastLineCheck > 200) {
        if (currentIRState == LOW) { // Off the white line
          digitalWrite(VIBRATOR_PIN, HIGH);
          Serial.println("[LINE FOLLOW] ⚠️  OFF THE LINE - Corrective vibration!");
        } else {
          // Only turn off if not obstacle detected
          long distance = measureDistance();
          if (distance >= 50 || distance == 0) {
            digitalWrite(VIBRATOR_PIN, LOW);
          }
        }
        lastLineCheck = millis();
      }
    }
    
    lastIRCheck = millis();
  }
}

void handleButtonPress() {
  static bool lastButtonState = HIGH;
  static unsigned long buttonPressTime = 0;
  
  int currentButtonState = digitalRead(BUTTON_PIN);
  
  // Button pressed (falling edge)
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    if (millis() - lastButtonPress > 300) { // Debounce
      buttonPressCount++;
      buttonPressTime = millis();
      lastButtonPress = millis();
      Serial.print("[BUTTON] Button pressed! Count: ");
      Serial.println(buttonPressCount);
    }
  }
  
  lastButtonState = currentButtonState;
  
  // Process button actions after timeout
  if (buttonPressCount > 0 && millis() - buttonPressTime > 600) {
    if (buttonPressCount == 1) {
      // Single press: Toggle line following
      isFollowingLine = !isFollowingLine;
      Serial.print("[BUTTON] Line following ");
      Serial.println(isFollowingLine ? "ENABLED" : "DISABLED");
      
      // Confirmation vibration
      digitalWrite(VIBRATOR_PIN, HIGH);
      delay(200);
      digitalWrite(VIBRATOR_PIN, LOW);
      
    } else if (buttonPressCount == 2) {
      // Double press: Emergency beep
      Serial.println("[BUTTON] 🚨 EMERGENCY BEEP ACTIVATED!");
      playEmergencyBeep();
    } else if (buttonPressCount >= 4) {
      // Four presses: Force stop line following and buzzer
      Serial.println("[BUTTON] 🛑 FORCE STOP - Line following disabled");
      isFollowingLine = false;
      digitalWrite(BUZZER_PIN, LOW); // Ensure buzzer is off
      buzzerActive = false;
      
      // Confirmation vibration pattern
      for (int i = 0; i < 3; i++) {
        digitalWrite(VIBRATOR_PIN, HIGH);
        delay(100);
        digitalWrite(VIBRATOR_PIN, LOW);
        delay(100);
      }
    }
    
    buttonPressCount = 0;
  }
}

long measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout
  if (duration == 0) return 0; // No echo received
  
  long distance = duration * 0.034 / 2;
  return distance;
}

void playEmergencyBeep() {
  buzzerActive = true;
  Serial.println("[BUZZER] 🚨 Playing emergency beep sequence...");
  
  // Turn off vibrator during beep for clear audio
  digitalWrite(VIBRATOR_PIN, LOW);
  
  // Play emergency sequence with active buzzer
  for (int i = 0; i < melodyLength; i++) {
    if (emergencyMelody[i] == 1) {
      digitalWrite(BUZZER_PIN, HIGH);  // Active buzzer ON
    } else {
      digitalWrite(BUZZER_PIN, LOW);   // Active buzzer OFF
    }
    
    delay(emergencyDurations[i]);
  }
  
  // Ensure buzzer is completely OFF at the end
  digitalWrite(BUZZER_PIN, LOW);
  buzzerActive = false;
  Serial.println("[BUZZER] ✅ Emergency beep sequence completed");
}

void testAllSensors() {
  Serial.println("\n=== SENSOR TEST ===");
  
  // Test Ultrasonic
  long dist = measureDistance();
  Serial.print("Ultrasonic sensor: ");
  Serial.print(dist);
  Serial.println(" cm");
  
  // Test IR
  int irState = digitalRead(IR_SENSOR_PIN);
  Serial.print("IR sensor: ");
  Serial.println(irState == HIGH ? "HIGH (line detected)" : "LOW (no line)");
  
  // Test button
  int buttonState = digitalRead(BUTTON_PIN);
  Serial.print("Button: ");
  Serial.println(buttonState == HIGH ? "NOT PRESSED" : "PRESSED");
  
  // Test vibrator
  Serial.println("Testing vibrator...");
  digitalWrite(VIBRATOR_PIN, HIGH);
  delay(500);
  digitalWrite(VIBRATOR_PIN, LOW);
  Serial.println("Vibrator test complete");
  
  // Test active buzzer
  Serial.println("Testing active buzzer...");
  digitalWrite(BUZZER_PIN, HIGH);
  delay(200);
  digitalWrite(BUZZER_PIN, LOW);
  delay(100);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(200);
  digitalWrite(BUZZER_PIN, LOW);
  Serial.println("Active buzzer test complete");
  
  Serial.println("=== SENSOR TEST COMPLETE ===\n");
}

// Web server handlers
void handleRoot() {
  String html = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Smart Navigation Device</title>
  <style>
    :root {
      --primary: #4361ee;
      --secondary: #3a0ca3;
      --success: #4cc9f0;
      --warning: #f72585;
      --dark: #212529;
      --light: #f8f9fa;
      --gray: #6c757d;
    }
    
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
    }
    
    body {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      padding: 20px;
      color: var(--dark);
    }
    
    .container {
      max-width: 1000px;
      margin: 0 auto;
    }
    
    .header {
      text-align: center;
      margin-bottom: 30px;
      color: white;
    }
    
    .header h1 {
      font-size: 2.5rem;
      margin-bottom: 10px;
      text-shadow: 0 2px 4px rgba(0,0,0,0.3);
    }
    
    .header p {
      font-size: 1.1rem;
      opacity: 0.9;
    }
    
    .dashboard {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
      gap: 20px;
      margin-bottom: 30px;
    }
    
    .card {
      background: rgba(255, 255, 255, 0.95);
      border-radius: 15px;
      padding: 20px;
      box-shadow: 0 10px 30px rgba(0, 0, 0, 0.1);
      backdrop-filter: blur(10px);
      border: 1px solid rgba(255, 255, 255, 0.2);
    }
    
    .card-header {
      display: flex;
      align-items: center;
      margin-bottom: 15px;
      padding-bottom: 10px;
      border-bottom: 1px solid rgba(0,0,0,0.1);
    }
    
    .card-header h2 {
      font-size: 1.3rem;
      margin-left: 10px;
    }
    
    .status-item {
      display: flex;
      justify-content: space-between;
      margin-bottom: 10px;
      padding: 8px 0;
    }
    
    .status-label {
      font-weight: 600;
      color: var(--gray);
    }
    
    .status-value {
      font-weight: 600;
    }
    
    .online {
      color: #28a745;
    }
    
    .offline {
      color: #dc3545;
    }
    
    .warning {
      color: #ffc107;
    }
    
    .location-card {
      grid-column: 1 / -1;
    }
    
    #location-display {
      background: rgba(0,0,0,0.05);
      border-radius: 10px;
      padding: 15px;
      margin: 15px 0;
      min-height: 120px;
      font-family: monospace;
    }
    
    .btn {
      display: inline-block;
      padding: 12px 24px;
      background: var(--primary);
      color: white;
      border: none;
      border-radius: 8px;
      font-size: 16px;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.3s ease;
      text-align: center;
      text-decoration: none;
      box-shadow: 0 4px 6px rgba(0,0,0,0.1);
    }
    
    .btn:hover {
      transform: translateY(-2px);
      box-shadow: 0 6px 12px rgba(0,0,0,0.15);
    }
    
    .btn:active {
      transform: translateY(0);
    }
    
    .btn-primary {
      background: linear-gradient(135deg, var(--primary), var(--secondary));
    }
    
    .btn-warning {
      background: linear-gradient(135deg, #ff6b6b, #ee5a24);
    }
    
    .btn-success {
      background: linear-gradient(135deg, #1dd1a1, #10ac84);
    }
    
    .controls {
      display: flex;
      gap: 15px;
      flex-wrap: wrap;
      margin-top: 20px;
    }
    
    .loading {
      display: inline-block;
      width: 20px;
      height: 20px;
      border: 3px solid rgba(255,255,255,0.3);
      border-radius: 50%;
      border-top-color: white;
      animation: spin 1s linear infinite;
      margin-right: 10px;
    }
    
    @keyframes spin {
      to { transform: rotate(360deg); }
    }
    
    .sensor-data {
      display: flex;
      align-items: center;
      justify-content: center;
      font-size: 2rem;
      font-weight: bold;
      height: 80px;
      margin: 10px 0;
    }
    
    .distance-indicator {
      width: 100%;
      height: 20px;
      background: #e9ecef;
      border-radius: 10px;
      overflow: hidden;
      margin: 15px 0;
    }
    
    .distance-fill {
      height: 100%;
      background: linear-gradient(90deg, #28a745, #ffc107, #dc3545);
      border-radius: 10px;
      transition: width 0.5s ease;
    }
    
    .instructions {
      background: #e3f2fd;
      border-left: 4px solid var(--primary);
      padding: 15px;
      border-radius: 0 8px 8px 0;
      margin: 15px 0;
    }
    
    .instructions h4 {
      margin-bottom: 10px;
      color: var(--primary);
    }
    
    .instructions ul {
      padding-left: 20px;
    }
    
    .instructions li {
      margin-bottom: 5px;
    }
    
    @media (max-width: 768px) {
      .dashboard {
        grid-template-columns: 1fr;
      }
      
      .controls {
        flex-direction: column;
      }
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>🦯 Smart Navigation Device</h1>
      <p>Real-time monitoring and control interface</p>
    </div>
    
    <div class="dashboard">
      <div class="card">
        <div class="card-header">
          <span>📊</span>
          <h2>Device Status</h2>
        </div>
        <div class="status-item">
          <span class="status-label">WiFi Status:</span>
          <span class="status-value online" id="wifi-status">Online</span>
        </div>
        <div class="status-item">
          <span class="status-label">Connected Clients:</span>
          <span class="status-value" id="client-count">0</span>
        </div>
        <div class="status-item">
          <span class="status-label">Line Following:</span>
          <span class="status-value" id="line-following">Inactive</span>
        </div>
        <div class="status-item">
          <span class="status-label">Uptime:</span>
          <span class="status-value" id="uptime">0s</span>
        </div>
      </div>
      
      <div class="card">
        <div class="card-header">
          <span>📏</span>
          <h2>Ultrasonic Sensor</h2>
        </div>
        <div class="status-item">
          <span class="status-label">Distance:</span>
          <span class="status-value" id="distance">0 cm</span>
        </div>
        <div class="distance-indicator">
          <div class="distance-fill" id="distance-visual" style="width: 0%"></div>
        </div>
        <div class="status-item">
          <span class="status-label">Obstacle Status:</span>
          <span class="status-value" id="obstacle-status">Clear</span>
        </div>
      </div>
      
      <div class="card">
        <div class="card-header">
          <span>🔄</span>
          <h2>IR Sensor</h2>
        </div>
        <div class="status-item">
          <span class="status-label">IR State:</span>
          <span class="status-value" id="ir-state">Unknown</span>
        </div>
        <div class="status-item">
          <span class="status-label">Line Detection:</span>
          <span class="status-value" id="line-detection">No Line</span>
        </div>
        <div class="status-item">
          <span class="status-label">Staircase Detection:</span>
          <span class="status-value" id="staircase">Inactive</span>
        </div>
      </div>
      
      <div class="card location-card">
        <div class="card-header">
          <span>📍</span>
          <h2>Location Services</h2>
        </div>
        <div id="location-display">)=====";
  html += locationData;
  html += R"=====(</div>
        <div class="controls">
          <button class="btn btn-primary" onclick="getLocation()" id="location-btn">
            <span class="loading" id="location-loading" style="display: none;"></span>
            📱 Get My Location
          </button>
          <button class="btn btn-success" onclick="toggleLineFollowing()" id="line-follow-btn">
            🚶 Toggle Line Following
          </button>
          <button class="btn btn-warning" onclick="emergencyBeep()" id="emergency-btn">
            🚨 Emergency Beep
          </button>
        </div>
        <div class="instructions">
          <h4>Device Control Instructions:</h4>
          <ul>
            <li><strong>Single Button Press:</strong> Toggle line following mode</li>
            <li><strong>Double Button Press:</strong> Play emergency beep</li>
            <li><strong>Quadruple Press:</strong> Force stop all functions</li>
            <li><strong>Vibration:</strong> Obstacle detected or off white line</li>
            <li><strong>Pattern Vibration:</strong> Staircase detected</li>
          </ul>
        </div>
      </div>
    </div>
  </div>

  <script>
    // Update device status periodically
    function updateStatus() {
      fetch('/status')
        .then(response => response.json())
        .then(data => {
          document.getElementById('distance').textContent = data.ultrasonic_distance + ' cm';
          document.getElementById('obstacle-status').textContent = 
            data.ultrasonic_distance > 0 && data.ultrasonic_distance < 50 ? 'OBSTACLE!' : 'Clear';
          document.getElementById('obstacle-status').className = 
            data.ultrasonic_distance > 0 && data.ultrasonic_distance < 50 ? 'status-value warning' : 'status-value';
          
          // Update distance visualization
          const visualWidth = Math.min(100, (data.ultrasonic_distance / 200) * 100);
          document.getElementById('distance-visual').style.width = visualWidth + '%';
          
          document.getElementById('ir-state').textContent = data.ir_sensor == 1 ? 'HIGH' : 'LOW';
          document.getElementById('line-detection').textContent = data.ir_sensor == 1 ? 'Line Detected' : 'No Line';
          document.getElementById('line-following').textContent = data.line_following ? 'Active' : 'Inactive';
          document.getElementById('line-following').className = data.line_following ? 'status-value online' : 'status-value';
          document.getElementById('uptime').textContent = data.uptime_seconds + 's';
        })
        .catch(error => {
          console.error('Error fetching status:', error);
        });
        
      fetch('/location')
        .then(response => response.json())
        .then(data => {
          if (data.location && data.location !== "Location not available") {
            document.getElementById('location-display').innerHTML = data.location.replace(/\n/g, '<br>');
          }
        });
    }
    
    // Get user's location
    function getLocation() {
      const btn = document.getElementById('location-btn');
      const loading = document.getElementById('location-loading');
      
      if (!navigator.geolocation) {
        alert("Geolocation is not supported by this browser.");
        return;
      }
      
      btn.disabled = true;
      loading.style.display = 'inline-block';
      btn.innerHTML = '<span class="loading" id="location-loading"></span> Getting Location...';
      
      navigator.geolocation.getCurrentPosition(
        function(position) {
          const lat = position.coords.latitude;
          const lon = position.coords.longitude;
          const acc = position.coords.accuracy;
          
          const locationHTML = 
            '🌍 <strong>Latitude:</strong> ' + lat.toFixed(6) + '<br>' +
            '🗺️ <strong>Longitude:</strong> ' + lon.toFixed(6) + '<br>' +
            '🎯 <strong>Accuracy:</strong> ±' + acc.toFixed(1) + ' meters<br>' +
            '🕐 <strong>Updated:</strong> ' + new Date().toLocaleTimeString();
          
          document.getElementById('location-display').innerHTML = locationHTML;
          
          // Send to device
          fetch('/updateLocation?lat=' + lat + '&lon=' + lon + '&acc=' + acc)
            .then(response => response.text())
            .then(data => {
              console.log('Location sent to device:', data);
            })
            .catch(error => {
              console.error('Error sending location:', error);
            });
            
          btn.disabled = false;
          loading.style.display = 'none';
          btn.innerHTML = '📱 Get My Location';
        },
        function(error) {
          let errorMsg;
          switch(error.code) {
            case error.PERMISSION_DENIED:
              errorMsg = "Location access denied by user.";
              break;
            case error.POSITION_UNAVAILABLE:
              errorMsg = "Location information unavailable.";
              break;
            case error.TIMEOUT:
              errorMsg = "Location request timed out.";
              break;
            default:
              errorMsg = "An unknown error occurred.";
              break;
          }
          
          document.getElementById('location-display').innerHTML = '❌ ' + errorMsg;
          btn.disabled = false;
          loading.style.display = 'none';
          btn.innerHTML = '📱 Get My Location';
        },
        {
          enableHighAccuracy: true,
          timeout: 10000,
          maximumAge: 60000
        }
      );
    }
    
    // Control functions
    function toggleLineFollowing() {
      fetch('/control?action=toggleLine')
        .then(response => response.text())
        .then(data => {
          console.log('Line following toggled:', data);
          updateStatus();
        });
    }
    
    function emergencyBeep() {
      fetch('/control?action=emergency')
        .then(response => response.text())
        .then(data => {
          console.log('Emergency beep activated:', data);
        });
    }
    
    // Initial status update
    updateStatus();
    
    // Update status every 2 seconds
    setInterval(updateStatus, 2000);
    
    // Update client count (simulated)
    setInterval(() => {
      // This would normally come from the device
      document.getElementById('client-count').textContent = 
        Math.floor(Math.random() * 3) + 1; // Simulate 1-3 clients
    }, 5000);
  </script>
</body>
</html>
)=====";
  
  server.send(200, "text/html", html);
}

void handleLocation() {
  String jsonResponse = "{";
  jsonResponse += "\"location\":\"" + escapeJsonString(locationData) + "\",";
  jsonResponse += "\"timestamp\":\"" + String(millis()) + "\",";
  jsonResponse += "\"lineFollowing\":" + String(isFollowingLine ? "true" : "false") + ",";
  jsonResponse += "\"distance\":" + String(lastDistance) + ",";
  jsonResponse += "\"irSensor\":" + String(irSensorState);
  jsonResponse += "}";
  
  server.send(200, "application/json", jsonResponse);
}

void handleUpdateLocation() {
  if (server.hasArg("lat") && server.hasArg("lon")) {
    float lat = server.arg("lat").toFloat();
    float lon = server.arg("lon").toFloat();
    float acc = server.hasArg("acc") ? server.arg("acc").toFloat() : 0;
    
    locationData = "Latitude: " + String(lat, 6) + "\n";
    locationData += "Longitude: " + String(lon, 6) + "\n";
    locationData += "Accuracy: " + String(acc, 1) + " meters\n";
    locationData += "Updated: " + String(millis() / 1000) + "s";
    
    Serial.println("\n[LOCATION] 📍 Location updated:");
    Serial.println("  Latitude: " + String(lat, 6));
    Serial.println("  Longitude: " + String(lon, 6));
    Serial.println("  Accuracy: " + String(acc, 1) + " meters");
    
    server.send(200, "text/plain", "Location updated successfully");
  } else {
    server.send(400, "text/plain", "Missing latitude or longitude");
  }
}

void handleStatus() {
  String status = "{\n";
  status += "  \"ultrasonic_distance\": " + String(lastDistance) + ",\n";
  status += "  \"ir_sensor\": " + String(irSensorState) + ",\n";
  status += "  \"button_state\": " + String(digitalRead(BUTTON_PIN)) + ",\n";
  status += "  \"line_following\": " + String(isFollowingLine ? "true" : "false") + ",\n";
  status += "  \"vibrator_active\": " + String(digitalRead(VIBRATOR_PIN) ? "true" : "false") + ",\n";
  status += "  \"buzzer_active\": " + String(buzzerActive ? "true" : "false") + ",\n";
  status += "  \"uptime_seconds\": " + String(millis() / 1000) + "\n";
  status += "}";
  
  server.send(200, "application/json", status);
}

void handleControl() {
  if (server.hasArg("action")) {
    String action = server.arg("action");
    
    if (action == "toggleLine") {
      isFollowingLine = !isFollowingLine;
      Serial.print("[WEB] Line following ");
      Serial.println(isFollowingLine ? "ENABLED" : "DISABLED");
      server.send(200, "text/plain", "Line following toggled");
    } 
    else if (action == "emergency") {
      Serial.println("[WEB] 🚨 EMERGENCY BEEP ACTIVATED!");
      playEmergencyBeep();
      server.send(200, "text/plain", "Emergency beep activated");
    }
    else {
      server.send(400, "text/plain", "Unknown action");
    }
  } else {
    server.send(400, "text/plain", "Missing action parameter");
  }
}

String escapeJsonString(String input) {
  input.replace("\"", "\\\"");
  input.replace("\n", "\\n");
  input.replace("\r", "\\r");
  input.replace("\t", "\\t");
  return input;
}