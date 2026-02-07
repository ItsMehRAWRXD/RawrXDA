import javax.tools.*;
import java.io.*;
import java.net.URI;
import java.util.*;
import javax.tools.JavaFileObject.Kind;

public class InMemoryJavaFileManager extends ForwardingJavaFileManager<JavaFileManager> {
    private final Map<String, JavaFileObject> fileObjects = new HashMap<>();

    public InMemoryJavaFileManager(JavaFileManager fileManager) {
        super(fileManager);
    }

    public void addSourceFile(String className, String sourceCode) {
        fileObjects.put(className, new CharSequenceJavaFileObject(className, sourceCode));
    }

    @Override
    public JavaFileObject getJavaFileForOutput(Location location, String className, Kind kind, FileObject sibling) throws IOException {
        if (kind == Kind.CLASS) {
            JavaFileObject fileObject = new ByteArrayJavaFileObject(className);
            fileObjects.put(className, fileObject);
            return fileObject;
        }
        return super.getJavaFileForOutput(location, className, kind, sibling);
    }

    @Override
    public JavaFileObject getJavaFileForInput(Location location, String className, Kind kind) throws IOException {
        if (location == StandardLocation.SOURCE_PATH && fileObjects.containsKey(className)) {
            return fileObjects.get(className);
        }
        return super.getJavaFileForInput(location, className, kind);
    }

    @Override
    public String inferBinaryName(Location location, JavaFileObject file) {
        if (file instanceof CharSequenceJavaFileObject) {
            return ((CharSequenceJavaFileObject) file).getClassName();
        }
        if (file instanceof ByteArrayJavaFileObject) {
            return ((ByteArrayJavaFileObject) file).getClassName();
        }
        return super.inferBinaryName(location, file);
    }

    @Override
    public Iterable<JavaFileObject> list(Location location, String packageName, Set<Kind> kinds, boolean recurse) throws IOException {
        if (location == StandardLocation.SOURCE_PATH) {
            List<JavaFileObject> result = new ArrayList<>();
            for (JavaFileObject fileObject : fileObjects.values()) {
                if (fileObject.getKind() == Kind.SOURCE && getPackageName(fileObject).equals(packageName)) {
                    result.add(fileObject);
                }
            }
            return result;
        }
        return super.list(location, packageName, kinds, recurse);
    }

    private String getPackageName(JavaFileObject fileObject) {
        String className = inferBinaryName(StandardLocation.SOURCE_PATH, fileObject);
        int lastDot = className.lastIndexOf('.');
        return (lastDot == -1) ? "" : className.substring(0, lastDot);
    }
}

class CharSequenceJavaFileObject extends SimpleJavaFileObject {
    private final String className;
    private final CharSequence content;

    public CharSequenceJavaFileObject(String className, CharSequence content) {
        super(URI.create("string:///" + className.replace('.', '/') + Kind.SOURCE.extension), Kind.SOURCE);
        this.className = className;
        this.content = content;
    }

    public String getClassName() {
        return className;
    }

    @Override
    public CharSequence getCharContent(boolean ignoreEncodingErrors) {
        return content;
    }
}

class ByteArrayJavaFileObject extends SimpleJavaFileObject {
    private final String className;
    private ByteArrayOutputStream bos = new ByteArrayOutputStream();

    public ByteArrayJavaFileObject(String name) {
        super(URI.create("bytes:///" + name.replace('.', '/') + Kind.CLASS.extension), Kind.CLASS);
        this.className = name;
    }

    public String getClassName() {
        return className;
    }

    @Override
    public OutputStream openOutputStream() {
        bos = new ByteArrayOutputStream();
        return bos;
    }

    public byte[] getBytes() {
        return bos.toByteArray();
    }
}
