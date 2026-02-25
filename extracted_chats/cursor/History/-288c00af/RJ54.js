import OpenAI from "openai";

const baseURL = process.env.BASE_URL ?? "https://example-tunnel.ngrok-free.app/v1";
const apiKey = process.env.API_KEY ?? "MYSECRET";
const model = process.env.MODEL ?? "deepseek-coder-v2-32k";

async function main() {
  const client = new OpenAI({ baseURL, apiKey });

  console.log(`>>> GET ${baseURL}/models`);
  const models = await client.models.list();
  console.log(models.data.map((m) => m.id));

  console.log("");
  console.log(">>> POST /chat/completions");
  const completion = await client.chat.completions.create({
    model,
    temperature: 0.1,
    messages: [
      { role: "system", content: "You are a helpful coding assistant." },
      { role: "user", content: "Return a Python function that adds two numbers." },
    ],
  });

  console.log(completion.choices[0]?.message?.content ?? "<no content>");
  console.log("");
  console.log("✅ Smoke test complete.");
}

main().catch((err) => {
  console.error("Smoke test failed:", err);
  process.exitCode = 1;
});

