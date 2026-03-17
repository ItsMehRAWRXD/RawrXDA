import XCTest
@testable import MobileCopilot

final class MobileCopilotTests: XCTestCase {
    func testSandboxWriteRead() throws {
        let data = Data((0..<1024).map { UInt8($0 % 256) })
        try SecureSandbox.shared.writeSecure(data: data, fileName: "test.bin")
        let read = try SecureSandbox.shared.readSecure(fileName: "test.bin")
        XCTAssertEqual(data, read)
    }

    func testStreamingClient() throws {
        let exp = expectation(description: "stream")
        var received = false
        let cancellable = StreamingClient.shared.stream(url: URL(string: "https://httpbin.org/stream/5")!)
            .sink(receiveCompletion: { _ in exp.fulfill() }, receiveValue: { _ in received = true })
        waitForExpectations(timeout: 10)
        cancellable.cancel()
        XCTAssertTrue(received)
    }
}
