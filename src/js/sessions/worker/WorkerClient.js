function isNodeRuntime() {
	try {
		return typeof process !== 'undefined' && !!process.versions?.node;
	} catch {
		return false;
	}
}

async function createNodeWorker(url) {
	const { Worker } = await import('node:worker_threads');
	return new Worker(url, { type: 'module' });
}

export async function createHeliosSessionWorker() {
	if (typeof globalThis.Worker === 'function') {
		let url;
		try {
			url = new URL('./HeliosSessionWorker.browser.js', import.meta.url);
		} catch (e) {
			throw new Error('runWorker() cannot resolve the worker script URL from this bundle. Use an ES module build served from a real URL (not a data:/blob: URL).');
		}

		if (typeof url?.href === 'string' && (url.href.startsWith('data:') || url.href.startsWith('blob:'))) {
			throw new Error('runWorker() is not supported when the library is loaded from a data:/blob: URL. Use an ES module build served from a real URL.');
		}

		// Keep the canonical Vite/Rollup worker pattern so bundlers can emit the worker chunk.
		// eslint-disable-next-line no-new
		return new Worker(new URL('./HeliosSessionWorker.browser.js', import.meta.url), { type: 'module' });
	}
	if (isNodeRuntime()) {
		const url = new URL('./HeliosSessionWorker.node.js', import.meta.url);
		return createNodeWorker(url);
	}
	throw new Error('Workers are not available in this environment');
}

export class WorkerSessionClient {
	constructor(worker) {
		this.worker = worker;
		this._nextId = 1;
		this._pending = new Map();
		this._onProgress = null;
		this._sessionId = 0;

		const handleMessage = (message) => {
			const data = message?.data ?? message;
			if (!data || typeof data !== 'object') return;
			if (data.event === 'progress' && data.sessionId === this._sessionId) {
				if (typeof this._onProgress === 'function') {
					this._onProgress(data.data);
				}
				return;
			}

			const pending = this._pending.get(data.id);
			if (!pending) return;
			if (data.ok) {
				pending.resolve(data);
			} else {
				pending.reject(new Error(data.error || 'Worker error'));
			}
			this._pending.delete(data.id);
		};

		// Browser: worker.onmessage
		// Node: worker.on('message')
		if (typeof worker.on === 'function') {
			worker.on('message', handleMessage);
		} else {
			worker.onmessage = handleMessage;
		}
	}

	_request(cmd, payload = {}, transfer = undefined) {
		const id = this._nextId++;
		const message = { id, cmd, ...payload };
		return new Promise((resolve, reject) => {
			this._pending.set(id, { resolve, reject });
			if (typeof this.worker.postMessage !== 'function') {
				this._pending.delete(id);
				reject(new Error('Worker does not support postMessage'));
				return;
			}
			this.worker.postMessage(message, transfer);
		});
	}

	async create(kind, payload, transfer) {
		const response = await this._request('create', { kind, payload }, transfer);
		this._sessionId = response.sessionId >>> 0;
		return this._sessionId;
	}

	onProgress(handler) {
		this._onProgress = handler;
	}

	run(options = {}) {
		return this._request('run', { sessionId: this._sessionId, options });
	}

	cancel() {
		return this._request('cancel', { sessionId: this._sessionId });
	}

	dispose() {
		return this._request('dispose', { sessionId: this._sessionId });
	}

	terminate() {
		if (typeof this.worker.terminate === 'function') {
			this.worker.terminate();
			return;
		}
		if (typeof this.worker.unref === 'function') {
			this.worker.unref();
		}
	}
}
