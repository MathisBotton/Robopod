#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>

// ===== WIFI =====
const char* ssid = "LABO";
const char* password = "Deezeur-1/4";

// ===== SERVOS =====
Servo servoGauche;
Servo servoDroit;

#define PIN_GAUCHE 25
#define PIN_DROIT 26

#define STOP 90
#define AVANT_G 0
#define AVANT_D 180

WebServer server(80);

// ===== CALIBRATION =====
float cm_par_pixel = 0.12;
float ms_par_cm = 28.57;
float ms_par_deg = 1300.0 / 360.0;

// ===== ETAT ROBOT =====
float robot_angle = 0;
bool first_segment = true;

// ===== PAGE WEB (segments connectés) =====
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Robot Dessinateur</title>
</head>
<body>

<h2>Robopod</h2>

<canvas id="canvas" width="500" height="500" style="border:1px solid black;"></canvas>
<br><br>

<button onclick="send()">Envoyer</button>
<button onclick="reset()">Reset</button>

<script>
let canvas = document.getElementById("canvas");
let ctx = canvas.getContext("2d");

let points = [];

canvas.addEventListener("click", (e) => {
  let rect = canvas.getBoundingClientRect();
  let x = e.clientX - rect.left;
  let y = e.clientY - rect.top;

  points.push({x, y});

  if (points.length > 1) {
    let p1 = points[points.length - 2];
    let p2 = points[points.length - 1];

    ctx.beginPath();
    ctx.moveTo(p1.x, p1.y);
    ctx.lineTo(p2.x, p2.y);
    ctx.stroke();
  } else {
    ctx.fillRect(x, y, 2, 2);
  }
});

function send() {
  fetch("/draw", {
    method: "POST",
    headers: {"Content-Type": "application/json"},
    body: JSON.stringify(points)
  });
}

function reset() {
  ctx.clearRect(0, 0, 300, 300);
  points = [];
}
</script>

</body>
</html>
)rawliteral";

// ===== MOTEURS =====
void stopMotors() {
  servoGauche.write(STOP);
  servoDroit.write(STOP);
}

void avancer(int duree) {
  servoGauche.write(AVANT_G);
  servoDroit.write(AVANT_D);
  delay(duree);
  stopMotors();
}

// ===== ROTATION PROPRE =====
void tournerVers(float target_angle) {

  float diff = target_angle - robot_angle;

  // normalisation [-180, 180]
  while (diff > 180) diff -= 360;
  while (diff < -180) diff += 360;

  // 🔥 zone morte (évite micro rotations)
  if (abs(diff) < 5) return;

  int duree = abs(diff) * ms_par_deg;

  if (diff > 0) {
    servoGauche.write(0);
    servoDroit.write(0);
  } else {
    servoGauche.write(180);
    servoDroit.write(180);
  }

  delay(duree);
  stopMotors();

  robot_angle = target_angle;
}

// ===== PAGE =====
void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

// ===== DESSIN =====
void handleDraw() {

  String body = server.arg("plain");

  DynamicJsonDocument doc(4096);
  deserializeJson(doc, body);

  JsonArray points = doc.as<JsonArray>();

  first_segment = true;

  for (int i = 0; i < points.size() - 1; i++) {

    int x1 = points[i]["x"];
    int y1 = points[i]["y"];
    int x2 = points[i + 1]["x"];
    int y2 = points[i + 1]["y"];

    int dx = x2 - x1;
    int dy = y2 - y1;

    // ===== ANGLE =====
    float angle = atan2(dy, dx) * 180.0 / PI;

    // 🔥 IMPORTANT : pas de rotation forcée au départ
    if (first_segment) {
      robot_angle = angle;
      first_segment = false;
    } else {
      tournerVers(angle);
    }

    // ===== DISTANCE =====
    float dist_px = sqrt(dx * dx + dy * dy);
    float dist_cm = dist_px * cm_par_pixel;
    int t = dist_cm * ms_par_cm;

    avancer(t);
    delay(20);
  }

  stopMotors();
  server.send(200, "text/plain", "OK");
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);

  servoGauche.attach(PIN_GAUCHE);
  servoDroit.attach(PIN_DROIT);

  stopMotors();

  WiFi.begin(ssid, password);

  Serial.print("Connexion");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnecté !");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/draw", HTTP_POST, handleDraw);

  server.begin();
}

// ===== LOOP =====
void loop() {
  server.handleClient();
}