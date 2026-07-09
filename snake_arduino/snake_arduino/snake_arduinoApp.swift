//
//  snake_arduinoApp.swift
//  snake_arduino
//
//  Created by Lilian Hammache on 08/07/2026.
//

import SwiftUI

@main
struct snake_arduinoApp: App {
    @StateObject private var connection = SnakeConnection()

    var body: some Scene {
        WindowGroup {
            ControllerView()
                .environmentObject(connection)
        }
    }
}
