# ==============================================================================
# Code Analysis Processor
# Handles code analysis, review, and optimization requests
# ==============================================================================

class CodeAnalysisProcessor : IRequestProcessor {
    [string] $Name = "CodeAnalyzer"
    [hashtable] $Capabilities
    
    CodeAnalysisProcessor() {
        $this.Capabilities = @{
            SupportedLanguages = @("powershell", "python", "javascript", "csharp", "java", "cpp")
            AnalysisTypes = @("syntax", "complexity", "security", "performance", "style")
            MaxCodeLength = 100000
        }
    }
    
    [object] ProcessRequest([object] $request) {
        $startTime = Get-Date
        $response = [BaseResponse]::new($request.Id, "CodeAnalysis")
        $response.ProcessorName = $this.Name
        
        try {
            if (-not $this.CanHandle($request.Type)) {
                $response.AddError("Cannot handle request type: $($request.Type)")
                return $response
            }
            
            $code = $request.GetCode()
            $language = $request.GetLanguage()
            $analysisTypes = $request.GetAnalysisTypes()
            
            # Perform different types of analysis
            foreach ($analysisType in $analysisTypes) {
                switch ($analysisType) {
                    "syntax" { 
                        $result = $this.AnalyzeSyntax($code, $language)
                        $response.SetData("syntaxAnalysis", $result)
                    }
                    "complexity" { 
                        $result = $this.AnalyzeComplexity($code, $language)
                        $response.SetData("complexityAnalysis", $result)
                    }
                    "security" { 
                        $result = $this.AnalyzeSecurity($code, $language)
                        $response.SetData("securityAnalysis", $result)
                    }
                    "performance" { 
                        $result = $this.AnalyzePerformance($code, $language)
                        $response.SetData("performanceAnalysis", $result)
                    }
                    "style" { 
                        $result = $this.AnalyzeStyle($code, $language)
                        $response.SetData("styleAnalysis", $result)
                    }
                    default { 
                        $response.AddWarning("Unknown analysis type: $analysisType")
                    }
                }
            }
            
            $response.SetMetadata("codeLength", $code.Length)
            $response.SetMetadata("language", $language)
            $response.SetMetadata("analysisTypes", $analysisTypes)
            
        } catch {
            $response.AddError("Code analysis failed: $($_.Exception.Message)")
        }
        
        $response.SetProcessingTime($startTime)
        return $response
    }
    
    [bool] CanHandle([string] $requestType) {
        return $requestType -eq "CodeAnalysis"
    }
    
    [string] GetProcessorName() {
        return $this.Name
    }
    
    [hashtable] GetCapabilities() {
        return $this.Capabilities
    }
    
    # Analysis methods
    hidden [hashtable] AnalyzeSyntax([string] $code, [string] $language) {
        $issues = @()
        $score = 100
        
        # Basic syntax checks based on language
        switch ($language.ToLower()) {
            "powershell" {
                # Check for common PowerShell syntax issues
                if ($code -match '\$\w+\s*=\s*\$null\s*;') {
                    $issues += @{ Type = "Warning"; Message = "Unnecessary semicolon after null assignment" }
                    $score -= 5
                }
                if ($code -match 'Write-Host') {
                    $issues += @{ Type = "Info"; Message = "Consider using Write-Output instead of Write-Host" }
                }
            }
            "python" {
                if ($code -match 'print\s*\(' -and $code -notmatch 'from __future__ import print_function') {
                    $issues += @{ Type = "Info"; Message = "Using Python 3 print syntax" }
                }
            }
            "javascript" {
                if ($code -match 'var\s+\w+') {
                    $issues += @{ Type = "Warning"; Message = "Consider using let or const instead of var" }
                    $score -= 10
                }
            }
        }
        
        return @{
            Score = $score
            Issues = $issues
            IsValid = $score -gt 70
        }
    }
    
