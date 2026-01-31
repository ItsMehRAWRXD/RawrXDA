import { getEndpointPlugin } from "@smithy/middleware-endpoint";
import { getSerdePlugin } from "@smithy/middleware-serde";
import { Command as $Command } from "@smithy/smithy-client";
import { commonParams } from "../endpoint/EndpointParameters";
import { de_DeleteImportedModelCommand, se_DeleteImportedModelCommand } from "../protocols/Aws_restJson1";
export { $Command };
export class DeleteImportedModelCommand extends $Command
    .classBuilder()
    .ep(commonParams)
    .m(function (Command, cs, config, o) {
    return [
        getSerdePlugin(config, this.serialize, this.deserialize),
        getEndpointPlugin(config, Command.getEndpointParameterInstructions()),
    ];
})
    .s("AmazonBedrockControlPlaneService", "DeleteImportedModel", {})
    .n("BedrockClient", "DeleteImportedModelCommand")
    .f(void 0, void 0)
    .ser(se_DeleteImportedModelCommand)
    .de(de_DeleteImportedModelCommand)
    .build() {
}
