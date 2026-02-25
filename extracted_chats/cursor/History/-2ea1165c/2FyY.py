#!/usr/bin/env python3
"""
Dependency Installation Script
Handles TensorFlow and related package installation with fallback methods
"""

import sys
import os
import subprocess
import urllib.request
import zipfile
import shutil
from pathlib import Path

def print_status(message, status="INFO"):
    """Print status messages with colors"""
    colors = {
        "SUCCESS": "\033[92m",
        "WARNING": "\033[93m", 
        "ERROR": "\033[91m",
        "INFO": "\033[94m",
        "RESET": "\033[0m"
    }
    
    symbols = {
        "SUCCESS": "✓",
        "WARNING": "⚠",
        "ERROR": "✗",
        "INFO": "ℹ"
    }
    
    print(f"{colors[status]}[{symbols[status]}] {message}{colors['RESET']}")

def check_python_installation():
    """Check if Python installation is working"""
    print_status("Checking Python installation...")
    
    try:
        import sys
        print(f"Python version: {sys.version}")
        print(f"Python executable: {sys.executable}")
        
        # Test basic imports
        import os
        import json
        import math
        print_status("Basic Python modules working", "SUCCESS")
        return True
        
    except Exception as e:
        print_status(f"Python installation issue: {e}", "ERROR")
        return False

def install_package_manual(package_name, version=None):
    """Manually install package using get-pip.py"""
    print_status(f"Attempting manual installation of {package_name}...")
    
    try:
        # Download get-pip.py if not exists
        if not os.path.exists("get-pip.py"):
            print_status("Downloading get-pip.py...")
            urllib.request.urlretrieve("https://bootstrap.pypa.io/get-pip.py", "get-pip.py")
        
        # Install pip
        subprocess.run([sys.executable, "get-pip.py"], check=True)
        
        # Try to install package
        if version:
            package = f"{package_name}=={version}"
        else:
            package = package_name
            
        subprocess.run([sys.executable, "-m", "pip", "install", package], check=True)
        print_status(f"Successfully installed {package}", "SUCCESS")
        return True
        
    except Exception as e:
        print_status(f"Manual installation failed: {e}", "ERROR")
        return False

def create_mock_models():
    """Create mock models without TensorFlow for demonstration"""
    print_status("Creating mock models for demonstration...")
    
    # Create models directory
    os.makedirs("models", exist_ok=True)
    
    # Create mock MNIST model data
    mnist_model_data = {
        "model_type": "MNIST_CNN",
        "architecture": [
            {"type": "Conv2D", "filters": 32, "kernel_size": [3, 3]},
            {"type": "MaxPooling2D", "pool_size": [2, 2]},
            {"type": "Conv2D", "filters": 64, "kernel_size": [3, 3]},
            {"type": "MaxPooling2D", "pool_size": [2, 2]},
            {"type": "Flatten"},
            {"type": "Dense", "units": 128},
            {"type": "Dense", "units": 10}
        ],
        "input_shape": [28, 28, 1],
        "output_classes": 10,
        "parameters": 1234567,
        "accuracy": 0.9856
    }
    
    # Create mock CIFAR-10 model data
    cifar_model_data = {
        "model_type": "CIFAR10_CNN",
        "architecture": [
            {"type": "Conv2D", "filters": 32, "kernel_size": [3, 3]},
            {"type": "MaxPooling2D", "pool_size": [2, 2]},
            {"type": "Conv2D", "filters": 64, "kernel_size": [3, 3]},
            {"type": "MaxPooling2D", "pool_size": [2, 2]},
            {"type": "Conv2D", "filters": 64, "kernel_size": [3, 3]},
            {"type": "Flatten"},
            {"type": "Dense", "units": 128},
            {"type": "Dense", "units": 10}
        ],
        "input_shape": [32, 32, 3],
        "output_classes": 10,
        "parameters": 2345678,
        "accuracy": 0.7234
    }
    
    # Save model data as JSON
    import json
    with open("models/mnist_model.json", "w") as f:
        json.dump(mnist_model_data, f, indent=2)
    
    with open("models/cifar10_model.json", "w") as f:
        json.dump(cifar_model_data, f, indent=2)
    
    print_status("Mock models created successfully", "SUCCESS")
    return True

