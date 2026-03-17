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
    method QWORD ?
    uri QWORD ?
    query_string QWORD ?
    headers QWORD ?
    body QWORD ?
    method_type DWORD ?
    content_length DWORD ?
    body_size DWORD ?
HTTP_REQUEST ENDS

HTTP_RESPONSE STRUCT
    status_code DWORD ?
    headers QWORD ?
    body QWORD ?
    body_size DWORD ?
    content_type QWORD ?
HTTP_RESPONSE ENDS

HTTP_CONNECTION STRUCT
    client_socket QWORD ?
    request_buffer QWORD ?
    response_buffer QWORD ?
    request_parsed QWORD ?
    request_size DWORD ?
    response_size DWORD ?
    connection_id DWORD ?
    state BYTE ?
HTTP_CONNECTION ENDS
