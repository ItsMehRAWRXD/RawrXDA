import { getEndpointPlugin } from "@smithy/middleware-endpoint";
import { getSerdePlugin } from "@smithy/middleware-serde";
import { Command as $Command } from "@smithy/smithy-client";
import { commonParams } from "../endpoint/EndpointParameters";
import { StartAsyncInvokeRequestFilterSensitiveLog, } from "../models/models_0";
import { de_StartAsyncInvokeCommand, se_StartAsyncInvokeCommand } from "../protocols/Aws_restJson1";
export { $Command };
export class StartAsyncInvokeCommand extends $Command
    .classBuilder()
    .ep(commonParams)
    .m(function (Command, cs, config, o) {
    return [
        getSerdePlugin(config, this.serialize, this.deserialize),
        getEndpointPlugin(config, Command.getEndpointParameterInstructions()),
    ];
})
    .s("AmazonBedrockFrontendService", "StartAsyncInvoke", {})
    .n("BedrockRuntimeClient", "StartAsyncInvokeCommand")
    .f(StartAsyncInvokeRequestFilterSensitiveLog, void 0)
    .ser(se_StartAsyncInvokeCommand)
    .de(de_StartAsyncInvokeCommand)
    .build() {
}
