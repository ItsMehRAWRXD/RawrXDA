import java.io.File;
import java.io.IOException;
import java.util.List;
import org.gradle.tooling.GradleConnector;
import org.gradle.tooling.ProjectConnection;
import org.gradle.tooling.BuildLauncher;
import org.gradle.tooling.model.GradleProject;

public class GradleManager {
    private final File projectDir;
    private ProjectConnection connection;

    public GradleManager(File projectDir) {
        this.projectDir = projectDir;
        connect();
    }

    private void connect() {
        connection = GradleConnector.newConnector()
            .forProjectDirectory(projectDir)
            .connect();
    }

    public void build() throws IOException {
        BuildLauncher build = connection.newBuild();
        build.forTasks("build");
        build.run();
    }

    public void clean() throws IOException {
        BuildLauncher build = connection.newBuild();
        build.forTasks("clean");
        build.run();
    }

    public void test() throws IOException {
        BuildLauncher build = connection.newBuild();
        build.forTasks("test");
        build.run();
    }

    public GradleProject getProjectModel() {
        return connection.getModel(GradleProject.class);
    }

    public void close() {
        if (connection != null) connection.close();
    }
}
