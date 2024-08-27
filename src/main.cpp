#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_Sensor.h> // Ensure this is installed

// Pin definitions
#define GAS_SENSOR_PIN 34
#define IR_SENSOR_PIN 35
#define SERVO_PIN 19
#define SERVO_PIN_R1 15
#define SERVO_PIN_GARAGE 18 // Pin for the garage door servo
#define RAIN_SENSOR_PIN 33
#define FLAME_SENSOR_PIN 32
#define DHT_PIN 23

// DHT sensor setup
#define DHT_TYPE DHT11 // Change to DHT22 if you use a DHT22 sensor
DHT dht(DHT_PIN, DHT_TYPE);

// Firebase setup
#define WIFI_SSID "tedaata"
#define WIFI_PASSWORD "034206196"
#define API_KEY "AIzaSyD9qbMxqikcdxQQ7KBLEA5ph_7edHxpUwA"
#define DB_URL "https://final-project-ff498-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "TbynUS7NwLZSZ6eSBH2RzezrSpF2"
#define USER_EMAIL "yosri12@gmail.com"

// LCD setup
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Keypad setup
const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};
byte rowPins[ROWS] = {13, 12, 25, 26};
byte colPins[COLS] = {27, 14, 2};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Function declarations
void openDoor();
void closeDoor();
void openWindow();
void closeWindow();
void openGarage();
void closeGarage();
void uploadSensorData();
void tokenStatusCallback(TokenInfo info);
void pubGasReadings();
void pubIRReadings();
void pubRainReadings();
void PubDHTReadings();
void PubFlameReadings();
void logToLCD(String message); // Function declaration

// Servo setup
Servo doorServo;
Servo windowServo;
Servo garageServo; // New servo for garage door

// Variables
bool authenticated = false;
bool firebaseReady = false;
unsigned long sendDataPreMillis = 0;
bool applicationRunning = true;
String keypadBuffer = "";
const char* correctPassword = "11111";
int incorrectAttempts = 0;
bool doorStatus = false;
bool windowStatus = false;
bool garageStatus = false; // Variable to track garage door status
int irValue = 0;
int gasValue = 0;
int rainValue = 0;
int flameValue = 0;
float humidity = 0.0;
float temperature = 0.0;

// Servo angles
const int OPEN_ANGLE = 110;
const int CLOSE_ANGLE = -20;
const int OPEN_WIN = 90;
const int CLOSE_WIN = -50;
const int OPEN_GARAGE = 100;
const int CLOSE_GARAGE = 0;

// Variable for cursor position
int col_num = 0;

void setup() {
  Serial.begin(115200);
  
  // Initialize Servos
  doorServo.attach(SERVO_PIN);
  windowServo.attach(SERVO_PIN_R1);
  garageServo.attach(SERVO_PIN_GARAGE);
  doorServo.write(CLOSE_ANGLE);
  windowServo.write(OPEN_WIN);
  garageServo.write(CLOSE_GARAGE); // Initialize garage door to closed

  // Initialize LCD with custom I2C pins
  Wire.begin(21, 22);  
  lcd.init();
  lcd.backlight();
  logToLCD("Initializing...");

  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  logToLCD("Wi-Fi Connected");

  // Initialize Firebase
  config.api_key = API_KEY;
  config.database_url = DB_URL;
  auth.token.uid = "TbynUS7NwLZSZ6eSBH2RzezrSpF2";
  auth.user.email = USER_EMAIL;
  auth.user.password = "123456";
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  if (auth.token.uid.length() > 0) {
    Serial.println("Signed in successfully");
    logToLCD("Signed In");
    firebaseReady = true;
  } else {
    Serial.println("Failed to sign in");
    logToLCD("Sign In Failed");
  }

  // Initialize pins
  pinMode(IR_SENSOR_PIN, INPUT);
  pinMode(GAS_SENSOR_PIN, INPUT);
 // pinMode(RAIN_SENSOR_PIN, INPUT);
  pinMode(FLAME_SENSOR_PIN, INPUT);
  dht.begin();
}

