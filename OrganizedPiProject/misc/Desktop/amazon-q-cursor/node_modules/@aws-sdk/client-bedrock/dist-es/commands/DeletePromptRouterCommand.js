import { getEndpointPlugin } from "@smithy/middleware-endpoint";
import { getSerdePlugin } from "@smithy/middleware-serde";
import { Command as $Command } from "@smithy/smithy-client";
import { commonParams } from "../endpoint/EndpointParameters";
import { de_DeletePromptRouterCommand, se_DeletePromptRouterCommand } from "../protocols/Aws_restJson1";
export { $Command };
export class DeletePromptRouterCommand extends $Command
    .classBuilder()
    .ep(commonParams)
    .m(function (Command, cs, config, o) {
    return [
        getSerdePlugin(config, this.serialize, this.deserialize),
        getEndpointPlugin(config, Command.getEndpointParameterInstructions()),
    ];
})
    .s("AmazonBedrockControlPlaneService", "DeletePromptRouter", {})
    .n("BedrockClient", "DeletePromptRouterCommand")
    .f(void 0, void 0)
    .ser(se_DeletePromptRouterCommand)
    .de(de_DeletePromptRouterCommand)
    .build() {
}
