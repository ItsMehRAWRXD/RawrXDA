import { getEndpointPlugin } from "@smithy/middleware-endpoint";
import { getSerdePlugin } from "@smithy/middleware-serde";
import { Command as $Command } from "@smithy/smithy-client";
import { commonParams } from "../endpoint/EndpointParameters";
import { de_UpdateMarketplaceModelEndpointCommand, se_UpdateMarketplaceModelEndpointCommand, } from "../protocols/Aws_restJson1";
export { $Command };
export class UpdateMarketplaceModelEndpointCommand extends $Command
    .classBuilder()
    .ep(commonParams)
    .m(function (Command, cs, config, o) {
    return [
        getSerdePlugin(config, this.serialize, this.deserialize),
        getEndpointPlugin(config, Command.getEndpointParameterInstructions()),
    ];
})
    .s("AmazonBedrockControlPlaneService", "UpdateMarketplaceModelEndpoint", {})
    .n("BedrockClient", "UpdateMarketplaceModelEndpointCommand")
    .f(void 0, void 0)
    .ser(se_UpdateMarketplaceModelEndpointCommand)
    .de(de_UpdateMarketplaceModelEndpointCommand)
    .build() {
}
