import java.util.prefs.Preferences;

/**
 * Manages global IDE settings using the Java Preferences API.
 * This provides a simple and persistent way to store user settings.
 */
public class IDESettings {

    private static final String SANDBOX_ENABLED_KEY = "isSandboxEnabled";
    private final Preferences prefs;

    public IDESettings() {
        // Preferences node for our IDE
        this.prefs = Preferences.userNodeForPackage(IDESettings.class);
    }

    /**
     * Checks if the plugin sandbox is enabled.
     * Defaults to true (secure mode) if not set.
     * @return true if sandboxing is enabled, false otherwise.
     */
    public boolean isSandboxEnabled() {
        return prefs.getBoolean(SANDBOX_ENABLED_KEY, true);
    }

    /**
     * Sets the plugin sandbox mode.
     * @param enabled true to enable the sandbox (secure mode), false to disable it (unsafe mode).
     */
    public void setSandboxEnabled(boolean enabled) {
        prefs.putBoolean(SANDBOX_ENABLED_KEY, enabled);
    }
}
