"""
GGUF Model Blob Analysis and Optimization Tool
Reverse engineers GGUF format to implement lazy loading and compression
"""

import struct
import json
import os
import gzip
import mmap
from typing import Dict, List, Tuple, Optional
from dataclasses import dataclass
from enum import IntEnum

class GGMLType(IntEnum):
    F32 = 0
    F16 = 1
    Q4_0 = 2
    Q4_1 = 3
    Q5_0 = 6
    Q5_1 = 7
    Q8_0 = 8
    Q8_1 = 9
    Q2_K = 10
    Q3_K = 11
    Q4_K = 12
    Q5_K = 13
    Q6_K = 14
    Q8_K = 15

@dataclass
class TensorInfo:
    name: str
    shape: List[int]
    type: GGMLType
    offset: int
    size: int
    is_critical: bool = True  # Whether tensor is needed for basic inference

class GGUFAnalyzer:
    """Analyzes GGUF files for optimization opportunities"""
    
    def __init__(self, filepath: str):
        self.filepath = filepath
        self.tensors: List[TensorInfo] = []
        self.metadata: Dict = {}
        self.header_size = 0
        
    def parse_header(self) -> None:
        """Parse GGUF header to extract metadata and tensor information"""
        with open(self.filepath, 'rb') as f:
            # Read magic number
            magic = f.read(4)
            if magic != b'GGUF':
                raise ValueError("Not a valid GGUF file")
            
            # Read version
            version = struct.unpack('<I', f.read(4))[0]
            
            # Read tensor count and metadata count
            tensor_count = struct.unpack('<Q', f.read(8))[0]
            metadata_count = struct.unpack('<Q', f.read(8))[0]
            
            # Parse metadata
            for _ in range(metadata_count):
                key_len = struct.unpack('<Q', f.read(8))[0]
                key = f.read(key_len).decode('utf-8')
                
                value_type = struct.unpack('<I', f.read(4))[0]
                value = self._read_metadata_value(f, value_type)
                self.metadata[key] = value
            
            # Parse tensor info
            for _ in range(tensor_count):
                name_len = struct.unpack('<Q', f.read(8))[0]
                name = f.read(name_len).decode('utf-8')
                
                # Read dimensions
                n_dims = struct.unpack('<I', f.read(4))[0]
                shape = []
                for _ in range(n_dims):
                    shape.append(struct.unpack('<Q', f.read(8))[0])
                
                # Read type and offset
                tensor_type = GGMLType(struct.unpack('<I', f.read(4))[0])
                offset = struct.unpack('<Q', f.read(8))[0]
                
                # Calculate size
                size = self._calculate_tensor_size(shape, tensor_type)
                
                # Determine criticality
                is_critical = self._is_critical_tensor(name)
                
                tensor_info = TensorInfo(name, shape, tensor_type, offset, size, is_critical)
                self.tensors.append(tensor_info)
            
            self.header_size = f.tell()
    
    def _read_metadata_value(self, f, value_type: int):
        """Read metadata value based on type"""
        if value_type == 8:  # String
            str_len = struct.unpack('<Q', f.read(8))[0]
            return f.read(str_len).decode('utf-8')
        elif value_type == 4:  # Uint32
            return struct.unpack('<I', f.read(4))[0]
        elif value_type == 5:  # Int32
            return struct.unpack('<i', f.read(4))[0]
        elif value_type == 6:  # Float32
            return struct.unpack('<f', f.read(4))[0]
        elif value_type == 9:  # Bool
            return struct.unpack('<?', f.read(1))[0]
        else:
            # Skip unknown types
            return None
    
    def _calculate_tensor_size(self, shape: List[int], tensor_type: GGMLType) -> int:
        """Calculate tensor size in bytes"""
        elements = 1
        for dim in shape:
            elements *= dim
        
        # Type sizes (approximate)
        type_sizes = {
            GGMLType.F32: 4,
            GGMLType.F16: 2,
            GGMLType.Q4_0: 0.5,
            GGMLType.Q4_1: 0.5,
            GGMLType.Q5_0: 0.625,
            GGMLType.Q5_1: 0.625,
            GGMLType.Q8_0: 1,
            GGMLType.Q8_1: 1,
            GGMLType.Q2_K: 0.25,
            GGMLType.Q3_K: 0.375,
            GGMLType.Q4_K: 0.5,
            GGMLType.Q5_K: 0.625,
            GGMLType.Q6_K: 0.75,
            GGMLType.Q8_K: 1,
        }
        
        return int(elements * type_sizes.get(tensor_type, 4))
    
    def _is_critical_tensor(self, name: str) -> bool:
        """Determine if tensor is critical for basic inference"""
        critical_patterns = [
            'token_embd',
            'output_norm',
            'output',
            'attn_norm',
            'attn_q',
            'attn_k',
            'attn_v',
            'attn_output',
        ]
        
        return any(pattern in name.lower() for pattern in critical_patterns)
    
    def analyze_compression_opportunities(self) -> Dict:
        """Analyze potential compression benefits"""
        total_size = sum(tensor.size for tensor in self.tensors)
        critical_size = sum(tensor.size for tensor in self.tensors if tensor.is_critical)
        non_critical_size = total_size - critical_size
        
        # Group tensors by layer for smart segmentation
        layer_groups = {}
        for tensor in self.tensors:
            layer_match = None
            for i in range(100):  # Assume max 100 layers
                if f'.{i}.' in tensor.name or f'_{i}_' in tensor.name:
                    layer_match = i
                    break
            
            group = f"layer_{layer_match}" if layer_match is not None else "global"
            if group not in layer_groups:
                layer_groups[group] = []
            layer_groups[group].append(tensor)
        
        return {
            'total_size': total_size,
            'critical_size': critical_size,
            'non_critical_size': non_critical_size,
            'critical_ratio': critical_size / total_size,
            'layer_groups': {k: len(v) for k, v in layer_groups.items()},
            'compression_potential': {
                'lazy_loading_savings': non_critical_size * 0.4,  # Estimate
                'compression_savings': total_size * 0.3,  # Estimate
                'combined_savings': total_size * 0.6  # Conservative estimate
            }
        }

