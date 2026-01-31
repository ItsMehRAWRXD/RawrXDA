public class PiEngineTest {
    public static void main(String[] args) {
        System.out.println("🧪 π-Engine Component Test");
        System.out.println("=========================");

        // Test PiBeacon
        System.out.println("\n1. Testing PiBeacon:");
        PiBeacon.log("3,14PIZL0G1C test-start");
        System.out.println("   ✓ PiBeacon logging active");

        // Test PiCompiler
        System.out.println("\n2. Testing PiCompiler:");
        try {
            PiCompiler.INSTANCE.generate(java.util.Map.of(
                "source.rawrz", "test data".getBytes(),
                "outName", "test_output.bin"
            ));
            System.out.println("   ✓ PiCompiler generation works");
        } catch (Exception e) {
            System.out.println("   ✗ PiCompiler failed: " + e.getMessage());
        }

        // Test file detection
        System.out.println("\n3. Testing Language Detection:");
        String[] testFiles = {"HelloPi.java", "HelloPi.c", "HelloPi.cs", "HelloPi.asm"};
        for (String file : testFiles) {
            java.nio.file.Path path = java.nio.file.Paths.get(file);
            if (java.nio.file.Files.exists(path)) {
                System.out.println("   ✓ " + file + " exists");
            } else {
                System.out.println("   ✗ " + file + " missing");
            }
        }

        // Test RoslynBox
        System.out.println("\n4. Testing RoslynBox Setup:");
        java.nio.file.Path roslynBox = java.nio.file.Paths.get("RoslynBox");
        if (java.nio.file.Files.exists(roslynBox)) {
            System.out.println("   ✓ RoslynBox directory exists");
            if (java.nio.file.Files.exists(roslynBox.resolve("RoslynBoxEngine.dll"))) {
                System.out.println("   ✓ RoslynBoxEngine.dll found");
            } else {
                System.out.println("   ✗ RoslynBoxEngine.dll missing");
            }
        } else {
            System.out.println("   ✗ RoslynBox directory missing");
        }

        // Test compilers availability
        System.out.println("\n5. Testing Compiler Availability:");
        testCompiler("javac", "Java");
        testCompiler("dotnet", ".NET/C#");

        System.out.println("\n✅ π-Engine Component Test Complete!");
        PiBeacon.log("3,14PIZL0G1C test-complete");
    }

    private static void testCompiler(String cmd, String name) {
        try {
            ProcessBuilder pb = new ProcessBuilder(cmd, "--version");
            Process p = pb.start();
            int exitCode = p.waitFor();
            if (exitCode == 0) {
                System.out.println("   ✓ " + name + " compiler available");
            } else {
                System.out.println("   ✗ " + name + " compiler not responding");
            }
        } catch (Exception e) {
            System.out.println("   ✗ " + name + " compiler not found: " + e.getMessage());
        }
    }
}