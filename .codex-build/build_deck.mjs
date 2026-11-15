import fs from "node:fs/promises";
import path from "node:path";
import { pathToFileURL } from "node:url";
import { createHash } from "node:crypto";
import { Presentation, PresentationFile } from "@oai/artifact-tool";

const workspaceDir = "/Users/sachinmaurya/Desktop/Projects/Barnes-Hut N-Body Simulator";
const skillDir = "/Users/sachinmaurya/.codex/plugins/cache/openai-primary-runtime/presentations/26.1015.12040/skills/presentations";
const runtimePython = "/Users/sachinmaurya/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3";
const buildDir = path.join(workspaceDir, ".codex-build");
const finalDir = path.join(workspaceDir, "output");
const finalPath = path.join(finalDir, "AZINHACK_2026_Barnes_Hut_Galaxy_Merger_Simulator_final.pptx");
const templatePath = "/Users/sachinmaurya/.codex/attachments/4ed2588c-a693-4ad8-a96d-1205593a917d/design-review.pptx";
const templateSha256 = createHash("sha256").update(await fs.readFile(templatePath)).digest("hex");
await fs.mkdir(buildDir, { recursive: true });
await fs.mkdir(finalDir, { recursive: true });

const C = {
  ink: "#191627", purple: "#7057B8", lavender: "#F0EDF8", muted: "#676078",
  white: "#FFFFFF", rule: "#DDD8E9", pale: "#FAF9FC", orange: "#E77B2B", blue: "#357EBD",
};
const W = 1280, H = 720;
const pres = Presentation.create({ slideSize: { width: W, height: H } });

async function imgBytes(file) {
  const b = await fs.readFile(file);
  return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength);
}
function box(slide, text, x, y, w, h, { size = 18, color = C.ink, bold = false, fill = "none", line = "none", align = "left", font = "Aptos", radius, name } = {}) {
  const s = slide.shapes.add({ geometry: "textbox", name, position: { left: x, top: y, width: w, height: h }, fill, line: { style: "solid", fill: line, width: line === "none" ? 0 : 1 }, borderRadius: radius });
  s.text = text;
  s.text.style = { fontSize: size, color, bold, typeface: font, alignment: align, verticalAlignment: "middle" };
  s.text.insets = 0;
  return s;
}
function rect(slide, x, y, w, h, fill, { line = "none", radius = "rounded-xl", name } = {}) {
  return slide.shapes.add({ geometry: "roundRect", name, position: { left: x, top: y, width: w, height: h }, fill, line: { style: "solid", fill: line, width: line === "none" ? 0 : 1 }, borderRadius: radius });
}
async function titleSlide(title, subtitle, imagePath, note) {
  const s = pres.slides.add();
  s.background.fill = C.ink;
  rect(s, 760, 0, 520, H, C.lavender, { radius: 0 });
  if (imagePath) s.images.add({ blob: await imgBytes(imagePath), contentType: "image/png", alt: "Galaxy merger simulation output", fit: "cover", position: { left: 832, top: 516, width: 336, height: 138 }, geometry: "roundRect", borderRadius: "rounded-xl" });
  box(s, "AZINHACK 2026", 78, 72, 460, 30, { size: 12, color: "#FFFFFF99", bold: true });
  box(s, title, 76, 175, 620, 155, { size: 51, color: C.white, bold: true, font: "Aptos Display" });
  box(s, subtitle, 82, 350, 590, 64, { size: 20, color: "#FFFFFFCC" });
  box(s, "Team", 872, 120, 220, 24, { size: 14, color: C.muted, bold: true });
  box(s, "Sachin, Parth, Mayank", 872, 156, 300, 48, { size: 24, color: C.ink, bold: true, font: "Aptos Display" });
  box(s, "Project", 872, 254, 220, 24, { size: 14, color: C.muted, bold: true });
  box(s, "C++20 physics simulator", 872, 290, 300, 50, { size: 23, color: C.ink, bold: true, font: "Aptos Display" });
  box(s, "Scope", 872, 382, 220, 24, { size: 14, color: C.muted, bold: true });
  box(s, "Educational model\nNot a real collision forecast", 872, 418, 290, 74, { size: 18, color: C.ink });
  s.speakerNotes.text = note; s.speakerNotes.setVisible(true); return s;
}
function base(title, subtitle, num, note) {
  const s = pres.slides.add(); s.background.fill = C.pale;
  box(s, "AZINHACK 2026", 68, 38, 260, 22, { size: 12, color: C.muted, bold: true });
  rect(s, 48, 46, 10, 10, C.purple, { radius: "rounded-full" });
  box(s, String(num).padStart(2, "0"), 1160, 36, 70, 22, { size: 13, color: C.muted, align: "right" });
  box(s, title, 64, 88, 1050, 48, { size: 35, color: C.ink, bold: true, font: "Aptos Display" });
  box(s, subtitle, 66, 145, 1040, 34, { size: 17, color: C.muted });
  rect(s, 64, 666, 1152, 1, C.rule, { radius: 0 });
  box(s, "Barnes–Hut N-Body Galaxy Merger Simulator", 64, 674, 520, 18, { size: 10, color: C.muted });
  s.speakerNotes.text = note; s.speakerNotes.setVisible(true); return s;
}
function labelValue(s, label, value, y, accent = false) {
  rect(s, 94, y, 1000, 76, accent ? C.ink : C.white, { line: C.rule, radius: "rounded-xl" });
  box(s, label, 126, y + 20, 210, 30, { size: 17, color: accent ? C.white : C.purple, bold: true });
  box(s, value, 360, y + 17, 680, 40, { size: 18, color: accent ? "#FFFFFFD0" : C.ink });
}
async function imageWithCaption(s, imagePath, x, y, w, h, caption) {
  s.images.add({ blob: await imgBytes(imagePath), contentType: "image/png", alt: caption, fit: "cover", position: { left: x, top: y, width: w, height: h }, geometry: "roundRect", borderRadius: "rounded-xl" });
  box(s, caption, x, y + h + 12, w, 28, { size: 13, color: C.muted, align: "center" });
}
function miniCard(s, title, body, x, y, w, h, { dark = false } = {}) {
  rect(s, x, y, w, h, dark ? C.ink : C.white, { line: dark ? "none" : C.rule, radius: "rounded-xl" });
  box(s, title, x + 24, y + 22, w - 48, 30, { size: 20, color: dark ? C.white : C.ink, bold: true, font: "Aptos Display" });
  box(s, body, x + 24, y + 64, w - 48, h - 78, { size: 16, color: dark ? "#FFFFFFD0" : C.muted });
}