void logToLCD(String message) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(message.substring(0, 16));  // Display first part
  if (message.length() > 16) {
    lcd.setCursor(0, 1);
    lcd.print(message.substring(16));  // Display remaining part
  }
}

void authenticateUser() {
  Serial.print("Please Enter Password:");
  lcd.clear();
  lcd.print("Enter Password:");
  
  String enteredPassword = "";
  const int maxAttempts = 3; // Maximum number of allowed incorrect attempts
  
  for (int attempt = 0; attempt < maxAttempts; attempt++) {
    enteredPassword = ""; // Reset enteredPassword for each attempt
    
    while (applicationRunning) {
      char key = keypad.getKey();
      if (key) {
        Serial.print("Key Pressed: ");
        Serial.println(key);
        
        if (key == '*') {  // '#' acts as the 'enter' key
          if (enteredPassword == correctPassword) {
            authenticated = true;
            logToLCD("Access Granted");
            
            // Perform actions based on IR sensor value
            if (irValue < 1000) {
              openDoor();
              openWindow();
            } else {
              closeDoor();
            }
            return; // Exit function on successful authentication
          } else {
            logToLCD("Wrong Password");
            Serial.println("Wrong password");
            delay(1000);
            lcd.clear();
            logToLCD("Incorrect attempts:");
            lcd.print(attempt + 1); // Display current attempt number
            delay(2000);
            lcd.clear();
            logToLCD("Enter Password");
            break; // Exit the while loop to start a new attempt
          }
        } else {
          enteredPassword += key;  // Add key press to password
          lcd.setCursor(0, 1);
          lcd.print(enteredPassword);
        }
      }
    }
  }
  
  // If maximum attempts are reached, lock the system
  lcd.clear();
  lcd.print("Potential");
  lcd.setCursor(0, 1);
  lcd.print("Hacker Detected");
  applicationRunning = false;
  Serial.println("System locked due to too many incorrect password attempts.");
}




void pubGasReadings() {
  gasValue = analogRead(GAS_SENSOR_PIN);
  Serial.print("Gas READING");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Gas: ");
  lcd.print(gasValue);
  if (gasValue > 1550) {
    lcd.setCursor(0, 1);
    lcd.print("Fire!");
    openWindow();

    delay(1000);
  } else {
    lcd.setCursor(0, 1);
    lcd.print("No Fire");
  }
  delay(2000);

  // Send gas value to Firebase
  if (Firebase.setInt(fbdo, "/Sensor/gas", gasValue)) {
    Serial.print("Gas value sent successfully: ");
    Serial.println(gasValue);
  } else {
    Serial.print("Failed to send data: ");
    Serial.println(fbdo.errorReason());
  }
}

void pubIRReadings() {
  irValue = analogRead(IR_SENSOR_PIN);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("IR: ");
  lcd.print(irValue);

  lcd.setCursor(0, 1);
  if (irValue < 250) { // Adjust this threshold based on your sensor's characteristics
    lcd.print("Object Detected!");
    Serial.print("object detected");
    openDoor();
  } else {
    closeDoor();
    Serial.print("No object ");
    lcd.print("No Object");
  }

  delay(1000);

  // Send IR value to Firebase
  if (Firebase.setInt(fbdo, "/Sensor/ir", irValue)) {
    Serial.print("IR value sent successfully: ");
    Serial.println(irValue);
  } else {
    Serial.print("Failed to send IR data: ");
    Serial.println(fbdo.errorReason());
  }
  delay(2000);
}

void pubRainReadings() {
  rainValue = analogRead(RAIN_SENSOR_PIN);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Rain: ");
  lcd.print(rainValue);

  lcd.setCursor(0, 1);
  if (rainValue <3000) { // Adjust this threshold based on your sensor's characteristics
    lcd.print("Rain Detected!");
    closeWindow();
  } else {
    lcd.print("No Rain");
  }

  delay(1000); // Delay before clearing the LCD

  // Send data to Firebase
  if (Firebase.setInt(fbdo, "/Sensor/rain", rainValue)) {
    Serial.print("Rain value sent successfully: ");
    Serial.println(rainValue);
  } else {
    Serial.print("Failed to send rain data: ");
    Serial.println(fbdo.errorReason());
  }
}

