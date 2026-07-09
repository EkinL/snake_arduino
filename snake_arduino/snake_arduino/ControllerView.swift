//
//  ControllerView.swift
//  snake_arduino
//
//  Écran manette : barre de statut, accès aux réglages et D-pad directionnel.
//

import SwiftUI
import UIKit

struct ControllerView: View {
    @EnvironmentObject private var connection: SnakeConnection
    @AppStorage("esp32Host") private var host = ""
    @AppStorage("esp32Port") private var port = 81
    @State private var showSettings = false

    var body: some View {
        NavigationStack {
            VStack(spacing: 32) {
                statusBar
                Spacer()
                DPad(isEnabled: connection.status.isConnected) { direction in
                    connection.send(direction)
                }
                Spacer()
            }
            .padding()
            .navigationTitle("Snake")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Button {
                        showSettings = true
                    } label: {
                        Image(systemName: "gearshape.fill")
                    }
                    .accessibilityLabel("Réglages")
                }
            }
            .sheet(isPresented: $showSettings) {
                SettingsView()
            }
            .onAppear(perform: connectIfPossible)
        }
    }

    private func connectIfPossible() {
        guard !host.isEmpty else {
            connection.stop()
            return
        }
        connection.start(host: host, port: port)
    }

    // MARK: - Barre de statut

    private var statusBar: some View {
        HStack(spacing: 10) {
            Circle()
                .fill(statusColor)
                .frame(width: 12, height: 12)
            Text(statusText)
                .font(.subheadline)
                .foregroundStyle(.secondary)
            Spacer()
        }
    }

    private var statusColor: Color {
        switch connection.status {
        case .connected:  return .green
        case .connecting: return .orange
        case .disconnected, .failed: return .red
        }
    }

    private var statusText: String {
        switch connection.status {
        case .connected:      return "Connecté à \(host)"
        case .connecting:     return "Connexion…"
        case .failed(let m):  return m
        case .disconnected:
            return host.isEmpty ? "Configurez l'IP avec ⚙️" : "Déconnecté"
        }
    }
}

// MARK: - D-pad

private struct DPad: View {
    let isEnabled: Bool
    let onPress: (Direction) -> Void

    private let buttonSize: CGFloat = 84

    var body: some View {
        Grid(horizontalSpacing: 16, verticalSpacing: 16) {
            GridRow {
                spacer
                button(.up, "chevron.up")
                spacer
            }
            GridRow {
                button(.left, "chevron.left")
                spacer
                button(.right, "chevron.right")
            }
            GridRow {
                spacer
                button(.down, "chevron.down")
                spacer
            }
        }
        .disabled(!isEnabled)
        .opacity(isEnabled ? 1 : 0.4)
    }

    private var spacer: some View {
        Color.clear.frame(width: buttonSize, height: buttonSize)
    }

    private func button(_ direction: Direction, _ symbol: String) -> some View {
        Button {
            UIImpactFeedbackGenerator(style: .medium).impactOccurred()
            onPress(direction)
        } label: {
            Image(systemName: symbol)
                .font(.system(size: 36, weight: .bold))
                .frame(width: buttonSize, height: buttonSize)
                .background(Color.accentColor, in: RoundedRectangle(cornerRadius: 20))
                .foregroundStyle(.white)
        }
        .buttonStyle(.plain)
        .accessibilityLabel(label(for: direction))
    }

    private func label(for direction: Direction) -> String {
        switch direction {
        case .up:    return "Haut"
        case .down:  return "Bas"
        case .left:  return "Gauche"
        case .right: return "Droite"
        }
    }
}

#Preview {
    ControllerView()
        .environmentObject(SnakeConnection())
}
