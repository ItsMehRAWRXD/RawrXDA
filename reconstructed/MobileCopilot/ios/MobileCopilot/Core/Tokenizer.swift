import Foundation

public final class BPETokenizer {
    private let vocab: [String: Int]
    private let merges: [(String, String)]
    private var cache: [String: [Int]] = [:]

    public init(vocab: [String: Int], merges: [(String, String)]) {
        self.vocab = vocab
        self.merges = merges
    }

    public func encode(_ text: String, maxTokens: Int = 512) -> [Int] {
        if let cached = cache[text] { return Array(cached.prefix(maxTokens)) }
        var tokens: [Int] = []
        let words = text.components(separatedBy: .whitespacesAndNewlines).filter { !$0.isEmpty }
        for w in words {
            var current = w.map { String($0) }
            var merged = true
            while merged {
                merged = false
                var bestIndex: Int? = nil
                var bestPair: (String, String)? = nil
                for i in 0..<current.count-1 {
                    let pair = (current[i], current[i+1])
                    if let idx = merges.firstIndex(where: { $0.0 == pair.0 && $0.1 == pair.1 }) {
                        if bestIndex == nil || idx < bestIndex! { bestIndex = idx; bestPair = pair }
                    }
                }
                if let bp = bestPair {
                    var newList: [String] = []
                    var i = 0
                    while i < current.count {
                        if i < current.count-1 && current[i] == bp.0 && current[i+1] == bp.1 {
                            newList.append(bp.0 + bp.1)
                            i += 2
                        } else { newList.append(current[i]); i += 1 }
                    }
                    current = newList; merged = true
                }
            }
            for piece in current {
                tokens.append(vocab[piece] ?? vocab["<unk>"] ?? 0)
            }
        }
        let out = Array(tokens.prefix(maxTokens))
        cache[text] = out
        return out
    }
}

public enum TokenizerLoader {
    public static func load() -> BPETokenizer {
        guard let vocabUrl = Bundle.module.url(forResource: "vocab", withExtension: "txt"),
              let mergesUrl = Bundle.module.url(forResource: "merges", withExtension: "txt") else {
            return BPETokenizer(vocab: ["<unk>":0], merges: [])
        }
        var vocab: [String: Int] = [:]
        if let vData = try? String(contentsOf: vocabUrl) {
            vData.components(separatedBy: .newlines).forEach { line in
                let parts = line.split(separator: " ")
                if parts.count == 2, let id = Int(parts[1]) { vocab[String(parts[0])] = id }
            }
        }
        var merges: [(String,String)] = []
        if let mData = try? String(contentsOf: mergesUrl) {
            mData.components(separatedBy: .newlines).forEach { line in
                let parts = line.split(separator: " ")
                if parts.count == 2 { merges.append((String(parts[0]), String(parts[1]))) }
            }
        }
        if vocab["<unk>"] == nil { vocab["<unk>"] = 0 }
        return BPETokenizer(vocab: vocab, merges: merges)
    }
}