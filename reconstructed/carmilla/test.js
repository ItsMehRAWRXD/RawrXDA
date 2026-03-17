// Carmilla Encryption System — Real Crypto Test Suite
const { Carmilla } = require('./dist/crypto/carmilla');

async function runTests() {
  let passed = 0;
  let failed = 0;

  async function test(name, fn) {
    try {
      await fn();
      console.log(`  ✅ ${name}`);
      passed++;
    } catch (err) {
      console.log(`  ❌ ${name}: ${err.message}`);
      failed++;
    }
  }

  console.log('\n🔐 Carmilla Encryption System — Test Suite');
  console.log('='.repeat(55));

  // ── Basic Encrypt / Decrypt ──────────────────────────────────────────

  console.log('\n[1] AES-256-GCM Encrypt & Decrypt');

  await test('encrypts plaintext', async () => {
    const ct = await Carmilla.encrypt('Hello, World!', 'my-secret-pass');
    if (!ct || ct.length < 10) throw new Error('ciphertext too short');
  });

  await test('decrypt recovers original plaintext', async () => {
    const pt = 'The quick brown fox jumps over the lazy dog.';
    const ct = await Carmilla.encrypt(pt, 'pass123');
    const dec = await Carmilla.decrypt(ct, 'pass123');
    if (dec !== pt) throw new Error(`expected "${pt}", got "${dec}"`);
  });

  await test('wrong passphrase throws', async () => {
    const ct = await Carmilla.encrypt('secret data', 'correct-pass');
    try {
      await Carmilla.decrypt(ct, 'wrong-pass');
      throw new Error('should have thrown');
    } catch (e) {
      if (!e.message.includes('tampered')) throw e;
    }
  });

  await test('each encryption produces unique ciphertext (unique salt/IV)', async () => {
    const ct1 = await Carmilla.encrypt('same data', 'same pass');
    const ct2 = await Carmilla.encrypt('same data', 'same pass');
    if (ct1 === ct2) throw new Error('ciphertext was identical — salt/IV reuse detected');
  });

  await test('handles unicode and emoji', async () => {
    const pt = '日本語テスト 🔐🚀 العربية';
    const ct = await Carmilla.encrypt(pt, 'unicode-pass');
    const dec = await Carmilla.decrypt(ct, 'unicode-pass');
    if (dec !== pt) throw new Error('unicode round-trip failed');
  });

  await test('handles large data (1 MB)', async () => {
    const pt = 'A'.repeat(1_000_000);
    const ct = await Carmilla.encrypt(pt, 'big-data-pass');
    const dec = await Carmilla.decrypt(ct, 'big-data-pass');
    if (dec.length !== pt.length) throw new Error(`length mismatch: ${dec.length} vs ${pt.length}`);
  });

  // ── Packaging ────────────────────────────────────────────────────────

  console.log('\n[2] Package & Unpackage');

  await test('repack produces valid base64', async () => {
    const ct = await Carmilla.encrypt('test', 'pass');
    const pkg = Carmilla.repack(ct, { passphrase_hint: 'test hint' });
    if (!pkg || pkg.length < 10) throw new Error('package too short');
    // Ensure it base64-decodes to valid JSON
    const json = Buffer.from(pkg, 'base64').toString('utf-8');
    const parsed = JSON.parse(json);
    if (parsed.meta.method !== 'aes-256-gcm') throw new Error('wrong method in meta');
    if (parsed.meta.kdf !== 'pbkdf2-sha512') throw new Error('wrong kdf in meta');
  });

  await test('unpack verifies HMAC integrity', async () => {
    const ct = await Carmilla.encrypt('test', 'pass');
    const pkg = Carmilla.repack(ct);
    const unpacked = Carmilla.unpack(pkg);
    if (!unpacked.meta || !unpacked.data) throw new Error('missing meta or data');
    if (!unpacked.meta.hmac) throw new Error('missing HMAC');
  });

  await test('unpack detects tampered data', async () => {
    const ct = await Carmilla.encrypt('test', 'pass');
    const pkg = Carmilla.repack(ct);
    // Tamper with the package
    const json = JSON.parse(Buffer.from(pkg, 'base64').toString('utf-8'));
    json.data = json.data.replace(json.data[5], json.data[5] === 'A' ? 'B' : 'A');
    const tampered = Buffer.from(JSON.stringify(json)).toString('base64');
    try {
      Carmilla.unpack(tampered);
      throw new Error('should have thrown');
    } catch (e) {
      if (!e.message.includes('integrity')) throw e;
    }
  });

  // ── Encrypt & Pack / Unpack & Decrypt ────────────────────────────────

  console.log('\n[3] Full Round-Trip (encrypt+pack → unpack+decrypt)');

  await test('encryptAndPack → unpackAndDecrypt round-trip', async () => {
    const pt = 'Carmilla round-trip test — real cryptography!';
    const pkg = await Carmilla.encryptAndPack(pt, 'round-trip-pass', {
      passphrase_hint: 'the trip',
    });
    const dec = await Carmilla.unpackAndDecrypt(pkg, 'round-trip-pass');
    if (dec !== pt) throw new Error(`round-trip failed: "${dec}" !== "${pt}"`);
  });

  // ── Selective / Batch ────────────────────────────────────────────────

  console.log('\n[4] Selective Encryption');

  await test('processTargets encrypts, skips, and reports correctly', async () => {
    const targets = [
      { type: 'obj', data: 'encrypt me', intent: 'encrypt' },
      { type: 'obj', data: 'skip me', intent: 'do-not-encrypt' },
      { type: 'obj', data: '', intent: 'cannot-encrypt' },
      { type: 'obj', data: 'also encrypt', intent: 'encrypt' },
    ];
    const results = await Carmilla.processTargets(targets, 'batch-pass');
    if (results.length !== 4) throw new Error(`expected 4 results, got ${results.length}`);
    if (results[0].status !== 'encrypted') throw new Error('first should be encrypted');
    if (results[1].status !== 'skipped') throw new Error('second should be skipped');
    if (results[2].status !== 'cannot encrypt') throw new Error('third should be cannot encrypt');
    if (results[3].status !== 'encrypted') throw new Error('fourth should be encrypted');
  });

  // ── Summary ──────────────────────────────────────────────────────────

  console.log('\n' + '='.repeat(55));
  console.log(`  Results: ${passed} passed, ${failed} failed`);
  if (failed === 0) {
    console.log('  🎉 ALL TESTS PASSED — Real crypto, zero placeholders');
  } else {
    console.log('  ⚠️  Some tests failed');
    process.exit(1);
  }
  console.log();
}

runTests().catch(err => {
  console.error('Fatal test error:', err);
  process.exit(1);
});
