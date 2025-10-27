// sync-version.cjs
// Synchronizes the version from package.json to meson.build

const fs = require('fs');
const path = require('path');

const pkgPath = path.join(__dirname, 'package.json');
const mesonPath = path.join(__dirname, 'meson.build');

// Read version from package.json
const pkg = JSON.parse(fs.readFileSync(pkgPath, 'utf8'));
const version = pkg.version;

// Read meson.build
let meson = fs.readFileSync(mesonPath, 'utf8');

// Replace the version in meson.build
meson = meson.replace(/version\s*:\s*'[^']*'/, `version : '${version}'`);

// Write back to meson.build
fs.writeFileSync(mesonPath, meson);

console.log(`Synchronized version to ${version} in meson.build`);
