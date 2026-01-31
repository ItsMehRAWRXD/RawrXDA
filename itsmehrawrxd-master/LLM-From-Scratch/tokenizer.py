"""
Complete BPE Tokenizer Implementation
A full implementation of Byte-Pair Encoding tokenizer from scratch
"""

import re
import json
from collections import defaultdict, Counter
from typing import List, Dict, Tuple, Set

class BPETokenizer:
    """
    Complete Byte-Pair Encoding tokenizer implementation
    """
    
    def __init__(self, vocab_size: int = 10000):
        self.vocab_size = vocab_size
        self.word_freqs = {}
        self.splits = {}
        self.merges = {}
        self.vocab = {}
        self.reverse_vocab = {}
        
        # Special tokens
        self.unk_token = "<UNK>"
        self.pad_token = "<PAD>"
        self.eos_token = "<EOS>"
        self.bos_token = "<BOS>"
        self.sep_token = "<SEP>"
        
        # Initialize with special tokens
        self._init_special_tokens()
    
    def _init_special_tokens(self):
        """Initialize special tokens in vocabulary"""
        special_tokens = [
            self.unk_token,
            self.pad_token,
            self.eos_token,
            self.bos_token,
            self.sep_token
        ]
        
        for i, token in enumerate(special_tokens):
            self.vocab[token] = i
            self.reverse_vocab[i] = token
    
    def get_stats(self, ids: List[int]) -> Dict[Tuple[int, int], int]:
        """
        Computes pair statistics in a list of integers.
        """
        counts = defaultdict(int)
        for pair in zip(ids, ids[1:]):
            counts[pair] += 1
        return dict(counts)
    
    def merge_pair(self, ids: List[int], pair: Tuple[int, int], new_id: int) -> List[int]:
        """
        Replaces a pair of integers with a new integer ID.
        """
        new_ids = []
        i = 0
        while i < len(ids):
            if i < len(ids) - 1 and (ids[i], ids[i+1]) == pair:
                new_ids.append(new_id)
                i += 2
            else:
                new_ids.append(ids[i])
                i += 1
        return new_ids
    
    def _get_word_freqs(self, text: str) -> Dict[str, int]:
        """Extract word frequencies from text"""
        # Simple word tokenization (can be improved with regex)
        words = re.findall(r'\S+', text)
        return Counter(words)
    
    def _get_splits(self, word_freqs: Dict[str, int]) -> Dict[str, List[str]]:
        """Split words into characters"""
        splits = {}
        for word, freq in word_freqs.items():
            splits[word] = list(word) + ['</w>']  # Add end-of-word marker
        return splits
    
    def train(self, text: str):
        """
        Train the BPE tokenizer on the given text
        """
        print("Training BPE tokenizer...")
        
        # Get word frequencies
        self.word_freqs = self._get_word_freqs(text)
        print(f"Found {len(self.word_freqs)} unique words")
        
        # Get initial splits
        self.splits = self._get_splits(self.word_freqs)
        
        # Initialize vocabulary with characters
        alphabet = set()
        for word in self.word_freqs.keys():
            for letter in word:
                alphabet.add(letter)
        alphabet.add('</w>')
        
        # Add characters to vocabulary
        vocab_id = len(self.vocab)
        for char in sorted(alphabet):
            if char not in self.vocab:
                self.vocab[char] = vocab_id
                self.reverse_vocab[vocab_id] = char
                vocab_id += 1
        
        # BPE training loop
        num_merges = self.vocab_size - len(self.vocab)
        print(f"Performing {num_merges} merge operations...")
        
        for i in range(num_merges):
            pairs = self._get_pairs()
            if not pairs:
                break
            
            # Find most frequent pair
            best_pair = max(pairs, key=pairs.get)
            
            # Merge the pair
            new_token = best_pair[0] + best_pair[1]
            new_id = len(self.vocab)
            
            self.vocab[new_token] = new_id
            self.reverse_vocab[new_id] = new_token
            self.merges[best_pair] = new_id
            
            # Update splits
            self._merge_splits(best_pair, new_token)
            
            if (i + 1) % 1000 == 0:
                print(f"Completed {i + 1} merges, vocab size: {len(self.vocab)}")
        
        print(f"Training complete. Final vocab size: {len(self.vocab)}")
    
    def _get_pairs(self) -> Dict[Tuple[str, str], int]:
        """Get all possible pairs and their frequencies"""
        pairs = defaultdict(int)
        
        for word, freq in self.word_freqs.items():
            symbols = self.splits[word]
            for i in range(len(symbols) - 1):
                pair = (symbols[i], symbols[i + 1])
                pairs[pair] += freq
        
        return dict(pairs)
    
    def _merge_splits(self, pair: Tuple[str, str], new_token: str):
        """Merge the given pair in all word splits"""
        for word in self.splits:
            symbols = self.splits[word]
            new_symbols = []
            i = 0
            
            while i < len(symbols):
                if (i < len(symbols) - 1 and 
                    symbols[i] == pair[0] and 
                    symbols[i + 1] == pair[1]):
                    new_symbols.append(new_token)
                    i += 2
                else:
                    new_symbols.append(symbols[i])
                    i += 1
            
            self.splits[word] = new_symbols
    
    def encode(self, text: str) -> List[int]:
        """
        Encode text into token IDs
        """
        # Simple word-level tokenization
        words = re.findall(r'\S+', text)
        token_ids = []
        
        for word in words:
            # Get the BPE tokens for this word
            word_tokens = self._tokenize_word(word)
            
            # Convert tokens to IDs
            for token in word_tokens:
                if token in self.vocab:
                    token_ids.append(self.vocab[token])
                else:
                    token_ids.append(self.vocab[self.unk_token])
        
        return token_ids
    
    def _tokenize_word(self, word: str) -> List[str]:
        """Tokenize a single word using BPE"""
        if word in self.splits:
            return self.splits[word]
        
        # If word not seen during training, apply BPE rules
        symbols = list(word) + ['</w>']
        
        # Apply learned merges
        while len(symbols) > 1:
            pairs = [(symbols[i], symbols[i + 1]) for i in range(len(symbols) - 1)]
            
            # Find the pair with highest priority (earliest merge)
            pair_to_merge = None
            for pair in pairs:
                if pair in self.merges:
                    pair_to_merge = pair
                    break
            
            if pair_to_merge is None:
                break
            
            # Merge the pair
            new_symbols = []
            i = 0
            while i < len(symbols):
                if (i < len(symbols) - 1 and 
                    symbols[i] == pair_to_merge[0] and 
                    symbols[i + 1] == pair_to_merge[1]):
                    new_symbols.append(pair_to_merge[0] + pair_to_merge[1])
                    i += 2
                else:
                    new_symbols.append(symbols[i])
                    i += 1
            
            symbols = new_symbols
        
        return symbols
    
    def decode(self, token_ids: List[int]) -> str:
        """
        Decode token IDs back to text
        """
        tokens = []
        for token_id in token_ids:
            if token_id in self.reverse_vocab:
                token = self.reverse_vocab[token_id]
                if token not in [self.unk_token, self.pad_token, self.eos_token, self.bos_token, self.sep_token]:
                    tokens.append(token)
        
        # Join tokens and clean up
        text = ''.join(tokens)
        text = text.replace('</w>', ' ')
        return text.strip()
    
    def save(self, filepath: str):
        """Save tokenizer to file"""
        data = {
            'vocab': self.vocab,
            'reverse_vocab': self.reverse_vocab,
            'merges': self.merges,
            'vocab_size': self.vocab_size,
            'word_freqs': self.word_freqs,
            'splits': self.splits
        }
        
        with open(filepath, 'w', encoding='utf-8') as f:
            json.dump(data, f, ensure_ascii=False, indent=2)
    
    def load(self, filepath: str):
        """Load tokenizer from file"""
        with open(filepath, 'r', encoding='utf-8') as f:
            data = json.load(f)
        
        self.vocab = data['vocab']
        self.reverse_vocab = data['reverse_vocab']
        self.merges = data['merges']
        self.vocab_size = data['vocab_size']
        self.word_freqs = data['word_freqs']
        self.splits = data['splits']
    
    def get_vocab_size(self) -> int:
        """Get vocabulary size"""
        return len(self.vocab)
    
    def get_unk_token_id(self) -> int:
        """Get UNK token ID"""
        return self.vocab[self.unk_token]
    
    def get_eos_token_id(self) -> int:
        """Get EOS token ID"""
        return self.vocab[self.eos_token]
    
    def get_bos_token_id(self) -> int:
        """Get BOS token ID"""
        return self.vocab[self.bos_token]


# Example usage and testing
if __name__ == "__main__":
    # Sample training text
    sample_text = """
    def hello_world():
        print("Hello, World!")
        return "success"
    
    class MyClass:
        def __init__(self):
            self.value = 42
        
        def get_value(self):
            return self.value
    
    # This is a comment
    x = 10
    y = 20
    z = x + y
    """
    
    # Create and train tokenizer
    tokenizer = BPETokenizer(vocab_size=1000)
    tokenizer.train(sample_text)
    
    # Test encoding/decoding
    test_text = "def hello_world(): print('Hello')"
    encoded = tokenizer.encode(test_text)
    decoded = tokenizer.decode(encoded)
    
    print(f"Original: {test_text}")
    print(f"Encoded: {encoded}")
    print(f"Decoded: {decoded}")
    print(f"Vocab size: {tokenizer.get_vocab_size()}")
    
    # Save tokenizer
    tokenizer.save("tokenizer.json")
    print("Tokenizer saved to tokenizer.json")
