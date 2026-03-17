"""Simple fuzzing payload generator for Ollama requests

Generates a set of mutated JSON payloads and stores them in `build/fuzz_payloads/
"""

import json
import os
import random
import string

PAYLOAD_DIR = os.path.join('build', 'fuzz_payloads')


def random_string(max_len=512):
    return ''.join(random.choice(string.printable) for _ in range(random.randint(0, max_len)))


def generate_mutations(count: int = 100):
    os.makedirs(PAYLOAD_DIR, exist_ok=True)
    base = {"model": "llama2", "prompt": "Hello world"}
    for i in range(count):
        s = random.choice(['truncate', 'random', 'swap', 'extra', 'nulls', 'invalid'])
        if s == 'truncate':
            d = json.dumps(base)
            d = d[:random.randint(1, len(d)-1)]
            raw = d.encode('utf-8', errors='ignore')
        elif s == 'random':
            raw = json.dumps({"model": random_string(64), "prompt": random_string(2048)}).encode('utf-8')
        elif s == 'swap':
            raw = json.dumps({"model": 1234, "prompt": random_string(20)}).encode('utf-8')
        elif s == 'extra':
            d = dict(base)
            for _ in range(random.randint(1, 20)):
                d[random_string(8)] = random_string(200)
            raw = json.dumps(d).encode('utf-8')
        elif s == 'nulls':
            b = bytearray(json.dumps(base).encode('utf-8'))
            for _ in range(random.randint(1, 8)):
                b[random.randint(0, len(b)-1)] = 0
            raw = bytes(b)
        else:
            b = bytearray(json.dumps(base).encode('utf-8'))
            for _ in range(random.randint(1, 6)):
                b[random.randint(0, len(b)-1)] = 0xff
            raw = bytes(b)

        fname = os.path.join(PAYLOAD_DIR, f"payload_{i:04d}.bin")
        with open(fname, 'wb') as f:
            f.write(raw)

    print(f"Wrote {count} payloads to {PAYLOAD_DIR}")


if __name__ == '__main__':
    generate_mutations(256)  
