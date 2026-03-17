"""Fuzzing harness for Ollama HTTP/JSON parsing and transport

- Generates mutated JSON bodies for the Ollama completion API
- If the native library is available, uses OllamaBridge to call native functions
- Otherwise, attempts to send mutated HTTP requests directly to a local server
- Logs responses, exceptions, and crashes

Usage:

python -m tests.fuzz_ollama --host 127.0.0.1 --port 11434 --iterations 1000

"""

import argparse
import json
import socket
import random
import string
import sys
import time
import os
from typing import Optional

# Attempt to import the local Ollama bridge
try:
    from src.ollama_bridge import OllamaBridge, GenerateRequest
    HAS_BRIDGE = True
except Exception as e:
    print(f"[!] OllamaBridge import failed: {e}")
    HAS_BRIDGE = False


def random_string(max_len: int = 2048):
    length = random.randint(0, max_len)
    return ''.join(random.choice(string.printable) for _ in range(length))


def mutate_json(base: dict) -> bytes:
    # Simple mutator: choose a mutation strategy
    strategies = [
        'truncate', 'randomize_string', 'swap_types', 'extra_fields', 'null_bytes', 'invalid_utf8'
    ]
    s = random.choice(strategies)
    data = json.dumps(base)

    if s == 'truncate':
        cut = random.randint(1, max(1, len(data) - 1))
        return data[:cut].encode('utf-8')

    if s == 'randomize_string':
        return json.dumps({k: random_string(1000) for k in base}).encode('utf-8')

    if s == 'swap_types':
        new = {}
        for k, v in base.items():
            if isinstance(v, str):
                new[k] = random.randint(0, 10**6)
            elif isinstance(v, (int, float)):
                new[k] = random_string(20)
            else:
                new[k] = None
        return json.dumps(new).encode('utf-8')

    if s == 'extra_fields':
        new = dict(base)
        for _ in range(random.randint(1, 10)):
            new[random_string(8)] = random_string(40)
        return json.dumps(new).encode('utf-8')

    if s == 'null_bytes':
        b = bytearray(json.dumps(base).encode('utf-8'))
        for _ in range(random.randint(1, 10)):
            pos = random.randint(0, len(b) - 1)
            b[pos] = 0
        return bytes(b)

    if s == 'invalid_utf8':
        b = bytearray(json.dumps(base).encode('utf-8'))
        for _ in range(random.randint(1, 6)):
            pos = random.randint(0, len(b) - 1)
            b[pos] = 0xFF
        return bytes(b)

    return data.encode('utf-8')


def build_http_request(host: str, port: int, body: bytes) -> bytes:
    headers = [
        f"POST /v1/generate HTTP/1.1",
        f"Host: {host}:{port}",
        "User-Agent: fuzz-ollama/1.0",
        "Content-Type: application/json",
        f"Content-Length: {len(body)}",
        "Connection: close",
        "", ""
    ]
    hdr = '\r\n'.join(headers).encode('utf-8')
    return hdr + body


def run_network_fuzz(host: str, port: int, iterations: int, delay: float = 0.01):
    payload_dir = os.path.join('build', 'fuzz_payloads')
    use_payloads = os.path.isdir(payload_dir) and len(os.listdir(payload_dir)) > 0
    payload_files = os.listdir(payload_dir) if use_payloads else []
    base = {"model": "llama2", "prompt": "Hello world"}
    stats = {'sent': 0, 'responses': 0, 'errors': 0}

    for i in range(iterations):
        try:
            if use_payloads:
                p = random.choice(payload_files)
                with open(os.path.join(payload_dir, p), 'rb') as f:
                    body = f.read()
            else:
                body = mutate_json(base)
            req = build_http_request(host, port, body)
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.settimeout(2)
                s.connect((host, port))
                s.sendall(req)
                # Try to receive, but be tolerant
                try:
                    data = s.recv(8192)
                    stats['responses'] += 1
                except socket.timeout:
                    stats['errors'] += 1
        except Exception as e:
            print(f"[E] Network fuzz error: {e}")
            stats['errors'] += 1
        finally:
            stats['sent'] += 1
        if delay:
            time.sleep(delay)
    print(stats)


def run_bridge_fuzz(host: str, port: int, iterations: int, delay: float = 0.01):
    if not HAS_BRIDGE:
        print('[!] run_bridge_fuzz: OllamaBridge module not available; skipping bridge fuzz')
        return
    try:
        bridge = OllamaBridge()
    except Exception as e:
        print(f"[!] OllamaBridge init error: {e} - skipping bridge fuzz")
        return
    ok = bridge.init(host, port)
    if not ok:
        print('[!] OllamaBridge init failed; skipping')
        return

    base = {"model": "llama2", "prompt": "Hello world"}
    import threading

    stats = {'sent': 0, 'ok': 0, 'error': 0}
    stats_lock = threading.Lock()

    def fuzz_worker():
        nonlocal stats
        try:
            body_json = {"model": random.choice(["llama2", "codellama", ""+random_string(50)]) ,
                         "prompt": random_string(1024)}
            req = GenerateRequest(
                model=body_json['model'] or 'llama2',
                prompt=body_json['prompt'],
                stream=False
            )
            resp = bridge.generate(req)
            with stats_lock:
                if resp is None:
                    stats['error'] += 1
                else:
                    stats['ok'] += 1
                stats['sent'] += 1
        except Exception as e:
            with stats_lock:
                stats['error'] += 1
                stats['sent'] += 1

    threads = []
    for i in range(iterations):
        t = threading.Thread(target=fuzz_worker)
        t.start()
        threads.append(t)
        if delay:
            time.sleep(delay)
    for t in threads:
        t.join()

    print(stats)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--host', default='127.0.0.1')
    parser.add_argument('--port', default=11434, type=int)
    parser.add_argument('--iterations', default=1000, type=int)
    parser.add_argument('--delay', default=0.01, type=float)
    parser.add_argument('--mode', default='net', choices=['net', 'bridge'])
    args = parser.parse_args()

    if args.mode == 'net':
        run_network_fuzz(args.host, args.port, args.iterations, args.delay)
    else:
        run_bridge_fuzz(args.host, args.port, args.iterations, args.delay)

