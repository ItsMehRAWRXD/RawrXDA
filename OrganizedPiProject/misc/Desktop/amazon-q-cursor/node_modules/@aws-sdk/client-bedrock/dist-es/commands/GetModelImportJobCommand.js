import { getEndpointPlugin } from "@smithy/middleware-endpoint";
import { getSerdePlugin } from "@smithy/middleware-serde";
import { Command as $Command } from "@smithy/smithy-client";
import { commonParams } from "../endpoint/EndpointParameters";
import { de_GetModelImportJobCommand, se_GetModelImportJobCommand } from "../protocols/Aws_restJson1";
export { $Command };
export class GetModelImportJobCommand extends $Command
    .classBuilder()
    .ep(commonParams)
    .m(function (Command, cs, config, o) {
    return [
        getSerdePlugin(config, this.serialize, this.deserialize),
        getEndpointPlugin(config, Command.getEndpointParameterInstructions()),
    ];
})
    .s("AmazonBedrockControlPlaneService", "GetModelImportJob", {})
    .n("BedrockClient", "GetModelImportJobCommand")
    .f(void 0, void 0)
    .ser(se_GetModelImportJobCommand)
    .de(de_GetModelImportJobCommand)
    .build() {
}
