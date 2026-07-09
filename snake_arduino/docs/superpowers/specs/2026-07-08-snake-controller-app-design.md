# Snake Controller — App iOS (design)

Date : 2026-07-08

## Objectif

Une app iOS **manette** qui pilote à distance le jeu Snake tournant sur un
ESP32. Le jeu s'affiche sur l'écran OLED de l'ESP32 ; l'app ne fait qu'envoyer
des directions via WebSocket.

## Contexte (firmware existant)

`snake/main.ino` — serveur WebSocket sur le **port 81**, path `/`. Il attend un
unique caractère texte : `U` / `D` / `L` / `R`. Rien n'est renvoyé au client.
Après un *Game Over*, n'importe quelle commande relance la partie.

## Approche technique

- **WebSocket natif** : `URLSessionWebSocketTask` (iOS 13+). Aucune dépendance.
- **MVVM léger** : une classe observable `SnakeConnection` gère l'unique socket
  et expose son état à deux vues SwiftUI.
- **Persistance** : IP + port stockés via `@AppStorage` (UserDefaults).
- **Réseau local** : `Info.plist` avec `NSAllowsLocalNetworking` (exception ATS
  pour le `ws://` en clair) et `NSLocalNetworkUsageDescription` (permission
  réseau local iOS 14+).

## Composants

| Fichier | Rôle |
|---|---|
| `SnakeConnection.swift` | `ObservableObject @MainActor` : socket, statut, `start/stop/send`, reconnexion auto. Contient `Direction`, `Status`, `TestResult` et une sonde `ConnectionProbe` pour le test. |
| `ControllerView.swift` | Écran manette : barre de statut, engrenage, D-pad (4 boutons). |
| `SettingsView.swift` | Feuille réglages : IP + port, bouton « Tester la connexion », enregistrer. |
| `snake_arduinoApp.swift` | Injecte `SnakeConnection`, lance `ControllerView`. |
| `Info.plist` | Exception ATS réseau local + description permission. |

## États de connexion

`disconnected` → `connecting` → `connected`, plus `failed(String)` (adresse
invalide). Détection fiable via le **delegate** `URLSessionWebSocketDelegate`
(`didOpen` / `didClose` / `didComplete`). Reconnexion automatique avec petit
backoff (1 → 5 s) tant que l'utilisateur n'a pas coupé.

## Flux

```
Tap ▲  →  SnakeConnection.send(.up)  →  ws.send("U")  →  ESP32
                    │
        @Published status  →  point 🟢/🟠/🔴 dans l'UI
```

Pas de logique anti-demi-tour (le firmware ignore déjà la direction inverse).
Pas d'état de jeu reçu — le jeu reste sur l'OLED.

## Test de connexion (réglages)

Sonde jetable `ConnectionProbe` : ouvre une socket temporaire, résout `success`
sur `didOpenWithProtocol`, `failure` sur erreur/timeout (5 s). N'affecte pas la
connexion principale.

## Hors périmètre (YAGNI)

Pas de swipe, pas de rendu du jeu côté téléphone, pas de découverte automatique
(Bonjour), pas de multi-appareils.
