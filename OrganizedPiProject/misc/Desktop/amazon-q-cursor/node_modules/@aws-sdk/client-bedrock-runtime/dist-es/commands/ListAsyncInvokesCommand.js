import { getEndpointPlugin } from "@smithy/middleware-endpoint";
import { getSerdePlugin } from "@smithy/middleware-serde";
import { Command as $Command } from "@smithy/smithy-client";
import { commonParams } from "../endpoint/EndpointParameters";
import { ListAsyncInvokesResponseFilterSensitiveLog, } from "../models/models_0";
import { de_ListAsyncInvokesCommand, se_ListAsyncInvokesCommand } from "../protocols/Aws_restJson1";
export { $Command };
export class ListAsyncInvokesCommand extends $Command
    .classBuilder()
    .ep(commonParams)
    .m(function (Command, cs, config, o) {
    return [
        getSerdePlugin(config, this.serialize, this.deserialize),
        getEndpointPlugin(config, Command.getEndpointParameterInstructions()),
    ];
})
    .s("AmazonBedrockFrontendService", "ListAsyncInvokes", {})
    .n("BedrockRuntimeClient", "ListAsyncInvokesCommand")
    .f(void 0, ListAsyncInvokesResponseFilterSensitiveLog)
    .ser(se_ListAsyncInvokesCommand)
    .de(de_ListAsyncInvokesCommand)
    .build() {
}
