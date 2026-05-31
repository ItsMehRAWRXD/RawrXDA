"""
Unified Extreme Memory Optimization System (UEMOS)
A complete memory management system for AI/ML applications
"""

import hashlib
import json
import pickle
import time
import threading
import uuid
from abc import ABC, abstractmethod
from collections import defaultdict, OrderedDict
from dataclasses import dataclass, field
from datetime import datetime, timedelta
from enum import Enum, auto
from typing import Any, Dict, List, Optional, Tuple, Union, Callable, Set
from pathlib import Path
import heapq
import sqlite3
import numpy as np
from contextlib import contextmanager
import logging

# ============================================================================
# CONFIGURATION AND CONSTANTS
# ============================================================================

class Config:
    """System configuration"""
    DEFAULT_STORAGE_PATH = "./uemos_memory"
    MAX_MEMORY_SIZE = 10_000
    CACHE_SIZE = 1000

    DEFAULT_TTL = 7 * 24 * 3600
    ARCHIVE_THRESHOLD = 3600
    CLEANUP_INTERVAL = 300

    MAX_RETRIEVAL_RESULTS = 100
    EMBEDDING_DIM = 128
    BATCH_SIZE = 32

    MAX_PERMISSION_DEPTH = 10

    LOG_LEVEL = logging.INFO


# ============================================================================
# ENUMS AND TYPES
# ============================================================================

class MemoryType(Enum):
    PLAINTEXT = auto()
    ACTIVATION = auto()
    PARAMETER = auto()


class MemoryState(Enum):
    GENERATED = auto()
    ACTIVATED = auto()
    ACCESSED = auto()
    MERGED = auto()
    ARCHIVED = auto()
    EXPIRED = auto()


class AccessLevel(Enum):
    PRIVATE = auto()
    SHARED = auto()
    PUBLIC = auto()
    READ_ONLY = auto()


class Priority(Enum):
    CRITICAL = 1
    HIGH = 2
    MEDIUM = 3
    LOW = 4
    ARCHIVE = 5


class EventType(Enum):
    CREATE = auto()
    READ = auto()
    UPDATE = auto()
    DELETE = auto()
    MERGE = auto()
    MIGRATE = auto()
    STATE_CHANGE = auto()
    ACCESS_DENIED = auto()


# ============================================================================
# CORE DATA STRUCTURES
# ============================================================================

@dataclass
class MemoryMetadata:
    memory_id: str
    memory_type: MemoryType
    created_at: datetime
    updated_at: datetime
    last_accessed: datetime
    state: MemoryState
    priority: Priority
    ttl: int
    access_count: int = 0
    size_bytes: int = 0

    owner: str = "system"
    access_level: AccessLevel = AccessLevel.PRIVATE
    allowed_users: Set[str] = field(default_factory=set)
    tags: Set[str] = field(default_factory=set)

    embedding: Optional[np.ndarray] = None
    semantic_hash: str = ""

    source: str = ""
    parent_ids: List[str] = field(default_factory=list)
    version: int = 1

    def to_dict(self) -> Dict:
        return {
            'memory_id': self.memory_id,
            'memory_type': self.memory_type.name,
            'created_at': self.created_at.isoformat(),
            'updated_at': self.updated_at.isoformat(),
            'last_accessed': self.last_accessed.isoformat(),
            'state': self.state.name,
            'priority': self.priority.value,
            'ttl': self.ttl,
            'access_count': self.access_count,
            'size_bytes': self.size_bytes,
            'owner': self.owner,
            'access_level': self.access_level.name,
            'allowed_users': list(self.allowed_users),
            'tags': list(self.tags),
            'semantic_hash': self.semantic_hash,
            'source': self.source,
            'parent_ids': self.parent_ids,
            'version': self.version
        }

    @classmethod
    def from_dict(cls, data: Dict) -> 'MemoryMetadata':
        data = dict(data)  # shallow copy; don't mutate caller
        data['memory_type'] = MemoryType[data['memory_type']]
        data['state'] = MemoryState[data['state']]
        data['priority'] = Priority(data['priority'])
        data['access_level'] = AccessLevel[data['access_level']]
        data['created_at'] = datetime.fromisoformat(data['created_at'])
        data['updated_at'] = datetime.fromisoformat(data['updated_at'])
        data['last_accessed'] = datetime.fromisoformat(data['last_accessed'])
        data['allowed_users'] = set(data.get('allowed_users', []))
        data['tags'] = set(data.get('tags', []))
        # Drop unknown keys not part of the dataclass
        valid = {f.name for f in cls.__dataclass_fields__.values()}
        data = {k: v for k, v in data.items() if k in valid}
        return cls(**data)


@dataclass
class MemoryPayload:
    content: Any
    format: str
    encoding: str = 'utf-8'

    def serialize(self) -> bytes:
        if self.format == 'text':
            return str(self.content).encode(self.encoding)
        elif self.format == 'tensor':
            return pickle.dumps(self.content)
        elif self.format in ['json', 'dict']:
            return json.dumps(self.content).encode(self.encoding)
        else:
            return pickle.dumps(self.content)

    @classmethod
    def deserialize(cls, data: bytes, format: str, encoding: str = 'utf-8') -> 'MemoryPayload':
        if format == 'text':
            content = data.decode(encoding)
        elif format == 'tensor':
            content = pickle.loads(data)
        elif format in ['json', 'dict']:
            content = json.loads(data.decode(encoding))
        else:
            content = pickle.loads(data)
        return cls(content=content, format=format, encoding=encoding)


