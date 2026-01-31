import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.locks.ReadWriteLock;
import java.util.concurrent.locks.ReentrantReadWriteLock;
import java.util.function.Function;

public class StateManager<K, V> {
    private final ConcurrentHashMap<K, V> store = new ConcurrentHashMap<>();
    private final ReadWriteLock lock = new ReentrantReadWriteLock();
    
    public V get(K key) {
        lock.readLock().lock();
        try { return store.get(key); } 
        finally { lock.readLock().unlock(); }
    }
    
    public void put(K key, V value) {
        lock.writeLock().lock();
        try { store.put(key, value); } 
        finally { lock.writeLock().unlock(); }
    }
    
    public V compute(K key, Function<V, V> updater) {
        lock.writeLock().lock();
        try { return store.compute(key, (k, v) -> updater.apply(v)); } 
        finally { lock.writeLock().unlock(); }
    }
    
    public void clear() {
        lock.writeLock().lock();
        try { store.clear(); } 
        finally { lock.writeLock().unlock(); }
    }
}