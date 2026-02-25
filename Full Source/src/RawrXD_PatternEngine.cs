// ============================================================================
// RawrXD Pattern Bridge - C# Backend Implementation
// ============================================================================
// This file is compiled by the PowerShell build system using Add-Type
// Provides high-performance pattern recognition without external dependencies
// ============================================================================

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text.RegularExpressions;
using System.Threading;

namespace RawrXD.PatternBridge
{
    /// <summary>Pattern classification types</summary>
    public enum PatternType : int
    {
        Unknown = 0,
        Template = 1,
        NonPattern = 2,
        Learned = 3
    }

    /// <summary>Performance statistics for pattern engine</summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct PatternStats
    {
        public ulong TotalClassifications;
        public ulong TemplateMatches;
        public ulong NonPatternMatches;
        public ulong LearnedMatches;
        public double AvgConfidence;
    }

    /// <summary>High-performance pattern recognition engine</summary>
    public static class PatternEngine
    {
        private static readonly ConcurrentDictionary<string, LearnedPattern> LearnedPatterns = new ConcurrentDictionary<string, LearnedPattern>();
        private static PatternStats Stats = new PatternStats();
        private static readonly ReaderWriterLockSlim StatsLock = new ReaderWriterLockSlim();

        // Pre-compiled regex patterns (equivalent to MASM signatures)
        private static readonly Dictionary<string, Regex> PatternSignatures = new Dictionary<string, Regex>
        {
            // Generic implementation patterns
            ["ImplementFeature"] = new Regex(@"\b(implement|add|create|build)\s+(JWT|authentication|authorization|validation|caching|logging|connection\s*pooling)", RegexOptions.Compiled | RegexOptions.IgnoreCase),
            ["FixIssue"] = new Regex(@"\b(fix|resolve|correct|repair)\s+(bug|issue|error|problem|missing)", RegexOptions.Compiled | RegexOptions.IgnoreCase),
            ["AddMissing"] = new Regex(@"\b(add|include|insert)\s+(missing|error\s*handling|try\s*catch|validation)", RegexOptions.Compiled | RegexOptions.IgnoreCase),
            
            // Code-specific patterns
            ["FunctionStub"] = new Regex(@"(function\s+\w+\s*\{\s*\}|function\s+\w+\s*\{\s*throw\s+)", RegexOptions.Compiled),
            ["MissingTryCatch"] = new Regex(@"(Invoke-RestMethod|Invoke-WebRequest).*try\s*\{", RegexOptions.Compiled | RegexOptions.IgnoreCase),
            ["RepeatedComputation"] = new Regex(@"(\$\w+)\s*=.*\1\s*=", RegexOptions.Compiled),
            ["MissingValidation"] = new Regex(@"param\s*\([^)]*\)(\s*(?!.*\[Validate))", RegexOptions.Compiled),
            ["BlockingCall"] = new Regex(@"(Invoke-RestMethod|Start-Process).*(-AsJob|Start-ThreadJob)", RegexOptions.Compiled | RegexOptions.IgnoreCase),
            ["PlaintextSecret"] = new Regex(@"(password|secret|key|token)\s*=\s*['""][^'""]+['""]", RegexOptions.Compiled | RegexOptions.IgnoreCase),
            ["MissingDispose"] = new Regex(@"(New-Object|::new\(\)).*(?!.*Dispose)", RegexOptions.Compiled),
            ["SilentOperation"] = new Regex(@"Write-Host.*(?!.*Write-Log)", RegexOptions.Compiled)
        };

        // Non-pattern detection (semantic/unique)
        private static readonly HashSet<string> NonPatternKeywords = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
        {
            "business rule", "domain logic", "requirement", "compliance", "regulation",
            "architecture", "redesign", "restructure", "component boundary",
            "API integration", "webhook", "external service", "vendor", "SaaS",
            "algorithm", "mathematical", "optimization problem", "graph theory",
            "UI", "UX", "user interface", "workflow", "user journey",
            "database schema", "data model", "entity relationship", "migration"
        };