@dataclass
class MemCube:
    metadata: MemoryMetadata
    payload: Optional[MemoryPayload] = None

    def __post_init__(self):
        if self.metadata.embedding is None:
            self.metadata.embedding = np.zeros(Config.EMBEDDING_DIM)
        if not self.metadata.semantic_hash and self.payload:
            self.metadata.semantic_hash = self._compute_hash()

    def _compute_hash(self) -> str:
        if self.payload is None:
            return ""
        content_str = str(self.payload.content)[:1000]
        return hashlib.sha256(content_str.encode()).hexdigest()[:16]

    def is_expired(self) -> bool:
        age = (datetime.now() - self.metadata.created_at).total_seconds()
        return age > self.metadata.ttl

    def should_archive(self) -> bool:
        inactive_time = (datetime.now() - self.metadata.last_accessed).total_seconds()
        return inactive_time > Config.ARCHIVE_THRESHOLD

    def get_priority_score(self) -> float:
        recency = (datetime.now() - self.metadata.last_accessed).total_seconds()
        frequency = self.metadata.access_count
        base_priority = 1.0 / self.metadata.priority.value
        recency_factor = 1.0 / (1.0 + recency / 3600.0)
        frequency_factor = 1.0 + np.log1p(frequency)
        return base_priority * recency_factor * frequency_factor


# ============================================================================
# EVENT AND AUDIT LOGGING
# ============================================================================

@dataclass
class MemoryEvent:
    event_id: str
    event_type: EventType
    memory_id: str
    timestamp: datetime
    user: str
    details: Dict[str, Any] = field(default_factory=dict)

    def to_dict(self) -> Dict:
        return {
            'event_id': self.event_id,
            'event_type': self.event_type.name,
            'memory_id': self.memory_id,
            'timestamp': self.timestamp.isoformat(),
            'user': self.user,
            'details': self.details
        }


