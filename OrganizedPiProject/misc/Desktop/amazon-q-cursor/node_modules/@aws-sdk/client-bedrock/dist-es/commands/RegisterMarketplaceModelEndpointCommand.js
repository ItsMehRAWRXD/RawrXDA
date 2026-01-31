import { getEndpointPlugin } from "@smithy/middleware-endpoint";
import { getSerdePlugin } from "@smithy/middleware-serde";
import { Command as $Command } from "@smithy/smithy-client";
import { commonParams } from "../endpoint/EndpointParameters";
import { de_RegisterMarketplaceModelEndpointCommand, se_RegisterMarketplaceModelEndpointCommand, } from "../protocols/Aws_restJson1";
export { $Command };
export class RegisterMarketplaceModelEndpointCommand extends $Command
    .classBuilder()
    .ep(commonParams)
    .m(function (Command, cs, config, o) {
    return [
        getSerdePlugin(config, this.serialize, this.deserialize),
        getEndpointPlugin(config, Command.getEndpointParameterInstructions()),
    ];
})
    .s("AmazonBedrockControlPlaneService", "RegisterMarketplaceModelEndpoint", {})
    .n("BedrockClient", "RegisterMarketplaceModelEndpointCommand")
    .f(void 0, void 0)
    .ser(se_RegisterMarketplaceModelEndpointCommand)
    .de(de_RegisterMarketplaceModelEndpointCommand)
    .build() {
}
