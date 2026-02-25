const express = require('express');
const cors = require('cors');
const path = require('path');

// Import memory optimizer functions
const { ModelOptimizer, DataCompressor } = require('./memory-optimizer');

const app = express();
const PORT = 3003;

app.use(cors());
app.use(express.json());

// Health check endpoint
app.get('/health', (req, res) => {
    res.json({ status: 'ok', timestamp: new Date().toISOString() });
});

// Chat endpoint for Glassquill
app.post('/chat/chatgpt', (req, res) => {
    const { message } = req.body;
    res.json({
        response: `Multi-AI Server received: ${message}`,
        timestamp: new Date().toISOString(),
        model: 'multi-ai-proxy'
    });
});

// AI models endpoint
app.get('/models', (req, res) => {
    res.json({
        models: [
            { id: 'ollama-proxy', name: 'Ollama Proxy' },
            { id: 'multi-ai', name: 'Multi-AI Router' }
        ]
    });
});

// Enhanced File Manager endpoint
app.get('/file-manager', (req, res) => {
    res.sendFile(path.join(__dirname, 'enhanced-file-manager.html'));
});

// Memory optimizer endpoint
app.post('/optimize-memory', (req, res) => {
    try {
        const { data, operation } = req.body;

        if (!data || !operation) {
            return res.status(400).json({ error: 'Data and operation required' });
        }

        let result;
        switch (operation) {
            case 'compress':
                result = DataCompressor.compressJSON(data);
                break;
            case 'decompress':
                result = DataCompressor.decompressJSON(data);
                break;
            case 'prune':
                result = ModelOptimizer.pruneModel(data, data.threshold || 0.1);
                break;
            case 'quantize':
                result = ModelOptimizer.quantizeModel(data, data.precision || 8);
                break;
            case 'entropy':
                result = { entropy: ModelOptimizer.calculateEntropy(JSON.stringify(data)) };
                break;
            default:
                return res.status(400).json({ error: 'Invalid operation' });
        }

        res.json({ success: true, result });
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

// File system API endpoint for real file operations
app.get('/api/files', async (req, res) => {
    try {
        // In a real implementation, this would use fs.readdir
        // For now, return the same data as the file manager
        const files = [
            { name: 'd3d', isDir: true, size: 0, modified: 1760975991 },
            { name: 'Home', isDir: true, size: 0, modified: 1759997702 },
            { name: 'New folder', isDir: true, size: 0, modified: 1759863737 },
            { name: 'ShadowExplorerPortable-0.9', isDir: true, size: 0, modified: 1760971966 },
            { name: '70200dedc564e680_0', isDir: false, size: 20388528, modified: 1760955298 },
            { name: 'All of It.txt', isDir: false, size: 249601, modified: 1760227632 },
            { name: 'codegen-backend.html', isDir: false, size: 12727, modified: 1759504162 },
            { name: 'copilot.txt', isDir: false, size: 1302, modified: 1759829355 },
            { name: 'Deepseek R1 2.5.txt', isDir: false, size: 1147, modified: 1759994641 },
            { name: 'desktop.ini', isDir: false, size: 282, modified: 1759474177 },
            { name: 'eli9.html', isDir: false, size: 80089, modified: 1760863787 },
            { name: 'ELWO.html', isDir: false, size: 230656, modified: 1758251700 },
            { name: 'entry-insert.html', isDir: false, size: 111421, modified: 1703196022 },
            { name: 'f_000011', isDir: false, size: 7874318, modified: 1760964333 },
            { name: 'Fixes.txt', isDir: false, size: 165, modified: 1760716493 },
            { name: 'foundry.modelinfo.json', isDir: false, size: 46960, modified: 1759765103 },
            { name: 'GitHub.txt', isDir: false, size: 58, modified: 1760921103 },
            { name: 'hm.txt', isDir: false, size: 0, modified: 1760913462 },
            { name: 'huh.txt', isDir: false, size: 1222965, modified: 1761025318 },
            { name: 'LICENSE', isDir: false, size: 1550, modified: 1713362648 },
            { name: 'local-index.1.db', isDir: false, size: 17338368, modified: 1759749187 },
            { name: 'Microsoft Edge.lnk', isDir: false, size: 2358, modified: 1761014454 },
            { name: 'nasm.exe', isDir: false, size: 1650688, modified: 1713362910 },
            { name: 'nasm.lnk', isDir: false, size: 2079, modified: 1760967788 },
            { name: 'ndisasm.exe', isDir: false, size: 1124864, modified: 1713362910 },
            { name: 'Rawr.txt', isDir: false, size: 160413, modified: 1760365506 },
            { name: 'rawrss.txt', isDir: false, size: 731115, modified: 1760913045 },
            { name: 'RawrZ-Working-Encryption.html', isDir: false, size: 18492, modified: 1760863907 },
            { name: 'READASAP123.txt', isDir: false, size: 716950, modified: 1760177556 },
            { name: 'Screenshot 2025-10-19 143536.png', isDir: false, size: 404439, modified: 1760898963 },
            { name: 'Screenshot 2025-10-20 081204.png', isDir: false, size: 5439476, modified: 1760962377 },
            { name: 'Screenshot 2025-10-20 104908.png', isDir: false, size: 6801864, modified: 1760971769 },
            { name: 'Spotify.lnk', isDir: false, size: 1856, modified: 1760916586 },
            { name: 'Todos.txt', isDir: false, size: 823, modified: 1760714293 },
            { name: 'workspace-chunks.db', isDir: false, size: 32768, modified: 1759747771 }
        ];

        res.json({ files });
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

app.listen(PORT, () => {
    console.log(`Multi-AI Server running on http://localhost:${PORT}`);
    console.log(`Health check: http://localhost:${PORT}/health`);
    console.log(`Glassquill endpoint: http://localhost:${PORT}/chat/chatgpt`);
});