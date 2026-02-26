# Contributing to RawrXD

**Thank you for your interest in contributing to the Cursor-killer!** 🦖

We welcome contributions from developers who believe AI should be local, fast, and free.

---

## 🎯 Ways to Contribute

### **1. Code Contributions**
- Fix bugs or implement new features
- Improve performance and optimization
- Add support for new model formats
- Enhance UI/UX components

### **2. Documentation**
- Improve README and guides
- Add code examples and tutorials
- Create video demos and screenshots
- Translate documentation

### **3. Testing**
- Report bugs with reproducible steps
- Test on different platforms (Windows/Linux/macOS)
- Benchmark performance with various models
- Validate GPU acceleration

### **4. Community**
- Help other users in Issues and Discussions
- Share your RawrXD setup and configurations
- Create plugins and extensions
- Write blog posts or tutorials

---

## 🚀 Getting Started

### **Prerequisites**
```powershell
# Windows
winget install Kitware.CMake
winget install Microsoft.VisualStudio.2022.BuildTools
winget install --id=TheQtCompany.Qt.6 -e

# Linux
sudo apt install build-essential cmake qt6-base-dev libvulkan-dev

# macOS
brew install cmake qt@6 vulkan-sdk
```

### **Fork and Clone**
```bash
# 1. Fork the repository on GitHub
# 2. Clone your fork
git clone https://github.com/YOUR_USERNAME/RawrXD.git
cd RawrXD

# 3. Add upstream remote
git remote add upstream https://github.com/ItsMehRAWRXD/RawrXD.git

# 4. Create a feature branch
git checkout -b feature/amazing-feature
```

### **Build and Test**
```powershell
# Build the project
cd RawrXD-ModelLoader
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Run tests
.\build\Release\benchmark_completions.exe --model path\to\model.gguf

# Run the IDE
.\build\Release\RawrXD-AgenticIDE.exe
```

---

## 📝 Contribution Workflow

### **1. Create an Issue**
Before starting work, create an issue describing:
- **Problem**: What bug or limitation exists?
- **Solution**: How do you plan to fix it?
- **Impact**: What does this change improve?

### **2. Develop Your Changes**
```bash
# Stay up-to-date with main branch
git fetch upstream
git rebase upstream/production-lazy-init

# Make your changes
# - Follow coding standards (see below)
# - Add tests for new features
# - Update documentation
```

### **3. Test Thoroughly**
```powershell
# Run all tests
cmake --build build --target benchmark_completions
.\build\Release\benchmark_completions.exe

# Verify no regressions
# - Check build succeeds on clean system
# - Test with multiple GGUF models
# - Validate GPU acceleration works
```

### **4. Commit with Convention**
```bash
# Use conventional commit messages
git commit -m "feat: Add support for Phi-3 models"
git commit -m "fix: Resolve memory leak in tokenizer"
git commit -m "docs: Update build instructions for Linux"
git commit -m "perf: Optimize GPU batch processing"

# Commit types:
# - feat: New feature
# - fix: Bug fix
# - docs: Documentation only
# - perf: Performance improvement
# - refactor: Code restructuring
# - test: Adding tests
# - chore: Maintenance tasks
```

### **5. Push and Create Pull Request**
```bash
# Push to your fork
git push origin feature/amazing-feature

# Open a Pull Request with:
# - Clear title and description
# - Reference related issues (#123)
# - Include benchmark results
# - Add screenshots/videos if UI change
```

---

## 🎨 Coding Standards

### **C++ Style**
```cpp
// Use modern C++20 features
// - Use auto where type is obvious
// - Prefer smart pointers (std::unique_ptr, std::shared_ptr)
// - Use std::optional for nullable values
// - Range-based for loops where possible

// Example: Good code style
class InferenceEngine {
public:
    // Clear, descriptive names
    bool loadModel(const QString& modelPath);
    
    // Use std::optional for nullable returns
    std::optional<QString> getModelName() const;
    
    // Smart pointers for ownership
    std::unique_ptr<Tokenizer> createTokenizer(TokenizerType type);
    
private:
    // Member variables with m_ prefix
    std::unique_ptr<Model> m_model;
    mutable std::mutex m_mutex;  // mutable for const methods
};

// Example: Token generation with modern C++
std::vector<int> generateTokens(const QString& prompt, int maxTokens) {
    std::vector<int> tokens;
    tokens.reserve(maxTokens);  // Reserve capacity
    
    // Range-based loop
    for (const auto& token : tokenize(prompt)) {
        tokens.push_back(token);
        if (tokens.size() >= maxTokens) break;
    }
    
    return tokens;  // Return value optimization (RVO)
}
```

### **Qt Conventions**
```cpp
// Use Qt signals/slots naming
class CompletionEngine : public QObject {
    Q_OBJECT

public:
    explicit CompletionEngine(QObject* parent = nullptr);

signals:
    void completionReady(const QString& text);
    void errorOccurred(const QString& message);

public slots:
    void generateCompletion(const QString& prompt);

private:
    // Qt types when interfacing with Qt APIs
    QString m_modelPath;
    QVector<int> m_tokenCache;
};
```

