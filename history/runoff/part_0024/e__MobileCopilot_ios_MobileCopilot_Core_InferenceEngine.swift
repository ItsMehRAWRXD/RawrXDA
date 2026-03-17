import Foundation
import CoreML
import os.log

public final class InferenceEngine {
    public static let shared = InferenceEngine()
    private let logger = Logger(subsystem: "com.rawr.mobilecopilot", category: "Inference")
    private var models: [String: MLModel] = [:]
    private let queue = DispatchQueue(label: "com.rawr.mobilecopilot.inference", qos: .userInitiated)
    
    private init() {}
    
    public func loadModel(at url: URL, key: String) throws {
        let compiledUrl: URL
        if url.pathExtension == "mlmodelc" {
            compiledUrl = url
        } else {
            compiledUrl = try MLModel.compileModel(at: url)
        }
        let model = try MLModel(contentsOf: compiledUrl)
        models[key] = model
        logger.log("Loaded model %{public}@", key)
    }
    
    public func unloadModel(key: String) {
        models.removeValue(forKey: key)
        logger.log("Unloaded model %{public}@", key)
    }
    
    public func predict(key: String, input: MLFeatureProvider) throws -> MLFeatureProvider {
        guard let model = models[key] else {
            throw InferenceError.modelNotLoaded
        }
        return try model.prediction(from: input)
    }

    // Load first available .mlmodel from bundled Models directory if not already loaded
    public func autoLoadBundledModel(key: String = "copilot-model") {
        guard models[key] == nil else { return }
        let fm = FileManager.default
        // Models directory inside bundle
        guard let modelsUrl = Bundle.module.url(forResource: "Models", withExtension: nil) else { return }
        do {
            let contents = try fm.contentsOfDirectory(at: modelsUrl, includingPropertiesForKeys: nil)
            if let modelUrl = contents.first(where: { $0.pathExtension == "mlmodel" }) {
                try loadModel(at: modelUrl, key: key)
            }
        } catch {
            logger.error("AutoLoad failure: \(error.localizedDescription)")
        }
    }
}

public enum InferenceError: Error {
    case modelNotLoaded
}