// 01 Title
await titleSlide("Barnes–Hut N-Body\nGalaxy Merger Simulator", "An interactive learning tool for exploring how gravitational systems evolve over time", path.join(workspaceDir, "results/galaxy_merger_long.png"), "Introduce the simulator as a learning-focused physics project. The visual shows a result from the team’s own simulation workflow. Set the boundary early: this is an illustrative model, not a prediction of a real galaxy collision.");

// 02 Problem
{ const s = base("Problem statement", "Pairwise gravity becomes expensive as the number of particles grows", 2, "Explain the core computational challenge. Each particle feels gravity from all others, so a direct calculation quickly becomes costly as particle counts increase. Students need a way to see the tradeoff between fidelity and computation without treating the result as astrophysical prediction.");
  labelValue(s, "Challenge", "Direct gravity checks every particle pair at every simulation step.", 220, true);
  labelValue(s, "Why it matters", "More particles make a simple educational simulation slower and harder to explore.", 326);
  labelValue(s, "Learning gap", "Learners need visible outputs that connect numerical settings to motion over time.", 432);
}

// 03 Solution
{ const s = base("Proposed solution", "Approximate distant groups of particles while keeping nearby interactions detailed", 3, "The Barnes–Hut method groups distant particles into tree nodes. Instead of evaluating every distant particle individually, the simulator uses a summarized gravitational contribution. The user controls the run and studies the resulting snapshots.");
  miniCard(s, "Input", "Set duration, timestep, total particle count, and starting separation.", 80, 228, 328, 260, { dark: true });
  miniCard(s, "Compute", "Build an octree and use Barnes–Hut force approximations during the simulation.", 476, 228, 328, 260);
  miniCard(s, "Inspect", "Write positions to CSV, then render colored plots and an animation.", 872, 228, 328, 260);
  box(s, "A simplified model for exploring numerical physics through a visible two-group interaction", 150, 548, 980, 34, { size: 19, color: C.purple, bold: true, align: "center" });
}

// 04 Target users
{ const s = base("Target users and use case", "Built for people learning physics, gravity, and computational simulation", 4, "Describe the use case in concrete terms. A student runs the supplied example with 256 particles, time one, timestep 0.01, and separation 10. The simulator saves positions; the plotting workflow turns those positions into a sequence the student can inspect.");
  miniCard(s, "Students", "Explore how initial conditions and timesteps change a simulation.", 80, 220, 320, 230, { dark: true });
  miniCard(s, "Educators", "Use a compact example to connect code, numerical methods, and physical intuition.", 480, 220, 320, 230);
  miniCard(s, "Curious builders", "Inspect a working C++ implementation and visualization pipeline.", 880, 220, 320, 230);
  box(s, "Example: run 256 particles, export CSV snapshots, then follow the two colored groups through the rendered sequence.", 120, 520, 1040, 52, { size: 19, color: C.ink, align: "center" });
}

