import { getEndpointPlugin } from "@smithy/middleware-endpoint";
import { getSerdePlugin } from "@smithy/middleware-serde";
import { Command as $Command } from "@smithy/smithy-client";
import { commonParams } from "../endpoint/EndpointParameters";
import { de_PutUseCaseForModelAccessCommand, se_PutUseCaseForModelAccessCommand } from "../protocols/Aws_restJson1";
export { $Command };
export class PutUseCaseForModelAccessCommand extends $Command
    .classBuilder()
    .ep(commonParams)
    .m(function (Command, cs, config, o) {
    return [
        getSerdePlugin(config, this.serialize, this.deserialize),
        getEndpointPlugin(config, Command.getEndpointParameterInstructions()),
    ];
})
    .s("AmazonBedrockControlPlaneService", "PutUseCaseForModelAccess", {})
    .n("BedrockClient", "PutUseCaseForModelAccessCommand")
    .f(void 0, void 0)
    .ser(se_PutUseCaseForModelAccessCommand)
    .de(de_PutUseCaseForModelAccessCommand)
    .build() {
}
