# Marketplace Loading Improvements

This note explains the recent marketplace loading changes so the startup experience matches the expectations of a VS Code-style catalog:

- The marketplace catalog is now warmed up automatically during the main form load, so the extension list is ready as soon as the user opens the marketplace window.
- The search and palette commands now combine installed extensions with remote entries fetched from the configured marketplace sources, ensuring all listings are available in a single view.
- The marketplace dialog now includes a refresh action, metadata columns (source, category, downloads, rating, install status), and a status label that surfaces the last refresh time and how many entries are showing.
- Users can trigger `Refresh catalog` to force the latest data from every source, and the status label updates without needing to restart the app.