// 05 AI
{ const s = base("Role of AI or machine learning", "No AI or machine learning is used in the current simulator", 5, "Be direct: the current project is a physics simulation, not an AI product. The user supplies parameters, the program computes a model, and the user interprets the output. This makes the project’s technical contribution clear and avoids overstating capabilities.");
  labelValue(s, "Current method", "Barnes–Hut approximation, numerical integration, and visualization scripts.", 220, true);
  labelValue(s, "Inputs", "Simulation duration, timestep, particle count, separation, and model configuration.", 326);
  labelValue(s, "Human role", "Choose settings, run experiments, and interpret outputs in their educational context.", 432);
}

// 06 Technical approach
{ const s = base("Proposed technical approach", "A C++20 command-line workflow from inputs to an inspectable visual result", 6, "Walk judges through the workflow left to right. The tree groups particles spatially, Barnes–Hut approximates distant forces, and the integrator advances motion. Snapshots are written to CSV before the separate plotting and video steps.");
  const stages = [
    ["User settings", "time · dt · particles · separation"], ["C++20 simulator", "Hernquist setup + leapfrog integration"], ["Octree", "Barnes–Hut force approximation"], ["CSV snapshots", "particle positions over time"], ["Plots & video", "frames, image, MP4"],
  ];
  const xs = [70, 300, 530, 760, 990];
  stages.forEach(([a,b], i) => { rect(s, xs[i], 265, 190, 130, i === 2 ? C.ink : C.white, { line: i === 2 ? "none" : C.rule, radius: "rounded-xl" }); box(s, a, xs[i]+18, 288, 154, 28, { size: 18, color: i === 2 ? C.white : C.ink, bold: true, align: "center", font: "Aptos Display" }); box(s, b, xs[i]+16, 330, 158, 42, { size: 13, color: i === 2 ? "#FFFFFFD0" : C.muted, align: "center" }); if (i < 4) { const line = s.shapes.add({ geometry: "line", position: { left: xs[i]+194, top: 330, width: 30, height: 0 }, line: { style: "solid", fill: C.purple, width: 2 } }); } });
  box(s, "Tech stack: C++20, Apple Clang, CSV, plotting scripts, and FFmpeg video output", 100, 500, 1080, 34, { size: 19, color: C.purple, bold: true, align: "center" });
}

// 07 TinyFish
{ const s = base("TinyFish integration", "Planned research support, not an implemented project feature", 7, "State the distinction clearly. TinyFish is not part of the current build. A future version could use it to gather publicly available astronomy material from selected sources, helping users choose scenarios and understand the context behind simulation inputs.");
  miniCard(s, "Implemented today", "The simulator runs entirely from configured inputs. No TinyFish connection is built or verified.", 104, 232, 460, 260, { dark: true });
  miniCard(s, "Planned use", "Gather selected public astronomy examples or properties to provide scenario context and setting suggestions.", 716, 232, 460, 260);
  box(s, "Status: proposed integration only", 250, 550, 780, 34, { size: 20, color: C.purple, bold: true, align: "center" });
}

// 08 Impact + evidence
{ const s = base("Expected impact", "A clearer bridge from simulation settings to visible motion", 8, "Frame benefits as likely learning value rather than measured outcomes. Report the available evidence precisely: project materials contain saved CSV snapshots, an image, and an approximately ten-second showcase video. No user-study metrics or independent performance benchmarks are claimed.");
  miniCard(s, "Likely benefit", "Students can connect parameters with changing positions across a run.", 74, 218, 300, 224, { dark: true });
  miniCard(s, "Likely benefit", "Educators gain a compact example of approximation in computational physics.", 490, 218, 300, 224);
  miniCard(s, "Verified project output", "CSV snapshots, rendered frames, a showcase image, and a video are present in the project workspace.", 906, 218, 300, 224);
  box(s, "No outcome statistics are claimed. These benefits are expected educational value, not measured impact.", 150, 510, 980, 34, { size: 18, color: C.muted, align: "center" });
}

