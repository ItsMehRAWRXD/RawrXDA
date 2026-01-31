import { getEndpointPlugin } from "@smithy/middleware-endpoint";
import { getSerdePlugin } from "@smithy/middleware-serde";
import { Command as $Command } from "@smithy/smithy-client";
import { commonParams } from "../endpoint/EndpointParameters";
import { BatchDeleteEvaluationJobRequestFilterSensitiveLog, BatchDeleteEvaluationJobResponseFilterSensitiveLog, } from "../models/models_0";
import { de_BatchDeleteEvaluationJobCommand, se_BatchDeleteEvaluationJobCommand } from "../protocols/Aws_restJson1";
export { $Command };
export class BatchDeleteEvaluationJobCommand extends $Command
    .classBuilder()
    .ep(commonParams)
    .m(function (Command, cs, config, o) {
    return [
        getSerdePlugin(config, this.serialize, this.deserialize),
        getEndpointPlugin(config, Command.getEndpointParameterInstructions()),
    ];
})
    .s("AmazonBedrockControlPlaneService", "BatchDeleteEvaluationJob", {})
    .n("BedrockClient", "BatchDeleteEvaluationJobCommand")
    .f(BatchDeleteEvaluationJobRequestFilterSensitiveLog, BatchDeleteEvaluationJobResponseFilterSensitiveLog)
    .ser(se_BatchDeleteEvaluationJobCommand)
    .de(de_BatchDeleteEvaluationJobCommand)
    .build() {
}
