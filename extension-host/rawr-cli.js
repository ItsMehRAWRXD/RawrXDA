'use strict';

const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const {
    canonicalizeManifestForSigning,
    verifyManifestSignature,
} = require('./manifest-signature');
const manager = require('./extension-host-manager');

function printHelp() {
    const text = [
        'RawrXD CLI',
        '',
        'Usage:',
        '  node rawr-cli.js manifest-init --id <extension.id> [--out <file>] [--name <display-name>]',
        '  node rawr-cli.js manifest-sign --manifest <file> --key <pem-file> --key-id <id> [--out <file>]',
        '  node rawr-cli.js manifest-lint --manifest <file> [--trust <json-map-file> | --public-key <pem-file> --key-id <id>]',
        '  node rawr-cli.js manifest-verify --manifest <file> (--trust <json-map-file> | --public-key <pem-file> --key-id <id>)',
        '',
        'Examples:',
        '  node rawr-cli.js manifest-init --id demo.extension --out demo.manifest.json',
        '  node rawr-cli.js manifest-sign --manifest demo.manifest.json --key signing.key.pem --key-id trusted-root',
        '  node rawr-cli.js manifest-lint --manifest demo.manifest.json --trust trust-anchors.json',
        '  node rawr-cli.js manifest-verify --manifest demo.manifest.json --public-key signing.pub.pem --key-id trusted-root',
    ].join('\n');
    process.stdout.write(text + '\n');
}

function fail(message, code = 1) {
    process.stderr.write('ERROR: ' + message + '\n');
    process.exit(code);
}

function parseArgs(argv) {
    const args = { _: [] };
    for (let index = 0; index < argv.length; index++) {
        const token = argv[index];
        if (!token.startsWith('--')) {
            args._.push(token);
            continue;
        }
        const key = token.slice(2);
        const next = argv[index + 1];
        if (!next || next.startsWith('--')) {
            args[key] = true;
            continue;
        }
        args[key] = next;
        index++;
    }
    return args;
}

function readJson(filePath) {
    const raw = fs.readFileSync(filePath, 'utf8');
    return JSON.parse(raw);
}

function writeJson(filePath, value) {
    fs.writeFileSync(filePath, JSON.stringify(value, null, 2) + '\n', 'utf8');
}

function readPem(filePath) {
    return fs.readFileSync(filePath, 'utf8');
}

function cmdManifestInit(args) {
    const id = String(args.id || '').trim();
    if (!id) fail('manifest-init requires --id');

    const outFile = String(args.out || (id + '.manifest.json')).trim();
    const displayName = String(args.name || id).trim();

    const manifest = {
        schemaVersion: 1,
        id,
        name: displayName,
        abiVersion: 1,
        requires: [
            'capability:filesystem@1',
        ],
        processAllowlist: {
            version: 1,
            commands: [],
        },
        networkAllowlist: {
            version: 1,
            hosts: [],
        },
    };

    writeJson(outFile, manifest);
    process.stdout.write('WROTE ' + path.resolve(outFile) + '\n');
}

function cmdManifestSign(args) {
    const manifestPath = String(args.manifest || '').trim();
    const keyPath = String(args.key || '').trim();
    const keyId = String(args['key-id'] || '').trim();
    if (!manifestPath) fail('manifest-sign requires --manifest');
    if (!keyPath) fail('manifest-sign requires --key');
    if (!keyId) fail('manifest-sign requires --key-id');

    const manifest = readJson(manifestPath);
    delete manifest.signature;

    const canonical = canonicalizeManifestForSigning(manifest);
    const privateKey = crypto.createPrivateKey(readPem(keyPath));
    const signature = crypto.sign(null, Buffer.from(canonical, 'utf8'), privateKey).toString('base64');

    const signed = {
        ...manifest,
        signature: {
            algorithm: 'ed25519',
            keyId,
            value: signature,
        },
    };

    const outFile = String(args.out || manifestPath).trim();
    writeJson(outFile, signed);
    process.stdout.write('SIGNED ' + path.resolve(outFile) + '\n');
}

function cmdManifestVerify(args) {
    const manifestPath = String(args.manifest || '').trim();
    if (!manifestPath) fail('manifest-verify requires --manifest');

    let trustAnchors = null;
    const trustPath = String(args.trust || '').trim();
    const publicKeyPath = String(args['public-key'] || '').trim();
    const keyId = String(args['key-id'] || '').trim();

    if (trustPath) {
        trustAnchors = readJson(trustPath);
    } else if (publicKeyPath && keyId) {
        trustAnchors = { [keyId]: readPem(publicKeyPath) };
    } else {
        fail('manifest-verify requires --trust OR (--public-key and --key-id)');
    }

    const manifest = readJson(manifestPath);
    const result = verifyManifestSignature(manifest, trustAnchors);
    if (!result.ok) {
        fail('verify_failed:' + result.error, 2);
    }

    process.stdout.write('OK keyId=' + result.keyId + ' hash=' + result.hash + '\n');
}

function resolveTrustAnchors(args, requireTrust) {
    const trustPath = String(args.trust || '').trim();
    const publicKeyPath = String(args['public-key'] || '').trim();
    const keyId = String(args['key-id'] || '').trim();

    if (trustPath) {
        return readJson(trustPath);
    }
    if (publicKeyPath && keyId) {
        return { [keyId]: readPem(publicKeyPath) };
    }
    if (requireTrust) {
        fail('manifest verification requires --trust OR (--public-key and --key-id)');
    }
    return null;
}

function cmdManifestLint(args) {
    const manifestPath = String(args.manifest || '').trim();
    if (!manifestPath) fail('manifest-lint requires --manifest');

    const manifest = readJson(manifestPath);
    const trustAnchors = resolveTrustAnchors(args, false);
    let verification = null;

    if (manifest.signature) {
        if (!trustAnchors) {
            fail('manifest-lint on signed manifests requires trust anchors', 2);
        }
        verification = verifyManifestSignature(manifest, trustAnchors);
        if (!verification.ok) {
            fail('lint_failed:' + verification.error, 2);
        }
    }

    try {
        manager._test._negotiateManifestOrThrow(manifest, JSON.stringify(manifest));
    } catch (error) {
        fail('lint_failed:' + (error && error.message ? error.message : 'manifest_invalid'), 2);
    }

    const requires = Array.isArray(manifest.requires) ? manifest.requires.length : 0;
    const signedState = verification ? ` signed keyId=${verification.keyId}` : ' unsigned';
    process.stdout.write(`LINT_OK requires=${requires}${signedState}\n`);
}

function main() {
    const argv = process.argv.slice(2);
    if (argv.length === 0 || argv[0] === 'help' || argv[0] === '--help' || argv[0] === '-h') {
        printHelp();
        return;
    }

    const command = argv[0];
    const args = parseArgs(argv.slice(1));

    switch (command) {
    case 'manifest-init':
        cmdManifestInit(args);
        return;
    case 'manifest-sign':
        cmdManifestSign(args);
        return;
    case 'manifest-lint':
        cmdManifestLint(args);
        return;
    case 'manifest-verify':
        cmdManifestVerify(args);
        return;
    default:
        fail('unknown_command:' + command);
    }
}

main();
