# Ollama Blob Scan Guide

This guide explains the non‑critical integration feature for scanning Ollama model blobs in the Blob to GGUF Converter.

## Overview

The Blob to GGUF Converter now includes an **Ollama Blob Scan** feature that:
- Scans an Ollama models directory for manifests and blob files
- Lists detected models in a dropdown
- Auto‑fills the blob path and output name when a model is selected

## Usage

1. Open **Blob to GGUF Converter** panel.
2. In **File Selection** group:
   - Enter the path to your Ollama models directory (e.g., `~/.ollama/models` on Linux, `%USERPROFILE%\.ollama\models` on Windows)
   - Click **Scan Blobs**
3. Choose a model from the **Detected Models** dropdown.
4. The blob path and GGUF output name will auto‑fill.
5. Proceed with conversion as usual.

## Telemetry

The scan emits telemetry events:
- `blob_scan_detected_models`: count of detected models and scan directory
- `blob_conversion_started`, `blob_conversion_completed`, `blob_conversion_failed`, `blob_conversion_cancelled`: conversion lifecycle
- `gguf_open_streaming`: streaming GGUF open events with tensor and zone counts

## Feature Flag

A settings key `EnableOllamaBlobScan` controls this feature. To disable:
```cpp
SettingsManager::instance().setValue("EnableOllamaBlobScan", false);
```

## Notes

- Only safe, non‑critical integration is added; existing logic remains unchanged.
- Log messages are tagged `[BlobScan]` and `[GGUFStream]` for filtering.