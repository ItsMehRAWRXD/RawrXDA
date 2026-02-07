@echo off
echo Starting AI System...
javac -cp "picocli-4.7.5.jar:javax.json-1.1.4.jar" *.java
java -cp ".:picocli-4.7.5.jar:javax.json-1.1.4.jar" SystemOrchestrator
pause