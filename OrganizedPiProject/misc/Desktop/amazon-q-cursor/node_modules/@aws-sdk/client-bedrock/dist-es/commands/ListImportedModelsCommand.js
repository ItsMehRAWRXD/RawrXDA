import { getEndpointPlugin } from "@smithy/middleware-endpoint";
import { getSerdePlugin } from "@smithy/middleware-serde";
import { Command as $Command } from "@smithy/smithy-client";
import { commonParams } from "../endpoint/EndpointParameters";
import { de_ListImportedModelsCommand, se_ListImportedModelsCommand } from "../protocols/Aws_restJson1";
export { $Command };
export class ListImportedModelsCommand extends $Command
    .classBuilder()
    .ep(commonParams)
    .m(function (Command, cs, config, o) {
    return [
        getSerdePlugin(config, this.serialize, this.deserialize),
        getEndpointPlugin(config, Command.getEndpointParameterInstructions()),
    ];
})
    .s("AmazonBedrockControlPlaneService", "ListImportedModels", {})
    .n("BedrockClient", "ListImportedModelsCommand")
    .f(void 0, void 0)
    .ser(se_ListImportedModelsCommand)
    .de(de_ListImportedModelsCommand)
    .build() {
}
