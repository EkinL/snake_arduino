//
//  SnakeConnection.swift
//  snake_arduino
//
//  Connexion WebSocket unique vers l'ESP32 : ouverture, reconnexion
//  automatique, envoi des directions et exposition de l'état à l'UI.
//

import Foundation
import Combine

/// Directions envoyées à l'ESP32. La valeur brute est le caractère attendu
/// par le firmware (`main.ino`).
enum Direction: String {
    case up = "U"
    case down = "D"
    case left = "L"
    case right = "R"
}

@MainActor
final class SnakeConnection: NSObject, ObservableObject {

    enum Status: Equatable {
        case disconnected
        case connecting
        case connected
        case failed(String)

        var isConnected: Bool { self == .connected }
    }

    enum TestResult: Equatable {
        case success
        case failure(String)
    }

    @Published private(set) var status: Status = .disconnected

    private var session: URLSession!
    private var task: URLSessionWebSocketTask?

    private var host = ""
    private var port = 81
    private var shouldStayConnected = false
    private var reconnectAttempts = 0

    override init() {
        super.init()
        session = URLSession(configuration: .default, delegate: self, delegateQueue: nil)
    }

    // MARK: - API publique

    /// Démarre (ou redémarre) la connexion, avec reconnexion automatique tant
    /// que `stop()` n'est pas appelé.
    func start(host: String, port: Int) {
        let trimmed = host.trimmingCharacters(in: .whitespaces)
        task?.cancel(with: .goingAway, reason: nil)
        task = nil
        self.host = trimmed
        self.port = port
        shouldStayConnected = true
        reconnectAttempts = 0
        openSocket()
    }

    /// Coupe la connexion et désactive la reconnexion automatique.
    func stop() {
        shouldStayConnected = false
        reconnectAttempts = 0
        task?.cancel(with: .goingAway, reason: nil)
        task = nil
        status = .disconnected
    }

    /// Envoie une direction si la socket est connectée.
    func send(_ direction: Direction) {
        guard let task, status.isConnected else { return }
        let id = ObjectIdentifier(task)
        task.send(.string(direction.rawValue)) { [weak self] error in
            guard error != nil else { return }
            Task { @MainActor in
                guard let self, let current = self.task,
                      ObjectIdentifier(current) == id else { return }
                self.connectionLost()
            }
        }
    }

    // MARK: - Cycle de vie de la socket

    private func openSocket() {
        guard !host.isEmpty, let url = URL(string: "ws://\(host):\(port)/") else {
            status = .failed("Adresse invalide")
            shouldStayConnected = false
            return
        }
        status = .connecting
        let task = session.webSocketTask(with: url)
        self.task = task
        task.resume()
        // Le delegate signalera `didOpen` (→ connecté) ou une erreur (→ perdu).
    }

    private func markConnected(_ id: ObjectIdentifier) {
        guard let task, ObjectIdentifier(task) == id else { return }
        status = .connected
        reconnectAttempts = 0
    }

    /// Appelé une seule fois par socket (grâce à la comparaison d'identité)
    /// quand la connexion tombe. Planifie une reconnexion si demandé.
    private func connectionLost() {
        task?.cancel(with: .abnormalClosure, reason: nil)
        task = nil

        guard shouldStayConnected else {
            status = .disconnected
            return
        }

        status = .connecting
        reconnectAttempts += 1
        let delay = min(1.0 + Double(reconnectAttempts) * 0.5, 5.0)

        Task { @MainActor in
            try? await Task.sleep(for: .seconds(delay))
            guard self.shouldStayConnected, self.task == nil else { return }
            self.openSocket()
        }
    }

    private func handleClosedTask(_ id: ObjectIdentifier) {
        guard let task, ObjectIdentifier(task) == id else { return }
        connectionLost()
    }

    // MARK: - Test de connexion (réglages)

    /// Teste une adresse sans toucher à la connexion principale.
    static func test(host: String, port: Int) async -> TestResult {
        let trimmed = host.trimmingCharacters(in: .whitespaces)
        guard !trimmed.isEmpty, let url = URL(string: "ws://\(trimmed):\(port)/") else {
            return .failure("Adresse invalide")
        }
        return await ConnectionProbe().probe(url: url)
    }
}

// MARK: - Delegate WebSocket (callbacks sur une file de fond)

extension SnakeConnection: URLSessionWebSocketDelegate {
    nonisolated func urlSession(_ session: URLSession,
                                webSocketTask: URLSessionWebSocketTask,
                                didOpenWithProtocol protocol: String?) {
        let id = ObjectIdentifier(webSocketTask)
        Task { @MainActor in self.markConnected(id) }
    }

    nonisolated func urlSession(_ session: URLSession,
                                webSocketTask: URLSessionWebSocketTask,
                                didCloseWith closeCode: URLSessionWebSocketTask.CloseCode,
                                reason: Data?) {
        let id = ObjectIdentifier(webSocketTask)
        Task { @MainActor in self.handleClosedTask(id) }
    }

    nonisolated func urlSession(_ session: URLSession,
                                task: URLSessionTask,
                                didCompleteWithError error: Error?) {
        let id = ObjectIdentifier(task)
        Task { @MainActor in self.handleClosedTask(id) }
    }
}

// MARK: - Sonde jetable pour le test des réglages

/// Ouvre une socket temporaire et résout dès la poignée de main réussie
/// (`didOpen`) ou en cas d'erreur/timeout. N'affecte pas la connexion principale.
private nonisolated final class ConnectionProbe: NSObject, URLSessionWebSocketDelegate, @unchecked Sendable {
    private var continuation: CheckedContinuation<SnakeConnection.TestResult, Never>?
    private var session: URLSession?
    private var task: URLSessionWebSocketTask?

    func probe(url: URL) async -> SnakeConnection.TestResult {
        await withCheckedContinuation { continuation in
            self.continuation = continuation
            let config = URLSessionConfiguration.default
            config.timeoutIntervalForRequest = 5
            config.timeoutIntervalForResource = 5
            let session = URLSession(configuration: config, delegate: self, delegateQueue: nil)
            let task = session.webSocketTask(with: url)
            self.session = session
            self.task = task
            task.resume()
        }
    }

    private func finish(_ result: SnakeConnection.TestResult) {
        guard let continuation else { return }
        self.continuation = nil
        task?.cancel(with: .goingAway, reason: nil)
        session?.invalidateAndCancel()
        continuation.resume(returning: result)
    }

    func urlSession(_ session: URLSession,
                    webSocketTask: URLSessionWebSocketTask,
                    didOpenWithProtocol protocol: String?) {
        finish(.success)
    }

    func urlSession(_ session: URLSession,
                    task: URLSessionTask,
                    didCompleteWithError error: Error?) {
        finish(.failure(error?.localizedDescription ?? "Connexion impossible"))
    }
}
