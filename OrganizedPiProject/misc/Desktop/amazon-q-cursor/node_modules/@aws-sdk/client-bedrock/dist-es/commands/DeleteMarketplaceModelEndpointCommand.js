import { getEndpointPlugin } from "@smithy/middleware-endpoint";
import { getSerdePlugin } from "@smithy/middleware-serde";
import { Command as $Command } from "@smithy/smithy-client";
import { commonParams } from "../endpoint/EndpointParameters";
import { de_DeleteMarketplaceModelEndpointCommand, se_DeleteMarketplaceModelEndpointCommand, } from "../protocols/Aws_restJson1";
export { $Command };
export class DeleteMarketplaceModelEndpointCommand extends $Command
    .classBuilder()
    .ep(commonParams)
    .m(function (Command, cs, config, o) {
    return [
        getSerdePlugin(config, this.serialize, this.deserialize),
        getEndpointPlugin(config, Command.getEndpointParameterInstructions()),
    ];
})
    .s("AmazonBedrockControlPlaneService", "DeleteMarketplaceModelEndpoint", {})
    .n("BedrockClient", "DeleteMarketplaceModelEndpointCommand")
    .f(void 0, void 0)
    .ser(se_DeleteMarketplaceModelEndpointCommand)
    .de(de_DeleteMarketplaceModelEndpointCommand)
    .build() {
}
