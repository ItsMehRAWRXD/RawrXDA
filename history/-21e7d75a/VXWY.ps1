# Network stress test
$port = 3000
$bytes = 0
$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

# Create dummy TCP server
$listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, $port)
$listener.Start()

# Connect through your forwarded port
$client = [System.Net.Sockets.TcpClient]::new("localhost", $port)
$stream = $client.GetStream()
$payload = New-Object byte[] (65536)

while ($stopwatch.ElapsedMilliseconds -lt 10000) {
    $stream.Write($payload, 0, $payload.Length)
    $bytes += $payload.Length
}

$mbps = ($bytes * 8 / 1MB) / ($stopwatch.ElapsedMilliseconds / 1000)
Write-Host "Throughput: $([math]::Round($mbps, 2)) Mbps"

# Expected: >9,000 Mbps (10GbE saturation) vs ~600 Mbps C++ baseline