param(
    [int] $Port = 8765,
    [string] $Prefix = "http://127.0.0.1:",
    [switch] $VerboseLogging
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'AgentTools.psm1') -Force

$listener = [System.Net.HttpListener]::new()
$listener.Prefixes.Add("$Prefix$Port/")
$listener.Start()

Write-Host "AgentTools server listening at $Prefix$Port/"

function Write-JsonResponse {
    param(
        [System.Net.HttpListenerResponse] $Response,
        [hashtable] $Object,
        [int] $StatusCode = 200
    )
    $json = ($Object | ConvertTo-Json -Depth 8)
    $bytes = [Text.Encoding]::UTF8.GetBytes($json)
    $Response.StatusCode = $StatusCode
    $Response.ContentType = 'application/json'
    $Response.ContentEncoding = [Text.Encoding]::UTF8
    $Response.ContentLength64 = $bytes.Length
    $Response.Headers.Add('Access-Control-Allow-Origin', '*')
    $Response.OutputStream.Write($bytes, 0, $bytes.Length)
    $Response.Close()
}

function Read-RequestBodyJson {
    param([System.Net.HttpListenerRequest] $Request)
    $reader = New-Object IO.StreamReader($Request.InputStream, $Request.ContentEncoding)
    $body = $reader.ReadToEnd()
    if ($VerboseLogging) { Write-Host "Request body: $body" }
    if ([string]::IsNullOrWhiteSpace($body)) { return @{} }
    return ($body | ConvertFrom-Json)
}

try {
    while ($listener.IsListening) {
        $context = $listener.GetContext()
        $request = $context.Request
        $response = $context.Response
        try {
            if ($request.HttpMethod -eq 'OPTIONS') {
                Write-JsonResponse -Response $response -Object @{ ok=$true } -StatusCode 200
                continue
            }
            $path = $request.Url.AbsolutePath.Trim('/').ToLowerInvariant()
            $body = Read-RequestBodyJson -Request $request

            switch ($path) {
                'fs/read' {
                    $p = @{ Path = $body.path; Encoding = ($body.encoding ?? 'utf8'); MaxBytes = [int]($body.maxBytes ?? 200000) }
                    if ($null -ne $body.startLine -and $null -ne $body.endLine) {
                        $p['StartLine'] = [int]$body.startLine
                        $p['EndLine'] = [int]$body.endLine
                    }
                    $result = Get-FileContentSafe @p
                    Write-JsonResponse -Response $response -Object @{ ok=$true; data=$result }
                }
                'fs/write' {
                    $meta = Set-FileContentSafe -Path $body.path -Content $body.content -Encoding ($body.encoding ?? 'utf8')
                    Write-JsonResponse -Response $response -Object @{ ok=$true; data=$meta }
                }
                'fs/append' {
                    $meta = Add-FileContentSafe -Path $body.path -Content $body.content -Encoding ($body.encoding ?? 'utf8')
                    Write-JsonResponse -Response $response -Object @{ ok=$true; data=$meta }
                }
                'fs/list' {
                    $list = Get-DirectoryListingSafe -Path $body.path -Recurse:([bool]$body.recurse)
                    Write-JsonResponse -Response $response -Object @{ ok=$true; data=$list }
                }
                'fs/delete' {
                    $ok = Remove-PathSafe -Path $body.path -Recurse:([bool]$body.recurse)
                    Write-JsonResponse -Response $response -Object @{ ok=$true; data=$ok }
                }
                'fs/copy' {
                    $meta = Copy-PathSafe -Source $body.source -Destination $body.destination -Recurse:([bool]$body.recurse)
                    Write-JsonResponse -Response $response -Object @{ ok=$true; data=$meta }
                }
                'fs/move' {
                    $meta = Move-PathSafe -Source $body.source -Destination $body.destination
                    Write-JsonResponse -Response $response -Object @{ ok=$true; data=$meta }
                }
                'grep/search' {
                    $results = Search-TextSafe -Path $body.path -Pattern $body.pattern -IsRegex:([bool]$body.isRegex) -Include $body.include -MaxResults ([int]($body.maxResults ?? 200))
                    Write-JsonResponse -Response $response -Object @{ ok=$true; data=$results }
                }
                'cmd/run' {
                    $res = Invoke-CommandSafe -Command $body.command -Arguments $body.arguments -Cwd $body.cwd -TimeoutSec ([int]($body.timeoutSec ?? 0)) -Background:([bool]$body.background)
                    Write-JsonResponse -Response $response -Object @{ ok=$true; data=$res }
                }
                default {
                    Write-JsonResponse -Response $response -Object @{ ok=$false; error="Unknown route: $path" } -StatusCode 404
                }
            }
        } catch {
            $err = $_.Exception.Message
            if ($VerboseLogging) { Write-Warning $err }
            Write-JsonResponse -Response $response -Object @{ ok=$false; error=$err } -StatusCode 500
        }
    }
} finally {
    $listener.Stop()
    $listener.Close()
}
