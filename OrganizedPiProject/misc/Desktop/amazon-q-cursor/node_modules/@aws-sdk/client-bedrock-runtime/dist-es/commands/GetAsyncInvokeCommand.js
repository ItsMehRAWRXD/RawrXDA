import { getEndpointPlugin } from "@smithy/middleware-endpoint";
import { getSerdePlugin } from "@smithy/middleware-serde";
import { Command as $Command } from "@smithy/smithy-client";
import { commonParams } from "../endpoint/EndpointParameters";
import { GetAsyncInvokeResponseFilterSensitiveLog, } from "../models/models_0";
import { de_GetAsyncInvokeCommand, se_GetAsyncInvokeCommand } from "../protocols/Aws_restJson1";
export { $Command };
export class GetAsyncInvokeCommand extends $Command
    .classBuilder()
    .ep(commonParams)
    .m(function (Command, cs, config, o) {
    return [
        getSerdePlugin(config, this.serialize, this.deserialize),
        getEndpointPlugin(config, Command.getEndpointParameterInstructions()),
    ];
})
    .s("AmazonBedrockFrontendService", "GetAsyncInvoke", {})
    .n("BedrockRuntimeClient", "GetAsyncInvokeCommand")
    .f(void 0, GetAsyncInvokeResponseFilterSensitiveLog)
    .ser(se_GetAsyncInvokeCommand)
    .de(de_GetAsyncInvokeCommand)
    .build() {
}
