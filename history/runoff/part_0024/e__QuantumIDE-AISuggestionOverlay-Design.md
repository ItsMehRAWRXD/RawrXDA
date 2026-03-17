# AI Suggestion Overlay — Design Specification

## Goals
- Provide non-intrusive, inline AI suggestions (ghost text and popover) in the editor.
- Support accept/apply, partial accept (token/line), reject, and pin.
- Keep UI responsive with streaming updates and debounced triggers.

## Triggers
- On idle (500–800 ms) after edit.
- On explicit command: `QShell -> Invoke-QAgent -Type Feature -Suggest`.
- On file-open with context warmup (limited to small diffs).

## UI Components (Qt Widgets)
- OverlayWidget: Semi-transparent layer over QTextEdit; renders ghost text.
- SuggestionPopover: Popover with actions: Accept, Accept Line, Copy, Diff, More.
- InlineDiffPanel: Side-by-side mini diff for suggestion vs current code.

## Data Flow
- EditorContextCollector: Gathers current file text, cursor location, recent edits.
- SuggestionEngine: Wraps StreamerClient to request suggestions; supports streaming chunks to overlay.
- Renderer: Applies syntax-aware rendering (respect indentation; match theme).

## API
- requestSuggestion(context): Starts streaming suggestion; updates overlay incrementally.
- cancelSuggestion(): Cancels current stream and clears overlay.
- applySuggestion(range?): Applies suggestion to editor; optional range for partial accept.
- setDebounce(ms): Controls idle trigger threshold.
- setModel(agentType): Feature/Security/Performance; default Feature.

## Keyboard Shortcuts
- Ctrl+Enter: Accept full suggestion.
- Ctrl+Shift+Enter: Accept current line.
- Esc: Cancel.
- Alt+D: Toggle inline diff.

## Integration Points
- MainWindow: Add “AI Suggest” toggle and status indicator in Agent Panel.
- QShell: `Invoke-QAgent -Type Feature -Suggest`, `Set-QConfig -SuggestDebounce <ms>`.
- StreamerClient: Reuse streaming; map chunkReceived to overlay updates.

## Error Handling
- Model Unavailable: Show tooltip and disable suggest; log to QShell.
- Stream Errors: Clear overlay; show error state; allow retry.

## Minimal Implementation Phase 1
- OverlayWidget with ghost text; hook StreamerClient to display streamed text.
- Basic accept/cancel actions and shortcuts.

## Phase 2
- Partial accept with selection and syntax-aware formatting.
- Inline diff and popover actions.

## Phase 3
- Multi-agent suggestions (Security hints, Performance optimizations) with tabs in popover.
- Learning preferences (store accepted patterns to improve relevance).
