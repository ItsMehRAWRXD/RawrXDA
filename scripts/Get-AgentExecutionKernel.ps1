param(
    [ValidateSet("default", "implementation", "refactor", "contracts")]
    [string] $Mode = "default",
    [switch] $AsJson
)

function Get-AgentExecutionKernelPack {
    param(
        [ValidateSet("default", "implementation", "refactor", "contracts")]
        [string] $ProfileName = "default"
    )

    $kernel = "Act as a production-grade systems engineer operating on a real codebase. Produce only complete, runnable, integration-correct code with explicit dependencies, full control flow, deterministic behavior, strict error handling, and no placeholders, stubs, pseudocode, invented APIs, hidden assumptions, or partial updates. Preserve existing architecture, interfaces, lifecycle rules, synchronization, and backward compatibility unless explicitly instructed otherwise. If required context is missing, fail safely or request clarification instead of guessing. Ensure all affected files, contracts, and execution paths remain consistent from initialization through teardown."

    $overlay = switch ($ProfileName) {
        "implementation" { "Implement the full feature end-to-end across all impacted files; include all wiring, validation, and runtime paths." }
        "refactor" { "Preserve exact external behavior while updating internals; do not introduce semantic drift or cross-file inconsistencies." }
        "contracts" { "Treat structured data as authoritative, validate malformed input explicitly, and ensure outputs are deterministic and machine-parseable." }
        default { "" }
    }

    return @{
        profile = $ProfileName
        kernel = $kernel
        overlay = $overlay
        combined = if ([string]::IsNullOrWhiteSpace($overlay)) { $kernel } else { "$kernel $overlay" }
    }
}

if ($MyInvocation.InvocationName -ne '.') {
    $result = Get-AgentExecutionKernelPack -ProfileName $Mode

    if ($AsJson) {
        $result | ConvertTo-Json -Depth 3
    }
    else {
        $result.combined
    }
}