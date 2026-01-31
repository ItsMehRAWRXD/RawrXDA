import { getEndpointPlugin } from "@smithy/middleware-endpoint";
import { getSerdePlugin } from "@smithy/middleware-serde";
import { Command as $Command } from "@smithy/smithy-client";
import { commonParams } from "../endpoint/EndpointParameters";
import { de_CreateModelImportJobCommand, se_CreateModelImportJobCommand } from "../protocols/Aws_restJson1";
export { $Command };
export class CreateModelImportJobCommand extends $Command
    .classBuilder()
    .ep(commonParams)
    .m(function (Command, cs, config, o) {
    return [
        getSerdePlugin(config, this.serialize, this.deserialize),
        getEndpointPlugin(config, Command.getEndpointParameterInstructions()),
    ];
})
    .s("AmazonBedrockControlPlaneService", "CreateModelImportJob", {})
    .n("BedrockClient", "CreateModelImportJobCommand")
    .f(void 0, void 0)
    .ser(se_CreateModelImportJobCommand)
    .de(de_CreateModelImportJobCommand)
    .build() {
}
