# Control handler for reverse engineering
# Handles control/unextract/release scenarios

param(
    [string]$Action,
    [string]$Target
)

switch ($Action) {
    "control" {
        # Take control of the target
        if (Test-Path $Target) {
            # Create backup before taking control
            .\CreateBackup.ps1 $Target
            Write-Host "Control established: $Target"
        }
    }
    "unextract" {
        # Reverse extraction
        if (Test-Path $Target) {
            .\SelfReverse.ps1 -Reverse
            Write-Host "Unextract complete: $Target"
        }
    }
    "release" {
        # Release control
        if (Test-Path $Target) {
            .\SelfReverse.ps1 -Release
            Write-Host "Control released: $Target"
        }
    }
    "verify" {
        # Verify integrity
        .\SelfReverse.ps1 -Verify
    }
}
