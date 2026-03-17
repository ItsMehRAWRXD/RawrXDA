Param(
    [string]$Host = '127.0.0.1',
    [int]$Port = 11434,
    [int]$Iterations = 100,
    [string]$Mode = 'net' # or 'bridge'
)

python tools\fuzz_generator.py
python -m tests.fuzz_ollama --mode $Mode --host $Host --port $Port --iterations $Iterations
