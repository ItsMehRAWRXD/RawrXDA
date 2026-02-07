import java.io.BufferedReader;
import java.io.File;
import java.io.InputStreamReader;
import java.net.URL;
import java.util.Collections;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.stream.Collectors;

/**
 * Manages and executes LLM plugins in a completely isolated, secure process.
 * This class uses a ProcessBuilder to launch a separate JVM for each plugin execution,
 * ensuring that untrusted plugin code cannot interfere with the main IDE or the underlying system.
 */
public class PluginSandbox {

    // This interface defines the contract for a plugin. 
    // The actual implementation will be loaded and run in a separate process.
    public interface LLMPlugin {
        String getName();
        String process(String input) throws Exception;
        void initialize(Map<String, String> config) throws Exception;
        void cleanup();
    }

    private final Map<String, PluginInfo> plugins = new ConcurrentHashMap<>();
    private final ExecutorService executor = Executors.newCachedThreadPool();
    private final String javaExecutable;
    private final String policyFile;
    private final String pluginRunnerClassPath;


    private static class PluginInfo {
        final String name;
        final URL jarUrl;
        final String className;

        PluginInfo(String name, URL jarUrl, String className) {
            this.name = name;
            this.jarUrl = jarUrl;
            this.className = className;
        }
    }

    public PluginSandbox() {
        // Locate the java executable for the current JVM.
        String javaHome = System.getProperty("java.home");
        this.javaExecutable = new File(javaHome, "bin/java").getAbsolutePath();
        
        // Define the path to the security policy file.
        this.policyFile = new File(System.getProperty("user.dir"), "plugin.policy").getAbsolutePath();
        
        // Define the classpath for the PluginRunner itself.
        this.pluginRunnerClassPath = System.getProperty("user.dir");
    }

    /**
     * Registers a plugin by storing its location and class name. Does not load any code.
     */
    public void loadPlugin(String name, URL pluginJarUrl, String className) {
        plugins.put(name, new PluginInfo(name, pluginJarUrl, className));
    }

    /**
     * Executes a plugin in a separate, sandboxed process.
     * @param name The name of the plugin to execute.
     * @param input The input string to pass to the plugin.
     * @return A CompletableFuture that will complete with the plugin's output or an error message.
     */
    public CompletableFuture<String> executePlugin(String name, String input) {
        PluginInfo pluginInfo = plugins.get(name);
        if (pluginInfo == null) {
            return CompletableFuture.failedFuture(new IllegalArgumentException("Plugin not found: " + name));
        }

        return CompletableFuture.supplyAsync(() -> {
            try {
                String jarPath = new File(pluginInfo.jarUrl.toURI()).getAbsolutePath();
                
                // The classpath for the new process includes the PluginRunner's location and the plugin's JAR.
                String classpath = pluginRunnerClassPath + File.pathSeparator + jarPath;

                ProcessBuilder pb = new ProcessBuilder(
                    javaExecutable,
                    "-Djava.security.manager", // Enable the security manager
                    "-Djava.security.policy==" + policyFile, // Specify our restrictive policy file
                    "-cp", classpath, // Set the classpath
                    "PluginRunner", // The main class to run in the new process
                    jarPath,
                    pluginInfo.className,
                    input
                );

                pb.redirectErrorStream(true); // Merge stdout and stderr
                Process process = pb.start();

                // Read the output from the process
                try (BufferedReader reader = new BufferedReader(new InputStreamReader(process.getInputStream()))) {
                    String output = reader.lines().collect(Collectors.joining(System.lineSeparator()));
                    int exitCode = process.waitFor();
                    if (exitCode == 0) {
                        return output;
                    } else {
                        return "Error: Plugin process exited with code " + exitCode + ". Output:\n" + output;
                    }
                }
            } catch (Exception e) {
                return "Error launching plugin process: " + e.getMessage();
            }
        }, executor);
    }

    public Set<String> getPluginNames() {
        return Collections.unmodifiableSet(plugins.keySet());
    }

    public void unloadPlugin(String name) {
        plugins.remove(name);
    }
}