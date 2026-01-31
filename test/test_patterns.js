// Test file for pattern engine validation
// ============================================

// TODO: Implement user authentication
function authenticate() {
  // FIXME: This is broken and needs refactoring
  return false;
}

// BUG: Crash on null input - critical issue
function processData(data) {
  // XXX: Temporary workaround for deadline
  if (!data) return;

  // HACK: Quick fix, clean up later
  data.processed = true;

  // NOTE: Important implementation detail
  // The data must be validated before processing

  // IDEA: Could optimize this with caching
  return data;
}

// REVIEW: Security audit required before deployment
function sendToServer(data) {
  // todo: add error handling (lowercase test)
  // fixme: timeout not implemented (lowercase test)
  // bug: race condition possible (lowercase test)
}

// Normal comment without patterns
// This should not be detected
function normalFunction() {
  return "Hello World";
}
