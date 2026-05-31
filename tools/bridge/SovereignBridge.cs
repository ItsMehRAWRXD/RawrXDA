/*
 * SovereignBridge.cs - C# Wrapper for Native Bridge
 * ==================================================
 * Provides managed access to the Sovereign IDE native core
 */

using System;
using System.Runtime.InteropServices;
using System.Text;

namespace RawrXD.Bridge
{
    /// <summary>
    /// Thinking effort levels for AI processing
    /// </summary>
    public enum ThinkingLevel
    {
        Off = 0,    // No thinking, direct output
        Low = 1,    // Minimal computation
        Medium = 2, // Balanced (default)
        High = 3,   // Detailed analysis
        Extra = 4,  // Exhaustive exploration
        Max = 5     // Full depth, no limits
    }

    /// <summary>
    /// Native bridge to the Sovereign IDE C core
    /// </summary>
    public class SovereignEditor : IDisposable
    {
        private IntPtr _handle;
        private bool _disposed;

        #region Native Imports

        [DllImport("sovereign_bridge.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr Bridge_CreateEditor();

        [DllImport("sovereign_bridge.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern void Bridge_DestroyEditor(IntPtr handle);

        [DllImport("sovereign_bridge.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int Bridge_GetEditorText(IntPtr handle, byte[] buffer, int size);

        [DllImport("sovereign_bridge.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern void Bridge_SetEditorText(IntPtr handle, string text);

        [DllImport("sovereign_bridge.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int Bridge_GetEditorLength(IntPtr handle);

        [DllImport("sovereign_bridge.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int Bridge_GetCursorPosition(IntPtr handle);

        [DllImport("sovereign_bridge.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern void Bridge_SetCursorPosition(IntPtr handle, int pos);

        [DllImport("sovereign_bridge.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern void Bridge_ExecuteThinkingCommand(IntPtr handle, string cmd, int level);

        [DllImport("sovereign_bridge.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int Bridge_GetThinkingResult(IntPtr handle, byte[] buffer, int size);

        [DllImport("sovereign_bridge.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int Bridge_GetRecommendedThinkingLevel(IntPtr handle, string task, double importance);

        [DllImport("sovereign_bridge.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int Bridge_LoadExtension(IntPtr handle, string path);

        [DllImport("sovereign_bridge.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int Bridge_ExecuteExtension(IntPtr handle, string extName, string func);

        [DllImport("sovereign_bridge.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int Bridge_GetExtensionCount(IntPtr handle);

        [DllImport("sovereign_bridge.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int Bridge_GetExtensionName(IntPtr handle, int index, byte[] buffer, int size);

        [DllImport("sovereign_bridge.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern void Bridge_IndexFile(IntPtr handle, string path);

        [DllImport("sovereign_bridge.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int Bridge_QueryVectorStore(IntPtr handle, string query, byte[] results, int size, int maxResults);

        [DllImport("sovereign_bridge.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int Bridge_ApplyDiff(IntPtr handle, string diffText);

        [DllImport("sovereign_bridge.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int Bridge_OpenFile(IntPtr handle, string path);

        [DllImport("sovereign_bridge.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int Bridge_SaveFile(IntPtr handle);

        [DllImport("sovereign_bridge.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int Bridge_IsDirty(IntPtr handle);

        [DllImport("sovereign_bridge.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr Bridge_GetVersion();

        #endregion

        /// <summary>
        /// Creates a new Sovereign editor instance
        /// </summary>
        public SovereignEditor()
        {
            _handle = Bridge_CreateEditor();
            if (_handle == IntPtr.Zero)
            {
                throw new InvalidOperationException("Failed to create Sovereign editor");
            }
        }

        /// <summary>
        /// Gets the editor text content
        /// </summary>
        public string Text
        {
            get
            {
                EnsureNotDisposed();
                byte[] buffer = new byte[65536];
                int len = Bridge_GetEditorText(_handle, buffer, buffer.Length);
                return Encoding.UTF8.GetString(buffer, 0, len);
            }
            set
            {
                EnsureNotDisposed();
                Bridge_SetEditorText(_handle, value);
            }
        }

        /// <summary>
        /// Gets the text length
        /// </summary>
        public int Length
        {
            get
            {
                EnsureNotDisposed();
                return Bridge_GetEditorLength(_handle);
            }
        }

        /// <summary>
        /// Gets or sets the cursor position
        /// </summary>
        public int CursorPosition
        {
            get
            {
                EnsureNotDisposed();
                return Bridge_GetCursorPosition(_handle);
            }
            set
            {
                EnsureNotDisposed();
                Bridge_SetCursorPosition(_handle, value);
            }
        }

        /// <summary>
        /// Gets whether the document has unsaved changes
        /// </summary>
        public bool IsDirty
        {
            get
            {
                EnsureNotDisposed();
                return Bridge_IsDirty(_handle) != 0;
            }
        }

        /// <summary>
        /// Gets the Sovereign IDE version
        /// </summary>
        public static string Version
        {
            get
            {
                IntPtr ptr = Bridge_GetVersion();
                return Marshal.PtrToStringAnsi(ptr);
            }
        }

        /// <summary>
        /// Opens a file in the editor
        /// </summary>
        public void OpenFile(string path)
        {
            EnsureNotDisposed();
            if (string.IsNullOrEmpty(path))
                throw new ArgumentException("Path cannot be null or empty", nameof(path));

            int result = Bridge_OpenFile(_handle, path);
            if (result != 0)
                throw new InvalidOperationException($"Failed to open file: {path}");
        }

        /// <summary>
        /// Saves the current file
        /// </summary>
        public void SaveFile()
        {
            EnsureNotDisposed();
            int result = Bridge_SaveFile(_handle);
            if (result != 0)
                throw new InvalidOperationException("Failed to save file");
        }

        /// <summary>
        /// Executes a command with thinking effort
        /// </summary>
        public void ExecuteWithThinking(string command, ThinkingLevel level = ThinkingLevel.Medium)
        {
            EnsureNotDisposed();
            if (string.IsNullOrEmpty(command))
                throw new ArgumentException("Command cannot be null or empty", nameof(command));

            Bridge_ExecuteThinkingCommand(_handle, command, (int)level);
        }

        /// <summary>
        /// Gets the recommended thinking level for a task
        /// </summary>
        public ThinkingLevel GetRecommendedLevel(string task, double importance = 0.5)
        {
            EnsureNotDisposed();
            int level = Bridge_GetRecommendedThinkingLevel(_handle, task, importance);
            return (ThinkingLevel)level;
        }

        /// <summary>
        /// Gets the last thinking result
        /// </summary>
        public string GetThinkingResult()
        {
            EnsureNotDisposed();
            byte[] buffer = new byte[4096];
            int len = Bridge_GetThinkingResult(_handle, buffer, buffer.Length);
            return Encoding.UTF8.GetString(buffer, 0, len);
        }

        /// <summary>
        /// Loads an extension
        /// </summary>
        public void LoadExtension(string path)
        {
            EnsureNotDisposed();
            if (string.IsNullOrEmpty(path))
                throw new ArgumentException("Path cannot be null or empty", nameof(path));

            int result = Bridge_LoadExtension(_handle, path);
            if (result != 0)
                throw new InvalidOperationException($"Failed to load extension: {path}");
        }

        /// <summary>
        /// Executes an extension function
        /// </summary>
        public void ExecuteExtension(string extensionName, string functionName)
        {
            EnsureNotDisposed();
            if (string.IsNullOrEmpty(extensionName))
                throw new ArgumentException("Extension name cannot be null or empty", nameof(extensionName));
            if (string.IsNullOrEmpty(functionName))
                throw new ArgumentException("Function name cannot be null or empty", nameof(functionName));

            int result = Bridge_ExecuteExtension(_handle, extensionName, functionName);
            if (result != 0)
                throw new InvalidOperationException($"Failed to execute {extensionName}.{functionName}");
        }

        /// <summary>
        /// Gets the number of loaded extensions
        /// </summary>
        public int ExtensionCount
        {
            get
            {
                EnsureNotDisposed();
                return Bridge_GetExtensionCount(_handle);
            }
        }

        /// <summary>
        /// Gets the name of an extension by index
        /// </summary>
        public string GetExtensionName(int index)
        {
            EnsureNotDisposed();
            byte[] buffer = new byte[256];
            int len = Bridge_GetExtensionName(_handle, index, buffer, buffer.Length);
            return Encoding.UTF8.GetString(buffer, 0, len);
        }

        /// <summary>
        /// Indexes a file for RAG
        /// </summary>
        public void IndexFile(string path)
        {
            EnsureNotDisposed();
            if (string.IsNullOrEmpty(path))
                throw new ArgumentException("Path cannot be null or empty", nameof(path));

            Bridge_IndexFile(_handle, path);
        }

        /// <summary>
        /// Queries the vector store
        /// </summary>
        public string QueryVectorStore(string query, int maxResults = 10)
        {
            EnsureNotDisposed();
            if (string.IsNullOrEmpty(query))
                throw new ArgumentException("Query cannot be null or empty", nameof(query));

            byte[] buffer = new byte[65536];
            int len = Bridge_QueryVectorStore(_handle, query, buffer, buffer.Length, maxResults);
            return Encoding.UTF8.GetString(buffer, 0, len);
        }

        /// <summary>
        /// Applies a unified diff
        /// </summary>
        public void ApplyDiff(string diffText)
        {
            EnsureNotDisposed();
            if (string.IsNullOrEmpty(diffText))
                throw new ArgumentException("Diff text cannot be null or empty", nameof(diffText));

            int result = Bridge_ApplyDiff(_handle, diffText);
            if (result != 0)
                throw new InvalidOperationException("Failed to apply diff");
        }

        /// <summary>
        /// Disposes the editor
        /// </summary>
        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (!_disposed)
            {
                if (_handle != IntPtr.Zero)
                {
                    Bridge_DestroyEditor(_handle);
                    _handle = IntPtr.Zero;
                }
                _disposed = true;
            }
        }

        private void EnsureNotDisposed()
        {
            if (_disposed)
                throw new ObjectDisposedException(nameof(SovereignEditor));
        }

        ~SovereignEditor()
        {
            Dispose(false);
        }
    }

    /// <summary>
    /// Event arguments for thinking process updates
    /// </summary>
    public class ThinkingEventArgs : EventArgs
    {
        public string Step { get; set; }
        public double Confidence { get; set; }
        public int Iteration { get; set; }
        public ThinkingLevel Level { get; set; }
    }

    /// <summary>
    /// Extension information
    /// </summary>
    public class ExtensionInfo
    {
        public string Name { get; set; }
        public string Path { get; set; }
        public bool IsLoaded { get; set; }
        public bool IsSandboxed { get; set; }
    }

    /// <summary>
    /// Search result from vector store
    /// </summary>
    public class VectorSearchResult
    {
        public string Name { get; set; }
        public string Path { get; set; }
        public double Score { get; set; }
    }
}