        /// <summary>Main classification entry point</summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static PatternType ClassifyPattern(string code, string context, ref double confidence)
        {
            if (string.IsNullOrWhiteSpace(code))
            {
                confidence = 0.0;
                return PatternType.Unknown;
            }

            // Fast path: Check learned patterns first (O(1) lookup)
            if (CheckLearnedPatterns(code, out confidence))
            {
                UpdateStats(PatternType.Learned);
                return PatternType.Learned;
            }

            // Check template signatures
            foreach (var signature in PatternSignatures)
            {
                if (signature.Value.IsMatch(code))
                {
                    confidence = CalculateConfidence(code, signature.Key);
                    UpdateStats(PatternType.Template);
                    return PatternType.Template;
                }
            }

            // Check for non-patterns (semantic complexity)
            if (IsNonPattern(code, context))
            {
                confidence = 0.95;
                UpdateStats(PatternType.NonPattern);
                return PatternType.NonPattern;
            }

            // Uncertain - needs manual review
            confidence = 0.5;
            UpdateStats(PatternType.Unknown);
            return PatternType.Unknown;
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private static bool CheckLearnedPatterns(string code, out double confidence)
        {
            confidence = 0.0;
            var key = GetPatternKey(code);
            
            if (LearnedPatterns.TryGetValue(key, out var pattern))
            {
                confidence = pattern.Confidence;
                return confidence > 0.75;
            }

            // Fuzzy match against learned patterns
            foreach (var learned in LearnedPatterns.Values)
            {
                var similarity = CalculateSimilarity(code, learned.OriginalPattern);
                if (similarity > 0.8)
                {
                    confidence = similarity;
                    return true;
                }
            }

            return false;
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private static bool IsNonPattern(string code, string context)
        {
            var combined = code + " " + context;
            
            // Fast keyword check
            foreach (var keyword in NonPatternKeywords)
            {
                if (combined.IndexOf(keyword, StringComparison.OrdinalIgnoreCase) >= 0)
                    return true;
            }

            // Check for collaboration indicators
            string[] collaborationIndicators = { "discuss with", "review with", "stakeholder", "architect", "design decision" };
            return collaborationIndicators.Any(ind => 
                combined.IndexOf(ind, StringComparison.OrdinalIgnoreCase) >= 0);
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private static double CalculateConfidence(string code, string patternName)
        {
            // Base confidence by pattern type
            var baseConfidence = patternName switch
            {
                "FunctionStub" => 0.95,
                "MissingTryCatch" => 0.88,
                "RepeatedComputation" => 0.82,
                "MissingValidation" => 0.90,
                "BlockingCall" => 0.85,
                "PlaintextSecret" => 0.92,
                "MissingDispose" => 0.87,
                "SilentOperation" => 0.80,
                _ => 0.75
            };

            // Boost for specific keywords
            if (code.IndexOf("implement", StringComparison.OrdinalIgnoreCase) >= 0)
                baseConfidence += 0.03;
            if (code.IndexOf("fix", StringComparison.OrdinalIgnoreCase) >= 0)
                baseConfidence += 0.02;

            return Math.Min(0.98, baseConfidence);
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private static double CalculateSimilarity(string a, string b)
        {
            // Jaccard similarity on word tokens
            var tokensA = new HashSet<string>(a.ToLower().Split(new[] { ' ', '\t', '\n', '\r', '.', ',', ';' }, StringSplitOptions.RemoveEmptyEntries));
            var tokensB = new HashSet<string>(b.ToLower().Split(new[] { ' ', '\t', '\n', '\r', '.', ',', ';' }, StringSplitOptions.RemoveEmptyEntries));
            
            var intersection = tokensA.Intersect(tokensB).Count();
            var union = tokensA.Union(tokensB).Count();
            
            return union == 0 ? 0.0 : (double)intersection / union;
        }

        private static string GetPatternKey(string code)
        {
            // Simple hash-based key
            var normalized = code.ToLower().Replace(" ", "").Replace("\t", "");
            return normalized.GetHashCode().ToString("X8");
        }

        private static void UpdateStats(PatternType type)
        {
            StatsLock.EnterWriteLock();
            try
            {
                Stats.TotalClassifications++;
                switch (type)
                {
                    case PatternType.Template: Stats.TemplateMatches++; break;
                    case PatternType.NonPattern: Stats.NonPatternMatches++; break;
                    case PatternType.Learned: Stats.LearnedMatches++; break;
                }
                // Rolling average
                Stats.AvgConfidence = (Stats.AvgConfidence * (Stats.TotalClassifications - 1) + 0.85) / Stats.TotalClassifications;
            }
            finally
            {
                StatsLock.ExitWriteLock();
            }
        }

        /// <summary>Learn from successful/failed pattern resolutions</summary>
        public static void LearnPattern(string originalPattern, string resolution, bool wasSuccessful)
        {
            var key = GetPatternKey(originalPattern);
            var pattern = new LearnedPattern
            {
                OriginalPattern = originalPattern,
                Resolution = resolution,
                Confidence = wasSuccessful ? 0.85 : 0.60,
                SuccessCount = wasSuccessful ? 1 : 0,
                LastUsed = DateTime.UtcNow
            };

            LearnedPatterns.AddOrUpdate(key, pattern, (k, v) => 
            {
                v.SuccessCount += wasSuccessful ? 1 : 0;
                v.Confidence = Math.Min(0.95, v.Confidence + (wasSuccessful ? 0.05 : -0.02));
                v.LastUsed = DateTime.UtcNow;
                return v;
            });
        }

        /// <summary>Get performance statistics</summary>
        public static PatternStats GetStats()
        {
            StatsLock.EnterReadLock();
            try
            {
                return Stats;
            }
            finally
            {
                StatsLock.ExitReadLock();
            }
        }

        private class LearnedPattern
        {
            public string OriginalPattern { get; set; }
            public string Resolution { get; set; }
            public double Confidence { get; set; }
            public int SuccessCount { get; set; }
            public DateTime LastUsed { get; set; }
        }
    }
}