    hidden [hashtable] AnalyzeComplexity([string] $code, [string] $language) {
        $lines = $code -split "`n"
        $totalLines = $lines.Count
        $commentLines = ($lines | Where-Object { $_ -match '^\s*#|^\s*//' }).Count
        $emptyLines = ($lines | Where-Object { $_ -match '^\s*$' }).Count
        $codeLines = $totalLines - $commentLines - $emptyLines
        
        # Cyclomatic complexity approximation
        $complexityKeywords = @('if', 'else', 'elif', 'while', 'for', 'switch', 'case', 'catch', 'try')
        $complexityCount = 1  # Base complexity
        
        foreach ($keyword in $complexityKeywords) {
            $keywordMatches = [regex]::Matches($code, "\b$keyword\b", [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
            $complexityCount += $keywordMatches.Count
        }
        
        $complexityRating = if ($complexityCount -le 10) { "Low" }
                           elseif ($complexityCount -le 20) { "Medium" }
                           elseif ($complexityCount -le 30) { "High" }
                           else { "Very High" }
        
        return @{
            TotalLines = $totalLines
            CodeLines = $codeLines
            CommentLines = $commentLines
            EmptyLines = $emptyLines
            CyclomaticComplexity = $complexityCount
            ComplexityRating = $complexityRating
            CommentRatio = if ($codeLines -gt 0) { [math]::Round($commentLines / $codeLines, 2) } else { 0 }
        }
    }
    
    hidden [hashtable] AnalyzeSecurity([string] $code, [string] $language) {
        $vulnerabilities = @()
        $riskLevel = "Low"
        
        # Common security pattern checks
        if ($code -match 'password\s*=\s*["''][^"'']+["'']') {
            $vulnerabilities += @{ Type = "High"; Issue = "Hardcoded password detected" }
            $riskLevel = "High"
        }
        
        if ($code -match 'eval\s*\(') {
            $vulnerabilities += @{ Type = "High"; Issue = "Use of eval() detected - potential code injection" }
            $riskLevel = "High"
        }
        
        if ($code -match 'Invoke-Expression') {
            $vulnerabilities += @{ Type = "Medium"; Issue = "Use of Invoke-Expression - validate input carefully" }
            if ($riskLevel -eq "Low") { $riskLevel = "Medium" }
        }
        
        if ($code -match 'System\.IO\.File\.ReadAllText|Get-Content.*-Raw') {
            $vulnerabilities += @{ Type = "Low"; Issue = "File operations detected - ensure proper validation" }
        }
        
        return @{
            RiskLevel = $riskLevel
            Vulnerabilities = $vulnerabilities
            SecurityScore = switch ($riskLevel) {
                "Low" { 90 }
                "Medium" { 70 }
                "High" { 40 }
                "Critical" { 10 }
            }
        }
    }
    
    hidden [hashtable] AnalyzePerformance([string] $code, [string] $language) {
        $issues = @()
        $score = 100
        
        # Performance anti-patterns
        if ($code -match '\+\=.*string|string.*\+\=') {
            $issues += @{ Type = "Performance"; Message = "String concatenation in loop - consider StringBuilder" }
            $score -= 15
        }
        
        if ($code -match 'Select-Object.*\|\s*Where-Object') {
            $issues += @{ Type = "Performance"; Message = "Consider filtering before selecting for better performance" }
            $score -= 10
        }
        
        if ($code -match 'foreach.*Get-.*\|') {
            $issues += @{ Type = "Performance"; Message = "Nested pipeline operations may impact performance" }
            $score -= 5
        }
        
        # Calculate estimated performance rating
        $rating = if ($score -ge 90) { "Excellent" }
                 elseif ($score -ge 75) { "Good" }
                 elseif ($score -ge 60) { "Average" }
                 else { "Needs Improvement" }
        
        return @{
            PerformanceScore = $score
            Rating = $rating
            Issues = $issues
            Recommendations = @(
                "Use appropriate data structures",
                "Minimize object creation in loops",
                "Consider async operations for I/O"
            )
        }
    }
    
    hidden [hashtable] AnalyzeStyle([string] $code, [string] $language) {
        $styleIssues = @()
        $score = 100
        
        # Style checks based on language
        switch ($language.ToLower()) {
            "powershell" {
                # PowerShell style guidelines
                if ($code -match '\$[a-z]') {
                    $styleIssues += @{ Type = "Style"; Message = "Consider using PascalCase for variables" }
                    $score -= 5
                }
                if ($code -notmatch '^\s*#.*Author|^\s*#.*Description') {
                    $styleIssues += @{ Type = "Documentation"; Message = "Consider adding header documentation" }
                }
            }
            "python" {
                if ($code -match 'def [A-Z]') {
                    $styleIssues += @{ Type = "Style"; Message = "Function names should use snake_case" }
                    $score -= 10
                }
            }
            "javascript" {
                if ($code -match 'function [A-Z]') {
                    $styleIssues += @{ Type = "Style"; Message = "Function names should use camelCase" }
                    $score -= 10
                }
            }
        }
        
        # General style checks
        $lines = $code -split "`n"
        $longLines = $lines | Where-Object { $_.Length -gt 120 }
        if ($longLines.Count -gt 0) {
            $styleIssues += @{ Type = "Style"; Message = "$($longLines.Count) lines exceed 120 characters" }
            $score -= ($longLines.Count * 2)
        }
        
        return @{
            StyleScore = [math]::Max($score, 0)
            Issues = $styleIssues
            Compliance = if ($score -ge 90) { "Excellent" } elseif ($score -ge 75) { "Good" } else { "Needs Work" }
        }
    }
}