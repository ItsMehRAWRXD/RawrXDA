import { getEndpointPlugin } from "@smithy/middleware-endpoint";
import { getSerdePlugin } from "@smithy/middleware-serde";
import { Command as $Command } from "@smithy/smithy-client";
import { commonParams } from "../endpoint/EndpointParameters";
import { ConverseRequestFilterSensitiveLog, ConverseResponseFilterSensitiveLog, } from "../models/models_0";
import { de_ConverseCommand, se_ConverseCommand } from "../protocols/Aws_restJson1";
export { $Command };
export class ConverseCommand extends $Command
    .classBuilder()
    .ep(commonParams)
    .m(function (Command, cs, config, o) {
    return [
        getSerdePlugin(config, this.serialize, this.deserialize),
        getEndpointPlugin(config, Command.getEndpointParameterInstructions()),
    ];
})
    .s("AmazonBedrockFrontendService", "Converse", {})
    .n("BedrockRuntimeClient", "ConverseCommand")
    .f(ConverseRequestFilterSensitiveLog, ConverseResponseFilterSensitiveLog)
    .ser(se_ConverseCommand)
    .de(de_ConverseCommand)
    .build() {
}
