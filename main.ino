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
#define GRID_W (SCREEN_WIDTH / CELL)
#define GRID_H (SCREEN_HEIGHT / CELL)

struct Point { int8_t x; int8_t y; };

Point snake[GRID_W * GRID_H];
int snakeLen;
int dirX, dirY;
Point food;
unsigned long lastMove = 0;
int moveDelay = 200;
bool gameOver = false;

void placeFood() {
  bool ok;
  do {
    ok = true;
    food.x = random(0, GRID_W);
    food.y = random(0, GRID_H);
    for (int i = 0; i < snakeLen; i++) {
      if (snake[i].x == food.x && snake[i].y == food.y) ok = false;
    }
  } while (!ok);
}

void resetGame() {
  snakeLen = 3;
  snake[0] = {10, 8};
  snake[1] = {9, 8};
  snake[2] = {8, 8};
  dirX = 1;
  dirY = 0;
  gameOver = false;
  moveDelay = 200;
  placeFood();
}

void draw() {
  display.clearDisplay();

  for (int i = 0; i < snakeLen; i++) {
    display.fillRect(snake[i].x * CELL, snake[i].y * CELL, CELL, CELL, SSD1306_WHITE);
  }
  display.fillRect(food.x * CELL, food.y * CELL, CELL, CELL, SSD1306_WHITE);

  if (gameOver) {
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(30, 28);
    display.println("GAME OVER");
  }

  display.display();
}

void moveSnake() {
  if (gameOver) return;

  Point newHead = { (int8_t)(snake[0].x + dirX), (int8_t)(snake[0].y + dirY) };

  if (newHead.x < 0 || newHead.x >= GRID_W || newHead.y < 0 || newHead.y >= GRID_H) {
    gameOver = true;
    draw();
    return;
  }
  for (int i = 0; i < snakeLen; i++) {
    if (snake[i].x == newHead.x && snake[i].y == newHead.y) {
      gameOver = true;
      draw();
      return;
    }
  }

  bool ate = (newHead.x == food.x && newHead.y == food.y);

  for (int i = snakeLen; i > 0; i--) {
    snake[i] = snake[i - 1];
  }
  snake[0] = newHead;

  if (ate) {
    snakeLen++;
    if (moveDelay > 80) moveDelay -= 8;
    placeFood();
  }

  draw();
}

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_CONNECTED) {
    Serial.println("client connecte");
  } else if (type == WStype_DISCONNECTED) {
    Serial.println("client deconnecte");
  }

  if (type == WStype_TEXT) {
    char c = (char)payload[0];
    Serial.print("recu : ");
    Serial.println(c);

    if (gameOver) {
      resetGame();
      draw();
      return;
    }

    if (c == 'U' && dirY != 1) { dirX = 0; dirY = -1; }
    else if (c == 'D' && dirY != -1) { dirX = 0; dirY = 1; }
    else if (c == 'L' && dirX != 1) { dirX = -1; dirY = 0; }
    else if (c == 'R' && dirX != -1) { dirX = 1; dirY = 0; }
  }
}

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

  resetGame();
  draw();
}

void loop() {
  webSocket.loop();

  if (millis() - lastMove > moveDelay) {
    moveSnake();
    lastMove = millis();
  }
}
