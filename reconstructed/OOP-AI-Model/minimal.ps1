class IRequestProcessor {
    [void] Test() {}
}

class BaseRequest {
}

class BaseResponse {
}

class UnifiedAgentProcessor : IRequestProcessor {
    [object] ProcessRequest([object] $request) {
        $typedRequest = $request -as [BaseRequest]
        $response = [BaseResponse]::new()
        return $response
    }
}
