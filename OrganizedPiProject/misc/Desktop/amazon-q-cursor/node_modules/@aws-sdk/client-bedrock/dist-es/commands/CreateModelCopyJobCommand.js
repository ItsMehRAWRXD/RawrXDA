import { getEndpointPlugin } from "@smithy/middleware-endpoint";
import { getSerdePlugin } from "@smithy/middleware-serde";
import { Command as $Command } from "@smithy/smithy-client";
import { commonParams } from "../endpoint/EndpointParameters";
import { de_CreateModelCopyJobCommand, se_CreateModelCopyJobCommand } from "../protocols/Aws_restJson1";
export { $Command };
export class CreateModelCopyJobCommand extends $Command
    .classBuilder()
    .ep(commonParams)
    .m(function (Command, cs, config, o) {
    return [
        getSerdePlugin(config, this.serialize, this.deserialize),
        getEndpointPlugin(config, Command.getEndpointParameterInstructions()),
    ];
})
    .s("AmazonBedrockControlPlaneService", "CreateModelCopyJob", {})
    .n("BedrockClient", "CreateModelCopyJobCommand")
    .f(void 0, void 0)
    .ser(se_CreateModelCopyJobCommand)
    .de(de_CreateModelCopyJobCommand)
    .build() {
}
