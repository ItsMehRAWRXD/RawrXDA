# HexMag Production-Lock CI Workflow

Commit `71fccdb` enabled the Windows runner pipeline that guards both the Win32 IDE build and the HexMag swarm endpoints.

## Trigger Matrix
- Pushes to `main` or `dev`
- Pull requests targeting any branch

## Step Breakdown
1. Checkout Code — pulls the repository onto the clean Windows host.
2. Add MSBuild — loads the Visual Studio toolchain required by the IDE build.
3. Build Win32 IDE — runs `scripts/build.ps1 -Config Release -A x64` to compile the Win32 window.
4. Set up Python 3.11 — prepares the interpreter for the swarm tests.
5. Install Python dependencies — runs `pip install -r services/requirements.txt` from the `services` folder.
6. Start HexMag Test Swarm — launches `hexmag/hexmag_engine.py --port 8001`, polling the `/health` route until it returns HTTP 200.
7. Smoke-test /ask and /agent — executes `python test_smoke.py`, which must print `ALL HEXMAG SMOKE TESTS PASSED`.
8. Kill test swarm — `taskkill /F /IM python.exe` in an `always()` block to clean up background processes.

## Log Cues to Watch
- `HexMag service is ready.` confirms the health probe succeeded.
- `ALL HEXMAG SMOKE TESTS PASSED` marks endpoint validation success.
- Any MSBuild errors or `HexMag service failed to start` will surface the failing step immediately.

## Green Check Meaning
A successful run verifies the Release/x64 IDE binary builds, the HexMag swarm boots on `localhost:8001`, and both `/ask` and `/agent` satisfy their smoke-test criteria. A failed run pinpoints regressions in the build pipeline, service boot, or agent goal satisfaction.
