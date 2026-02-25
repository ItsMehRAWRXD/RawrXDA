
import express from "express";
import bodyParser from "body-parser";
import cors from "cors";

const app = express();
const PORT = 11500;

app.use(cors());
app.use(bodyParser.json({ limit: "100mb" }));

// --------------------------------------------------
// Local inference simulation (replace with real model later)
// --------------------------------------------------
function localInference(messages) {
  try {
    const input = messages.map(
      m => `${m.role.toUpperCase()}: ${m.content}`
    ).join("\n");

    const reply =
      "🤖 BigDaddyG (local): processed " +
      messages.length +
      " messages\n" +
      "Message: \"" + messages[messages.length - 1].content + "\"\n\n" +
      "[Local reasoning simulation complete]";

    return reply;
  } catch (err) {
    return "Local inference error: " + err.message;
  }
}

// --------------------------------------------------
// Health check
// --------------------------------------------------
app.get("/health", (req, res) => {
  res.json({
    status: "ok",
    model: "BigDaddyG Local Runtime"
  });
});

// --------------------------------------------------
// Chat endpoint
// --------------------------------------------------
app.post("/api/chat", (req, res) => {
  try {
    const { messages } = req.body;
    if (!messages || !Array.isArray(messages)) {
      return res.status(400).json({ error: "Invalid messages array" });
    }

    const output = localInference(messages);

    res.json({
      message: {
        role: "assistant",
        content: output
      }
    });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// --------------------------------------------------
// Start server
// --------------------------------------------------
app.listen(PORT, () => {
  console.log(`🧠 BigDaddyG ModelServer active on port ${PORT}`);
});