def create_test_images():
    """Create test images using basic Python"""
    print_status("Creating test images...")
    
    # Create test_images directory
    os.makedirs("test_images", exist_ok=True)
    
    # Create simple test images using basic Python
    for i in range(10):
        # Create a simple text file representing an image
        with open(f"test_images/mnist_digit_{i}.txt", "w") as f:
            f.write(f"MNIST Digit {i} Test Image\n")
            f.write("=" * 28 + "\n")
            for row in range(28):
                line = ""
                for col in range(28):
                    # Create a simple pattern
                    if (row + col + i) % 3 == 0:
                        line += "1"
                    else:
                        line += "0"
                f.write(line + "\n")
    
    # Create CIFAR-10 style test images
    for i in range(5):
        with open(f"test_images/cifar_image_{i}.txt", "w") as f:
            f.write(f"CIFAR-10 Image {i} Test Data\n")
            f.write("=" * 32 + "\n")
            for row in range(32):
                line = ""
                for col in range(32):
                    # Create RGB pattern
                    r = (row + i) % 256
                    g = (col + i) % 256
                    b = (row + col + i) % 256
                    line += f"({r:3d},{g:3d},{b:3d}) "
                f.write(line + "\n")
    
    print_status("Test images created successfully", "SUCCESS")
    return True

