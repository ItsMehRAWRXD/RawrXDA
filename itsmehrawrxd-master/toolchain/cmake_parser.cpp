// This parser understands CMakeLists.txt files.
class CMakeParser {
public:
    Project parse(const std::string& cmake_path) {
        Project project;
        FileStream cmake_file(cmake_path);
        
        std::string line;
        while (cmake_file.readLine(line)) {
            // Find directives like 'add_executable'.
            if (line.find("add_executable") != std::string::npos) {
                // Parse out the target name and source files.
                // ...
                project.addExecutable(target_name, source_files);
            }
        }
        return project;
    }
};
