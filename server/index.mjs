import express from "express";
import cors from "cors";
import bodyParser from "body-parser";
import morgan from "morgan";
import dot from "dotenv";
import { deepseek } from "./routes/deepseek.mjs";
dot.config();

async function main() {
  const port = +(process.env.PORT ?? 8080);
  if (!port || !Number.isInteger(port)) {
    console.error("bad port");
    process.exit(1);
  }

  const app = express();
  app.use(morgan("dev"));
  app.use(cors());
  app.use(bodyParser.text());
  app.use((req, res, next) => {
    console.log(req.headers.authorization);
    next();
  });

  // Simple health check
  app.get("/health", (_req, res) => {
    res.json({ ok: true, model: process.env.OLLAMA_MODEL || "deepseek-r1:latest" });
  });

  // Ollama/DeepSeek-R1 API
  app.use("/gpt", await deepseek());

  app.listen(port, () => {
    console.log(`TI-32 Server with DeepSeek-R1 listening on port ${port}`);
    console.log(`Make sure Ollama is running on ${process.env.OLLAMA_URL || 'http://localhost:11434'}`);
  });
}

main();