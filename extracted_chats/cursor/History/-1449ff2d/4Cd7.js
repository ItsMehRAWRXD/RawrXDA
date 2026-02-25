// Add ALL your models to the IDE dropdown
const fs = require('fs');

console.log('📦 Adding ALL models to IDE...\n');

let html = fs.readFileSync('ide/BigDaddyG-IDE.html', 'utf8');

// Your complete model list
const allModels = {
    'Local Models (Ollama)': [
        { value: 'bigdaddyg:latest', name: '🧠 BigDaddyG Latest', desc: '2.0 GB • Your custom model' },
        { value: 'gemma3:1b', name: '💎 Gemma 3 1B', desc: '815 MB • Fast' },
        { value: 'gemma3:12b', name: '💎 Gemma 3 12B', desc: '8.1 GB • Best quality' },
        { value: 'llama3.2:latest', name: '🦙 Llama 3.2', desc: '2.0 GB • General' }
    ],
    '1M Context Models': [
        { value: 'cheetah-stealth:latest', name: '🐆 Cheetah Stealth', desc: 'Stealth • 1M context • $1.25/M' },
        { value: 'code-supernova:1m', name: '💫 Code Supernova', desc: 'Coding • 1M context • Free' }
    ],
    'Cloud Models (Optional)': [
        { value: 'gpt-oss:120b-cloud', name: '🌐 GPT-OSS 120B', desc: 'Cloud • Huge model' },
        { value: 'qwen3-vl:235b-cloud', name: '🌐 Qwen3-VL 235B', desc: 'Cloud • Vision + Language' }
    ],
    'BigDaddyG Specialized': [
        { value: 'BigDaddyG:Code', name: '🔧 BigDaddyG Code', desc: 'Code generation specialist' },
        { value: 'BigDaddyG:Debug', name: '🐛 BigDaddyG Debug', desc: 'Debugging expert' },
        { value: 'BigDaddyG:Crypto', name: '🔐 BigDaddyG Crypto', desc: 'Security & encryption' }
    ]
};

// Find the model select dropdown
const modelSelectStart = html.indexOf('<select id="model-select"');
const modelSelectEnd = html.indexOf('</select>', modelSelectStart) + '</select>'.length;

if (modelSelectStart === -1) {
    console.error('❌ Could not find model select dropdown');
    process.exit(1);
}

// Build new dropdown
let newSelect = '<select id="model-select" onchange="updateCurrentModel()">\n';

for (const [category, models] of Object.entries(allModels)) {
    newSelect += `                        <optgroup label="${category}">\n`;
    models.forEach(model => {
        const selected = model.value === 'bigdaddyg:latest' ? ' selected' : '';
        newSelect += `                            <option value="${model.value}"${selected}>${model.name}</option>\n`;
    });
    newSelect += '                        </optgroup>\n';
}

newSelect += '                    </select>';

// Replace the old select
html = html.substring(0, modelSelectStart) + newSelect + html.substring(modelSelectEnd);

fs.writeFileSync('ide/BigDaddyG-IDE.html', html);

console.log('✅ Added ALL models to IDE dropdown!\n');
console.log('Models added:');
Object.entries(allModels).forEach(([category, models]) => {
    console.log(`\n${category}:`);
    models.forEach(m => console.log(`  ✅ ${m.name}`));
});

console.log('\n💡 Total:', Object.values(allModels).flat().length, 'models available');

