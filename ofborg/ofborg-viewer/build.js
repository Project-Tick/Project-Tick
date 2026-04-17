#!/usr/bin/env node
/**
 * Build script for tickborg-viewer.
 * Replaces ancient webpack 3 + Node 6 setup with esbuild.
 */
const { execSync } = require("child_process");
const fs = require("fs");
const path = require("path");

const ROOT = __dirname;
const DIST = path.join(ROOT, "dist");

// Ensure dist/
if (!fs.existsSync(DIST)) {
    fs.mkdirSync(DIST, { recursive: true });
}

// 1. Get git revision + version
let GIT_REVISION = "";
const revFile = path.join(ROOT, ".git-revision");
if (fs.existsSync(revFile)) {
    GIT_REVISION = fs.readFileSync(revFile, "utf-8").trim();
} else {
    try {
        GIT_REVISION = execSync("git rev-parse --short HEAD", { cwd: ROOT, encoding: "utf-8" }).trim();
    } catch (e) {
        // ignore
    }
}
const VERSION = JSON.parse(fs.readFileSync(path.join(ROOT, "package.json"), "utf-8")).version;

// 2. Compile LESS → CSS using Less JS API (avoids ESM extensionless bin issue)
console.log("Compiling LESS...");
const lessFile = path.join(ROOT, "src/styles/index.less");
const helperFile = path.join(ROOT, ".less-compile.mjs");
fs.writeFileSync(helperFile, `
import less from "less";
import { readFileSync } from "fs";
const src = readFileSync(process.argv[2], "utf-8");
const out = await less.render(src, { filename: process.argv[2] });
process.stdout.write(out.css);
`);
const cssOutput = execSync(`node ${helperFile} ${lessFile}`, {
    encoding: "utf-8",
    cwd: ROOT,
});
fs.unlinkSync(helperFile);

// 3. Build JS bundle with esbuild
console.log("Bundling JS...");
execSync(
    `npx esbuild src/index.js --bundle --outfile=dist/bundle.js --format=iife --target=es2018 --minify ` +
    `--define:GIT_REVISION='"${GIT_REVISION}"' --define:VERSION='"${VERSION}"'`,
    { cwd: ROOT, stdio: "inherit" }
);

// 4. Generate index.html
console.log("Generating HTML...");
const html = `<!DOCTYPE html>
<html>
<head>
    <meta name="theme-color" content="#AFFFFF">
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <meta charset="utf-8">
    <title>TickBorg Log Viewer</title>
    <style>
${cssOutput}
    </style>
</head>
<body>
    <div id="tickborg-logviewer">
        <div class="app">
            <div class="loading">
                <strong>Loading...</strong>
                <em>you may need to enable some JavaScript for this to work.</em>
            </div>
        </div>
    </div>
    <script src="bundle.js"></script>
</body>
</html>`;

fs.writeFileSync(path.join(DIST, "index.html"), html);
console.log(`Build complete → ${DIST}/`);
