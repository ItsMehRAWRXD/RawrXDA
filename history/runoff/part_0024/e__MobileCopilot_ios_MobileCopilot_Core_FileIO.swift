import Foundation
import os.log

public final class NativeFileManager {
    public static let shared = NativeFileManager()
    private let logger = Logger(subsystem: "com.rawr.mobilecopilot", category: "FileIO")
    private let queue = DispatchQueue(label: "com.rawr.mobilecopilot.fileio", qos: .userInitiated)
    
    private init() {}
    
    public func readFile(at url: URL) throws -> Data {
        guard url.isFileURL else { throw FileIOError.invalidURL }
        return try Data(contentsOf: url)
    }
    
    public func writeFile(data: Data, to url: URL, options: Data.WritingOptions = [.atomic]) throws {
        guard url.isFileURL else { throw FileIOError.invalidURL }
        try data.write(to: url, options: options)
    }
    
    public func streamRead(url: URL, chunkSize: Int = 8192, handler: @escaping (Data, Int64) -> Void) throws {
        guard url.isFileURL else { throw FileIOError.invalidURL }
        let handle = try FileHandle(forReadingFrom: url)
        defer { try? handle.close() }
        
        var offset: UInt64 = 0
        while true {
            let chunk = try handle.read(upToCount: chunkSize) ?? Data()
            if chunk.isEmpty { break }
            handler(chunk, Int64(offset))
            offset += UInt64(chunk.count)
        }
    }
    
    public func mapFile(url: URL, readOnly: Bool = true) throws -> MappedFile {
        return try MappedFile(url: url, readOnly: readOnly)
    }
}

public enum FileIOError: Error {
    case invalidURL
    case mappingFailed
}

public final class MappedFile {
    public let url: URL
    public let length: Int
    private let descriptor: Int32
    private let pointer: UnsafeMutableRawPointer
    private let readOnly: Bool
    
    public init(url: URL, readOnly: Bool) throws {
        guard url.isFileURL else { throw FileIOError.invalidURL }
        self.url = url
        self.readOnly = readOnly
        
        let path = url.path
        descriptor = open(path, readOnly ? O_RDONLY : O_RDWR)
        if descriptor == -1 { throw FileIOError.mappingFailed }
        
        var statInfo = stat()
        guard fstat(descriptor, &statInfo) != -1 else {
            close(descriptor)
            throw FileIOError.mappingFailed
        }
        length = Int(statInfo.st_size)
        
        let prot: Int32 = readOnly ? PROT_READ : (PROT_READ | PROT_WRITE)
        let flags: Int32 = readOnly ? MAP_PRIVATE : MAP_SHARED
        guard let addr = mmap(nil, length, prot, flags, descriptor, 0), addr != MAP_FAILED else {
            close(descriptor)
            throw FileIOError.mappingFailed
        }
        pointer = addr!
    }
    
    public func readBytes(offset: Int, length: Int) -> Data {
        let bytes = pointer.advanced(by: offset).assumingMemoryBound(to: UInt8.self)
        return Data(bytes: bytes, count: length)
    }
    
    public func writeBytes(offset: Int, data: Data) {
        precondition(!readOnly, "Cannot write to read-only mapping")
        data.copyBytes(to: pointer.advanced(by: offset).assumingMemoryBound(to: UInt8.self), count: data.count)
        msync(pointer, data.count, MS_SYNC)
    }
    
    deinit {
        munmap(pointer, length)
        close(descriptor)
    }
}
