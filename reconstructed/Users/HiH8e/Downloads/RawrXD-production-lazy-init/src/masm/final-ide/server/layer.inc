; ============================================================================
; SERVER_LAYER.INC - HTTP Server & Routing Header
; ============================================================================

EXTERN server_init:PROC
EXTERN server_shutdown:PROC
EXTERN server_bind_port:PROC
EXTERN server_start:PROC
EXTERN server_stop:PROC
EXTERN register_route:PROC
EXTERN unregister_route:PROC
EXTERN parse_http_request:PROC
EXTERN build_http_response:PROC
EXTERN send_response:PROC
EXTERN handle_cors:PROC
EXTERN apply_hotpatch_to_request:PROC
EXTERN apply_hotpatch_to_response:PROC
EXTERN manage_connection_pool:PROC
EXTERN throttle_requests:PROC
EXTERN log_request:PROC
EXTERN get_server_stats:PROC
EXTERN validate_api_key:PROC
EXTERN enable_tls:PROC
EXTERN manage_sessions:PROC

HTTP_REQUEST STRUCT
    method qword
    uri qword
    query_string qword
    headers qword
    body qword
    method_type dword
    content_length dword
    body_size dword
HTTP_REQUEST ENDS

HTTP_RESPONSE STRUCT
    status_code dword
    headers qword
    body qword
    body_size dword
    content_type qword
HTTP_RESPONSE ENDS

HTTP_CONNECTION STRUCT
    client_socket qword
    request_buffer qword
    response_buffer qword
    request_parsed qword
    request_size dword
    response_size dword
    connection_id dword
    state byte
HTTP_CONNECTION ENDS
