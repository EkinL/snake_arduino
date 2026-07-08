# 🐍 Snake ESP32 + Client Python

Jeu **Snake** qui tourne sur un **ESP32** avec un écran **OLED SSD1306**.
L'ESP32 héberge un serveur **WebSocket** ; on le pilote à distance depuis un
**client Python** lancé dans le terminal.

```
 ┌────────────────────┐        Wi-Fi (même réseau)        ┌──────────────────────┐
 │   PC / Terminal    │  ──────────────────────────────► │        ESP32          │
 │  snake_client.py   │      ws://<IP_ESP32>:81/          │   getIp.ino           │
 │  (touches Z Q S D) │  ◄────────────────────────────── │   OLED 128x64 + jeu   │
 └────────────────────┘         commandes U/D/L/R         └──────────────────────┘
```

Le jeu s'affiche et se joue **sur l'écran OLED**. Le client Python ne fait
qu'envoyer les directions.

---

## 🧰 Matériel requis

- Une carte **ESP32**
- Un écran **OLED SSD1306** 128×64 en **I²C**
- Câblage I²C par défaut dans le code :
  - `SDA` → **GPIO 18**
  - `SCL` → **GPIO 17**
  - Adresse I²C : `0x3C`

> Ces broches sont définies en haut de [`getIp.ino`](getIp.ino) (`SDA_PIN`, `SCL_PIN`, `OLED_ADDR`) — adaptez-les à votre câblage si besoin.

---

## 1️⃣ Partie ESP32 (Arduino)

### a. Installer les bibliothèques

Dans l'IDE Arduino (**Croquis → Inclure une bibliothèque → Gérer les bibliothèques**), installez :

| Bibliothèque | Auteur |
|---|---|
| **WebSockets** | Markus Sattler (`links2004`) |
| **Adafruit GFX Library** | Adafruit |
| **Adafruit SSD1306** | Adafruit |

> `WiFi` et `Wire` sont fournis avec le cœur ESP32, rien à installer.

Assurez-vous aussi d'avoir le **support des cartes ESP32** (menu *Outils → Type de carte → Gestionnaire de cartes → esp32*).

### b. Configurer le Wi-Fi

En haut de [`getIp.ino`](getIp.ino), renseignez **votre** réseau (le PC qui lance le client Python doit être sur **le même réseau**) :

```cpp
const char* ssid     = "VOTRE_RESEAU";
const char* password = "VOTRE_MOT_DE_PASSE";
```

### c. Téléverser

1. Branchez l'ESP32 en USB.
2. Sélectionnez la bonne carte et le bon port dans **Outils**.
3. Cliquez sur **Téléverser (→)**.

### d. Récupérer l'adresse IP 🌐

Au démarrage, l'ESP32 se connecte au Wi-Fi puis **affiche son adresse IP** à deux endroits :

- **Sur l'écran OLED** pendant 5 secondes (`IP : 192.168.x.x`)
- **Sur le moniteur série** (*Outils → Moniteur série*, vitesse **115200 bauds**)

**Notez cette adresse IP**, elle sera nécessaire côté Python. 👇

---

## 2️⃣ Partie Python (client / manette)

### a. Prérequis

- **Python 3**
- La dépendance **`websocket-client`** :

```bash
pip install -r requirements.txt
# ou directement :
pip install websocket-client
```

> Le module `curses` est fourni de base sur **macOS/Linux**.
> Sous **Windows**, ajoutez : `pip install windows-curses`.

### b. Renseigner l'IP de l'ESP32

Ouvrez [`snake_client.py`](snake_client.py) et remplacez l'IP par celle affichée par l'ESP32 :

```python
ESP32_IP = "192.168.x.x"   # ← l'IP affichée sur l'OLED / le moniteur série
```

### c. Lancer le client

```bash
python3 snake_client.py
```

Le terminal passe en mode plein écran (curses) et se connecte à l'ESP32.

---

## 🎮 Mode d'emploi

Une fois l'ESP32 sous tension **et** le client Python lancé :

| Touche | Action |
|:---:|---|
| **Z** | Aller en **haut** |
| **S** | Aller en **bas** |
| **Q** | Aller à **gauche** |
| **D** | Aller à **droite** |
| **A** | **Quitter** le client |

Règles du jeu :

- Mangez la nourriture 🍎 : le serpent grandit et **accélère**.
- **Partie perdue** si le serpent touche un mur ou se mord la queue → `GAME OVER` sur l'OLED.
- Après un *Game Over*, **appuyez sur n'importe quelle direction** pour **relancer** une partie.

> ℹ️ Le message affiché au lancement (« Fleches pour jouer ») est indicatif :
> les vraies touches sont **Z Q S D** (comme un clavier AZERTY) et **A** pour quitter.

---

## 🩹 Dépannage

| Problème | Piste |
|---|---|
| `Ecran OLED non detecte` (série) | Vérifiez le câblage I²C, l'adresse `0x3C` et les broches `SDA`/`SCL`. |
| L'ESP32 reste bloqué sur `...` | Mauvais SSID / mot de passe Wi-Fi, ou réseau hors de portée. |
| Le client Python ne se connecte pas | IP erronée dans `ESP32_IP`, PC et ESP32 pas sur le **même réseau**, ou pare-feu bloquant le port **81**. |
| `ModuleNotFoundError: websocket` | `pip install websocket-client` (et **pas** `websockets`). |
| Erreur `curses` sous Windows | `pip install windows-curses`. |

---

## 🔁 Intégration continue (CI)

Le workflow [`.github/workflows/arduino-ci.yml`](.github/workflows/arduino-ci.yml)
**compile automatiquement** `getIp.ino` pour l'ESP32 à chaque *push* / *pull request*
(via [`arduino-cli`](https://arduino.github.io/arduino-cli/)), afin de garantir que
le croquis compile toujours.

Pour l'activer, ce dossier doit être un dépôt Git poussé sur GitHub :

```bash
git init
git add .
git commit -m "Snake ESP32 + client Python"
git branch -M main
git remote add origin <URL_DE_VOTRE_DEPOT>
git push -u origin main
```

La CI apparaît ensuite dans l'onglet **Actions** du dépôt.

---

## 📂 Structure du projet

```
snake/
├── getIp.ino                     # Croquis ESP32 (Snake + OLED + serveur WebSocket)
├── snake_client.py               # Client Python (manette dans le terminal)
├── requirements.txt              # Dépendances Python
├── README.md                     # Ce fichier
└── .github/workflows/
    └── arduino-ci.yml            # CI : compilation du croquis Arduino
```
