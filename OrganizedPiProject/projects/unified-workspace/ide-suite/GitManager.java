import java.io.File;
import java.io.IOException;
import java.util.List;
import org.eclipse.jgit.api.Git;
import org.eclipse.jgit.api.errors.GitAPIException;
import org.eclipse.jgit.diff.DiffEntry;
import org.eclipse.jgit.lib.Repository;
import org.eclipse.jgit.storage.file.FileRepositoryBuilder;

public class GitManager {
	private Git git;

	public GitManager(String repoPath) throws IOException {
		FileRepositoryBuilder builder = new FileRepositoryBuilder();
		Repository repository = builder.setGitDir(new File(repoPath + "/.git"))
			.readEnvironment()
			.findGitDir()
			.build();
		git = new Git(repository);
	}

	public String getStatus() throws GitAPIException {
		return git.status().call().toString();
	}

	public void commit(String message) throws GitAPIException {
		git.commit().setMessage(message).call();
	}

	public void push() throws GitAPIException {
		git.push().call();
	}

	public void pull() throws GitAPIException {
		git.pull().call();
	}

	public List<DiffEntry> getDiff() throws GitAPIException, IOException {
		return git.diff().call();
	}

	public List<String> listBranches() throws GitAPIException {
		return git.branchList().call().stream().map(ref -> ref.getName()).toList();
	}

	public void createBranch(String name) throws GitAPIException {
		git.branchCreate().setName(name).call();
	}

	public void checkoutBranch(String name) throws GitAPIException {
		git.checkout().setName(name).call();
	}
}
