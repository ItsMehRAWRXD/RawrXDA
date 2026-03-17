# Check if you have the required dependencies
pip install -r requirements.txt

# Verify the model paths are correct
Get-ChildItem D:\ -Recurse -Filter *.gguf | Where-Object { $_.Length -gt 4GB -and $_.Length -lt 5.5GB }

# Run with proper error handling
python beast-training-suite.py --model-path "D:\path\to\your\large-model.gguf" --output "D:\output\path"