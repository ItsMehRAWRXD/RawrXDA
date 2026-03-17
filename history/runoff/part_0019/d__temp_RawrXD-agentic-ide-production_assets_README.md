# Assets Directory

This directory contains visual assets for the RawrXD project documentation.

## 📸 Screenshots (Coming Soon)

### IDE Interface
- `ide-main-window.png` - Main RawrXD IDE interface
- `ide-completion-demo.gif` - Real-time code completion in action
- `ide-model-selector.png` - GGUF model loading interface

### Performance Graphs
- `latency-comparison.png` - RawrXD vs Cursor latency chart
- `throughput-benchmark.png` - Tokens/second performance graph
- `resource-usage.png` - CPU/GPU/RAM usage during inference

### Architecture Diagrams
- `architecture-overview.svg` - High-level system architecture
- `inference-pipeline.svg` - Token generation flow diagram
- `build-system.svg` - CMake dependency graph

## 🎬 Demo Videos (Planned)

- `quick-start-demo.mp4` - 2-minute quick start tutorial
- `model-loading.mp4` - How to load and test GGUF models
- `performance-comparison.mp4` - Side-by-side RawrXD vs Cursor

## 🖼️ Contributing Assets

To add new screenshots or demos:

1. **Screenshots**: Use PNG format, 1920x1080 resolution preferred
2. **GIFs**: Use optimized GIF (<5MB), 1280x720 resolution
3. **Videos**: Use MP4 (H.264), 1080p, <50MB file size
4. **Diagrams**: Use SVG format for scalability

### Naming Convention
```
{category}-{description}-{version}.{ext}
Examples:
  ide-completion-demo-v1.gif
  performance-latency-comparison-v2.png
  architecture-inference-pipeline-v1.svg
```

### File Size Limits
- Screenshots (PNG): <2MB
- Animated GIFs: <5MB
- Videos (MP4): <50MB
- Diagrams (SVG): <500KB

## 📝 Usage in Documentation

Reference assets using relative paths from repository root:

```markdown
![IDE Main Window](assets/ide-main-window.png)
![Completion Demo](assets/ide-completion-demo.gif)
```

---

**Note**: Actual screenshots and performance graphs will be added as development progresses. Placeholder assets are acceptable for initial releases.