class AuditLog:
    def __init__(self, storage_path: Path):
        self.storage_path = storage_path
        self.log_file = storage_path / "audit.log"
        self.lock = threading.Lock()
        self._init_db()

    def _init_db(self):
        self.db_path = self.storage_path / "audit.db"
        conn = sqlite3.connect(str(self.db_path))
        cursor = conn.cursor()
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS events (
                event_id TEXT PRIMARY KEY,
                event_type TEXT,
                memory_id TEXT,
                timestamp TEXT,
                user TEXT,
                details TEXT
            )
        ''')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_memory ON events(memory_id)')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_timestamp ON events(timestamp)')
        conn.commit()
        conn.close()

    def log_event(self, event: MemoryEvent):
        with self.lock:
            conn = sqlite3.connect(str(self.db_path))
            cursor = conn.cursor()
            cursor.execute(
                'INSERT INTO events VALUES (?, ?, ?, ?, ?, ?)',
                (event.event_id, event.event_type.name, event.memory_id,
                 event.timestamp.isoformat(), event.user, json.dumps(event.details))
            )
            conn.commit()
            conn.close()

    def query_events(self, memory_id: Optional[str] = None,
                     event_type: Optional[EventType] = None,
                     start_time: Optional[datetime] = None,
                     end_time: Optional[datetime] = None,
                     limit: int = 100) -> List[MemoryEvent]:
        conn = sqlite3.connect(str(self.db_path))
        cursor = conn.cursor()

        query = 'SELECT * FROM events WHERE 1=1'
        params: List[Any] = []

        if memory_id:
            query += ' AND memory_id = ?'; params.append(memory_id)
        if event_type:
            query += ' AND event_type = ?'; params.append(event_type.name)
        if start_time:
            query += ' AND timestamp >= ?'; params.append(start_time.isoformat())
        if end_time:
            query += ' AND timestamp <= ?'; params.append(end_time.isoformat())

        query += f' ORDER BY timestamp DESC LIMIT {int(limit)}'
        cursor.execute(query, params)
        rows = cursor.fetchall()
        conn.close()

        return [
            MemoryEvent(
                event_id=row[0],
                event_type=EventType[row[1]],
                memory_id=row[2],
                timestamp=datetime.fromisoformat(row[3]),
                user=row[4],
                details=json.loads(row[5])
            ) for row in rows
        ]


# ============================================================================
# STORAGE BACKEND
# ============================================================================

class StorageBackend(ABC):
    @abstractmethod
    def store(self, memcube: MemCube) -> bool: ...
    @abstractmethod
    def retrieve(self, memory_id: str) -> Optional[MemCube]: ...
    @abstractmethod
    def delete(self, memory_id: str) -> bool: ...
    @abstractmethod
    def list_memories(self, filters: Optional[Dict] = None) -> List[str]: ...
    @abstractmethod
    def update_metadata(self, memory_id: str, updates: Dict) -> bool: ...


class DiskStorage(StorageBackend):
    def __init__(self, storage_path: Path):
        self.storage_path = storage_path
        self.storage_path.mkdir(parents=True, exist_ok=True)
        self.metadata_path = storage_path / "metadata"
        self.payload_path = storage_path / "payloads"
        self.metadata_path.mkdir(exist_ok=True)
        self.payload_path.mkdir(exist_ok=True)
        self.index_db = storage_path / "index.db"
        self._init_index()

    def _init_index(self):
        conn = sqlite3.connect(str(self.index_db))
        cursor = conn.cursor()
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS memory_index (
                memory_id TEXT PRIMARY KEY,
                memory_type TEXT,
                state TEXT,
                owner TEXT,
                created_at TEXT,
                last_accessed TEXT,
                priority INTEGER,
                tags TEXT,
                format TEXT
            )
        ''')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_type ON memory_index(memory_type)')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_state ON memory_index(state)')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_owner ON memory_index(owner)')
        conn.commit()
        conn.close()

    def store(self, memcube: MemCube) -> bool:
        try:
            metadata_file = self.metadata_path / f"{memcube.metadata.memory_id}.json"
            meta_dict = memcube.metadata.to_dict()
            if memcube.payload is not None:
                meta_dict['format'] = memcube.payload.format
            with open(metadata_file, 'w') as f:
                json.dump(meta_dict, f)

            if memcube.payload:
                payload_file = self.payload_path / f"{memcube.metadata.memory_id}.bin"
                with open(payload_file, 'wb') as f:
                    f.write(memcube.payload.serialize())

            self._update_index(memcube)
            return True
        except Exception as e:
            logging.error(f"Failed to store memory {memcube.metadata.memory_id}: {e}")
            return False

    def retrieve(self, memory_id: str) -> Optional[MemCube]:
        try:
            metadata_file = self.metadata_path / f"{memory_id}.json"
            if not metadata_file.exists():
                return None
            with open(metadata_file, 'r') as f:
                metadata_dict = json.load(f)
            fmt = metadata_dict.get('format', 'text')
            metadata = MemoryMetadata.from_dict(metadata_dict)

            payload = None
            payload_file = self.payload_path / f"{memory_id}.bin"
            if payload_file.exists():
                with open(payload_file, 'rb') as f:
                    payload_data = f.read()
                payload = MemoryPayload.deserialize(payload_data, fmt)
            return MemCube(metadata=metadata, payload=payload)
        except Exception as e:
            logging.error(f"Failed to retrieve memory {memory_id}: {e}")
            return None

    def delete(self, memory_id: str) -> bool:
        try:
            metadata_file = self.metadata_path / f"{memory_id}.json"
            payload_file = self.payload_path / f"{memory_id}.bin"
            if metadata_file.exists(): metadata_file.unlink()
            if payload_file.exists(): payload_file.unlink()

            conn = sqlite3.connect(str(self.index_db))
            cursor = conn.cursor()
            cursor.execute('DELETE FROM memory_index WHERE memory_id = ?', (memory_id,))
            conn.commit()
            conn.close()
            return True
        except Exception as e:
            logging.error(f"Failed to delete memory {memory_id}: {e}")
            return False

    def list_memories(self, filters: Optional[Dict] = None) -> List[str]:
        conn = sqlite3.connect(str(self.index_db))
        cursor = conn.cursor()
        query = 'SELECT memory_id FROM memory_index WHERE 1=1'
        params: List[Any] = []
        if filters:
            if 'memory_type' in filters:
                mt = filters['memory_type']
                query += ' AND memory_type = ?'
                params.append(mt.name if isinstance(mt, MemoryType) else mt)
            if 'state' in filters:
                s = filters['state']
                query += ' AND state = ?'
                params.append(s.name if isinstance(s, MemoryState) else s)
            if 'owner' in filters:
                query += ' AND owner = ?'; params.append(filters['owner'])
        cursor.execute(query, params)
        results = [row[0] for row in cursor.fetchall()]
        conn.close()
        return results

    def update_metadata(self, memory_id: str, updates: Dict) -> bool:
        memcube = self.retrieve(memory_id)
        if not memcube:
            return False
        for key, value in updates.items():
            if hasattr(memcube.metadata, key):
                setattr(memcube.metadata, key, value)
        memcube.metadata.updated_at = datetime.now()
        return self.store(memcube)

    def _update_index(self, memcube: MemCube):
        conn = sqlite3.connect(str(self.index_db))
        cursor = conn.cursor()
        fmt = memcube.payload.format if memcube.payload else 'text'
        cursor.execute('''
            INSERT OR REPLACE INTO memory_index
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        ''', (
            memcube.metadata.memory_id,
            memcube.metadata.memory_type.name,
            memcube.metadata.state.name,
            memcube.metadata.owner,
            memcube.metadata.created_at.isoformat(),
            memcube.metadata.last_accessed.isoformat(),
            memcube.metadata.priority.value,
            json.dumps(list(memcube.metadata.tags)),
            fmt,
        ))
        conn.commit()
        conn.close()


