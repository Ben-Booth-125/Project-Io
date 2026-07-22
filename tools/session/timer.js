#!/usr/bin/env node
// Session wall-clock timer for DEVLOG Runtime lines.
//
// Usage:
//   node tools/session/timer.js start
//   node tools/session/timer.js stop
//
// `start` records the current time to tools/session/.timer_state (gitignored).
// `stop` reads it, prints elapsed wall-clock time, and deletes the state file.

const fs = require("fs");
const path = require("path");

const STATE_FILE = path.join(__dirname, ".timer_state");

function start() {
  fs.writeFileSync(STATE_FILE, String(Date.now()));
  console.log(`Session timer started at ${new Date().toLocaleString()}`);
}

function stop() {
  if (!fs.existsSync(STATE_FILE)) {
    console.log("No timer running — call `start` first.");
    process.exit(1);
  }
  const began = Number(fs.readFileSync(STATE_FILE, "utf8"));
  const elapsedMs = Date.now() - began;
  fs.unlinkSync(STATE_FILE);

  const totalSeconds = Math.floor(elapsedMs / 1000);
  const hours = Math.floor(totalSeconds / 3600);
  const minutes = Math.floor((totalSeconds % 3600) / 60);
  const seconds = totalSeconds % 60;

  const parts = [];
  if (hours) parts.push(`${hours}h`);
  if (hours || minutes) parts.push(`${minutes}m`);
  parts.push(`${seconds}s`);

  console.log(`Session runtime: ${parts.join(" ")}`);
}

const cmd = process.argv[2];
if (cmd === "start") start();
else if (cmd === "stop") stop();
else {
  console.log("Usage: node tools/session/timer.js [start|stop]");
  process.exit(1);
}
