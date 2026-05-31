# AGENT_COMPLETION_PROTOCOL.md

## System Instruction: Completion Protocol

Whenever you successfully complete a multi-step task, a user request, or reach a logical stopping point, you MUST output a final status report using the exact Markdown format below. Do not deviate from this structure.

```markdown
## 🎯 Session Complete 

I have successfully completed the requested tasks. Here is the summary of what was delivered:

### ✅ What's Done
- **[Task 1]**: [Brief description of what was accomplished] ✓
- **[Task 2]**: [Brief description] ✓
- **[Task 3]**: [Brief description] ✓

### 📊 Current State
| Metric | Current | Target | Status |
|--------|---------|--------|--------|
| **[Metric 1]** | [Value] | [Value] | [Status] |
| **[Metric 2]** | [Value] | [Value] | [Status] |

### 🚀 Next Phases 
1. **[Phase X]**: [Brief description of what comes next]
2. **[Phase Y]**: [Brief description]

### 📝 Key Commands (Ready to Execute)
```bash
# [Purpose of command]
[CLI Command]
```

### 📍 Relevant Links / Artifacts
- **[File/PR/Branch]**: [Path or URL]

**Ready to continue with [Next Phase], or shall we wrap here?**
```

## Protocol Enforcement Rules

1. **Mandatory**: This format MUST be used at the completion of any multi-step task
2. **Consistency**: Do not deviate from the structure, emojis, or section ordering
3. **Completeness**: All sections must be filled with relevant information
4. **Action-Oriented**: Always include ready-to-execute commands for next steps
5. **Verifiable**: Include concrete metrics and artifact references

## Example Usage

### For Phase Completion:
```markdown
## 🎯 Session Complete 

I have successfully completed Phase 2B GPU validation. Here is the summary:

### ✅ What's Done
- **[GPU Lane Validation]**: Proven Vulkan isolation with AMD RX 7800 XT ✓
- **[Trace Provenance]**: Added self-describing JSON schema with pipeline mode fields ✓
- **[Agentic Harness]**: 17/17 validation checks passed ✓

### 📊 Current State
| Metric | Current | Target | Status |
|--------|---------|--------|--------|
| **Throughput** | 28-29 tok/s | 50+ tok/s | Needs optimization |
| **Test Coverage** | 17/17 | 17/17 | ✅ Complete |
| **LOC Budget** | ~850k | <1M | ✅ Within target |

### 🚀 Next Phases 
1. **[Phase 2C]**: GPU kernel performance tuning (tg128_fused + Q4_K optimization)
2. **[Phase 3]**: Extension host process isolation and sandboxing

### 📝 Key Commands (Ready to Execute)
```bash
# Run kernel A/B performance sweep
& D:\RawrXD\scripts\run_kernel_ab_sweep.ps1 -ForceTg128 -Runs 5
```

### 📍 Relevant Links / Artifacts
- **[Branch]**: `feature/phase2b-gpu-validation-trace-provenance-sealed`
- **[Documentation]**: `PHASE_2B_COMPLETION_STATUS.md`
- **[Validation]**: `RawrXD-Agentic-Test.ps1` (17/17 pass)

**Ready to continue with Phase 2C kernel tuning, or shall we wrap here?**
```

## Integration Points

### 1. Agent Custom Instructions
Add this protocol to your agent's system prompt in:
- Cursor Rules
- GitHub Copilot instructions  
- Custom GPT configurations
- VS Code agent settings

### 2. Automated Validation
Use the completion validator to ensure protocol compliance:
```powershell
& D:\RawrXD\scripts\agent_completion_validator.ps1 -LogFile agent_output.log
```

### 3. CI/CD Enforcement
Add protocol validation to your build pipeline:
```yaml
- name: Validate Completion Protocol
  run: pwsh -File scripts/agent_completion_validator.ps1 -LogFile ${{ github.workspace }}/agent.log -Strict
```

## Benefits

1. **Predictability**: Structured output eliminates rambling and forces categorization
2. **Visual Hierarchy**: Emoji anchors make reports instantly scannable
3. **Action-Oriented**: Ensures momentum continues with ready next steps
4. **Machine-Verifiable**: Structured format enables automated validation
5. **Consistency**: Standardized reporting across all agent interactions

## Implementation Checklist

- [ ] Add protocol to agent system instructions
- [ ] Update existing agent workflows to use protocol
- [ ] Test protocol enforcement with validator
- [ ] Integrate with CI/CD pipelines
- [ ] Document protocol usage for all team members

## Version History

- **v1.0** (2026-05-05): Initial protocol definition
- **v1.1**: Added machine-validation compatibility
- **v1.2**: Integrated with AGENT_DONE contract system