class MemoryCache:
    def __init__(self, capacity: int = Config.CACHE_SIZE):
        self.capacity = capacity
        self.cache: "OrderedDict[str, MemCube]" = OrderedDict()
        self.lock = threading.Lock()

    def get(self, memory_id: str) -> Optional[MemCube]:
        with self.lock:
            if memory_id in self.cache:
                self.cache.move_to_end(memory_id)
                return self.cache[memory_id]
            return None

    def put(self, memcube: MemCube):
        with self.lock:
            mid = memcube.metadata.memory_id
            if mid in self.cache:
                self.cache.move_to_end(mid)
                self.cache[mid] = memcube
            else:
                if len(self.cache) >= self.capacity:
                    self.cache.popitem(last=False)
                self.cache[mid] = memcube

    def remove(self, memory_id: str):
        with self.lock:
            self.cache.pop(memory_id, None)

    def clear(self):
        with self.lock:
            self.cache.clear()


# ============================================================================
# EMBEDDING AND SIMILARITY
# ============================================================================

class EmbeddingEngine:
    def __init__(self, dim: int = Config.EMBEDDING_DIM):
        self.dim = dim

    def embed(self, text: str) -> np.ndarray:
        hash_values = []
        for i in range(self.dim):
            hash_input = f"{text}_{i}".encode()
            hash_val = int(hashlib.md5(hash_input).hexdigest(), 16)
            hash_values.append((hash_val % 10000) / 10000.0)
        embedding = np.array(hash_values, dtype=np.float32)
        norm = np.linalg.norm(embedding)
        if norm > 0:
            embedding = embedding / norm
        return embedding

    def embed_batch(self, texts: List[str]) -> np.ndarray:
        return np.array([self.embed(t) for t in texts])

    @staticmethod
    def cosine_similarity(a: np.ndarray, b: np.ndarray) -> float:
        norm_a = np.linalg.norm(a); norm_b = np.linalg.norm(b)
        if norm_a == 0 or norm_b == 0: return 0.0
        return float(np.dot(a, b) / (norm_a * norm_b))

    @staticmethod
    def find_similar(query_embedding: np.ndarray,
                     embeddings: Dict[str, np.ndarray],
                     top_k: int = 10) -> List[Tuple[str, float]]:
        similarities = []
        for mem_id, emb in embeddings.items():
            sim = EmbeddingEngine.cosine_similarity(query_embedding, emb)
            similarities.append((mem_id, sim))
        similarities.sort(key=lambda x: x[1], reverse=True)
        return similarities[:top_k]


# ============================================================================
# GOVERNANCE AND ACCESS CONTROL
# ============================================================================

class GovernanceEngine:
    def __init__(self):
        self.user_permissions: Dict[str, Set[str]] = defaultdict(set)
        self.role_permissions: Dict[str, Set[str]] = defaultdict(set)
        self.user_roles: Dict[str, Set[str]] = defaultdict(set)
        self.lock = threading.Lock()

    def check_access(self, user: str, memory_id: str, memcube: MemCube,
                     access_type: str = 'read') -> bool:
        if user == memcube.metadata.owner:
            return True
        if memory_id in self.user_permissions.get(user, set()):
            return True
        for role in self.user_roles.get(user, set()):
            if memory_id in self.role_permissions.get(role, set()):
                return True
        if memcube.metadata.access_level == AccessLevel.PUBLIC:
            return True
        elif memcube.metadata.access_level == AccessLevel.SHARED:
            return user in memcube.metadata.allowed_users
        elif memcube.metadata.access_level == AccessLevel.READ_ONLY:
            return access_type == 'read' and user in memcube.metadata.allowed_users
        return False

    def grant_access(self, user: str, memory_id: str, role: Optional[str] = None):
        with self.lock:
            if role: self.role_permissions[role].add(memory_id)
            else: self.user_permissions[user].add(memory_id)

    def revoke_access(self, user: str, memory_id: str, role: Optional[str] = None):
        with self.lock:
            if role: self.role_permissions[role].discard(memory_id)
            else: self.user_permissions[user].discard(memory_id)

    def assign_role(self, user: str, role: str):
        with self.lock: self.user_roles[user].add(role)

    def remove_role(self, user: str, role: str):
        with self.lock: self.user_roles[user].discard(role)


# ============================================================================
# LIFECYCLE MANAGER
# ============================================================================

