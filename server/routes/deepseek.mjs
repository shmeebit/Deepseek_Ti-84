import express from "express";
import axios from "axios";

export async function deepseek() {
  const routes = express.Router();

  const OLLAMA_URL = process.env.OLLAMA_URL || "http://localhost:11434";
  const MODEL = process.env.OLLAMA_MODEL || "deepseek-r1:latest";

  // simply answer a question
  routes.get("/ask", async (req, res) => {
    const question = req.query.question ?? "";
    if (Array.isArray(question)) {
      res.sendStatus(400);
      return;
    }

    try {
      const response = await axios.post(`${OLLAMA_URL}/api/chat`, {
        model: MODEL,
        messages: [
          {
            role: "system",
            content: "You are a helpful AI assistant. Be concise and clear. Do not use emojis. Provide brief, accurate answers suitable for displaying on a calculator screen."
          },
          { 
            role: "user", 
            content: question 
          }
        ],
        stream: false
      });

      const answer = response.data.message?.content ?? "no response";
      res.send(answer);
    } catch (e) {
      console.error("Ollama Error:", e.message);
      res.status(500).send("Error connecting to Ollama");
    }
  });

  // solve a math equation - simplified without image support
  routes.post("/solve", async (req, res) => {
    try {
      const question_number = req.query.n;
      const question = question_number
        ? `Solve question number ${question_number}.`
        : "Solve this math problem.";

      const prompt = req.body.toString() || question;

      console.log("Math solve prompt:", prompt);

      const response = await axios.post(`${OLLAMA_URL}/api/chat`, {
        model: MODEL,
        messages: [
          {
            role: "system",
            content: "You are a helpful math tutor. Provide answers as succinctly as possible. Be as accurate as possible. Only provide the answer, no explanation."
          },
          {
            role: "user",
            content: prompt
          }
        ],
        stream: false
      });

      const answer = response.data.message?.content ?? "no response";
      res.send(answer);
    } catch (e) {
      console.error("Ollama Error:", e.message);
      res.status(500).send("Error solving problem");
    }
  });

  return routes;
}
