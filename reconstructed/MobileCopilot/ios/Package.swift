// swift-tools-version:5.9
import PackageDescription

let package = Package(
    name: "MobileCopilot",
    platforms: [
        .iOS(.v15)
    ],
    products: [
        .library(
            name: "MobileCopilot",
            targets: ["MobileCopilot"])
    ],
    targets: [
        .target(
            name: "MobileCopilot",
            path: "MobileCopilot",
            resources: [
                // Add CoreML / model resources placed under MobileCopilot/Models
                .process("Models")
            ])
    ]
)