class LifecycleManager:
    def __init__(self, storage: StorageBackend, audit_log: AuditLog):
        self.storage = storage
        self.audit_log = audit_log
        self.state_transitions = {
            MemoryState.GENERATED: [MemoryState.ACTIVATED, MemoryState.ARCHIVED],
            MemoryState.ACTIVATED: [MemoryState.ACCESSED, MemoryState.MERGED, MemoryState.ARCHIVED],
            MemoryState.ACCESSED: [MemoryState.ACTIVATED, MemoryState.MERGED, MemoryState.ARCHIVED],
            MemoryState.MERGED: [MemoryState.ACTIVATED, MemoryState.ARCHIVED],
            MemoryState.ARCHIVED: [MemoryState.ACTIVATED, MemoryState.EXPIRED],
            MemoryState.EXPIRED: []
        }

    def transition(self, memcube: MemCube, new_state: MemoryState, user: str = "system") -> bool:
        current_state = memcube.metadata.state
        if new_state not in self.state_transitions.get(current_state, []):
            logging.warning(f"Invalid state transition: {current_state} -> {new_state}")
            return False
        memcube.metadata.state = new_state
        memcube.metadata.updated_at = datetime.now()
        self.storage.update_metadata(
            memcube.metadata.memory_id,
            {'state': new_state, 'updated_at': memcube.metadata.updated_at}
        )
        self.audit_log.log_event(MemoryEvent(
            event_id=str(uuid.uuid4()),
            event_type=EventType.STATE_CHANGE,
            memory_id=memcube.metadata.memory_id,
            timestamp=datetime.now(),
            user=user,
            details={'from_state': current_state.name, 'to_state': new_state.name}
        ))
        return True

    def merge_memories(self, memory_ids: List[str], user: str = "system") -> Optional[str]:
        memories = [self.storage.retrieve(mid) for mid in memory_ids]
        memories = [m for m in memories if m is not None]
        if len(memories) < 2:
            return None

        merged_id = str(uuid.uuid4())
        merged_content = "\n\n".join([
            f"--- Source: {m.metadata.memory_id} ---\n{m.payload.content if m.payload else ''}"
            for m in memories
        ])

        merged_metadata = MemoryMetadata(
            memory_id=merged_id,
            memory_type=memories[0].metadata.memory_type,
            created_at=datetime.now(),
            updated_at=datetime.now(),
            last_accessed=datetime.now(),
            state=MemoryState.MERGED,
            priority=Priority.MEDIUM,
            ttl=max(m.metadata.ttl for m in memories),
            owner=user,
            parent_ids=memory_ids
        )
        merged_payload = MemoryPayload(content=merged_content, format='text')
        merged_memcube = MemCube(metadata=merged_metadata, payload=merged_payload)
        self.storage.store(merged_memcube)

        for mem in memories:
            self.transition(mem, MemoryState.MERGED, user)

        self.audit_log.log_event(MemoryEvent(
            event_id=str(uuid.uuid4()),
            event_type=EventType.MERGE,
            memory_id=merged_id,
            timestamp=datetime.now(),
            user=user,
            details={'source_ids': memory_ids}
        ))
        return merged_id


# ============================================================================
# RETRIEVAL ENGINE
# ============================================================================

class RetrievalEngine:
    def __init__(self, storage: StorageBackend, embedding_engine: EmbeddingEngine):
        self.storage = storage
        self.embedding_engine = embedding_engine
        self.embeddings_cache: Dict[str, np.ndarray] = {}
        self.lock = threading.Lock()

    def retrieve_by_id(self, memory_id: str) -> Optional[MemCube]:
        return self.storage.retrieve(memory_id)

    def retrieve_semantic(self, query: str, top_k: int = 10,
                          filters: Optional[Dict] = None) -> List[Tuple[MemCube, float]]:
        query_embedding = self.embedding_engine.embed(query)
        self._update_embeddings_cache(filters)
        similar = EmbeddingEngine.find_similar(query_embedding, self.embeddings_cache, top_k)
        results = []
        for mem_id, score in similar:
            memcube = self.storage.retrieve(mem_id)
            if memcube:
                results.append((memcube, score))
        return results

    def retrieve_keyword(self, keywords: List[str], top_k: int = 10,
                         filters: Optional[Dict] = None) -> List[MemCube]:
        memory_ids = self.storage.list_memories(filters)
        results = []
        for mem_id in memory_ids:
            memcube = self.storage.retrieve(mem_id)
            if memcube and memcube.payload:
                content = str(memcube.payload.content).lower()
                if all(kw.lower() in content for kw in keywords):
                    results.append(memcube)
                    if len(results) >= top_k:
                        break
        return results

    def retrieve_hybrid(self, query: str, keywords: Optional[List[str]] = None,
                        top_k: int = 10, filters: Optional[Dict] = None,
                        semantic_weight: float = 0.7) -> List[Tuple[MemCube, float]]:
        semantic_results = self.retrieve_semantic(query, top_k * 2, filters)
        keyword_results = self.retrieve_keyword(keywords, top_k * 2, filters) if keywords else []

        combined_scores: Dict[str, float] = {}
        for memcube, score in semantic_results:
            combined_scores[memcube.metadata.memory_id] = score * semantic_weight
        for memcube in keyword_results:
            mid = memcube.metadata.memory_id
            combined_scores[mid] = combined_scores.get(mid, 0.0) + (1.0 - semantic_weight)

        sorted_ids = sorted(combined_scores.items(), key=lambda x: x[1], reverse=True)[:top_k]
        results = []
        for mem_id, score in sorted_ids:
            memcube = self.storage.retrieve(mem_id)
            if memcube:
                results.append((memcube, score))
        return results

    def _update_embeddings_cache(self, filters: Optional[Dict] = None):
        memory_ids = self.storage.list_memories(filters)
        with self.lock:
            stale_ids = set(self.embeddings_cache.keys()) - set(memory_ids)
            for mem_id in stale_ids:
                del self.embeddings_cache[mem_id]
            for mem_id in memory_ids:
                if mem_id not in self.embeddings_cache:
                    memcube = self.storage.retrieve(mem_id)
                    if memcube and memcube.metadata.embedding is not None and np.any(memcube.metadata.embedding):
                        self.embeddings_cache[mem_id] = memcube.metadata.embedding
                    elif memcube and memcube.payload:
                        emb = self.embedding_engine.embed(str(memcube.payload.content))
                        memcube.metadata.embedding = emb
                        self.embeddings_cache[mem_id] = emb


