# debug_markdown_extraction.ps1

$testContent = @"
Some agent execution output
With various log messages

## 🎯 Session Complete 

### ✅ What's Done
- **[Completion Protocol Definition]**: Created comprehensive markdown protocol specification ✓
- **[Agent Validator Enhancement]**: Updated validator to parse markdown completion format ✓
- **[Execution Wrapper]**: Built agent wrapper with strict contract enforcement ✓
- **[C++ Helper Library]**: Added native completion emission for Win32IDE integration ✓

### 📊 Current State
| Metric | Current | Target | Status |
|--------|---------|--------|--------|
| **Protocol Compliance** | 4/4 sections | 4/4 sections | ✅ Complete |
| **Validation Coverage** | Text+JSON+Markdown | All formats | ✅ Complete |
| **Execution Enforcement** | Strict mode active | Required | ✅ Implemented |

### 🚀 Next Phases 
1. **[Phase 2C Integration]**: Apply protocol to GPU kernel tuning workflow
2. **[CI/CD Integration]**: Add protocol validation to build pipelines
3. **[Team Rollout]**: Document protocol for all agent developers

### 📝 Key Commands (Ready to Execute)
\`\`\`bash
# Validate protocol compliance on any agent output
& D:\RawrXD\scripts\agent_completion_validator.ps1 -LogFile agent_output.log -Strict

# Run agent with protocol enforcement wrapper
& D:\RawrXD\scripts\agent_execution_wrapper.ps1 -AgentScript your_agent.ps1 -Strict
\`\`\`

### 📍 Relevant Links / Artifacts
- **[Protocol Documentation]**: \`docs/AGENT_COMPLETION_PROTOCOL.md\`
- **[Validator Script]**: \`scripts/agent_completion_validator.ps1\`
- **[C++ Helper]**: \`src/core/agent_completion.h\`
- **[Execution Wrapper]**: \`scripts/agent_execution_wrapper.ps1\`

**Ready to continue with Phase 2C kernel tuning integration, or shall we wrap here?**

More log output after completion block
"@

# Test the section extraction
if ($testContent -match "### ✅ What's Done[\s\S]*?### 📊") {
    $tasksSection = $Matches[0]
    Write-Host "Tasks section found! Length: $($tasksSection.Length)"
    Write-Host "Content: $tasksSection"
    
    $taskCount = ([regex]::Matches($tasksSection, "- \[")).Count
    Write-Host "Task count: $taskCount"
    
    # Show all matches
    $matches = [regex]::Matches($tasksSection, "- \[")
    Write-Host "Individual matches:"
    foreach ($match in $matches) {
        Write-Host "  - $($match.Value) at position $($match.Index)"
    }
} else {
    Write-Host "No tasks section found"
}