class ModelOptimizer:
    """Optimizes model storage and loading"""
    
    def __init__(self, analyzer: GGUFAnalyzer):
        self.analyzer = analyzer
    
    def create_compressed_segments(self, output_dir: str, segment_size: int = 1024*1024*1024) -> None:
        """Create compressed segments with smart tensor grouping"""
        os.makedirs(output_dir, exist_ok=True)
        
        # Group tensors by criticality and size
        segments = []
        current_segment = []
        current_size = 0
        
        # Sort tensors: critical first, then by size
        sorted_tensors = sorted(self.analyzer.tensors, 
                              key=lambda t: (not t.is_critical, -t.size))
        
        for tensor in sorted_tensors:
            if current_size + tensor.size > segment_size and current_segment:
                segments.append(current_segment)
                current_segment = [tensor]
                current_size = tensor.size
            else:
                current_segment.append(tensor)
                current_size += tensor.size
        
        if current_segment:
            segments.append(current_segment)
        
        # Create segment files
        segment_info = []
        with open(self.analyzer.filepath, 'rb') as source_file:
            for i, segment in enumerate(segments):
                segment_file = os.path.join(output_dir, f"segment_{i:03d}.gz")
                
                with gzip.open(segment_file, 'wb', compresslevel=9) as gz_file:
                    segment_data = {
                        'segment_id': i,
                        'tensors': [],
                        'is_critical': all(t.is_critical for t in segment),
                        'total_size': sum(t.size for t in segment)
                    }
                    
                    # Write segment header
                    header = json.dumps(segment_data).encode('utf-8')
                    gz_file.write(struct.pack('<Q', len(header)))
                    gz_file.write(header)
                    
                    # Write tensor data
                    for tensor in segment:
                        source_file.seek(self.analyzer.header_size + tensor.offset)
                        data = source_file.read(tensor.size)
                        gz_file.write(data)
                        
                        segment_data['tensors'].append({
                            'name': tensor.name,
                            'shape': tensor.shape,
                            'type': tensor.type.value,
                            'size': tensor.size,
                            'is_critical': tensor.is_critical
                        })
                
                segment_info.append(segment_data)
        
        # Save optimization metadata
        optimization_data = {
            'original_file': os.path.basename(self.analyzer.filepath),
            'segments': segment_info,
            'header_size': self.analyzer.header_size,
            'metadata': self.analyzer.metadata,
            'optimization_stats': self.analyzer.analyze_compression_opportunities()
        }
        
        with open(os.path.join(output_dir, 'optimization_metadata.json'), 'w') as f:
            json.dump(optimization_data, f, indent=2)

def main():
    """Example usage"""
    import sys
    
    if len(sys.argv) < 2:
        print("Usage: python gguf_optimizer.py <model_file> [output_dir]")
        sys.exit(1)
    
    model_file = sys.argv[1]
    output_dir = sys.argv[2] if len(sys.argv) > 2 else "optimized_model"
    
    print(f"Analyzing model: {model_file}")
    
    # Analyze model
    analyzer = GGUFAnalyzer(model_file)
    analyzer.parse_header()
    
    print(f"Found {len(analyzer.tensors)} tensors")
    
    # Get compression analysis
    analysis = analyzer.analyze_compression_opportunities()
    print(f"Total size: {analysis['total_size'] / (1024**3):.2f} GB")
    print(f"Critical tensors: {analysis['critical_ratio']:.2%}")
    print(f"Estimated savings: {analysis['compression_potential']['combined_savings'] / (1024**3):.2f} GB")
    
    # Create optimized segments
    optimizer = ModelOptimizer(analyzer)
    optimizer.create_compressed_segments(output_dir)
    
    print(f"Optimization complete. Output saved to: {output_dir}")

if __name__ == "__main__":
    main()