import Foundation
import Combine
import os.log

public final class StreamingClient: NSObject, URLSessionDataDelegate {
    public static let shared = StreamingClient()
    private let logger = Logger(subsystem: "com.rawr.mobilecopilot", category: "Streaming")
    private lazy var session: URLSession = {
        let configuration = URLSessionConfiguration.default
        configuration.waitsForConnectivity = true
        configuration.httpMaximumConnectionsPerHost = 4
        return URLSession(configuration: configuration, delegate: self, delegateQueue: nil)
    }()
    
    private var subjects: [Int: PassthroughSubject<Data, Error>] = [:]
    private let lock = NSLock()
    
    public func stream(url: URL, method: String = "GET", body: Data? = nil) -> AnyPublisher<Data, Error> {
        var request = URLRequest(url: url)
        request.httpMethod = method
        request.httpBody = body
        request.setValue("keep-alive", forHTTPHeaderField: "Connection")
        request.setValue("chunked", forHTTPHeaderField: "Transfer-Encoding")
        
        let subject = PassthroughSubject<Data, Error>()
        let task = session.dataTask(with: request)
        lock.lock()
        subjects[task.taskIdentifier] = subject
        lock.unlock()
        task.resume()
        
        return subject.handleEvents(receiveCancel: { [weak self] in
            task.cancel()
            self?.removeSubject(for: task.taskIdentifier)
        }).eraseToAnyPublisher()
    }
    
    private func removeSubject(for identifier: Int) {
        lock.lock(); defer { lock.unlock() }
        subjects.removeValue(forKey: identifier)
    }
    
    // MARK: URLSessionDataDelegate
    public func urlSession(_ session: URLSession, dataTask: URLSessionDataTask, didReceive data: Data) {
        lock.lock(); let subject = subjects[dataTask.taskIdentifier]; lock.unlock()
        subject?.send(data)
    }
    
    public func urlSession(_ session: URLSession, task: URLSessionTask, didCompleteWithError error: Error?) {
        lock.lock(); let subject = subjects[task.taskIdentifier]; subjects[task.taskIdentifier] = nil; lock.unlock()
        if let error = error {
            subject?.send(completion: .failure(error))
        } else {
            subject?.send(completion: .finished)
        }
    }
}

public final class StreamSocket {
    private var inputStream: InputStream?
    private var outputStream: OutputStream?
    private let bufferSize = 8192
    
    public func connect(host: String, port: Int) throws {
        var readStream: Unmanaged<CFReadStream>?
        var writeStream: Unmanaged<CFWriteStream>?
        CFStreamCreatePairWithSocketToHost(nil, host as CFString, UInt32(port), &readStream, &writeStream)
        guard let input = readStream?.takeRetainedValue(), let output = writeStream?.takeRetainedValue() else {
            throw StreamError.connectionFailed
        }
        inputStream = input
        outputStream = output
        inputStream?.open()
        outputStream?.open()
    }
    
    public func readChunk() throws -> Data? {
        guard let inputStream = inputStream else { return nil }
        var buffer = [UInt8](repeating: 0, count: bufferSize)
        let bytesRead = inputStream.read(&buffer, maxLength: bufferSize)
        if bytesRead < 0 { throw inputStream.streamError ?? StreamError.readFailed }
        if bytesRead == 0 { return nil }
        return Data(buffer.prefix(bytesRead))
    }
    
    public func write(data: Data) throws {
        guard let outputStream = outputStream else { throw StreamError.writeFailed }
        let result = data.withUnsafeBytes { ptr -> Int in
            guard let base = ptr.baseAddress else { return -1 }
            return outputStream.write(base.assumingMemoryBound(to: UInt8.self), maxLength: data.count)
        }
        if result < 0 { throw outputStream.streamError ?? StreamError.writeFailed }
    }
    
    public func close() {
        inputStream?.close()
        outputStream?.close()
    }
}

public enum StreamError: Error {
    case connectionFailed
    case readFailed
    case writeFailed
}
