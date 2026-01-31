import java.io.File;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.Map;

/**
 * This class is the entry point for the sandboxed plugin process.
 * It's designed to be called from the main IDE using ProcessBuilder.
 * It sets up a strict security manager, loads the plugin, and executes it.
 */
public class PluginRunner {
    public static void main(String[] args) {
        if (args.length < 2) {
            System.err.println("Usage: PluginRunner <jar-path> <class-name> [input-string]");
            System.exit(1);
        }

        String jarPath = args[0];
        String className = args[1];
        String input = (args.length > 2) ? args[2] : "";

        // The security manager and policy are enabled via JVM arguments by the ProcessBuilder
        // in PluginSandbox.java (-Djava.security.manager -Djava.security.policy=...)

        try (URLClassLoader pluginClassLoader = new URLClassLoader(new URL[]{jarUrl})) {
            File jarFile = new File(jarPath);
            URL jarUrl = jarFile.toURI().toURL();
            
            // Create a class loader for the plugin JAR.

            // Load the plugin class.
            Class<?> pluginClass = pluginClassLoader.loadClass(className);
            PluginSandbox.LLMPlugin plugin = (PluginSandbox.LLMPlugin) pluginClass.getDeclaredConstructor().newInstance();

            // Initialize and process.
            // The config map would be passed via serialized args or a temp file in a real implementation.
            // For now, we pass an empty map.
            plugin.initialize(java.util.Collections.emptyMap());
            String output = plugin.process(input);
            plugin.cleanup();

            // Print the output to standard out, so the main process can read it.
            System.out.println(output);
            System.exit(0);

        } catch (Exception e) {
            // Print any errors to standard error for the main process.
            e.printStackTrace(System.err);
            System.exit(1);
        }
    }
}
