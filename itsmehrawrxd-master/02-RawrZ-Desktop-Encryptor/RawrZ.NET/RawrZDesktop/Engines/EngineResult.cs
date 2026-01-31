using System.Collections.Generic;

namespace RawrZDesktop.Engines
{
    /// <summary>
    /// Result of engine execution
    /// </summary>
    public class EngineResult
    {
        /// <summary>
        /// Gets or sets whether the operation was successful
        /// </summary>
        public bool Success { get; set; }

        /// <summary>
        /// Gets or sets the result data
        /// </summary>
        public byte[]? Data { get; set; }

        /// <summary>
        /// Gets or sets the error message if operation failed
        /// </summary>
        public string? Error { get; set; }

        /// <summary>
        /// Gets or sets additional metadata about the operation
        /// </summary>
        public Dictionary<string, object>? Metadata { get; set; }

        /// <summary>
        /// Gets or sets the processing time in milliseconds
        /// </summary>
        public double ProcessingTimeMs { get; set; }

        /// <summary>
        /// Gets or sets the timestamp when the operation completed
        /// </summary>
        public System.DateTime Timestamp { get; set; } = System.DateTime.UtcNow;
    }
}
