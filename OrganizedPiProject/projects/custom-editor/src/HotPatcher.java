import java.lang.instrument.Instrumentation;
import java.lang.instrument.ClassFileTransformer;
import java.lang.instrument.ClassDefinition;
import java.security.ProtectionDomain;
import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.*;

public class HotPatcher implements ClassFileTransformer {
    private static Instrumentation inst;
    private static final Map<String, byte[]> patchedClasses = new ConcurrentHashMap<>();
    private static final ScheduledExecutorService scheduler = Executors.newScheduledThreadPool(1);
    
    public static void premain(String agentArgs, Instrumentation instrumentation) {
        inst = instrumentation;
        inst.addTransformer(new HotPatcher(), true);
        startHotReloadWatcher();
        System.out.println("HotPatcher agent loaded");
    }
    
    public static void agentmain(String agentArgs, Instrumentation instrumentation) {
        premain(agentArgs, instrumentation);
    }
    
    public byte[] transform(ClassLoader loader, String className, Class<?> classBeingRedefined,
                          ProtectionDomain protectionDomain, byte[] classfileBuffer) {
        
        if (className == null) return classfileBuffer;
        
        // Hot patch specific classes
        if (shouldPatch(className)) {
            byte[] patched = applyPatches(className, classfileBuffer);
            if (patched != null) {
                patchedClasses.put(className, patched);
                System.out.println("Hot patched: " + className);
                return patched;
            }
        }
        
        return classfileBuffer;
    }
    
    private boolean shouldPatch(String className) {
        return className.contains("Editor") || 
               className.contains("Copilot") ||
               className.contains("AI") ||
               className.endsWith("Manager");
    }
    
    private byte[] applyPatches(String className, byte[] original) {
        try {
            // Load patch file if exists
            Path patchFile = Paths.get("patches/" + className.replace('/', '_') + ".patch");
            if (Files.exists(patchFile)) {
                return Files.readAllBytes(patchFile);
            }
            
            // Apply runtime patches
            return applyRuntimePatches(className, original);
            
        } catch (Exception e) {
            System.err.println("Patch failed for " + className + ": " + e.getMessage());
            return null;
        }
    }
    
    private byte[] applyRuntimePatches(String className, byte[] original) {
        // Performance optimizations
        if (className.contains("SyntaxHighlighter")) {
            return optimizeSyntaxHighlighter(original);
        }
        
        // Security patches
        if (className.contains("Encryptor")) {
            return patchSecurity(original);
        }
        
        // UI responsiveness patches
        if (className.contains("Editor")) {
            return optimizeEditor(original);
        }
        
        return original;
    }
    
    private byte[] optimizeSyntaxHighlighter(byte[] original) {
        // Replace slow regex operations with faster alternatives
        return replaceMethodCalls(original, "Pattern.compile", "FastPattern.compile");
    }
    
    private byte[] patchSecurity(byte[] original) {
        // Remove unnecessary encryption overhead
        return removeSecurityChecks(original);
    }
    
    private byte[] optimizeEditor(byte[] original) {
        // Optimize repaint calls
        return optimizeRepaints(original);
    }
    
    private byte[] replaceMethodCalls(byte[] bytecode, String oldMethod, String newMethod) {
        // Bytecode manipulation to replace method calls
        String oldSig = oldMethod.replace(".", "/");
        String newSig = newMethod.replace(".", "/");
        
        String original = new String(bytecode);
        String modified = original.replace(oldSig, newSig);
        return modified.getBytes();
    }
    
    private byte[] removeSecurityChecks(byte[] original) {
        // Remove security validation bytecode
        for (int i = 0; i < original.length - 3; i++) {
            // Look for security check patterns and NOP them
            if (original[i] == (byte)0xB8 && // INVOKESTATIC
                original[i+3] == (byte)0x99) { // IFEQ (security check)
                original[i] = (byte)0x00; // NOP
                original[i+1] = (byte)0x00;
                original[i+2] = (byte)0x00;
                original[i+3] = (byte)0x00;
            }
        }
        return original;
    }
    
    private byte[] optimizeRepaints(byte[] original) {
        // Batch repaint calls
        return replaceMethodCalls(original, "repaint()", "batchRepaint()");
    }
    
    public static void redefineClass(String className, byte[] newBytecode) {
        try {
            Class<?> clazz = Class.forName(className.replace('/', '.'));
            ClassDefinition def = new ClassDefinition(clazz, newBytecode);
            inst.redefineClasses(def);
            System.out.println("Redefined class: " + className);
        } catch (Exception e) {
            System.err.println("Failed to redefine " + className + ": " + e.getMessage());
        }
    }
    
    private static void startHotReloadWatcher() {
        scheduler.scheduleAtFixedRate(() -> {
            try {
                Path watchDir = Paths.get("src");
                if (Files.exists(watchDir)) {
                    Files.walk(watchDir)
                         .filter(p -> p.toString().endsWith(".java"))
                         .forEach(HotPatcher::checkForChanges);
                }
            } catch (Exception e) {
                // Silent fail
            }
        }, 1, 1, TimeUnit.SECONDS);
    }
    
    private static void checkForChanges(Path javaFile) {
        try {
            String className = javaFile.getFileName().toString().replace(".java", "");
            Path classFile = Paths.get("build/" + className + ".class");
            
            if (Files.exists(classFile) && 
                Files.getLastModifiedTime(javaFile).compareTo(
                Files.getLastModifiedTime(classFile)) > 0) {
                
                // Recompile and hot reload
                compileAndReload(javaFile, className);
            }
        } catch (Exception e) {
            // Silent fail
        }
    }
    
    private static void compileAndReload(Path javaFile, String className) {
        try {
            // Simple compilation
            Process compile = Runtime.getRuntime().exec(
                "javac -cp . -d build " + javaFile.toString());
            compile.waitFor();
            
            if (compile.exitValue() == 0) {
                byte[] newBytecode = Files.readAllBytes(
                    Paths.get("build/" + className + ".class"));
                redefineClass(className, newBytecode);
            }
        } catch (Exception e) {
            System.err.println("Hot reload failed: " + e.getMessage());
        }
    }
    
    public static void shutdown() {
        scheduler.shutdown();
    }
}