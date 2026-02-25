const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const test = require('node:test');

const { validateFsPath } = require('../../security/path-utils');

function createFixture() {
  const rootDir = fs.mkdtempSync(path.join(os.tmpdir(), 'bdg-path-'));
  const nestedDir = path.join(rootDir, 'nested');
  const filePath = path.join(nestedDir, 'test.txt');

  fs.mkdirSync(nestedDir, { recursive: true });
  fs.writeFileSync(filePath, 'hello world', 'utf8');

  return { rootDir, nestedDir, filePath };
}

test('validateFsPath allows files within allowed directories', (t) => {
  const { rootDir, nestedDir, filePath } = createFixture();
  t.teardown(() => fs.rmSync(rootDir, { recursive: true, force: true }));

  const resolvedFile = validateFsPath(filePath, {
    allowedBasePaths: [rootDir],
    allowFiles: true,
    allowDirectories: false,
    mustExist: true,
  });

  const resolvedDir = validateFsPath(nestedDir, {
    allowedBasePaths: [rootDir],
    allowFiles: false,
    allowDirectories: true,
    mustExist: true,
  });

  assert.strictEqual(resolvedFile, path.resolve(filePath));
  assert.strictEqual(resolvedDir, path.resolve(nestedDir));
});

test('validateFsPath blocks traversal outside allowed directories', (t) => {
  const { rootDir } = createFixture();
  t.teardown(() => fs.rmSync(rootDir, { recursive: true, force: true }));

  assert.throws(
    () =>
      validateFsPath(path.join(rootDir, '..', 'outside.txt'), {
        allowedBasePaths: [rootDir],
        allowFiles: true,
        allowDirectories: false,
      }),
    /not within an allowed directory/i,
  );
});

test('validateFsPath rejects null bytes', (t) => {
  const { rootDir, filePath } = createFixture();
  t.teardown(() => fs.rmSync(rootDir, { recursive: true, force: true }));

  assert.throws(
    () =>
      validateFsPath(`${filePath}\u0000`, {
        allowedBasePaths: [rootDir],
        allowFiles: true,
        allowDirectories: false,
      }),
    /null byte/i,
  );
});

test('validateFsPath respects mustExist=false for future files', (t) => {
  const { rootDir, nestedDir } = createFixture();
  t.teardown(() => fs.rmSync(rootDir, { recursive: true, force: true }));

  const futureFile = path.join(nestedDir, 'new-file.txt');
  const futureResolved = validateFsPath(futureFile, {
    allowedBasePaths: [rootDir],
    allowFiles: true,
    allowDirectories: false,
    mustExist: false,
  });

  assert.strictEqual(futureResolved, path.resolve(futureFile));
});

test('validateFsPath normalises path casing within allowed directories', (t) => {
  const { rootDir } = createFixture();
  t.teardown(() => fs.rmSync(rootDir, { recursive: true, force: true }));

  const caseVariant = path.join(rootDir, 'nested', 'TEST.txt');
  const resolved = validateFsPath(caseVariant, {
    allowedBasePaths: [rootDir],
    allowFiles: true,
    allowDirectories: false,
    mustExist: false,
  });

  assert.strictEqual(resolved, path.resolve(caseVariant));
});

