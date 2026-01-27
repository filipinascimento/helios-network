import pkg from '../../package.json' assert { type: 'json' };

export const PACKAGE_VERSION = typeof pkg?.version === 'string' ? pkg.version : '0.0.0';