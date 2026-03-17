import SwiftUI
import Combine

public struct MobileCopilotView: View {
    @State private var inputText: String = ""
    @State private var messages: [ChatMessage] = [
        ChatMessage(id: UUID(), content: "Model initializing...", sender: .system, timestamp: Date())
    ]
    @State private var isRecording = false
    @State private var latencyMs: Int = 0
    @State private var tokensPerSec: Double = 0.0
    @StateObject private var speech = SpeechEngine()
    private let haptic = UIImpactFeedbackGenerator(style: .light)
    
    public init() {}
    
    public var body: some View {
        VStack(spacing: 0) {
            header
            messageList
            inputBar
        }
        .background(Color(.systemBackground))
        .edgesIgnoringSafeArea(.bottom)
        .task {
            // Attempt automatic model load
            InferenceEngine.shared.autoLoadBundledModel()
        }
    }
    
    private var header: some View {
        HStack {
            VStack(alignment: .leading) {
                Text("Mobile Copilot")
                    .font(.title2).bold()
                Text("Latency: \(latencyMs) ms  Tokens/sec: \(String(format: "%.2f", tokensPerSec))")
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
            Spacer()
            Circle()
                .fill(Color.green)
                .frame(width: 10, height: 10)
        }
        .padding()
    }
    
    private var messageList: some View {
        ScrollViewReader { proxy in
            ScrollView {
                LazyVStack(spacing: 12) {
                    ForEach(messages) { message in
                        MessageBubble(message: message)
                            .id(message.id)
                            .transition(.move(edge: .bottom).combined(with: .opacity))
                    }
                }
                .padding(.horizontal)
                .padding(.vertical, 8)
            }
            .onChange(of: messages.count) { _ in
                if let last = messages.last {
                    withAnimation {
                        proxy.scrollTo(last.id, anchor: .bottom)
                    }
                }
            }
        }
    }
    
    private var inputBar: some View {
        HStack(alignment: .bottom, spacing: 12) {
            Button(action: toggleRecording) {
                Image(systemName: isRecording ? "stop.circle.fill" : "mic.fill")
                    .font(.title2)
            }
            .padding(8)
            .background(Color(.secondarySystemBackground))
            .clipShape(Circle())
            
            TextField("Ask anything...", text: $inputText, axis: .vertical)
                .lineLimit(1...4)
                .padding(12)
                .background(Color(.secondarySystemBackground))
                .clipShape(RoundedRectangle(cornerRadius: 16))
            
            Button(action: sendMessage) {
                Image(systemName: "paperplane.fill")
                    .font(.title2)
            }
            .padding(8)
            .background(Color.accentColor)
            .foregroundColor(.white)
            .clipShape(Circle())
            .disabled(inputText.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
        }
        .padding()
    }
    
    private func sendMessage() {
        guard !inputText.isEmpty else { return }
        let text = inputText
        let newMessage = ChatMessage(id: UUID(), content: text, sender: .user, timestamp: Date())
        messages.append(newMessage)
        inputText = ""
        
        let start = Date().timeIntervalSince1970
        latencyMs = 0; tokensPerSec = 0
        // Real inference pipeline with metrics
        Task {
            do {
                let tokenizer = TokenizerLoader.load()
                let tokens = tokenizer.encode(text)
                
                // Create MLFeatureProvider for model input
                let inputFeatures = try createFeatureProvider(tokens: tokens)
                
                // Run inference
                let output = try InferenceEngine.shared.predict(key: "copilot-model", input: inputFeatures)
                
                // Decode output tokens to text
                let responseText = try decodeOutput(output)
                
                // Stream response chunks
                var emitted = 0
                for chunk in responseText.split(separator: " ") {
                    if emitted == 0 { // first token -> latency
                        let firstLatency = (Date().timeIntervalSince1970 - start) * 1000.0
                        latencyMs = Int(firstLatency)
                    }
                    emitted += 1
                    await MainActor.run {
                        messages.append(ChatMessage(
                            id: UUID(),
                            content: String(chunk),
                            sender: .ai,
                            timestamp: Date()
                        ))
                        let elapsed = Date().timeIntervalSince1970 - start
                        if elapsed > 0 { tokensPerSec = Double(emitted) / elapsed }
                    }
                    try await Task.sleep(nanoseconds: 50_000_000) // 50ms delay for streaming effect
                }
            } catch {
                await MainActor.run {
                    messages.append(ChatMessage(
                        id: UUID(),
                        content: "Error: \(error.localizedDescription)",
                        sender: .ai,
                        timestamp: Date()
                    ))
                }
            }
        }
    }
    
    // Tokenization helper
    // Removed old simple tokenizer; using BPETokenizer instead.
    
    // Feature provider creation
    private func createFeatureProvider(tokens: [Int]) throws -> MLFeatureProvider {
        // TODO: Create proper MLFeatureProvider based on model input requirements
        // This is a placeholder that needs model-specific implementation
        throw NSError(domain: "Tokenization", code: 1, userInfo: [NSLocalizedDescriptionKey: "Feature provider creation not yet implemented"])
    }
    
    // Output decoder
    private func decodeOutput(_ output: MLFeatureProvider) throws -> String {
        // TODO: Decode model output tokens to text
        return "Model output decoding not yet implemented"
    }

    private func toggleRecording() {
        haptic.impactOccurred()
        if isRecording {
            speech.stop(); isRecording = false
            messages.append(ChatMessage(id: UUID(), content: "[voice stopped]", sender: .system, timestamp: Date()))
        } else {
            do { try speech.start(); isRecording = true
                messages.append(ChatMessage(id: UUID(), content: "[voice listening]", sender: .system, timestamp: Date()))
            } catch {
                messages.append(ChatMessage(id: UUID(), content: "Voice error: \(error.localizedDescription)", sender: .system, timestamp: Date()))
            }
        }
    }

    // Observe speech transcript and trigger incremental inference (placeholder)
    private func observeTranscript() {
        // In production, use Combine .onReceive; simplified below.
    }

    // Auto-send when final transcript arrives
    private func bindSpeech() {
        // Use Combine publisher to observe final transcript
        _ = speech.$finalTranscript
            .removeDuplicates()
            .sink { value in
                if isRecording && !value.isEmpty {
                    inputText = value
                    sendMessage()
                }
            }
    }
}

struct MessageBubble: View {
    let message: ChatMessage
    
    var body: some View {
        HStack(alignment: .bottom) {
            if message.sender == .ai { Spacer() }
            Text(message.content)
                .padding(12)
                .background(message.sender == .user ? Color.accentColor : Color(.secondarySystemBackground))
                .foregroundColor(message.sender == .user ? .white : .primary)
                .clipShape(RoundedRectangle(cornerRadius: 16))
            if message.sender == .user { Spacer() }
        }
    }
}

public struct ChatMessage: Identifiable {
    public enum Sender { case user, ai }
    public let id: UUID
    public let content: String
    public let sender: Sender
    public let timestamp: Date
}
