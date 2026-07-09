#include <WiFi.h>
#include <WebSocketsServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

const char* ssid = "decode-etudiants";
const char* password = "learnByDoing25!";

#define SDA_PIN 18
#define SCL_PIN 17
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
WebSocketsServer webSocket = WebSocketsServer(81);

#define CELL 4
#define GRID_W (SCREEN_WIDTH / CELL)   // 32
#define GRID_H (SCREEN_HEIGHT / CELL)  // 16

struct Point { int8_t x; int8_t y; };

// ---- Etat de jeu ----
enum GameState { LOBBY, COUNTDOWN, RUNNING, GAME_OVER };
GameState state = LOBBY;

// ---- Joueurs (slot 0 = J1, slot 1 = J2) ----
int clientNum[2] = { -1, -1 };  // num du client WebSocket pour chaque slot, -1 = libre

// ---- Deux serpents ----
Point snake1[GRID_W * GRID_H];
Point snake2[GRID_W * GRID_H];
int len1, len2;
int dx1, dy1, dx2, dy2;
Point food;

// ---- Timing ----
unsigned long lastMove = 0;
int moveDelay = 200;

int countdown = 3;
unsigned long lastCountdownTick = 0;
const unsigned long COUNTDOWN_INTERVAL = 800;

unsigned long gameOverAt = 0;
const unsigned long RESULT_DURATION = 3000;

int winner = 0;  // 0 = egalite, 1 = J1, 2 = J2

// ========================= Utilitaires joueurs =========================

bool bothConnected() { return clientNum[0] >= 0 && clientNum[1] >= 0; }

int slotForNum(int num) {
  if (clientNum[0] == num) return 0;
  if (clientNum[1] == num) return 1;
  return -1;
}

int assignSlot(int num) {
  if (clientNum[0] < 0) { clientNum[0] = num; return 0; }
  if (clientNum[1] < 0) { clientNum[1] = num; return 1; }
  return -1;
}

// ========================= Utilitaires jeu =========================

bool outOfBounds(Point p) {
  return p.x < 0 || p.x >= GRID_W || p.y < 0 || p.y >= GRID_H;
}

bool cellInSnake(Point p, Point* s, int len) {
  for (int i = 0; i < len; i++)
    if (s[i].x == p.x && s[i].y == p.y) return true;
  return false;
}

void placeFood() {
  bool ok;
  do {
    ok = true;
    food.x = random(0, GRID_W);
    food.y = random(0, GRID_H);
    if (cellInSnake(food, snake1, len1) || cellInSnake(food, snake2, len2)) ok = false;
  } while (!ok);
}

void resetRound() {
  len1 = 3;
  snake1[0] = {8, 4}; snake1[1] = {7, 4}; snake1[2] = {6, 4};
  dx1 = 1; dy1 = 0;

  len2 = 3;
  snake2[0] = {23, 11}; snake2[1] = {24, 11}; snake2[2] = {25, 11};
  dx2 = -1; dy2 = 0;

  moveDelay = 200;
  placeFood();
}

void advance(Point* s, int &len, Point head, bool ate) {
  if (ate) {                              // grandir : on decale et on ajoute la tete
    for (int i = len; i > 0; i--) s[i] = s[i - 1];
    s[0] = head;
    len++;
  } else {                                // avancer : on decale, la queue disparait
    for (int i = len - 1; i > 0; i--) s[i] = s[i - 1];
    s[0] = head;
  }
}

void applyDir(char c, int slot) {
  if (slot == 0) {
    if (c == 'U' && dy1 != 1)      { dx1 = 0;  dy1 = -1; }
    else if (c == 'D' && dy1 != -1){ dx1 = 0;  dy1 = 1;  }
    else if (c == 'L' && dx1 != 1) { dx1 = -1; dy1 = 0;  }
    else if (c == 'R' && dx1 != -1){ dx1 = 1;  dy1 = 0;  }
  } else {
    if (c == 'U' && dy2 != 1)      { dx2 = 0;  dy2 = -1; }
    else if (c == 'D' && dy2 != -1){ dx2 = 0;  dy2 = 1;  }
    else if (c == 'L' && dx2 != 1) { dx2 = -1; dy2 = 0;  }
    else if (c == 'R' && dx2 != -1){ dx2 = 1;  dy2 = 0;  }
  }
}

