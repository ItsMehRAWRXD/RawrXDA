# Simple prompt test to debug tokenization
Set-Location "D:\temp\RawrXD-agentic-ide-production\RawrXD-ModelLoader\build\bin\Release"

$input = "Hello, how are you?`nquit"
$input | .\test_chat_streaming.exe 2>&1 | Select-String -Pattern "tokenize|generate|AI:"
