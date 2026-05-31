const body = {
  model: 'phi3:mini',
  stream: false,
  prompt: 'You are scaffolding a minimal Vite React TypeScript dev server. Return a concise plan with commands, expected files, and one note about running npm run dev.'
};

fetch('http://127.0.0.1:11434/api/generate', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify(body)
})
  .then((response) => response.json())
  .then((json) => {
    console.log('MODEL_RESPONSE_START');
    console.log((json.response || '').slice(0, 1600));
    console.log('MODEL_RESPONSE_END');
  })
  .catch((error) => {
    console.error(`MODEL_ERROR:${error.message}`);
    process.exit(1);
  });