// ========================= Rendu OLED =========================

void drawField() {
  // J1 = carres pleins
  for (int i = 0; i < len1; i++)
    display.fillRect(snake1[i].x * CELL, snake1[i].y * CELL, CELL, CELL, SSD1306_WHITE);
  // J2 = carres evides
  for (int i = 0; i < len2; i++)
    display.drawRect(snake2[i].x * CELL, snake2[i].y * CELL, CELL, CELL, SSD1306_WHITE);
  // Pomme = petit point
  display.fillCircle(food.x * CELL + CELL / 2, food.y * CELL + CELL / 2, 1, SSD1306_WHITE);
}

void drawCountdown() {
  // fond noir derriere le chiffre pour qu'il reste lisible sur le terrain
  display.fillRect(SCREEN_WIDTH / 2 - 14, SCREEN_HEIGHT / 2 - 16, 28, 32, SSD1306_BLACK);
  display.drawRect(SCREEN_WIDTH / 2 - 14, SCREEN_HEIGHT / 2 - 16, 28, 32, SSD1306_WHITE);
  display.setTextSize(3);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(SCREEN_WIDTH / 2 - 8, SCREEN_HEIGHT / 2 - 11);
  display.print(countdown);
  display.setTextSize(1);
}

void drawLobby() {
  int n = (clientNum[0] >= 0 ? 1 : 0) + (clientNum[1] >= 0 ? 1 : 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("SNAKE 2 JOUEURS");
  display.println();
  if (n == 0) display.println("Attente joueurs...");
  else        display.println("Attente joueur 2...");
  display.println();
  display.print("Connectes: ");
  display.print(n);
  display.println("/2");
}

void drawGameOver() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("GAME OVER");
  display.println();
  if (winner == 0) {
    display.println("Egalite !");
  } else {
    display.print("Joueur ");
    display.print(winner);
    display.println(" gagne !");
  }
  display.println();
  display.print("J1:"); display.print(len1);
  display.print("  J2:"); display.println(len2);
}

void draw() {
  display.clearDisplay();
  switch (state) {
    case LOBBY:     drawLobby(); break;
    case COUNTDOWN: drawField(); drawCountdown(); break;
    case RUNNING:   drawField(); break;
    case GAME_OVER: drawGameOver(); break;
  }
  display.display();
}

// ========================= Protocole (ESP32 -> tel) =========================

void sendPhaseTo(int num) {
  switch (state) {
    case LOBBY:     webSocket.sendTXT(num, "LOBBY"); break;
    case COUNTDOWN: { char buf[4] = { 'C', 'D', (char)('0' + countdown), 0 }; webSocket.sendTXT(num, buf); break; }
    case RUNNING:   webSocket.sendTXT(num, "PLAY"); break;
    case GAME_OVER: webSocket.sendTXT(num, "LOBBY"); break;
  }
}

void sendResult(int slot, int w) {
  if (clientNum[slot] < 0) return;
  const char* msg;
  if (w == 0)                                      msg = "DRAW";
  else if ((w == 1 && slot == 0) || (w == 2 && slot == 1)) msg = "WIN";
  else                                             msg = "LOSE";
  webSocket.sendTXT(clientNum[slot], msg);
}

// ========================= Transitions d'etat =========================

void goLobby() {
  state = LOBBY;
  webSocket.broadcastTXT("LOBBY");
  draw();
}

void startCountdown() {
  resetRound();
  state = COUNTDOWN;
  countdown = 3;
  lastCountdownTick = millis();
  webSocket.broadcastTXT("CD3");
  draw();
}

void startRound() {
  state = RUNNING;
  lastMove = millis();
  webSocket.broadcastTXT("PLAY");
  draw();
}

