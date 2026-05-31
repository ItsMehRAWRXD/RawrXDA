'use strict';

const mode = process.env.RAWRXD_EXT_TEST_MODE || 'echo';

let buffer = '';

function writeResponse(obj) {
    process.stdout.write(JSON.stringify(obj) + '\n');
}

function respond(request, success, result, error) {
    writeResponse({
        version: 1,
        kind: 'response',
        requestId: request.requestId,
        type: request.type,
        success,
        result,
        error,
        timestamp: new Date().toISOString(),
        source: 'mock-extension-runtime',
    });
}

process.stdin.setEncoding('utf8');
process.stdin.on('data', (chunk) => {
    buffer += chunk;
    const lines = buffer.split('\n');
    buffer = lines.pop();
    for (const line of lines) {
        const trimmed = line.trim();
        if (!trimmed) continue;
        let request;
        try {
            request = JSON.parse(trimmed);
        } catch {
            continue;
        }

        switch (mode) {
        case 'echo':
            respond(request, true, {
                echoedType: request.type,
                echoedPayload: request.payload,
                source: request.source,
            });
            break;
        case 'malformed-json':
            process.stdout.write('{not-json}\n');
            break;
        case 'invalid-envelope':
            writeResponse({ bogus: true, requestId: request.requestId });
            break;
        case 'timeout':
            break;
        case 'exit-midflight':
            process.exit(13);
            break;
        case 'delayed':
            setTimeout(() => respond(request, true, { delayed: true }), 300);
            break;
        case 'error-response':
            respond(request, false, undefined, 'mock failure');
            break;
        case 'lsp-error-response':
            respond(request, false, undefined, 'lsp_error_code=0x0005');
            break;
        case 'lsp-code-stderr':
            process.stderr.write('lsp_error_code=0x0004\n');
            respond(request, true, { ok: true, via: 'stderr' });
            break;
        default:
            respond(request, true, { mode });
            break;
        }
    }
});