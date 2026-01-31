#!/bin/bash
echo "Selective cleanup - preserving valuable features..."

# Archive valuable branches first (checkout and save code)
echo "Archiving valuable features..."
git checkout cursor/automated-web-vulnerability-scanner-88fd
mkdir -p archived-features/vulnerability-scanner
git archive HEAD | tar -x -C archived-features/vulnerability-scanner/

git checkout cursor/automated-droplet-setup-and-deployment-73fb  
mkdir -p archived-features/droplet-automation
git archive HEAD | tar -x -C archived-features/droplet-automation/

git checkout cursor/fetch-all-uncensored-apis-2a77
mkdir -p archived-features/api-discovery
git archive HEAD | tar -x -C archived-features/api-discovery/

git checkout main

# Delete only the UUID/experimental branches (safe to delete)
git push origin --delete \
  cursor/bc-00b8e443-72d8-441b-8c27-99011a13078c-7be6 \
  cursor/bc-471a45cb-c6c5-466d-80ae-a489fc664379-449e \
  cursor/bc-4efb90f9-f755-48b3-bb0c-dd709754355c-e55c \
  cursor/bc-639570bd-96ac-4b46-85f1-6bc02fcea5c7-1fea \
  cursor/bc-8f0bc901-1b2a-4766-bb5e-0e618ef9e510-bb9f \
  cursor/bc-a6643161-6b53-4889-ba1c-7e7e14066b9d-213a \
  cursor/bc-a71d1b40-a9c0-4591-ba27-12a7c249eddf-1f13 \
  cursor/bc-bc4f7be2-9841-42af-9c2b-923611d43467-10b1 \
  cursor/bc-c0d74d77-789a-4d08-bcfb-f8fc697cf6df-ba93 \
  cursor/bc-e689af09-f824-40ef-a1a6-b4bd5b0bf553-1283 \
  cursor/bc-ec7fb115-6657-4377-84b9-c3b5d9357a1d-27eb

# Delete duplicate deployment branches (keep one)
git push origin --delete \
  cursor/deploy-application-to-digital-ocean-87b4 \
  cursor/deploy-application-to-digital-ocean-b5b6 \
  cursor/deploy-application-to-digital-ocean-bd3a \
  cursor/deploy-application-to-digital-ocean-c83c \
  cursor/deploy-application-to-digital-ocean-cff7 \
  cursor/deploy-application-to-digital-ocean-d09f

# Delete duplicate ollama branches (keep one)
git push origin --delete \
  cursor/add-local-ollama-connection-4dc0 \
  cursor/add-local-ollama-connection-bc4e \
  cursor/add-local-ollama-connection-bff6 \
  cursor/add-local-ollama-connection-c435

echo "Selective cleanup complete - valuable features preserved!"