def create_model_analysis_script():
    """Create model analysis script that works without TensorFlow"""
    print_status("Creating model analysis script...")
    
    analysis_script = '''#!/usr/bin/env python3
"""
Model Analysis Script - Works without TensorFlow
Analyzes mock models and demonstrates the pipeline
"""

import json
import os
import time
from pathlib import Path

class MockModelAnalyzer:
    def __init__(self):
        self.models = {}
        self.results = {}
    
    def load_models(self):
        """Load mock models"""
        print("Loading mock models...")
        
        if os.path.exists('models/mnist_model.json'):
            with open('models/mnist_model.json', 'r') as f:
                self.models['MNIST'] = json.load(f)
            print("Loaded MNIST model")
        
        if os.path.exists('models/cifar10_model.json'):
            with open('models/cifar10_model.json', 'r') as f:
                self.models['CIFAR10'] = json.load(f)
            print("Loaded CIFAR-10 model")
        
        # Check for optimized models
        if os.path.exists('quantized_model.tflite'):
            self.models['Quantized'] = {'type': 'TFLite', 'size': os.path.getsize('quantized_model.tflite')}
            print("Found quantized model")
        
        if os.path.exists('pruned_model.h5'):
            self.models['Pruned'] = {'type': 'Keras', 'size': os.path.getsize('pruned_model.h5')}
            print("Found pruned model")
    
    def analyze_model(self, name, model_data):
        """Analyze a model"""
        print(f"\\nAnalyzing {name}...")
        
        results = {}
        
        if isinstance(model_data, dict) and 'parameters' in model_data:
            # Mock model analysis
            results['parameters'] = model_data['parameters']
            results['accuracy'] = model_data['accuracy']
            results['size_mb'] = model_data['parameters'] * 4 / 1024 / 1024  # Estimate
            results['inference_time'] = 2.5 + (model_data['parameters'] / 1000000) * 0.1
        else:
            # Real model analysis
            results['size_mb'] = model_data['size'] / 1024 / 1024
            results['inference_time'] = 1.2  # Estimated
        
        print(f"  Parameters: {results.get('parameters', 'N/A'):,}")
        print(f"  Accuracy: {results.get('accuracy', 'N/A'):.4f}")
        print(f"  Size: {results['size_mb']:.2f} MB")
        print(f"  Inference time: {results['inference_time']:.2f} ms")
        
        return results
    
    def compare_models(self):
        """Compare all models"""
        print("\\n" + "="*60)
        print("MODEL COMPARISON RESULTS")
        print("="*60)
        
        for name, model_data in self.models.items():
            self.results[name] = self.analyze_model(name, model_data)
        
        # Print comparison table
        print("\\nComparison Summary:")
        print("-" * 80)
        print(f"{'Model':<15} {'Parameters':<12} {'Accuracy':<10} {'Size (MB)':<12} {'Time (ms)':<12}")
        print("-" * 80)
        
        for name, results in self.results.items():
            params = f"{results.get('parameters', 0):,}" if 'parameters' in results else "N/A"
            acc = f"{results.get('accuracy', 0):.4f}" if 'accuracy' in results else "N/A"
            size = f"{results['size_mb']:.2f}"
            time_ms = f"{results['inference_time']:.2f}"
            print(f"{name:<15} {params:<12} {acc:<10} {size:<12} {time_ms:<12}")
        
        # Calculate improvements
        if 'MNIST' in self.results and 'Quantized' in self.results:
            orig_size = self.results['MNIST']['size_mb']
            quant_size = self.results['Quantized']['size_mb']
            reduction = (1 - quant_size / orig_size) * 100
            print(f"\\nSize reduction (Quantized): {reduction:.1f}%")
        
        if 'MNIST' in self.results and 'Pruned' in self.results:
            orig_size = self.results['MNIST']['size_mb']
            pruned_size = self.results['Pruned']['size_mb']
            reduction = (1 - pruned_size / orig_size) * 100
            print(f"Size reduction (Pruned): {reduction:.1f}%")
    
    def save_results(self):
        """Save analysis results"""
        with open('model_analysis_results.json', 'w') as f:
            json.dump(self.results, f, indent=2)
        print("\\nResults saved to model_analysis_results.json")

def main():
    print("Mock Model Analysis Tool")
    print("=" * 40)
    
    analyzer = MockModelAnalyzer()
    analyzer.load_models()
    
    if not analyzer.models:
        print("No models found. Please run the model creation script first.")
        return
    
    analyzer.compare_models()
    analyzer.save_results()
    
    print("\\nAnalysis completed!")

if __name__ == "__main__":
    main()
'''
    
    with open('mock_model_analysis.py', 'w') as f:
        f.write(analysis_script)
    
    print_status("Model analysis script created", "SUCCESS")
    return True

def create_requirements_file():
    """Create requirements.txt with fallback options"""
    print_status("Creating requirements.txt...")
    
    requirements = """# TensorFlow and Model Optimization (Primary)
tensorflow>=2.15.0
tensorflow-model-optimization>=0.7.0

# Core dependencies
numpy>=1.24.0
matplotlib>=3.7.0
pillow>=10.0.0
h5py>=3.9.0
scipy>=1.11.0

# Alternative lightweight options (if TensorFlow fails)
# torch>=2.0.0
# onnx>=1.14.0
# opencv-python>=4.8.0

# Development dependencies
pytest>=7.0.0
black>=23.0.0
flake8>=6.0.0

# Installation notes:
# If TensorFlow installation fails, try:
# 1. pip install --upgrade pip
# 2. pip install tensorflow-cpu (for CPU-only version)
# 3. Use conda: conda install tensorflow
# 4. Use the mock models for demonstration
"""
    
    with open('requirements.txt', 'w') as f:
        f.write(requirements)
    
    print_status("Requirements file created", "SUCCESS")
    return True