void PubDHTReadings() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  if (humidity > 80|| temperature > 42) {
    Serial.print("Weather is too hot  ");
    logToLCD("Weather is too hot");
    closeDoor();
    closeWindow();
  }

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Display on LCD
  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(temperature);
  lcd.print(" C");

  lcd.setCursor(0, 1);
  lcd.print("Hum: ");
  lcd.print(humidity);
  lcd.print(" %");

  delay(2000); // Delay before clearing the LCD

  lcd.clear();

  // Send data to Firebase
  if (Firebase.setFloat(fbdo, "/DHT/temperature", temperature)) {
    Serial.print("Temperature sent successfully: ");
    Serial.println(temperature);
  } else {
    Serial.print("Failed to send temperature data: ");
    Serial.println(fbdo.errorReason());
  }

  if (Firebase.setFloat(fbdo, "/DHT/humidity", humidity)) {
    Serial.print("Humidity sent successfully: ");
    Serial.println(humidity);
  } else {
    Serial.print("Failed to send humidity data: ");
    Serial.println(fbdo.errorReason());
  }
}

void PubFlameReadings() {
  flameValue = analogRead(FLAME_SENSOR_PIN);
  Serial.println("flame reading:");
  Serial.println(flameValue);
  // Display on LCD
  lcd.setCursor(0, 0);
  lcd.print("Flame: ");
  lcd.print(flameValue);

  // Show flame detected or not
  lcd.setCursor(0, 1);
  if (flameValue <3000) { // Adjust this threshold based on your sensor's characteristics
    lcd.print("Fire Detected!");
    openGarage(); // Open the garage door if fire is detected
  } else {
    closeGarage();
    lcd.print("No Fire");

  }

  delay(1000); // Delay before clearing the LCD

  lcd.clear();

  // Send flame data to Firebase
  if (Firebase.setInt(fbdo, "/Sensor/flame", flameValue)) {
    Serial.print("Flame value sent successfully: ");
    Serial.println(flameValue);
  } else {
    Serial.print("Failed to send flame data: ");
    Serial.println(fbdo.errorReason());
  }
  
  // Send garage door status to Firebase
  if (Firebase.setBool(fbdo, "/Control/garage", garageStatus)) {
    Serial.print("Garage door status sent successfully: ");
    Serial.println(garageStatus ? "Open" : "Closed");
  } else {
    Serial.print("Failed to send garage door status: ");
    Serial.println(fbdo.errorReason());
  }
}


void openDoor() {
  doorServo.write(OPEN_ANGLE);
  doorStatus = true;
   if (Firebase.setBool(fbdo, "/Control/door", doorStatus)) {
    Serial.print("door status sent successfully: ");
    Serial.println(doorStatus ? "Open" : "Closed");
  } else {
    Serial.print("Failed to send door status: ");
    Serial.println(fbdo.errorReason());
  }
}

void closeDoor() {
  doorServo.write(CLOSE_ANGLE);
  doorStatus = false;
  
}

void openWindow() {
  windowServo.write(OPEN_WIN);
  windowStatus = true;
   if (Firebase.setBool(fbdo, "/Control/window", windowStatus)) {
    Serial.print("window status sent successfully: ");
    Serial.println(windowStatus ? "Open" : "Closed");
  } else {
    Serial.print("Failed to send window status: ");
    Serial.println(fbdo.errorReason());
  }
}

void closeWindow() {
  windowServo.write(CLOSE_WIN);
  windowStatus = false;
}

void openGarage() {
  garageServo.write(OPEN_GARAGE);
  garageStatus = true;
}

void closeGarage() {
  garageServo.write(CLOSE_GARAGE);
  garageStatus = false;
}

void loop() {
  if (!authenticated) {
    authenticateUser();
  } else {
    pubGasReadings();
    pubIRReadings();
    pubRainReadings();
    PubDHTReadings();
    PubFlameReadings();

    // Lock system after authentication fails
    if (!applicationRunning) {
      lcd.clear();
      lcd.println("System Locked");
      Serial.println("System locked due to too many incorrect password attempts.");
    
    
      while (true) {
        delay(1000);
      }
    }
  }}

