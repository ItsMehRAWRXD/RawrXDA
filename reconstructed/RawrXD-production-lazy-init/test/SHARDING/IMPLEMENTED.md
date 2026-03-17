# Test Sharding Implementation

## Overview
Implemented test sharding to accelerate CI execution by parallelizing test runs across multiple runners.

## Changes

### 1. `tests.yml` (Windows Unit & Component Tests)
- **Sharded `test-suite`:** 
  - Added `shard: [0, 1, 2, 3]` matrix strategy.
  - Updated **Google Test** execution to filter test suites based on shard index.
  - Updated **Qt Test** execution to distribute `*_test.exe` and `test_*.exe` binaries across shards.
- **Parallel Component Tests:** 
  - Removed `needs: test-suite` dependency from `test-voice-processor`, `test-ai-merge-resolver`, etc., allowing them to run concurrently with the main suite.
- **Enhanced Discovery:** 
  - Updated binary search filter to include both `*_test.exe` and `test_*.exe`, ensuring coverage of new production readiness tests (`test_readiness_checker`, etc.).

### 2. `ci-cd.yml` (Cross-Platform Tests)
- **Sharded `unit-tests`:**
  - Added `shard: [0, 1, 2, 3]` to matrix.
  - Updated `ctest` execution to use `-I start,end,stride` for deterministic test distribution.
  - Logic: `ctest -I "shard+1,,4"` (runs every 4th test).

## Verification
- **Test Suite:** Now runs in 4 parallel shards (x2 Configs = 8 jobs).
- **Unit Tests:** Now runs in 4 parallel shards per OS (x2 OS = 8 jobs).
- **Total Throughput:** Theoretical 4x speedup for test execution phase.

## Usage
No manual intervention required. CI pipelines automatically shard on push/PR.
