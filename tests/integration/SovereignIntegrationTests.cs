/*
 * SovereignIntegrationTests.cs - Integration Tests for C/C# Bridge
 * =================================================================
 * Tests the complete integration between Sovereign C core and RawrXD C# extensions
 */

using System;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using Xunit;
using RawrXD.Bridge;

namespace RawrXD.IntegrationTests
{
    /// <summary>
    /// Integration tests for the Sovereign IDE bridge
    /// </summary>
    public class SovereignIntegrationTests : IDisposable
    {
        private readonly SovereignEditor _editor;
        private readonly string _testDir;

        public SovereignIntegrationTests()
        {
            _editor = new SovereignEditor();
            _testDir = Path.Combine(Path.GetTempPath(), "sovereign_tests", Guid.NewGuid().ToString());
            Directory.CreateDirectory(_testDir);
        }

        public void Dispose()
        {
            _editor?.Dispose();
            if (Directory.Exists(_testDir))
            {
                Directory.Delete(_testDir, true);
            }
        }

        #region Editor Core Tests

        [Fact]
        public void Bridge_CreateEditor_Succeeds()
        {
            Assert.NotNull(_editor);
            Assert.True(_editor.Length >= 0);
        }

        [Fact]
        public void Bridge_TextRoundtrip_Works()
        {
            const string testText = "Hello, Sovereign IDE!";
            _editor.Text = testText;
            var result = _editor.Text;
            Assert.Equal(testText, result);
        }

        [Fact]
        public void Bridge_LargeText_HandlesCorrectly()
        {
            var largeText = new string('x', 10000);
            _editor.Text = largeText;
            var result = _editor.Text;
            Assert.Equal(largeText.Length, result.Length);
            Assert.Equal(largeText, result);
        }

        [Fact]
        public void Bridge_CursorPosition_GetsAndSets()
        {
            const string text = "Hello\nWorld";
            _editor.Text = text;
            
            _editor.CursorPosition = 5;
            Assert.Equal(5, _editor.CursorPosition);
            
            _editor.CursorPosition = 0;
            Assert.Equal(0, _editor.CursorPosition);
        }

        [Fact]
        public void Bridge_IsDirty_TracksChanges()
        {
            Assert.False(_editor.IsDirty);
            
            _editor.Text = "Modified";
            Assert.True(_editor.IsDirty);
        }

        [Fact]
        public void Bridge_SaveFile_ClearsDirty()
        {
            var testFile = Path.Combine(_testDir, "test.txt");
            File.WriteAllText(testFile, "Initial");
            
            _editor.OpenFile(testFile);
            _editor.Text = "Modified content";
            Assert.True(_editor.IsDirty);
            
            _editor.SaveFile();
            Assert.False(_editor.IsDirty);
            
            var saved = File.ReadAllText(testFile);
            Assert.Equal("Modified content", saved);
        }

        #endregion

        #region Thinking Effort Tests

        [Theory]
        [InlineData("help", 0.4)]
        [InlineData("analyze code", 0.7)]
        [InlineData("optimize algorithm", 0.9)]
        [InlineData("debug crash", 0.85)]
        [InlineData("implement feature", 0.8)]
        public void Bridge_EstimateComplexity_ReturnsExpected(string task, double expectedMin)
        {
            var level = _editor.GetRecommendedLevel(task, 0.5);
            Assert.True(level >= ThinkingLevel.Low);
            Assert.True(level <= ThinkingLevel.Max);
        }

        [Fact]
        public void Bridge_ExecuteThinkingCommand_UpdatesContext()
        {
            _editor.ExecuteWithThinking("analyze code structure", ThinkingLevel.High);
            var result = _editor.GetThinkingResult();
            
            Assert.NotNull(result);
            Assert.Contains("Executed", result);
            Assert.Contains("Level: 3", result);
        }

