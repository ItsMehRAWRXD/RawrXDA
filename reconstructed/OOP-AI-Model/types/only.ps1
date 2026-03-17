using namespace System

class IRequestProcessor {
    [object] ProcessRequest([object] $request) {
        throw [NotImplementedException]::new("ProcessRequest must be implemented in derived class")
    }

    [bool] CanHandle([string] $requestType) {
        throw [NotImplementedException]::new("CanHandle must be implemented in derived class")
    }

    [string] GetProcessorName() {
        throw [NotImplementedException]::new("GetProcessorName must be implemented in derived class")
    }

    [hashtable] GetCapabilities() {
        throw [NotImplementedException]::new("GetCapabilities must be implemented in derived class")
    }
}
