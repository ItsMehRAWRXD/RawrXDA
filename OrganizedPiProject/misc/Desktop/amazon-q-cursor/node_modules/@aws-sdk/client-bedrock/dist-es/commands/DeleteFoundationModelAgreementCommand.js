import { getEndpointPlugin } from "@smithy/middleware-endpoint";
import { getSerdePlugin } from "@smithy/middleware-serde";
import { Command as $Command } from "@smithy/smithy-client";
import { commonParams } from "../endpoint/EndpointParameters";
import { de_DeleteFoundationModelAgreementCommand, se_DeleteFoundationModelAgreementCommand, } from "../protocols/Aws_restJson1";
export { $Command };
export class DeleteFoundationModelAgreementCommand extends $Command
    .classBuilder()
    .ep(commonParams)
    .m(function (Command, cs, config, o) {
    return [
        getSerdePlugin(config, this.serialize, this.deserialize),
        getEndpointPlugin(config, Command.getEndpointParameterInstructions()),
    ];
})
    .s("AmazonBedrockControlPlaneService", "DeleteFoundationModelAgreement", {})
    .n("BedrockClient", "DeleteFoundationModelAgreementCommand")
    .f(void 0, void 0)
    .ser(se_DeleteFoundationModelAgreementCommand)
    .de(de_DeleteFoundationModelAgreementCommand)
    .build() {
}
