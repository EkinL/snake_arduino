//
//  SettingsView.swift
//  snake_arduino
//
//  Réglages : adresse IP + port de l'ESP32, test de connexion, enregistrement.
//

import SwiftUI

struct SettingsView: View {
    @EnvironmentObject private var connection: SnakeConnection
    @Environment(\.dismiss) private var dismiss

    @AppStorage("esp32Host") private var host = ""
    @AppStorage("esp32Port") private var port = 81

    @State private var draftHost = ""
    @State private var draftPort = "81"
    @State private var testState: TestState = .idle

    private enum TestState: Equatable {
        case idle, testing, success
        case failure(String)
    }

    var body: some View {
        NavigationStack {
            Form {
                Section("ESP32") {
                    TextField("Adresse IP (ex. 192.168.1.42)", text: $draftHost)
                        .keyboardType(.numbersAndPunctuation)
                        .textInputAutocapitalization(.never)
                        .autocorrectionDisabled()
                        .onChange(of: draftHost) { testState = .idle }
                    TextField("Port", text: $draftPort)
                        .keyboardType(.numberPad)
                        .onChange(of: draftPort) { testState = .idle }
                }

                Section {
                    Button(action: runTest) {
                        HStack {
                            Text("Tester la connexion")
                            Spacer()
                            testIndicator
                        }
                    }
                    .disabled(draftHost.isEmpty || testState == .testing)

                    switch testState {
                    case .success:
                        Text("Connexion réussie ✅")
                            .font(.caption)
                            .foregroundStyle(.green)
                    case .failure(let message):
                        Text("Échec : \(message)")
                            .font(.caption)
                            .foregroundStyle(.red)
                    case .idle, .testing:
                        EmptyView()
                    }
                }
            }
            .navigationTitle("Réglages")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Annuler") { dismiss() }
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Enregistrer", action: save)
                        .disabled(draftHost.isEmpty)
                }
            }
            .onAppear {
                draftHost = host
                draftPort = String(port)
            }
        }
    }

    @ViewBuilder
    private var testIndicator: some View {
        switch testState {
        case .idle:
            EmptyView()
        case .testing:
            ProgressView()
        case .success:
            Image(systemName: "checkmark.circle.fill").foregroundStyle(.green)
        case .failure:
            Image(systemName: "xmark.circle.fill").foregroundStyle(.red)
        }
    }

    private func runTest() {
        let targetHost = draftHost
        let targetPort = Int(draftPort) ?? 81
        testState = .testing
        Task {
            let result = await SnakeConnection.test(host: targetHost, port: targetPort)
            switch result {
            case .success:
                testState = .success
            case .failure(let message):
                testState = .failure(message)
            }
        }
    }

    private func save() {
        let cleanHost = draftHost.trimmingCharacters(in: .whitespaces)
        let cleanPort = Int(draftPort) ?? 81
        host = cleanHost
        port = cleanPort
        connection.start(host: cleanHost, port: cleanPort)
        dismiss()
    }
}

#Preview {
    SettingsView()
        .environmentObject(SnakeConnection())
}
