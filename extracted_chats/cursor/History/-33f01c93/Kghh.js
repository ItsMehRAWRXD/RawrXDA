# 1. Save as cursor-fileless-bridge.js
# 2. Install dependencies
npm install express cors

# 3. Start the bridge
node cursor-fileless-bridge.js

# 4. Test immediately
curl -X POST http://localhost:11442/v1/completions \
  -H "Content-Type: application/json" \
  -d '{"prompt": "Create a function to calculate fibonacci", "model": "your-custom-model"}'