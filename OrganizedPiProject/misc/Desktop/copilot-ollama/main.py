import os
from flask import Flask, request, jsonify
import requests

app = Flask(__name__)

@app.route('/v1/chat/completions', methods=['POST'])
def chat_completions():
    data = request.json
    data['model'] = 'moonshotai/kimi-k2'
    response = requests.post('https://openrouter.ai/api/v1/chat/completions', 
        headers={'Authorization': f'Bearer {os.environ.get("OPENROUTER_API_KEY")}', 'Content-Type': 'application/json'}, 
        json=data)
    return response.json()

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=11434)