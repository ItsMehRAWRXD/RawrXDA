# Change Log

## 0.17.0

### Features

- Handoff plans from UI to agents using extension

## 0.16.10

### Fixes

- Login flow patch

## 0.16.9

### Fixes

- Login flow patch

## 0.16.9

### Fixes

- Minor bugs

## 0.16.8

### Fixes

- Add telemetry events

## 0.16.7

### Fixes

- Minor bugs

## 0.16.6

### Fixes

- Patch issue with login redirect url

## 0.16.5

### Improvements

- Have "Fix all with AI" enabled for all plans

## 0.16.4

### Improvements

- Update README

## 0.16.3

### Fixes

- Fix issue with review title not getting applied
- Fix reviews being cancelled for jj(jujutsu) users

## 0.16.2

### Fixes

- fix state management for plans

## 0.16.1

### Fixes

- Minor bugs

## 0.16.0

### Fixes(Breaking)

- Move all workspace state data to a file-backed storage service. Cached reviews stored in earlier versions will not carry over when you upgrade.

### Additions

- Add the `Cleanup previous reviews` command to delete cached review artifacts from workspace state which was causing extension to crash.

## 0.15.2

### Fixes

- Minor bugs

## 0.15.1

### Fixes

- Minor fix on client id duplications

## 0.15.0

### Improvements

- Optimizations for extension load time

## 0.14.3

### Improvements

- Show review comment severity in comment label

### Fixes

- Fix minor bug with reconnection on review error

## 0.14.2

### Improvements

- Fix dark theme in vscode

## 0.14.1

### Fixes

- Fix websocket connections issues

## 0.14.0

### Improvements

- Upgrade version of used packages and improve connection issues

## 0.13.6

### Improvements

- Add events logs from server

## 0.13.5

### Fixes

- Fix opencode agent not starting properly

## 0.13.4

### Fixes

- Show full data in log output channel

## 0.13.3

### Fixes

- Fix duplicate user being created for azure-devops

## 0.13.2

### Fixes

- Fix null access a field on older reviews

## 0.13.1

### Improvements

- Show logs in output channel

## 0.13.0

### Fixes

- Remove reviews from other branch on branch change.
- Support detached head state for reviews

## 0.12.1

### Fixes

- Improve storing of github pat for self-hosted CodeRabbit with GitHub

## 0.12.0

### Improvements

- CodeRabbit extension now works with self-hosted CodeRabbit

## 0.11.2

### Improvements

- Add opencode as new agent for "Fix with AI"

## 0.11.1

### Improvements

- Show plan the user is on in the account section

### Fixes

- Fix "Fix all issues" for terminal agents, claude code/codex cli

## 0.11.0

### Improvements

- "Fix all with AI" is now available for pro plan users.
- Add "Provide feedback" to submit feedback for comments that are not helpful.
- Improve the UI to show progress of fixing issues identified.
- UI enhancements to allowing unresolving a comment, show comments that are resolved in a better way.

## 0.10.2

### Improvements

- "Fix with AI" now properly provides context to Cursor agent
- Add Augment code to "Fix with AI" agents

## 0.10.1

### Improvements

- Minor enhancements in icons

## 0.10.0

### Improvements

- Review comments are resolved when actions are performed on them

## 0.9.1

### Improvements

- Show nitpick comments for reviews

## 0.9.0

### Improvements

- Fix issues with applying suggestions
- Improve comment rendering logic

## 0.8.3

### Improvements

- Added settings icon in sidebar

## 0.8.2

### Improvements

- Show file status indicators
- Change auto review default to prompt the user to review

### Fixes

- Fix styling issues in the sidebar
- Cancel review in old branch when switching to a new branch

## 0.8.1

### Improvements

- Add "Fix with AI" support for Cline, Roo and Kilo Code

### Fixes

- Fix first time uncommitted review issue

## 0.8.0

### Improvements

- Reviews are now incremental for a branch.

## 0.7.11

### Fixes

- Optimize file comparison

## 0.7.10

### Fixes

- Fix file diffing in case of outdated base branch
- Improve rate limit messaging

## 0.7.9

### Improvements

- Show login link for users in case redirect fails
- Change auto review default to auto
- Show notification when review comments are available

### Fixes

- Fix state issues in login
- Remove status indicator

## 0.7.8

### Improvements

- Send local .coderabbit.yaml in review
- Show update notification when new version is available

### Fixes

- Fix auto review issue on default branch
- Fix issue in coderabbit line decorations

## 0.7.7

- Improved – Gracefully handle cases where Git is not enabled in the workspace.

## 0.7.6

- Fixed issues with detecting the correct base branch during reviews

## 0.7.5

- CodeRabbit now supports logging in using a token

## 0.7.4

- Bug fixes and performance improvements

## 0.7.3

- Support subfolders of parent git repo

## 0.7.2

- Bug fixes and performance improvements

## 0.7.1

- Fix bug related to reviews being skipped when a large number of files are excluded by the system

## 0.7.0

- Bug fixes and improvements

## 0.6.3

- Bug fixes

## 0.6.2

- Bug fixes and performance improvements

## 0.6.1

- Bug fixes and UI improvements

## 0.6.0

- CodeRabbit now works on uncommitted changes
- CodeRabbit can now automatically review your code when you commit changes
- Bug fixes and UI improvements

## 0.5.7

- You can now fix comments by CodeRabbit with other AI Agents

## 0.5.6

- Bug fixes and UI improvements

## 0.5.5

- Bug fixes and UI improvements

## 0.5.4

- Fixed an issue where the extension would lose connectivity to the server while a review was in progress
- Improved error handling and connection issues

## 0.5.3

- Fix an issue where reviews could get stuck waiting to start

## 0.5.0

- Show comments in editor

## 0.4.6

- Bug fixes and UI improvements

## 0.4.5

- Initial release
