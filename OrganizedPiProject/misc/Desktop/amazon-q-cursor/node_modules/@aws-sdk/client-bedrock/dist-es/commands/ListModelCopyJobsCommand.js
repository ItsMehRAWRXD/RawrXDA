import { getEndpointPlugin } from "@smithy/middleware-endpoint";
import { getSerdePlugin } from "@smithy/middleware-serde";
import { Command as $Command } from "@smithy/smithy-client";
import { commonParams } from "../endpoint/EndpointParameters";
import { de_ListModelCopyJobsCommand, se_ListModelCopyJobsCommand } from "../protocols/Aws_restJson1";
export { $Command };
export class ListModelCopyJobsCommand extends $Command
    .classBuilder()
    .ep(commonParams)
    .m(function (Command, cs, config, o) {
    return [
        getSerdePlugin(config, this.serialize, this.deserialize),
        getEndpointPlugin(config, Command.getEndpointParameterInstructions()),
    ];
})
    .s("AmazonBedrockControlPlaneService", "ListModelCopyJobs", {})
    .n("BedrockClient", "ListModelCopyJobsCommand")
    .f(void 0, void 0)
    .ser(se_ListModelCopyJobsCommand)
    .de(de_ListModelCopyJobsCommand)
    .build() {
}
