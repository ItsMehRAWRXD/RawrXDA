import java.io.*;
import java.util.*;
import java.util.prefs.Preferences;

public class ExtensionManager {
    private static final String EXTENSIONS_KEY = "ide_extensions";
    private Preferences prefs = Preferences.userNodeForPackage(ExtensionManager.class);
    private Map<String, Extension> extensions = new HashMap<>();
    
    public void saveExtension(String name, String code) {
        extensions.put(name, new Extension(name, code));
        saveToPrefs();
    }
    
    public String loadExtension(String name) {
        Extension ext = extensions.get(name);
        return ext != null ? ext.code : null;
    }
    
    public Set<String> getExtensionNames() {
        return extensions.keySet();
    }
    
    private void saveToPrefs() {
        try {
            ByteArrayOutputStream baos = new ByteArrayOutputStream();
            ObjectOutputStream oos = new ObjectOutputStream(baos);
            oos.writeObject(extensions);
            prefs.putByteArray(EXTENSIONS_KEY, baos.toByteArray());
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
    
    @SuppressWarnings("unchecked")
    public void loadFromPrefs() {
        byte[] data = prefs.getByteArray(EXTENSIONS_KEY, null);
        if (data != null) {
            try {
                ObjectInputStream ois = new ObjectInputStream(new ByteArrayInputStream(data));
                extensions = (Map<String, Extension>) ois.readObject();
            } catch (Exception e) {
                extensions = new HashMap<>();
            }
        }
    }
    
    private static class Extension implements Serializable {
        String name;
        String code;
        
        Extension(String name, String code) {
            this.name = name;
            this.code = code;
        }
    }
}