# ============================================================================
# MEMORY SCHEDULER
# ============================================================================

class MemoryScheduler:
    def __init__(self, storage: StorageBackend, cache: MemoryCache,
                 lifecycle_manager: LifecycleManager, audit_log: AuditLog):
        self.storage = storage
        self.cache = cache
        self.lifecycle_manager = lifecycle_manager
        self.audit_log = audit_log
        self.priority_queue: List[Tuple[float, str]] = []
        self.lock = threading.Lock()

    def schedule_loading(self, context: str, task_type: str, top_k: int = 10) -> List[MemCube]:
        memory_ids = self.storage.list_memories()
        priority_scores = []
        for mem_id in memory_ids:
            memcube = self.storage.retrieve(mem_id)
            if memcube:
                score = self._calculate_priority(memcube, context, task_type)
                priority_scores.append((score, memcube))
        priority_scores.sort(key=lambda x: x[0], reverse=True)

        loaded = []
        for score, memcube in priority_scores[:top_k]:
            self.cache.put(memcube)
            loaded.append(memcube)
            self._record_access(memcube)
        return loaded

    def schedule_eviction(self):
        evict_candidates = []
        with self.cache.lock:
            for mem_id, memcube in list(self.cache.cache.items()):
                if memcube.should_archive() or memcube.is_expired():
                    evict_candidates.append(memcube)

        for memcube in evict_candidates:
            self.cache.remove(memcube.metadata.memory_id)
            if memcube.is_expired():
                self.lifecycle_manager.transition(memcube, MemoryState.EXPIRED)
            else:
                self.lifecycle_manager.transition(memcube, MemoryState.ARCHIVED)

    def _calculate_priority(self, memcube: MemCube, context: str, task_type: str) -> float:
        base_priority = memcube.get_priority_score()
        task_factor = 1.5 if task_type in memcube.metadata.tags else 1.0
        context_factor = 1.0
        if memcube.payload:
            content = str(memcube.payload.content).lower()
            if context.lower() in content:
                context_factor = 1.3
        return base_priority * task_factor * context_factor

    def _record_access(self, memcube: MemCube):
        memcube.metadata.access_count += 1
        memcube.metadata.last_accessed = datetime.now()
        self.storage.update_metadata(
            memcube.metadata.memory_id,
            {'access_count': memcube.metadata.access_count,
             'last_accessed': memcube.metadata.last_accessed}
        )
        if memcube.metadata.state == MemoryState.GENERATED:
            self.lifecycle_manager.transition(memcube, MemoryState.ACTIVATED)
        elif memcube.metadata.state == MemoryState.ARCHIVED:
            self.lifecycle_manager.transition(memcube, MemoryState.ACTIVATED)


# ============================================================================
# MEMORY API
# ============================================================================