### **Documentation**
```cpp
/**
 * @brief Generates text completions using the loaded GGUF model
 * 
 * @param prompt The input text to complete
 * @param maxTokens Maximum number of tokens to generate
 * @param temperature Sampling temperature (0.0-1.0), higher = more random
 * @return Generated completion text, or empty string on error
 * 
 * @note Requires a model to be loaded via loadModel() first
 * @throws std::runtime_error if model not loaded or generation fails
 * 
 * @example
 * engine->loadModel("phi-3-mini.gguf");
 * QString result = engine->generate("def fibonacci(n):", 50, 0.7f);
 */
QString generate(const QString& prompt, int maxTokens = 50, float temperature = 0.7f);
```

---

## 🧪 Testing Requirements

### **Unit Tests**
Every new feature must include tests:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "inference_engine.h"

TEST_CASE("InferenceEngine::loadModel", "[inference]") {
    InferenceEngine engine;
    
    SECTION("Load valid GGUF model") {
        REQUIRE(engine.loadModel("test-model.gguf"));
        REQUIRE(engine.isModelLoaded());
    }
    
    SECTION("Fail on non-existent model") {
        REQUIRE_FALSE(engine.loadModel("nonexistent.gguf"));
    }
}
```

### **Benchmark Tests**
Performance-critical changes require benchmarks:

```cpp
#include "benchmark.h"

BENCHMARK_CASE("Token generation latency", "[performance]") {
    InferenceEngine engine;
    engine.loadModel("phi-3-mini.gguf");
    
    auto start = std::chrono::high_resolution_clock::now();
    engine.generate("test prompt", 10);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    REQUIRE(latency.count() < 100);  // Must complete in <100ms
}
```

---

## 🔍 Code Review Process

### **What Reviewers Look For**
1. **Correctness**: Does the code work as intended?
2. **Performance**: Are there optimization opportunities?
3. **Safety**: Are there memory leaks or race conditions?
4. **Style**: Does it follow project conventions?
5. **Tests**: Are changes adequately tested?
6. **Documentation**: Are APIs documented?

### **Review Timeline**
- **Simple fixes**: 1-2 days
- **New features**: 3-7 days
- **Major changes**: 1-2 weeks

### **Addressing Feedback**
```bash
# Make requested changes
git add .
git commit -m "refactor: Address review feedback"

# Force push if you rebased
git push origin feature/amazing-feature --force-with-lease
```

---

## 🎯 Feature Requests

### **Before Submitting**
1. Check existing issues - maybe someone already requested it
2. Consider if it aligns with RawrXD's goals (local, fast, free)
3. Think about implementation complexity

### **Template**
```markdown
**Feature Description**
A clear description of the feature

**Use Case**
Why is this useful? Who benefits?

**Proposed Implementation**
How might this work technically?

**Alternatives Considered**
What other approaches exist?

**Performance Impact**
Will this affect latency or resource usage?
```

---

## 🐛 Bug Reports

### **Template**
```markdown
**Bug Description**
A clear description of the bug

**Reproduction Steps**
1. Load model "model.gguf"
2. Generate completion with prompt "..."
3. Observe error

**Expected Behavior**
What should happen?

**Actual Behavior**
What actually happens?

**Environment**
- OS: Windows 11 23H2
- GPU: RTX 3070
- Qt Version: 6.7.3
- Model: phi-3-mini-4k-q4.gguf

**Logs/Screenshots**
Include relevant logs or screenshots
```

---

## 📜 License

By contributing to RawrXD, you agree that your contributions will be licensed under the MIT License.

**This means:**
- ✅ Anyone can use your code freely
- ✅ Commercial use is allowed
- ✅ Modifications are permitted
- ✅ Distribution is unrestricted

---

## 🏆 Recognition

Contributors are recognized in:
- **README.md** - Listed in acknowledgments section
- **AUTHORS** file - Permanent record of contributors
- **Release notes** - Highlighted for significant contributions
- **GitHub insights** - Contributor graphs and statistics

---

## 💬 Communication

### **Where to Ask Questions**
- **GitHub Issues**: Bug reports and feature requests
- **GitHub Discussions**: General questions and community chat
- **Discord** (coming soon): Real-time community support

### **Response Times**
- **Critical bugs**: Within 24 hours
- **Feature requests**: 3-7 days
- **Pull requests**: 1-2 weeks
- **General questions**: As availability permits

---

## 🎓 Learning Resources

### **Understanding the Codebase**
1. Start with [BUILD_VICTORY_SUMMARY.md](BUILD_VICTORY_SUMMARY.md)
2. Read [ARTIFACT_VERIFICATION.md](ARTIFACT_VERIFICATION.md)
3. Review [RawrXD-ModelLoader/README.md](RawrXD-ModelLoader/README.md)
4. Study key headers in `include/`

### **GGML and GGUF**
- [GGML Documentation](https://github.com/ggerganov/ggml)
- [GGUF Specification](https://github.com/ggerganov/ggml/blob/master/docs/gguf.md)
- [llama.cpp Examples](https://github.com/ggerganov/llama.cpp/tree/master/examples)

### **Qt6 Development**
- [Qt6 Documentation](https://doc.qt.io/qt-6/)
- [Qt Creator IDE](https://www.qt.io/product/development-tools)
- [Qt Examples](https://doc.qt.io/qt-6/qtexamples.html)

---

## 🚀 Next Steps

1. **Star the repository** ⭐ to show support
2. **Fork and clone** the code
3. **Join Discussions** to introduce yourself
4. **Pick an issue** labeled `good-first-issue`
5. **Submit your first PR** - we're excited to see your work!

---

<div align="center">

**Together, we're building the future of local AI development!** 🦖

[Back to Top](#contributing-to-rawrxd)

</div>