// 09 Feasibility
{ const s = base("Feasibility", "The core simulation and visualization workflow already run locally", 9, "Focus on what exists. The project has a C++20 CLI, named options, even-particle validation, CSV output, and visualization scripts. A practical next step is to polish documentation and add a reproducible command sequence or simple parameter presets.");
  labelValue(s, "Built", "CLI simulator, parameter options, CSV snapshots, plotting frames, image, and MP4 workflow.", 220, true);
  labelValue(s, "Dependencies", "Apple Clang, CMake, Python with NumPy and Matplotlib, plus FFmpeg for video output.", 326);
  labelValue(s, "Next step", "Finish README review and package a guided example run for students and educators.", 432);
}

// 10 Future
{ const s = base("Future scope", "Practical improvements beyond the hackathon prototype", 10, "These are future ideas, not current features. Keep them tied to the project’s educational purpose: easier parameter exploration, clearer comparison of approximate and direct methods, and optional sourced context through a future TinyFish workflow.");
  miniCard(s, "Guided experiments", "Add curated parameter presets and short questions that prompt observation.", 80, 230, 330, 240, { dark: true });
  miniCard(s, "Method comparison", "Show Barnes–Hut and direct-summation behavior side by side on small cases.", 475, 230, 330, 240);
  miniCard(s, "Context lookup", "Propose TinyFish-supported source gathering for selected public astronomy context.", 870, 230, 330, 240);
}

// 11 Close
{ const s = base("One clear takeaway", "Barnes–Hut makes an otherwise expensive gravity model practical enough to explore and visualize", 11, "Close with the project’s central value: a working C++ learning tool pairs a computational approximation with visual outputs. Mention that no GitHub or live demo link was provided, so the deck does not invent one.");
  rect(s, 78, 220, 690, 250, C.ink, { radius: "rounded-xl" });
  box(s, "A working educational simulator that turns many-body gravity into an experiment students can run and inspect.", 124, 270, 598, 138, { size: 28, color: C.white, bold: true, font: "Aptos Display" });
  await imageWithCaption(s, path.join(workspaceDir, "results/galaxy_merger_showcase.png"), 834, 216, 300, 262, "Project-generated merger snapshot");
  box(s, "GitHub repository: not provided\nLive demo: not provided", 100, 540, 620, 50, { size: 18, color: C.muted });
}

// 12 Checklist
{ const s = base("Submission checklist", "All nine required topics are covered in the presentation", 12, "Use this final slide as a quick checklist for judges. The deck covers every required submission topic and clearly labels both the absence of AI/ML and TinyFish’s planned status.");
  const items = ["Problem statement", "Proposed solution", "Target users and use case", "AI/ML role", "Technical approach", "TinyFish integration", "Expected impact", "Feasibility", "Future scope"];
  items.forEach((item, i) => { const col = i % 3, row = Math.floor(i / 3); const x = 112 + col * 360, y = 220 + row * 105; rect(s, x, y, 314, 68, i === 0 ? C.ink : C.white, { line: i === 0 ? "none" : C.rule, radius: "rounded-xl" }); box(s, "✓", x+24, y+18, 28, 28, { size: 19, color: i === 0 ? C.white : C.purple, bold: true, align: "center" }); box(s, item, x+66, y+18, 220, 28, { size: 16, color: i === 0 ? C.white : C.ink, bold: true }); });
}

for (const [i, slide] of pres.slides.items.entries()) {
  const png = await pres.export({ slide, format: "png", scale: 1 });
  await fs.writeFile(path.join(buildDir, `slide-${String(i+1).padStart(2,"0")}.png`), new Uint8Array(await png.arrayBuffer()));
}
const { finalizePresentation } = await import(pathToFileURL(path.join(skillDir, "container_tools/artifact_tool_utils.mjs")).href);
const candidatePath = path.join(buildDir, "candidate.pptx");
await (await PresentationFile.exportPptx(pres)).save(candidatePath);
const result = await finalizePresentation({
  explicitTotalSlideCount: 12,
  workspaceDir,
  candidatePath,
  finalPath,
  pythonExecutable: runtimePython,
  integrityValidatorPath: path.join(skillDir, "container_tools/inspect_presentation_package_integrity.py"),
  layoutValidatorPath: path.join(skillDir, "container_tools/inspect_presentation_layout_geometry.py"),
  layoutArgs: ["--expected-slide-size-emu", "12192000,6858000", "--validate-bullet-geometry", "--validate-heading-fit"],
  fontPolicy: { basis: "reference", families: ["Aptos", "Aptos Display"], referencePath: templatePath, referenceSha256: templateSha256 },
  verifyArtifactToolImport: true,
  receiptPath: path.join(buildDir, "validation-final.json"),
});
console.log(JSON.stringify({ finalPath, result }, null, 2));
