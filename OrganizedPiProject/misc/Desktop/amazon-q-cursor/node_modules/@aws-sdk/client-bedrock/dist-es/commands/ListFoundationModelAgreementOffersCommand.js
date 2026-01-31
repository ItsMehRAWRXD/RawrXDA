import { getEndpointPlugin } from "@smithy/middleware-endpoint";
import { getSerdePlugin } from "@smithy/middleware-serde";
import { Command as $Command } from "@smithy/smithy-client";
import { commonParams } from "../endpoint/EndpointParameters";
import { de_ListFoundationModelAgreementOffersCommand, se_ListFoundationModelAgreementOffersCommand, } from "../protocols/Aws_restJson1";
export { $Command };
export class ListFoundationModelAgreementOffersCommand extends $Command
    .classBuilder()
    .ep(commonParams)
    .m(function (Command, cs, config, o) {
    return [
        getSerdePlugin(config, this.serialize, this.deserialize),
        getEndpointPlugin(config, Command.getEndpointParameterInstructions()),
    ];
})
    .s("AmazonBedrockControlPlaneService", "ListFoundationModelAgreementOffers", {})
    .n("BedrockClient", "ListFoundationModelAgreementOffersCommand")
    .f(void 0, void 0)
    .ser(se_ListFoundationModelAgreementOffersCommand)
    .de(de_ListFoundationModelAgreementOffersCommand)
    .build() {
}