class MemoryAPI:
    def __init__(self, storage_path: str = Config.DEFAULT_STORAGE_PATH):
        self.storage_path = Path(storage_path)
        self.storage_path.mkdir(parents=True, exist_ok=True)

        self.storage = DiskStorage(self.storage_path)
        self.cache = MemoryCache()
        self.audit_log = AuditLog(self.storage_path)
        self.embedding_engine = EmbeddingEngine()
        self.governance = GovernanceEngine()
        self.lifecycle = LifecycleManager(self.storage, self.audit_log)
        self.retrieval = RetrievalEngine(self.storage, self.embedding_engine)
        self.scheduler = MemoryScheduler(self.storage, self.cache, self.lifecycle, self.audit_log)

        self._cleanup_stop = threading.Event()
        self._start_cleanup_thread()

    def stop(self):
        self._cleanup_stop.set()

    def create_memory(self, content: Any, memory_type: MemoryType,
                      owner: str = "system",
                      tags: Optional[Set[str]] = None,
                      access_level: AccessLevel = AccessLevel.PRIVATE,
                      ttl: int = Config.DEFAULT_TTL,
                      priority: Priority = Priority.MEDIUM,
                      format: str = 'text',
                      **kwargs) -> str:
        memory_id = str(uuid.uuid4())
        embedding = self.embedding_engine.embed(str(content))
        metadata = MemoryMetadata(
            memory_id=memory_id,
            memory_type=memory_type,
            created_at=datetime.now(),
            updated_at=datetime.now(),
            last_accessed=datetime.now(),
            state=MemoryState.GENERATED,
            priority=priority,
            ttl=ttl,
            owner=owner,
            access_level=access_level,
            tags=tags or set(),
            embedding=embedding,
            **kwargs
        )
        payload = MemoryPayload(content=content, format=format)
        memcube = MemCube(metadata=metadata, payload=payload)
        self.storage.store(memcube)
        self.cache.put(memcube)
        self.audit_log.log_event(MemoryEvent(
            event_id=str(uuid.uuid4()),
            event_type=EventType.CREATE,
            memory_id=memory_id,
            timestamp=datetime.now(),
            user=owner,
            details={'type': memory_type.name}
        ))
        return memory_id

    def read_memory(self, memory_id: str, user: str = "system") -> Optional[MemCube]:
        memcube = self.cache.get(memory_id)
        if not memcube:
            memcube = self.storage.retrieve(memory_id)
        if not memcube:
            return None
        if not self.governance.check_access(user, memory_id, memcube, 'read'):
            self.audit_log.log_event(MemoryEvent(
                event_id=str(uuid.uuid4()),
                event_type=EventType.ACCESS_DENIED,
                memory_id=memory_id,
                timestamp=datetime.now(),
                user=user,
                details={'reason': 'Permission denied'}
            ))
            return None
        memcube.metadata.access_count += 1
        memcube.metadata.last_accessed = datetime.now()
        self.storage.update_metadata(memory_id, {
            'access_count': memcube.metadata.access_count,
            'last_accessed': memcube.metadata.last_accessed
        })
        self.cache.put(memcube)
        self.audit_log.log_event(MemoryEvent(
            event_id=str(uuid.uuid4()),
            event_type=EventType.READ,
            memory_id=memory_id,
            timestamp=datetime.now(),
            user=user
        ))
        return memcube

    def update_memory(self, memory_id: str, updates: Dict, user: str = "system") -> bool:
        memcube = self.storage.retrieve(memory_id)
        if not memcube: return False
        if not self.governance.check_access(user, memory_id, memcube, 'write'):
            return False
        if 'content' in updates:
            memcube.payload = MemoryPayload(content=updates['content'],
                                            format=updates.get('format', 'text'))
            memcube.metadata.embedding = self.embedding_engine.embed(str(updates['content']))
        for key, value in updates.items():
            if key not in ('content', 'format') and hasattr(memcube.metadata, key):
                setattr(memcube.metadata, key, value)
        memcube.metadata.updated_at = datetime.now()
        memcube.metadata.version += 1
        self.storage.store(memcube)
        self.cache.put(memcube)
        self.audit_log.log_event(MemoryEvent(
            event_id=str(uuid.uuid4()),
            event_type=EventType.UPDATE,
            memory_id=memory_id,
            timestamp=datetime.now(),
            user=user,
            details={'updates': list(updates.keys())}
        ))
        return True

    def delete_memory(self, memory_id: str, user: str = "system") -> bool:
        memcube = self.storage.retrieve(memory_id)
        if not memcube: return False
        if memcube.metadata.owner != user: return False
        self.storage.delete(memory_id)
        self.cache.remove(memory_id)
        self.audit_log.log_event(MemoryEvent(
            event_id=str(uuid.uuid4()),
            event_type=EventType.DELETE,
            memory_id=memory_id,
            timestamp=datetime.now(),
            user=user
        ))
        return True

    def search_memories(self, query: str,
                        keywords: Optional[List[str]] = None,
                        memory_type: Optional[MemoryType] = None,
                        tags: Optional[Set[str]] = None,
                        top_k: int = 10,
                        user: str = "system") -> List[Tuple[MemCube, float]]:
        filters = {}
        if memory_type:
            filters['memory_type'] = memory_type
        results = self.retrieval.retrieve_hybrid(query, keywords, top_k, filters)
        accessible = []
        for memcube, score in results:
            if self.governance.check_access(user, memcube.metadata.memory_id, memcube, 'read'):
                if tags is None or tags.issubset(memcube.metadata.tags):
                    accessible.append((memcube, score))
        return accessible

    def get_memories_by_tags(self, tags: Set[str], user: str = "system") -> List[MemCube]:
        memory_ids = self.storage.list_memories()
        results = []
        for mem_id in memory_ids:
            memcube = self.storage.retrieve(mem_id)
            if memcube and tags.issubset(memcube.metadata.tags):
                if self.governance.check_access(user, mem_id, memcube, 'read'):
                    results.append(memcube)
        return results

    def merge_memories(self, memory_ids: List[str], user: str = "system") -> Optional[str]:
        return self.lifecycle.merge_memories(memory_ids, user)

    def grant_access(self, memory_id: str, user: str, target_user: str):
        memcube = self.storage.retrieve(memory_id)
        if memcube and memcube.metadata.owner == user:
            memcube.metadata.allowed_users.add(target_user)
            self.storage.store(memcube)
            self.governance.grant_access(target_user, memory_id)

    def get_audit_trail(self, memory_id: str, limit: int = 100) -> List[MemoryEvent]:
        return self.audit_log.query_events(memory_id=memory_id, limit=limit)

    def export_memory(self, memory_id: str) -> Optional[Dict]:
        memcube = self.storage.retrieve(memory_id)
        if not memcube: return None
        return {
            'metadata': memcube.metadata.to_dict(),
            'payload': {
                'content': str(memcube.payload.content) if memcube.payload else None,
                'format': memcube.payload.format if memcube.payload else None
            }
        }

    def import_memory(self, data: Dict, user: str = "system") -> str:
        metadata_dict = dict(data['metadata'])
        payload_data = data.get('payload', {}) or {}
        metadata_dict['memory_id'] = str(uuid.uuid4())
        metadata_dict['owner'] = user
        metadata_dict['created_at'] = datetime.now().isoformat()
        metadata_dict['updated_at'] = datetime.now().isoformat()
        metadata_dict['last_accessed'] = datetime.now().isoformat()
        metadata = MemoryMetadata.from_dict(metadata_dict)
        payload = None
        if payload_data.get('content') is not None:
            payload = MemoryPayload(
                content=payload_data['content'],
                format=payload_data.get('format', 'text')
            )
        memcube = MemCube(metadata=metadata, payload=payload)
        self.storage.store(memcube)
        return metadata.memory_id

    def _start_cleanup_thread(self):
        def cleanup_loop():
            while not self._cleanup_stop.is_set():
                if self._cleanup_stop.wait(Config.CLEANUP_INTERVAL):
                    break
                try:
                    self.scheduler.schedule_eviction()
                except Exception as e:
                    logging.error(f"Cleanup error: {e}")
        thread = threading.Thread(target=cleanup_loop, daemon=True)
        thread.start()


