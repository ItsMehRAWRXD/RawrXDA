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
  eventStreamHandlingMiddleware: () => eventStreamHandlingMiddleware,
  eventStreamHandlingMiddlewareOptions: () => eventStreamHandlingMiddlewareOptions,
  eventStreamHeaderMiddleware: () => eventStreamHeaderMiddleware,
  eventStreamHeaderMiddlewareOptions: () => eventStreamHeaderMiddlewareOptions,
  getEventStreamPlugin: () => getEventStreamPlugin,
  resolveEventStreamConfig: () => resolveEventStreamConfig
});
module.exports = __toCommonJS(index_exports);

// src/eventStreamConfiguration.ts
function resolveEventStreamConfig(input) {
  const eventSigner = input.signer;
  const messageSigner = input.signer;
  const newInput = Object.assign(input, {
    eventSigner,
    messageSigner
  });
  const eventStreamPayloadHandler = newInput.eventStreamPayloadHandlerProvider(newInput);
  return Object.assign(newInput, {
    eventStreamPayloadHandler
  });
}
__name(resolveEventStreamConfig, "resolveEventStreamConfig");

// src/eventStreamHandlingMiddleware.ts
var import_protocol_http = require("@smithy/protocol-http");
var eventStreamHandlingMiddleware = /* @__PURE__ */ __name((options) => (next, context) => async (args) => {
  const { request } = args;
  if (!import_protocol_http.HttpRequest.isInstance(request)) return next(args);
  return options.eventStreamPayloadHandler.handle(next, args, context);
}, "eventStreamHandlingMiddleware");
var eventStreamHandlingMiddlewareOptions = {
  tags: ["EVENT_STREAM", "SIGNATURE", "HANDLE"],
  name: "eventStreamHandlingMiddleware",
  relation: "after",
  toMiddleware: "awsAuthMiddleware",
  override: true
};

// src/eventStreamHeaderMiddleware.ts

var eventStreamHeaderMiddleware = /* @__PURE__ */ __name((next) => async (args) => {
  const { request } = args;
  if (!import_protocol_http.HttpRequest.isInstance(request)) return next(args);
  request.headers = {
    ...request.headers,
    "content-type": "application/vnd.amazon.eventstream",
    "x-amz-content-sha256": "STREAMING-AWS4-HMAC-SHA256-EVENTS"
  };
  return next({
    ...args,
    request
  });
}, "eventStreamHeaderMiddleware");
var eventStreamHeaderMiddlewareOptions = {
  step: "build",
  tags: ["EVENT_STREAM", "HEADER", "CONTENT_TYPE", "CONTENT_SHA256"],
  name: "eventStreamHeaderMiddleware",
  override: true
};

// src/getEventStreamPlugin.ts
var getEventStreamPlugin = /* @__PURE__ */ __name((options) => ({
  applyToStack: /* @__PURE__ */ __name((clientStack) => {
    clientStack.addRelativeTo(eventStreamHandlingMiddleware(options), eventStreamHandlingMiddlewareOptions);
    clientStack.add(eventStreamHeaderMiddleware, eventStreamHeaderMiddlewareOptions);
  }, "applyToStack")
}), "getEventStreamPlugin");
// Annotate the CommonJS export names for ESM import in node:

0 && (module.exports = {
  resolveEventStreamConfig,
  eventStreamHandlingMiddleware,
  eventStreamHandlingMiddlewareOptions,
  eventStreamHeaderMiddleware,
  eventStreamHeaderMiddlewareOptions,
  getEventStreamPlugin
});