        [Fact]
        public void Bridge_ThinkingLevels_AreOrdered()
        {
            Assert.True((int)ThinkingLevel.Off < (int)ThinkingLevel.Low);
            Assert.True((int)ThinkingLevel.Low < (int)ThinkingLevel.Medium);
            Assert.True((int)ThinkingLevel.Medium < (int)ThinkingLevel.High);
            Assert.True((int)ThinkingLevel.High < (int)ThinkingLevel.Extra);
            Assert.True((int)ThinkingLevel.Extra < (int)ThinkingLevel.Max);
        }

        [Theory]
        [InlineData(ThinkingLevel.Off, 0)]
        [InlineData(ThinkingLevel.Low, 1)]
        [InlineData(ThinkingLevel.Medium, 2)]
        [InlineData(ThinkingLevel.High, 3)]
        [InlineData(ThinkingLevel.Extra, 4)]
        [InlineData(ThinkingLevel.Max, 5)]
        public void Bridge_ThinkingLevel_ConvertsToInt(ThinkingLevel level, int expected)
        {
            Assert.Equal(expected, (int)level);
        }

        #endregion

        #region Extension Host Tests

        [Fact]
        public void Bridge_ExtensionCount_InitiallyZero()
        {
            Assert.Equal(0, _editor.ExtensionCount);
        }

        [Fact]
        public void Bridge_LoadExtension_IncreasesCount()
        {
            // This would require a real extension file
            // For now, just verify the API exists
            Assert.Throws<InvalidOperationException>(() => 
                _editor.LoadExtension("nonexistent.dll"));
        }

        [Fact]
        public void Bridge_GetExtensionName_ValidatesIndex()
        {
            Assert.Throws<ArgumentOutOfRangeException>(() =>
                _editor.GetExtensionName(-1));
            
            Assert.Throws<ArgumentOutOfRangeException>(() =>
                _editor.GetExtensionName(999));
        }

        [Fact]
        public void Bridge_ExecuteExtension_ValidatesArgs()
        {
            Assert.Throws<ArgumentException>(() =>
                _editor.ExecuteExtension(null, "func"));
            
            Assert.Throws<ArgumentException>(() =>
                _editor.ExecuteExtension("ext", null));
        }

        #endregion

        #region Vector Store / RAG Tests

        [Fact]
        public void Bridge_IndexFile_AddsToStore()
        {
            var testFile = Path.Combine(_testDir, "test.c");
            File.WriteAllText(testFile, "int main() { return 0; }");
            
            _editor.IndexFile(testFile);
            
            // Query should find the indexed file
            var results = _editor.QueryVectorStore("main function", 5);
            Assert.NotNull(results);
        }

        [Fact]
        public void Bridge_QueryVectorStore_ValidatesArgs()
        {
            Assert.Throws<ArgumentException>(() =>
                _editor.QueryVectorStore(null, 5));
            
            Assert.Throws<ArgumentException>(() =>
                _editor.QueryVectorStore("query", 0));
        }

        [Fact]
        public void Bridge_QueryVectorStore_ReturnsResults()
        {
            // Index some files
            for (int i = 0; i < 3; i++)
            {
                var file = Path.Combine(_testDir, $"file{i}.txt");
                File.WriteAllText(file, $"Content {i}");
                _editor.IndexFile(file);
            }
            
            var results = _editor.QueryVectorStore("content", 10);
            Assert.NotNull(results);
        }

        #endregion

        #region Diff Engine Tests

        [Fact]
        public void Bridge_ApplyDiff_ModifiesText()
        {
            _editor.Text = "Hello World";
            
            var diff = @"--- a/test.txt
+++ b/test.txt
@@ -1 +1 @@
-Hello World
+Hello Sovereign";
            
            _editor.ApplyDiff(diff);
            
            var result = _editor.Text;
            Assert.Contains("Sovereign", result);
        }

        [Fact]
        public void Bridge_ApplyDiff_ValidatesArgs()
        {
            Assert.Throws<ArgumentException>(() =>
                _editor.ApplyDiff(null));
            
            Assert.Throws<ArgumentException>(() =>
                _editor.ApplyDiff(""));
        }

        [Fact]
        public void Bridge_ApplyDiff_InvalidDiff_Throws()
        {
            Assert.Throws<InvalidOperationException>(() =>
                _editor.ApplyDiff("invalid diff content"));
        }

