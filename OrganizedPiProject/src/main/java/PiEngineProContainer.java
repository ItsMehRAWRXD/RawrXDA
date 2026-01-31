public class PiEngineProContainer {
    
    // π-Engine Pro: Ultimate container isolation
    static ExecutionResult executeInContainer(String language, String source) throws Exception {
        String[] dockerCmd = {
            "docker", "run", "--rm", 
            "--memory=256m", "--cpus=0.5", "--timeout=60s",
            "--network=none", "--read-only", 
            "-v", WORKSPACE + ":/workspace:rw",
            "pi-engine-sandbox:" + language,
            "/workspace/run.sh"
        };
        
        return executeCommand(dockerCmd);
    }
    
    // Container health check
    static boolean isContainerReady(String language) {
        try {
            Process p = new ProcessBuilder("docker", "image", "inspect", "pi-engine-sandbox:" + language).start();
            return p.waitFor() == 0;
        } catch (Exception e) { return false; }
    }
}