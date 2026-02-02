// sync-version.cjs
// Synchronises versions across all native and JavaScript build metadata.

const fs = require('fs');
const path = require('path');

const rootDir = __dirname;

const args = process.argv.slice(2);
let requestedVersion = null;
for (let i = 0; i < args.length; i += 1) {
	if (args[i] === '--version' && args[i + 1]) {
		requestedVersion = args[i + 1].trim();
		break;
	}
}

function readJson(filePath) {
	return JSON.parse(fs.readFileSync(filePath, 'utf8'));
}

function writeJson(filePath, data) {
	fs.writeFileSync(filePath, `${JSON.stringify(data, null, 2)}\n`);
}

function updateTextFile(filePath, replacer) {
	const original = fs.readFileSync(filePath, 'utf8');
	const updated = replacer(original);
	if (updated === null) {
		throw new Error(`Failed to update version in ${filePath}`);
	}
	if (updated !== original) {
		fs.writeFileSync(filePath, updated);
	}
}

const packageJsonPath = path.join(rootDir, 'package.json');
const packageLockPath = path.join(rootDir, 'package-lock.json');
const mesonPath = path.join(rootDir, 'meson.build');
const cmakePath = path.join(rootDir, 'CMakeLists.txt');
const vcpkgManifestPath = path.join(rootDir, 'packaging', 'vcpkg', 'helios-network', 'vcpkg.json');
const conanRecipePath = path.join(rootDir, 'packaging', 'conan', 'conanfile.py');
const cxNetworkHeaderPath = path.join(rootDir, 'src', 'native', 'include', 'helios', 'CXNetwork.h');
const pythonPyprojectPath = path.join(rootDir, 'python', 'pyproject.toml');
const pythonMesonPath = path.join(rootDir, 'python', 'meson.build');

const packageJson = readJson(packageJsonPath);
const newVersion = requestedVersion || packageJson.version;

if (!newVersion) {
	throw new Error('Version is not defined. Pass --version <semver> or ensure package.json has a version.');
}

if (requestedVersion) {
	packageJson.version = newVersion;
	writeJson(packageJsonPath, packageJson);
	console.log(`Updated package.json to ${newVersion}`);
}

if (fs.existsSync(packageLockPath)) {
	const packageLock = readJson(packageLockPath);
	if (packageLock.version !== newVersion || (packageLock.packages && packageLock.packages[''] && packageLock.packages[''].version !== newVersion)) {
		packageLock.version = newVersion;
		if (packageLock.packages && packageLock.packages['']) {
			packageLock.packages[''].version = newVersion;
		}
		writeJson(packageLockPath, packageLock);
		console.log(`Updated package-lock.json to ${newVersion}`);
	}
}

updateTextFile(mesonPath, (contents) => {
	const replaced = contents.replace(/version\s*:\s*'[^']*'/, `version : '${newVersion}'`);
	return replaced === contents && !contents.includes(`'${newVersion}'`) ? null : replaced;
});
console.log(`Updated meson.build to ${newVersion}`);

if (fs.existsSync(pythonMesonPath)) {
	updateTextFile(pythonMesonPath, (contents) => {
		const replaced = contents.replace(/version\s*:\s*'[^']*'/, `version : '${newVersion}'`);
		return replaced === contents && !contents.includes(`'${newVersion}'`) ? null : replaced;
	});
	console.log(`Updated python/meson.build to ${newVersion}`);
}

updateTextFile(cmakePath, (contents) => {
	const pattern = /(project\s*\(\s*HeliosNetwork[\s\S]*?VERSION\s+)(\d+\.\d+\.\d+)/;
	if (!pattern.test(contents)) {
		return null;
	}
	return contents.replace(pattern, (_, prefix) => `${prefix}${newVersion}`);
});
console.log(`Updated CMakeLists.txt to ${newVersion}`);

const versionParts = newVersion.split('.');
const majorPart = versionParts[0] || '0';
const minorPart = versionParts[1] || '0';
const patchPart = versionParts[2] || '0';

if (fs.existsSync(cxNetworkHeaderPath)) {
	updateTextFile(cxNetworkHeaderPath, (contents) => {
		let updated = contents;
		const macros = [
			{ regex: /(#define\s+CXNETWORK_VERSION_MAJOR\s+)\d+/, value: `$1${majorPart}` },
			{ regex: /(#define\s+CXNETWORK_VERSION_MINOR\s+)\d+/, value: `$1${minorPart}` },
			{ regex: /(#define\s+CXNETWORK_VERSION_PATCH\s+)\d+/, value: `$1${patchPart}` },
			{ regex: /(#define\s+CXNETWORK_VERSION_STRING\s+)"[^"]*"/, value: `$1"${newVersion}"` },
		];
		for (const macro of macros) {
			if (!macro.regex.test(updated)) {
				return null;
			}
			updated = updated.replace(macro.regex, macro.value);
		}
		return updated;
	});
	console.log(`Updated CXNetwork.h macros to ${newVersion}`);
}

if (fs.existsSync(vcpkgManifestPath)) {
	const vcpkgManifest = readJson(vcpkgManifestPath);
	if (vcpkgManifest.version !== newVersion) {
		vcpkgManifest.version = newVersion;
		writeJson(vcpkgManifestPath, vcpkgManifest);
		console.log(`Updated vcpkg manifest to ${newVersion}`);
	}
}

if (fs.existsSync(conanRecipePath)) {
	updateTextFile(conanRecipePath, (contents) => {
		const pattern = /(version\s*=\s*")(\d+\.\d+\.\d+)(")/;
		if (!pattern.test(contents)) {
			return null;
		}
		return contents.replace(pattern, (_, start, _old, end) => `${start}${newVersion}${end}`);
	});
	console.log(`Updated Conan recipe to ${newVersion}`);
}

if (fs.existsSync(pythonPyprojectPath)) {
	updateTextFile(pythonPyprojectPath, (contents) => {
		const pattern = /(\[project\][\s\S]*?version\s*=\s*\")([^\"]+)(\")/;
		if (!pattern.test(contents)) {
			return null;
		}
		return contents.replace(pattern, (_, start, _old, end) => `${start}${newVersion}${end}`);
	});
	console.log(`Updated python/pyproject.toml to ${newVersion}`);
}

console.log(`Version synchronisation complete: ${newVersion}`);
