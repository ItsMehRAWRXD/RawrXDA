const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');

const { validateFsPath } = require('../../security/path-utils');

function withTempDir(callback) {
  const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'bdg-path-'));
  try {
    return callback(tempDir);
  } finally {
    fs.rmSync(tempDir, { recursive: true, force: true });
  }
}

function runTests() {
  withTempDir((rootDir) => {
    const nestedDir = path.join(rootDir, 'nested');
    const filePath = path.join(nestedDir, 'test.txt');
    fs.mkdirSync(nestedDir, { recursive: true });
    fs.writeFileSync(filePath, 'hello world', 'utf8');

    const allowed = [rootDir];

    const resolvedFile = validateFsPath(filePath, {
      allowedBasePaths: allowed,
      allowFiles: true,
      allowDirectories: false,
      mustExist: true,
    });
    assert.strictEqual(resolvedFile, path.resolve(filePath), 'Expected file path to resolve correctly');

    const resolvedDir = validateFsPath(nestedDir, {
      allowedBasePaths: allowed,
      allowFiles: false,
      allowDirectories: true,
      mustExist: true,
    });
    assert.strictEqual(resolvedDir, path.resolve(nestedDir), 'Expected directory path to resolve correctly');

    assert.throws(
      () =>
        validateFsPath(path.join(rootDir, '..', 'outside.txt'), {
          allowedBasePaths: allowed,
          allowFiles: true,
          allowDirectories: false,
        }),
      /not within an allowed directory/i,
      'Expected path traversal to be blocked',
    );

    assert.throws(
      () =>
        validateFsPath(`${filePath}\u0000`, {
          allowedBasePaths: allowed,
          allowFiles: true,
          allowDirectories: false,
        }),
      /null byte/i,
      'Expected null byte to be rejected',
    );

    const futureFile = path.join(nestedDir, 'new-file.txt');
    const futureResolved = validateFsPath(futureFile, {
      allowedBasePaths: allowed,
      allowFiles: true,
      allowDirectories: false,
      mustExist: false,
    });
    assert.strictEqual(futureResolved, path.resolve(futureFile), 'Expected new file path to be permitted when mustExist=false');

    const caseVariant = path.join(rootDir, 'nested', 'TEST.txt');
    const crossCheck = validateFsPath(caseVariant, {
      allowedBasePaths: allowed,
      allowFiles: true,
      allowDirectories: false,
      mustExist: false,
    });
    assert.strictEqual(
      crossCheck,
      path.resolve(caseVariant),
      'Expected case variations to resolve within allowed base path',
    );
  });

  console.log('✅ All path-utils security validation tests passed');
}

if (require.main === module) {
  runTests();
}

module.exports = { runTests };

