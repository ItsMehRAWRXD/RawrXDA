import { getEndpointPlugin } from "@smithy/middleware-endpoint";
import { getSerdePlugin } from "@smithy/middleware-serde";
import { Command as $Command } from "@smithy/smithy-client";
import { commonParams } from "../endpoint/EndpointParameters";
import { de_GetMarketplaceModelEndpointCommand, se_GetMarketplaceModelEndpointCommand, } from "../protocols/Aws_restJson1";
export { $Command };
export class GetMarketplaceModelEndpointCommand extends $Command
    .classBuilder()
    .ep(commonParams)
    .m(function (Command, cs, config, o) {
    return [
        getSerdePlugin(config, this.serialize, this.deserialize),
        getEndpointPlugin(config, Command.getEndpointParameterInstructions()),
    ];
})
    .s("AmazonBedrockControlPlaneService", "GetMarketplaceModelEndpoint", {})
    .n("BedrockClient", "GetMarketplaceModelEndpointCommand")
    .f(void 0, void 0)
    .ser(se_GetMarketplaceModelEndpointCommand)
    .de(de_GetMarketplaceModelEndpointCommand)
    .build() {
}