void endRound(int w) {
  winner = w;
  state = GAME_OVER;
  gameOverAt = millis();
  sendResult(0, w);
  sendResult(1, w);
  draw();
}

// ========================= Tick de jeu =========================

void tick() {
  Point h1 = { (int8_t)(snake1[0].x + dx1), (int8_t)(snake1[0].y + dy1) };
  Point h2 = { (int8_t)(snake2[0].x + dx2), (int8_t)(snake2[0].y + dy2) };

  bool headOn = (h1.x == h2.x && h1.y == h2.y);
  bool dead1 = outOfBounds(h1) || cellInSnake(h1, snake1, len1) || cellInSnake(h1, snake2, len2) || headOn;
  bool dead2 = outOfBounds(h2) || cellInSnake(h2, snake2, len2) || cellInSnake(h2, snake1, len1) || headOn;

  if (dead1 || dead2) {
    int w;
    if (dead1 && dead2) {
      if (len1 > len2)      w = 1;
      else if (len2 > len1) w = 2;
      else                  w = 0;  // egalite
    } else if (dead1) {
      w = 2;
    } else {
      w = 1;
    }
    endRound(w);
    return;
  }

  bool ate1 = (h1.x == food.x && h1.y == food.y);
  bool ate2 = (h2.x == food.x && h2.y == food.y);

  advance(snake1, len1, h1, ate1);
  advance(snake2, len2, h2, ate2);

  if (ate1 || ate2) {
    placeFood();
    if (moveDelay > 90) moveDelay -= 6;
  }
  draw();
}

// ========================= Evenements WebSocket =========================

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_CONNECTED) {
    int slot = assignSlot(num);
    if (slot < 0) {                          // deja 2 joueurs : on refuse
      webSocket.sendTXT(num, "FULL");
      webSocket.disconnect(num);
      return;
    }
    webSocket.sendTXT(num, slot == 0 ? "P1" : "P2");
    sendPhaseTo(num);
    Serial.printf("Joueur %d connecte (num %d)\n", slot + 1, num);
    // Si les 2 sont la et qu'on attendait, la boucle lancera le decompte.

  } else if (type == WStype_DISCONNECTED) {
    int slot = slotForNum(num);
    if (slot >= 0) clientNum[slot] = -1;
    Serial.printf("Joueur %d deconnecte (num %d)\n", slot + 1, num);
    if (state == RUNNING) {
      endRound((slot == 0) ? 2 : 1);         // l'autre gagne
    } else if (state == COUNTDOWN || state == GAME_OVER) {
      goLobby();
    } else {
      draw();
    }

  } else if (type == WStype_TEXT) {
    if (length < 1) return;
    int slot = slotForNum(num);
    if (slot < 0) return;
    if (state == RUNNING) applyDir((char)payload[0], slot);
  }
}

// ========================= Setup / Loop =========================

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0));

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("Ecran OLED non detecte");
    while (true) delay(1000);
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("IP : ");
  Serial.println(WiFi.localIP());

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("IP :");
  display.println(WiFi.localIP());
  display.display();
  delay(5000);

  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);

  state = LOBBY;
  draw();
}

void loop() {
  webSocket.loop();
  unsigned long now = millis();

  switch (state) {
    case LOBBY:
      if (bothConnected()) startCountdown();
      break;

    case COUNTDOWN:
      if (now - lastCountdownTick >= COUNTDOWN_INTERVAL) {
        countdown--;
        lastCountdownTick = now;
        if (countdown > 0) {
          char buf[4] = { 'C', 'D', (char)('0' + countdown), 0 };
          webSocket.broadcastTXT(buf);
          draw();
        } else {
          startRound();
        }
      }
      break;

    case RUNNING:
      if (now - lastMove >= (unsigned long)moveDelay) {
        lastMove = now;
        tick();
      }
      break;

    case GAME_OVER:
      if (now - gameOverAt >= RESULT_DURATION) {
        if (bothConnected()) startCountdown();
        else                 goLobby();
      }
      break;
  }
}