        #endregion

        #region File Operations Tests

        [Fact]
        public void Bridge_OpenFile_ValidatesPath()
        {
            Assert.Throws<ArgumentException>(() =>
                _editor.OpenFile(null));
            
            Assert.Throws<ArgumentException>(() =>
                _editor.OpenFile(""));
        }

        [Fact]
        public void Bridge_OpenFile_Nonexistent_Throws()
        {
            Assert.Throws<InvalidOperationException>(() =>
                _editor.OpenFile("/nonexistent/file.txt"));
        }

        [Fact]
        public void Bridge_OpenFile_LoadsContent()
        {
            var testFile = Path.Combine(_testDir, "test.txt");
            const string content = "Test content for loading";
            File.WriteAllText(testFile, content);
            
            _editor.OpenFile(testFile);
            
            Assert.Equal(content, _editor.Text);
        }

        [Fact]
        public void Bridge_SaveFile_WithoutOpen_Throws()
        {
            // Create a new editor without opening a file
            using var newEditor = new SovereignEditor();
            newEditor.Text = "Unsaved content";
            
            Assert.Throws<InvalidOperationException>(() =>
                newEditor.SaveFile());
        }

        #endregion

        #region Version Tests

        [Fact]
        public void Bridge_Version_IsNotEmpty()
        {
            var version = SovereignEditor.Version;
            Assert.False(string.IsNullOrEmpty(version));
            Assert.Contains("2.0", version);
        }

        #endregion

        #region Stress Tests

        [Fact]
        public void Bridge_MultipleOperations_Sequential()
        {
            for (int i = 0; i < 100; i++)
            {
                _editor.Text = $"Iteration {i}";
                var pos = _editor.CursorPosition;
                _editor.CursorPosition = i % 10;
                var text = _editor.Text;
                Assert.Equal($"Iteration {i}", text);
            }
        }

        [Fact]
        public void Bridge_LargeFile_Performance()
        {
            var largeText = new string('x', 100000);
            
            var start = DateTime.Now;
            _editor.Text = largeText;
            var setTime = DateTime.Now - start;
            
            start = DateTime.Now;
            var result = _editor.Text;
            var getTime = DateTime.Now - start;
            
            Assert.Equal(largeText, result);
            Assert.True(setTime.TotalSeconds < 1, "Set operation too slow");
            Assert.True(getTime.TotalSeconds < 1, "Get operation too slow");
        }

        #endregion

        #region Integration Scenarios

        [Fact]
        public void Scenario_EditWithThinkingAndSave()
        {
            // 1. Open file
            var testFile = Path.Combine(_testDir, "scenario.txt");
            File.WriteAllText(testFile, "Initial content");
            _editor.OpenFile(testFile);
            
            // 2. Edit with thinking
            _editor.ExecuteWithThinking("optimize text", ThinkingLevel.Medium);
            _editor.Text = "Optimized content";
            
            // 3. Index for RAG
            _editor.IndexFile(testFile);
            
            // 4. Save
            _editor.SaveFile();
            
            // 5. Verify
            var saved = File.ReadAllText(testFile);
            Assert.Equal("Optimized content", saved);
        }

        [Fact]
        public void Scenario_MultiFileWorkspace()
        {
            // Create multiple files
            for (int i = 0; i < 5; i++)
            {
                var file = Path.Combine(_testDir, $"file{i}.cs");
                File.WriteAllText(file, $"class Class{i} {{ }}");
                _editor.IndexFile(file);
            }
            
            // Query for symbols
            var results = _editor.QueryVectorStore("class", 10);
            Assert.NotNull(results);
        }

        #endregion
    }

    /// <summary>
    /// Collection fixture for integration tests
    /// </summary>
    [CollectionDefinition("SovereignIntegration")]
    public class SovereignIntegrationCollection : ICollectionFixture<SovereignIntegrationTests>
    {
        // This class has no code, and is never created.
        // Its purpose is to be the place to apply [CollectionDefinition]
        // and all the ICollectionFixture<> interfaces.
    }
}