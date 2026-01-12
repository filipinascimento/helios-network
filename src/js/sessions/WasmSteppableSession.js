export class WasmSteppableSession {
	constructor(module, network, ptr, handlers) {
		this.module = module;
		this.network = network;
		this.ptr = ptr;
		this._scratch = 0;
		this._scratchBytes = handlers.scratchBytes ?? 0;
		this._destroy = handlers.destroy;
		this._step = handlers.step;
		this._getProgress = handlers.getProgress;
		this._cancelOn = handlers.cancelOn ?? null;
		this._isTerminalPhase = handlers.isTerminalPhase ?? null;
		this._isDonePhase = handlers.isDonePhase ?? null;
		this._isFailedPhase = handlers.isFailedPhase ?? null;
		this._canceledReason = null;
		this._versionBaseline = this._cancelOn ? this._captureVersions(this._cancelOn) : null;
	}

	_ensureActive() {
		if (this._canceledReason) {
			throw new Error(`Session canceled: ${this._canceledReason}`);
		}
		if (!this.ptr) {
			throw new Error('Session has been disposed');
		}
		this.network._ensureActive();
	}

	_captureVersions(cancelOn) {
		const versions = {};
		const topology = cancelOn.topology ?? null;
		if (topology) {
			versions.topology = this.network.getTopologyVersions();
		}

		const attributes = cancelOn.attributes ?? null;
		if (attributes) {
			const out = { node: {}, edge: {}, network: {} };
			for (const name of attributes.node ?? []) {
				out.node[name] = this.network.getNodeAttributeVersion(name);
			}
			for (const name of attributes.edge ?? []) {
				out.edge[name] = this.network.getEdgeAttributeVersion(name);
			}
			for (const name of attributes.network ?? []) {
				out.network[name] = this.network.getNetworkAttributeVersion(name);
			}
			versions.attributes = out;
		}

		return versions;
	}

	_checkCancellation() {
		if (!this._cancelOn || !this._versionBaseline) {
			return;
		}

		const baseline = this._versionBaseline;
		let reason = null;
		const topologyMode = this._cancelOn.topology ?? null;
		if (topologyMode && baseline.topology) {
			const current = this.network.getTopologyVersions();
			if (!reason && (topologyMode === 'node' || topologyMode === 'both') && current.node !== baseline.topology.node) {
				reason = 'node topology changed';
			}
			if (!reason && (topologyMode === 'edge' || topologyMode === 'both') && current.edge !== baseline.topology.edge) {
				reason = 'edge topology changed';
			}
		}

		const baselineAttributes = baseline.attributes;
		if (!reason && baselineAttributes) {
			for (const [name, version] of Object.entries(baselineAttributes.node ?? {})) {
				if (this.network.getNodeAttributeVersion(name) !== version) {
					reason = `node attribute "${name}" changed`;
					break;
				}
			}
			for (const [name, version] of Object.entries(baselineAttributes.edge ?? {})) {
				if (reason) break;
				if (this.network.getEdgeAttributeVersion(name) !== version) {
					reason = `edge attribute "${name}" changed`;
					break;
				}
			}
			for (const [name, version] of Object.entries(baselineAttributes.network ?? {})) {
				if (reason) break;
				if (this.network.getNetworkAttributeVersion(name) !== version) {
					reason = `network attribute "${name}" changed`;
					break;
				}
			}
		}

		if (reason) {
			this.cancel(reason);
			throw new Error(`Session canceled: ${reason}`);
		}
	}

	_ensureScratch() {
		if (!this._scratchBytes) {
			throw new Error('Session scratch storage is not configured');
		}
		if (this._scratch) {
			return this._scratch;
		}
		this.network._assertCanAllocate('session scratch allocation');
		const ptr = this.module._malloc(this._scratchBytes);
		if (!ptr) {
			throw new Error('Failed to allocate session scratch memory');
		}
		this._scratch = ptr;
		return ptr;
	}

	dispose() {
		if (this.ptr && this._destroy) {
			this._destroy(this.ptr);
			this.ptr = 0;
		}
		if (this._scratch) {
			this.module._free(this._scratch);
			this._scratch = 0;
		}
	}

	cancel(reason = 'canceled') {
		if (this._canceledReason) {
			return;
		}
		this._canceledReason = String(reason || 'canceled');
		this.dispose();
	}

	getProgress() {
		this._ensureActive();
		this._checkCancellation();
		if (!this._getProgress) {
			throw new Error('Session does not support progress reporting');
		}
		return this._getProgress(this.ptr, this._ensureScratch());
	}

	step(options = {}) {
		this._ensureActive();
		this.network._assertCanAllocate('session step');
		this._checkCancellation();
		if (!this._step) {
			throw new Error('Session does not support stepping');
		}
		const {
			budget = 5000,
			timeoutMs = null,
			chunkBudget = 5000,
		} = options;

		const now = typeof globalThis !== 'undefined' && globalThis.performance && typeof globalThis.performance.now === 'function'
			? globalThis.performance.now.bind(globalThis.performance)
			: Date.now;

		const isTerminalPhase = this._isTerminalPhase ?? (() => false);

		if (timeoutMs == null) {
			const phase = this._step(this.ptr, budget >>> 0);
			return { phase, ...this.getProgress() };
		}

		const deadline = now() + Math.max(0, Number(timeoutMs) || 0);
		const budgetPerChunk = Math.max(1, chunkBudget >>> 0);
		let phase = 0;
		do {
			phase = this._step(this.ptr, budgetPerChunk);
			if (isTerminalPhase(phase)) {
				break;
			}
		} while (now() < deadline);

		return { phase, ...this.getProgress() };
	}

	async run(options = {}) {
		this._ensureActive();
		const {
			stepOptions = {},
			yieldMs = 0,
			yield: yieldFn = null,
			onProgress = null,
			signal = null,
			maxIterations = Infinity,
		} = options;

		const defer = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
		const isTerminal = this._isTerminalPhase ?? (() => false);
		const isDone = this._isDonePhase ?? ((phase) => isTerminal(phase));
		const isFailed = this._isFailedPhase ?? (() => false);

		let last = null;
		for (let iteration = 0; iteration < maxIterations; iteration += 1) {
			if (signal && signal.aborted) {
				this.cancel('aborted');
				throw new Error('Session canceled: aborted');
			}
			last = this.step(stepOptions);
			if (typeof onProgress === 'function') {
				onProgress(last);
			}
			if (isDone(last.phase)) {
				return last;
			}
			if (isFailed(last.phase)) {
				throw new Error('Session failed');
			}
			if (typeof yieldFn === 'function') {
				await yieldFn(last);
			} else {
				await defer(Math.max(0, Number(yieldMs) || 0));
			}
		}

		return last;
	}

	isComplete() {
		const progress = this.getProgress();
		const phase = progress?.phase ?? 0;
		if (this._isDonePhase) {
			return Boolean(this._isDonePhase(phase));
		}
		if (this._isTerminalPhase) {
			return Boolean(this._isTerminalPhase(phase));
		}
		return false;
	}

	isFailed() {
		const progress = this.getProgress();
		const phase = progress?.phase ?? 0;
		return this._isFailedPhase ? Boolean(this._isFailedPhase(phase)) : false;
	}
}