# ============================================================================
# CONVENIENCE FUNCTIONS
# ============================================================================

def create_memory_system(storage_path: str = Config.DEFAULT_STORAGE_PATH) -> MemoryAPI:
    return MemoryAPI(storage_path)


# ============================================================================
# EXAMPLE USAGE AND TESTING
# ============================================================================

def example_usage():
    mem_sys = create_memory_system("./demo_memory")

    print("=" * 60)
    print("Unified Extreme Memory Optimization System (UEMOS) Demo")
    print("=" * 60)

    print("\n1. Creating memories...")
    mem_id_1 = mem_sys.create_memory(
        content="AI systems require efficient memory management for long-context reasoning",
        memory_type=MemoryType.PLAINTEXT, owner="user1",
        tags={"ai", "memory", "reasoning"}, priority=Priority.HIGH)
    mem_id_2 = mem_sys.create_memory(
        content="Memory hierarchy enables faster access to frequently used data",
        memory_type=MemoryType.PLAINTEXT, owner="user1",
        tags={"memory", "hierarchy", "performance"}, priority=Priority.HIGH)
    mem_id_3 = mem_sys.create_memory(
        content="Knowledge graphs represent structured relationships between entities",
        memory_type=MemoryType.PLAINTEXT, owner="user2",
        tags={"knowledge", "graph", "entities"}, priority=Priority.MEDIUM)
    print(f"   Created: {mem_id_1[:8]}, {mem_id_2[:8]}, {mem_id_3[:8]}")

    print("\n2. Reading memories...")
    mem_1 = mem_sys.read_memory(mem_id_1, "user1")
    assert mem_1 is not None
    print(f"   State: {mem_1.metadata.state.name}")

    print("\n3. Searching memories...")
    results = mem_sys.search_memories("memory systems", tags={"memory"}, top_k=5, user="user1")
    print(f"   Found {len(results)} memories")

    print("\n4. Updating memory...")
    assert mem_sys.update_memory(
        mem_id_1,
        {"content": "AI systems require efficient memory management for reasoning and learning",
         "tags": {"ai", "memory", "reasoning", "learning"}},
        "user1")

    print("\n5. Access control...")
    mem_sys.grant_access(mem_id_1, "user1", "user2")
    assert mem_sys.read_memory(mem_id_1, "user2") is not None

    print("\n6. Merging memories...")
    merged_id = mem_sys.merge_memories([mem_id_1, mem_id_2], "user1")
    assert merged_id is not None

    print("\n7. Audit trail...")
    events = mem_sys.get_audit_trail(mem_id_1)
    print(f"   {len(events)} events")

    print("\n8. Export/import...")
    exported = mem_sys.export_memory(mem_id_1)
    assert exported is not None
    imported_id = mem_sys.import_memory(exported, "user3")
    assert imported_id != mem_id_1

    print("\n9. Statistics...")
    all_memories = mem_sys.storage.list_memories()
    print(f"   Total: {len(all_memories)}  Cache: {len(mem_sys.cache.cache)}")

    print("\n10. Filter by type...")
    plaintext = mem_sys.storage.list_memories({'memory_type': MemoryType.PLAINTEXT})
    print(f"   Plaintext: {len(plaintext)}")

    mem_sys.stop()
    print("\n" + "=" * 60)
    print("Demo completed successfully!")
    print("=" * 60)


def benchmark_performance():
    mem_sys = create_memory_system("./benchmark_memory")

    print("\n" + "=" * 60)
    print("Performance Benchmark")
    print("=" * 60)

    n_memories = 200
    print(f"\nCreating {n_memories} memories...")
    start = time.time()
    for i in range(n_memories):
        mem_sys.create_memory(
            content=f"Memory content {i}: benchmark data",
            memory_type=MemoryType.PLAINTEXT, owner="bench",
            tags={f"tag_{i % 10}", "benchmark"}, priority=Priority.MEDIUM)
    create_time = time.time() - start
    print(f"  Create: {create_time:.2f}s ({n_memories/create_time:.0f} ops/s)")

    all_ids = mem_sys.storage.list_memories()
    print(f"\nReading {len(all_ids)} memories...")
    start = time.time()
    for mid in all_ids:
        mem_sys.read_memory(mid, "bench")
    read_time = time.time() - start
    print(f"  Read: {read_time:.2f}s ({len(all_ids)/read_time:.0f} ops/s)")

    print(f"\nSearching...")
    start = time.time()
    results = mem_sys.search_memories("memory content", top_k=10, user="bench")
    search_time = time.time() - start
    print(f"  Search: {search_time*1000:.1f}ms  Results: {len(results)}")

    mem_sys.stop()
    print("\n" + "=" * 60)


if __name__ == "__main__":
    example_usage()
    benchmark_performance()
