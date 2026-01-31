import java.io.*;
import java.util.Scanner;

public class LaunchIDE {
    public static void main(String[] args) {
        Scanner scanner = new Scanner(System.in);
        
        System.out.println("? IDE Launcher");
        System.out.println("1. Simple Cursor IDE (Java)");
        System.out.println("2. Clean Cursor IDE (Java)");
        System.out.println("3. Secure IDE Java");
        System.out.println("4. Exit");
        System.out.print("Choose IDE (1-4): ");
        
        String choice = scanner.nextLine();
        
        try {
            ProcessBuilder pb;
            switch (choice) {
                case "1":
                    System.out.println("Starting Simple Cursor IDE...");
                    pb = new ProcessBuilder("java", "SimpleCursor");
                    break;
                case "2":
                    System.out.println("Starting Clean Cursor IDE...");
                    pb = new ProcessBuilder("java", "CleanCursor");
                    break;
                case "3":
                    System.out.println("Starting Secure IDE Java...");
                    pb = new ProcessBuilder("java", "SecureIDE");
                    break;
                default:
                    System.out.println("Goodbye!");
                    return;
            }
            
            pb.directory(new File("."));
            pb.inheritIO();
            Process process = pb.start();
            process.waitFor();
            
        } catch (Exception e) {
            System.out.println("Error starting IDE: " + e.getMessage());
        }
    }
}