"use strict";
var __defProp = Object.defineProperty;
var __getOwnPropDesc = Object.getOwnPropertyDescriptor;
var __getOwnPropNames = Object.getOwnPropertyNames;
var __hasOwnProp = Object.prototype.hasOwnProperty;
var __name = (target, value) => __defProp(target, "name", { value, configurable: true });
var __export = (target, all) => {
  for (var name in all)
    __defProp(target, name, { get: all[name], enumerable: true });
};
var __copyProps = (to, from, except, desc) => {
  if (from && typeof from === "object" || typeof from === "function") {
    for (let key of __getOwnPropNames(from))
      if (!__hasOwnProp.call(to, key) && key !== except)
        __defProp(to, key, { get: () => from[key], enumerable: !(desc = __getOwnPropDesc(from, key)) || desc.enumerable });
  }
  return to;
};
var __toCommonJS = (mod) => __copyProps(__defProp({}, "__esModule", { value: true }), mod);

// src/index.ts
var index_exports = {};
__export(index_exports, {
  eventStreamPayloadHandlerProvider: () => eventStreamPayloadHandlerProvider
});
module.exports = __toCommonJS(index_exports);

// src/EventStreamPayloadHandler.ts
var import_eventstream_codec = require("@smithy/eventstream-codec");


// src/EventSigningStream.ts
var import_stream = require("stream");
var EventSigningStream = class extends import_stream.Transform {
  static {
    __name(this, "EventSigningStream");
  }
  priorSignature;
  messageSigner;
  eventStreamCodec;
  systemClockOffsetProvider;
  constructor(options) {
    super({
      autoDestroy: true,
      readableObjectMode: true,
      writableObjectMode: true,
      ...options
    });
    this.priorSignature = options.priorSignature;
    this.eventStreamCodec = options.eventStreamCodec;
    this.messageSigner = options.messageSigner;
    this.systemClockOffsetProvider = options.systemClockOffsetProvider;
  }
  async _transform(chunk, encoding, callback) {
    try {
      const now = new Date(Date.now() + await this.systemClockOffsetProvider());
      const dateHeader = {
        ":date": { type: "timestamp", value: now }
      };
      const signedMessage = await this.messageSigner.sign(
        {
          message: {
            body: chunk,
            headers: dateHeader
          },
          priorSignature: this.priorSignature
        },
        {
          signingDate: now
        }
      );
      this.priorSignature = signedMessage.signature;
      const serializedSigned = this.eventStreamCodec.encode({
        headers: {
          ...dateHeader,
          ":chunk-signature": {
            type: "binary",
            value: getSignatureBinary(signedMessage.signature)
          }
        },
        body: chunk
      });
      this.push(serializedSigned);
      return callback();
    } catch (err) {
      callback(err);
    }
  }
};
function getSignatureBinary(signature) {
  const buf = Buffer.from(signature, "hex");
  return new Uint8Array(buf.buffer, buf.byteOffset, buf.byteLength / Uint8Array.BYTES_PER_ELEMENT);
}
__name(getSignatureBinary, "getSignatureBinary");

// src/EventStreamPayloadHandler.ts
var EventStreamPayloadHandler = class {
  static {
    __name(this, "EventStreamPayloadHandler");
  }
  messageSigner;
  eventStreamCodec;
  systemClockOffsetProvider;
  constructor(options) {
    this.messageSigner = options.messageSigner;
    this.eventStreamCodec = new import_eventstream_codec.EventStreamCodec(options.utf8Encoder, options.utf8Decoder);
    this.systemClockOffsetProvider = async () => options.systemClockOffset ?? 0;
  }
  async handle(next, args, context = {}) {
    const request = args.request;
    const { body: payload, query } = request;
    if (!(payload instanceof import_stream.Readable)) {
      throw new Error("Eventstream payload must be a Readable stream.");
    }
    const payloadStream = payload;
    request.body = new import_stream.PassThrough({
      objectMode: true
    });
    const match = request.headers?.authorization?.match(/Signature=([\w]+)$/);
    const priorSignature = match?.[1] ?? query?.["X-Amz-Signature"] ?? "";
    const signingStream = new EventSigningStream({
      priorSignature,
      eventStreamCodec: this.eventStreamCodec,
      messageSigner: await this.messageSigner(),
      systemClockOffsetProvider: this.systemClockOffsetProvider
    });
    (0, import_stream.pipeline)(payloadStream, signingStream, request.body, (err) => {
      if (err) {
        throw err;
      }
    });
    let result;
    try {
      result = await next(args);
    } catch (e) {
      request.body.end();
      throw e;
    }
    return result;
  }
};

// src/provider.ts
var eventStreamPayloadHandlerProvider = /* @__PURE__ */ __name((options) => new EventStreamPayloadHandler(options), "eventStreamPayloadHandlerProvider");
// Annotate the CommonJS export names for ESM import in node:

0 && (module.exports = {
  eventStreamPayloadHandlerProvider
});

