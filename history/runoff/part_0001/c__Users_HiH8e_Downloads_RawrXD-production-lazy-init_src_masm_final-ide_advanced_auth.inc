; Advanced Auth Header File
; Phase D Component 4: OAuth2, JWT, RBAC authentication

; Authentication error codes
AUTH_SUCCESS equ 0
AUTH_ERROR_INVALID equ 1
AUTH_ERROR_OOM equ 2
AUTH_ERROR_NOT_FOUND equ 3
AUTH_ERROR_EXISTS equ 4
AUTH_ERROR_FULL equ 5
AUTH_ERROR_INVALID_CREDENTIALS equ 6
AUTH_ERROR_EXPIRED equ 7
AUTH_ERROR_INSUFFICIENT_PERMISSIONS equ 8

; Authentication system structure
AUTH_SYSTEM struct
    auth_mutex dword ?
    user_count dword ?
    max_users dword ?
    token_count dword ?
    max_tokens dword ?
    users_ptr dword ?
    tokens_ptr dword ?
    secret_key_ptr dword ?
    secret_key_len dword ?
    oauth_providers_ptr dword ?
    allocator_ptr dword ?
    deallocator_ptr dword ?
    stats_enabled dword ?
    reserved dword 12 dup(?)
AUTH_SYSTEM ends

; User structure
USER struct
    user_id dword ?
    username_ptr dword ?
    username_len dword ?
    password_hash_ptr dword ?
    password_hash_len dword ?
    roles_ptr dword ?
    roles_count dword ?
    created_at dword ?
    last_login dword ?
    flags dword ?
    reserved dword 4 dup(?)
USER ends

; Token structure
TOKEN struct
    token_id dword ?
    user_id dword ?
    token_ptr dword ?
    token_len dword ?
    token_type dword ?
    expires_at dword ?
    scopes_ptr dword ?
    scopes_count dword ?
    created_at dword ?
    flags dword ?
    reserved dword 2 dup(?)
TOKEN ends

; OAuth provider structure
OAUTH_PROVIDER struct
    provider_id dword ?
    name_ptr dword ?
    name_len dword ?
    client_id_ptr dword ?
    client_id_len dword ?
    client_secret_ptr dword ?
    client_secret_len dword ?
    auth_url_ptr dword ?
    token_url_ptr dword ?
    reserved dword 4 dup(?)
OAUTH_PROVIDER ends

; Constants
USER_MAX_USERNAME_LEN equ 256
USER_MAX_PASSWORD_LEN equ 256
TOKEN_MAX_LEN equ 4096
OAUTH_MAX_NAME_LEN equ 256
OAUTH_MAX_CLIENT_ID_LEN equ 256
OAUTH_MAX_CLIENT_SECRET_LEN equ 256
OAUTH_MAX_URL_LEN equ 1024

; Function declarations
EXTERN auth_init:proc
EXTERN auth_shutdown:proc
EXTERN auth_authenticate:proc
EXTERN auth_authorize:proc