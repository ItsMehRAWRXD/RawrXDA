# ==============================================================================
# IRequestProcessor Interface
# Object-Oriented AI Model Core Interface
# ==============================================================================

class IRequestProcessor {
    # Interface for processing different types of AI requests
    
    [object] ProcessRequest([object] $request) {
        throw [System.NotImplementedException]::new("ProcessRequest must be implemented by derived classes")
    }
    
    [bool] CanHandle([string] $requestType) {
        throw [System.NotImplementedException]::new("CanHandle must be implemented by derived classes")
    }
    
    [string] GetProcessorName() {
        throw [System.NotImplementedException]::new("GetProcessorName must be implemented by derived classes")
    }
    
    [hashtable] GetCapabilities() {
        throw [System.NotImplementedException]::new("GetCapabilities must be implemented by derived classes")
    }
}