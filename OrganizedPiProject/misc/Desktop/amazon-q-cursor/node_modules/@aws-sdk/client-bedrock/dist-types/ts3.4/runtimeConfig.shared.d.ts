import { AwsSdkSigV4Signer } from "@aws-sdk/core";
import { HttpBearerAuthSigner } from "@smithy/core";
import { IdentityProviderConfig } from "@smithy/types";
import { BedrockClientConfig } from "./BedrockClient";
export declare const getRuntimeConfig: (config: BedrockClientConfig) => {
  apiVersion: string;
  base64Decoder: import("@smithy/types").Decoder;
  base64Encoder: (_input: Uint8Array | string) => string;
  disableHostPrefix: boolean;
  endpointProvider: (
    endpointParams: import("./endpoint/EndpointParameters").EndpointParameters,
    context?: {
      logger?: import("@smithy/types").Logger;
    }
  ) => import("@smithy/types").EndpointV2;
  extensions: import("./runtimeExtensions").RuntimeExtension[];
  httpAuthSchemeProvider: import("./auth/httpAuthSchemeProvider").BedrockHttpAuthSchemeProvider;
  httpAuthSchemes: (
    | {
        schemeId: string;
        identityProvider: (
          ipc: IdentityProviderConfig
        ) =>
          | import("@smithy/types").IdentityProvider<
              import("@smithy/types").Identity
            >
          | undefined;
        signer: AwsSdkSigV4Signer;
      }
    | {
        schemeId: string;
        identityProvider: (
          ipc: IdentityProviderConfig
        ) =>
          | import("@smithy/types").IdentityProvider<
              import("@smithy/types").Identity
            >
          | undefined;
        signer: HttpBearerAuthSigner;
      }
  )[];
  logger: import("@smithy/types").Logger;
  serviceId: string;
  urlParser: import("@smithy/types").UrlParser;
  utf8Decoder: import("@smithy/types").Decoder;
  utf8Encoder: (input: Uint8Array | string) => string;
};
