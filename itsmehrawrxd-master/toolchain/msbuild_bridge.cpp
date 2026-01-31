// This module uses a process manager to call the MSBuild executable.
class MSBuildBridge {
public:
    BuildResult invoke(const std::string& project_file, const std::string& target) {
        Process msbuild_proc;
        msbuild_proc.setExecutable("C:\\Program Files\\Microsoft Visual Studio\\...\\MSBuild\\Current\\Bin\\MSBuild.exe");
        msbuild_proc.setArguments({project_file, "/t:" + target});
        
        BuildResult result = msbuild_proc.run();
        
        // Capture and parse the output for errors, warnings, etc.
        // We can use a regex engine written in our bootstrapped C++ to do this.
        if (!result.success) {
            parseOutputForErrors(result.output);
        }
        
        return result;
    }
};
