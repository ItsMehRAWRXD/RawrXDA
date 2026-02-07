#!/bin/bash
echo "Deleting all stale cursor branches from Star5IDE..."

# Batch 1 - Ollama connection branches
git push origin --delete \
  cursor/add-local-ollama-connection-3cd3 \
  cursor/add-local-ollama-connection-4dc0 \
  cursor/add-local-ollama-connection-bc4e \
  cursor/add-local-ollama-connection-bff6 \
  cursor/add-local-ollama-connection-c435

# Batch 2 - Droplet setup branches  
git push origin --delete \
  cursor/automated-droplet-setup-and-deployment-73fb \
  cursor/automated-droplet-setup-with-cloud-init-0983 \
  cursor/automated-droplet-setup-with-cloud-init-4fc3 \
  cursor/automated-droplet-setup-with-cloud-init-960b \
  cursor/automated-droplet-setup-with-cloud-init-b216

# Batch 3 - Vulnerability scanner branches
git push origin --delete \
  cursor/automated-web-vulnerability-scanner-88fd \
  cursor/automated-web-vulnerability-scanner-efab

# Batch 4 - BC UUID branches (part 1)
git push origin --delete \
  cursor/bc-00b8e443-72d8-441b-8c27-99011a13078c-7be6 \
  cursor/bc-471a45cb-c6c5-466d-80ae-a489fc664379-449e \
  cursor/bc-4efb90f9-f755-48b3-bb0c-dd709754355c-e55c \
  cursor/bc-639570bd-96ac-4b46-85f1-6bc02fcea5c7-1fea \
  cursor/bc-8f0bc901-1b2a-4766-bb5e-0e618ef9e510-bb9f

# Batch 5 - BC UUID branches (part 2)
git push origin --delete \
  cursor/bc-a6643161-6b53-4889-ba1c-7e7e14066b9d-213a \
  cursor/bc-a71d1b40-a9c0-4591-ba27-12a7c249eddf-1f13 \
  cursor/bc-bc4f7be2-9841-42af-9c2b-923611d43467-10b1 \
  cursor/bc-c0d74d77-789a-4d08-bcfb-f8fc697cf6df-ba93 \
  cursor/bc-e689af09-f824-40ef-a1a6-b4bd5b0bf553-1283

# Batch 6 - BC UUID branches (part 3)
git push origin --delete \
  cursor/bc-ec7fb115-6657-4377-84b9-c3b5d9357a1d-27eb

# Batch 7 - Deploy branches (part 1)
git push origin --delete \
  cursor/deploy-application-to-digital-ocean-48fc \
  cursor/deploy-application-to-digital-ocean-87b4 \
  cursor/deploy-application-to-digital-ocean-b5b6 \
  cursor/deploy-application-to-digital-ocean-bd3a \
  cursor/deploy-application-to-digital-ocean-c83c

# Batch 8 - Deploy branches (part 2)
git push origin --delete \
  cursor/deploy-application-to-digital-ocean-cff7 \
  cursor/deploy-application-to-digital-ocean-d09f

# Batch 9 - Remaining branches
git push origin --delete \
  cursor/fetch-all-uncensored-apis-2a77 \
  cursor/finish-application-development-2869 \
  cursor/query-missing-api-endpoints-6d0b \
  cursor/remove-old-build-message-8671

# Batch 10 - Agent update branches
git push origin --delete \
  cursor/update-agent-with-missing-features-4808 \
  cursor/update-agent-with-missing-features-cec9 \
  cursor/update-agent-with-missing-features-f2be

# Delete copilot branch
git push origin --delete \
  copilot/fix-6bfa7582-6018-4ac7-841e-8858c1cb93e3

echo "All stale branches deleted successfully!"