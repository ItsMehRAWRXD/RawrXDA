'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const crypto = require('crypto');
const { spawnSync } = require('child_process');

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

function runCli(args, cwd) {
    return spawnSync(process.execPath, ['rawr-cli.js', ...args], {
        cwd,
        encoding: 'utf8',
        windowsHide: true,
    });
}

function main() {
    const root = __dirname;
    const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'rawr-cli-test-'));

    const manifestPath = path.join(tmp, 'demo.manifest.json');
    const trustPath = path.join(tmp, 'trust.json');
    const privateKeyPath = path.join(tmp, 'signing.key.pem');
    const publicKeyPath = path.join(tmp, 'signing.pub.pem');

    const pair = crypto.generateKeyPairSync('ed25519');
    fs.writeFileSync(privateKeyPath, pair.privateKey.export({ format: 'pem', type: 'pkcs8' }), 'utf8');
    fs.writeFileSync(publicKeyPath, pair.publicKey.export({ format: 'pem', type: 'spki' }), 'utf8');

    const init = runCli(['manifest-init', '--id', 'demo.extension', '--out', manifestPath], root);
    assert(init.status === 0, 'manifest-init failed:' + init.stderr);
    assert(fs.existsSync(manifestPath), 'manifest-init did not create file');

    const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
    manifest.requires = ['capability:process@1', 'capability:network@1'];
    manifest.processAllowlist = { version: 1, commands: ['cmd.exe'] };
    manifest.networkAllowlist = { version: 1, hosts: ['127.0.0.1'] };
    fs.writeFileSync(manifestPath, JSON.stringify(manifest, null, 2) + '\n', 'utf8');

    const sign = runCli([
        'manifest-sign',
        '--manifest', manifestPath,
        '--key', privateKeyPath,
        '--key-id', 'trusted-root',
    ], root);
    assert(sign.status === 0, 'manifest-sign failed:' + sign.stderr);

    const trust = { 'trusted-root': fs.readFileSync(publicKeyPath, 'utf8') };
    fs.writeFileSync(trustPath, JSON.stringify(trust, null, 2) + '\n', 'utf8');

    const verify = runCli([
        'manifest-verify',
        '--manifest', manifestPath,
        '--trust', trustPath,
    ], root);
    assert(verify.status === 0, 'manifest-verify failed:' + verify.stderr);

    const lint = runCli([
        'manifest-lint',
        '--manifest', manifestPath,
        '--trust', trustPath,
    ], root);
    assert(lint.status === 0, 'manifest-lint failed:' + lint.stderr);
    assert(/LINT_OK/.test(lint.stdout), 'manifest-lint did not report success');

    const invalidManifestPath = path.join(tmp, 'invalid.manifest.json');
    const invalidManifest = {
        schemaVersion: 1,
        id: 'invalid.extension',
        abiVersion: 1,
        requires: ['capability:filesystem@1'],
        processAllowlist: { version: 1, commands: ['cmd.exe'] },
    };
    fs.writeFileSync(invalidManifestPath, JSON.stringify(invalidManifest, null, 2) + '\n', 'utf8');
    const lintInvalid = runCli([
        'manifest-lint',
        '--manifest', invalidManifestPath,
    ], root);
    assert(lintInvalid.status !== 0, 'invalid manifest unexpectedly linted successfully');
    assert(/manifest_process_allowlist_requires_process_capability/.test(lintInvalid.stderr), 'manifest-lint did not surface negotiation failure');

    const tampered = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
    tampered.networkAllowlist.hosts.push('127.0.0.2');
    fs.writeFileSync(manifestPath, JSON.stringify(tampered, null, 2) + '\n', 'utf8');

    const verifyTampered = runCli([
        'manifest-verify',
        '--manifest', manifestPath,
        '--trust', trustPath,
    ], root);
    assert(verifyTampered.status !== 0, 'tampered manifest unexpectedly verified');
    assert(/verify_failed/.test(verifyTampered.stderr), 'tampered manifest failure reason missing');

    console.log('rawr-cli manifest init/sign/verify tests ok');
    console.log('PASS');
}

main();