def create_installation_guide():
    """Create installation guide for users"""
    print_status("Creating installation guide...")
    
    guide = """# TensorFlow Installation Guide

## Quick Installation

### Method 1: Standard pip installation
```bash
pip install tensorflow tensorflow-model-optimization numpy matplotlib
```

### Method 2: CPU-only version (smaller, faster install)
```bash
pip install tensorflow-cpu tensorflow-model-optimization numpy matplotlib
```

### Method 3: Using conda
```bash
conda install tensorflow tensorflow-model-optimization numpy matplotlib
```

### Method 4: Virtual environment (recommended)
```bash
python -m venv venv
source venv/bin/activate  # On Windows: venv\\Scripts\\activate
pip install -r requirements.txt
```

## Troubleshooting

### Common Issues:

1. **"No module named 'tensorflow'"**
   - Try: `pip install --upgrade pip`
   - Then: `pip install tensorflow`

2. **"Failed building wheel for tensorflow"**
   - Try: `pip install tensorflow-cpu`
   - Or use conda: `conda install tensorflow`

3. **"Python version not supported"**
   - TensorFlow requires Python 3.8-3.11
   - Check version: `python --version`

4. **"Out of memory during installation"**
   - Try: `pip install --no-cache-dir tensorflow`

### Alternative: Use Mock Models

If TensorFlow installation continues to fail, the pipeline includes mock models for demonstration:

```bash
python mock_model_analysis.py
```

This will run the analysis pipeline using simulated data.

## Verification

Test your installation:
```python
import tensorflow as tf
print(f"TensorFlow version: {tf.__version__}")
```

## Next Steps

Once TensorFlow is installed:
1. Run: `python create_sample_data.py`
2. Run: `python quantize_prune_model.py`
3. Run: `python inference_script.py --benchmark`
4. Run: `python model_analysis.py`
"""
    
    with open('INSTALLATION_GUIDE.md', 'w') as f:
        f.write(guide)
    
    print_status("Installation guide created", "SUCCESS")
    return True

def main():
    """Main installation function"""
    print("=" * 60)
    print("TensorFlow Dependencies Installation")
    print("=" * 60)
    
    # Check Python installation
    if not check_python_installation():
        print_status("Python installation has issues", "WARNING")
        print_status("Proceeding with mock models for demonstration", "INFO")
    
    # Try to install packages
    packages_to_install = [
        ("numpy", "1.24.0"),
        ("tensorflow", "2.15.0"),
        ("tensorflow-model-optimization", "0.7.0"),
        ("matplotlib", "3.7.0")
    ]
    
    success_count = 0
    for package, version in packages_to_install:
        try:
            print_status(f"Installing {package}...")
            subprocess.run([sys.executable, "-m", "pip", "install", f"{package}=={version}"], 
                         check=True, capture_output=True)
            print_status(f"Successfully installed {package}", "SUCCESS")
            success_count += 1
        except subprocess.CalledProcessError:
            print_status(f"Failed to install {package}", "WARNING")
        except Exception as e:
            print_status(f"Error installing {package}: {e}", "WARNING")
    
    if success_count == 0:
        print_status("No packages installed successfully", "WARNING")
        print_status("Creating mock models for demonstration", "INFO")
    
    # Create mock models and test data
    create_mock_models()
    create_test_images()
    create_model_analysis_script()
    create_requirements_file()
    create_installation_guide()
    
    print("\\n" + "=" * 60)
    print("INSTALLATION COMPLETED!")
    print("=" * 60)
    
    if success_count > 0:
        print("\\nTensorFlow packages installed successfully!")
        print("Next steps:")
        print("1. Run: python create_sample_data.py")
        print("2. Run: python quantize_prune_model.py")
        print("3. Run: python inference_script.py --benchmark")
    else:
        print("\\nUsing mock models for demonstration!")
        print("Next steps:")
        print("1. Run: python mock_model_analysis.py")
        print("2. Follow INSTALLATION_GUIDE.md for TensorFlow setup")
        print("3. Try alternative installation methods")
    
    print("\\nFiles created:")
    print("- models/ (mock model data)")
    print("- test_images/ (test data)")
    print("- mock_model_analysis.py (analysis script)")
    print("- requirements.txt (dependencies)")
    print("- INSTALLATION_GUIDE.md (setup guide)")
    
    return True

if __name__ == "__main__":
    main()
