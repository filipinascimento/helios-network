import createHeliosModule from '../../moduleFactory.js';

function allocCString(module, text) {
	const str = String(text ?? '');
	const bytes = module.lengthBytesUTF8(str) + 1;
	const ptr = module._malloc(bytes);
	if (!ptr) {
		throw new Error('Failed to allocate string');
	}
	module.stringToUTF8(str, ptr, bytes);
	return {
		ptr,
		free() {
			module._free(ptr);
		},
	};
}

function parseStepOptions(stepOptions) {
	const options = stepOptions ?? {};
	const budget = options.budget == null ? 5000 : options.budget >>> 0;
	const timeoutMs = options.timeoutMs === undefined
		? 60
		: (options.timeoutMs === null ? null : Math.max(0, Number(options.timeoutMs) || 0));
	const chunkBudget = options.chunkBudget == null ? 5000 : Math.max(1, options.chunkBudget >>> 0);
	return { budget, timeoutMs, chunkBudget };
}

function nowMs() {
	return typeof globalThis !== 'undefined' && globalThis.performance && typeof globalThis.performance.now === 'function'
		? globalThis.performance.now()
		: Date.now();
}

async function yieldToLoop(ms) {
	await new Promise((resolve) => setTimeout(resolve, Math.max(0, Number(ms) || 0)));
}

let modulePromise = null;
function getModule() {
	if (!modulePromise) {
		modulePromise = createHeliosModule();
	}
	return modulePromise;
}

function makeResponder(post) {
	return {
		ok(id, data = {}, transfer = undefined) {
			post({ id, ok: true, ...data }, transfer);
		},
		fail(id, error) {
			const message = error instanceof Error ? error.message : String(error ?? 'Unknown error');
			post({ id, ok: false, error: message });
		},
		progress(id, sessionId, data) {
			post({ id, ok: true, event: 'progress', sessionId, data });
		},
	};
}

async function handleCreateLeiden(module, payload) {
	const {
		directed = false,
		nodeCount,
		edgePairsBuffer,
		edgeWeightsBuffer = null,
		edgeWeightAttribute = null,
		resolution = 1,
		seed = 0,
		maxLevels = 32,
		maxPasses = 8,
		outNodeCommunityAttribute = 'community',
	} = payload;

	const pairs = new Uint32Array(edgePairsBuffer);
	const edgeCount = (pairs.length / 2) >>> 0;

	const networkPtr = module._CXNewNetworkWithCapacity(directed ? 1 : 0, nodeCount >>> 0, edgeCount);
	if (!networkPtr) {
		throw new Error('Failed to allocate worker network');
	}

	const nodeOk = module._CXNetworkAddNodes(networkPtr, nodeCount >>> 0, 0);
	if (!nodeOk) {
		module._CXFreeNetwork(networkPtr);
		throw new Error('Failed to add nodes in worker');
	}

	const edgesPtr = module._malloc(pairs.byteLength);
	const outIndicesPtr = module._malloc(Uint32Array.BYTES_PER_ELEMENT * edgeCount);
	if (!edgesPtr || !outIndicesPtr) {
		if (edgesPtr) module._free(edgesPtr);
		if (outIndicesPtr) module._free(outIndicesPtr);
		module._CXFreeNetwork(networkPtr);
		throw new Error('Failed to allocate edge import buffers');
	}

	try {
		module.HEAPU32.set(pairs, edgesPtr / Uint32Array.BYTES_PER_ELEMENT);
		const okAdd = module._CXNetworkAddEdges(networkPtr, edgesPtr, edgeCount, outIndicesPtr);
		if (!okAdd) {
			throw new Error('Failed to add edges in worker');
		}

		if (edgeWeightsBuffer && edgeWeightAttribute) {
			const weights = new Float64Array(edgeWeightsBuffer);
			const name = allocCString(module, edgeWeightAttribute);
			try {
				const okDef = module._CXNetworkDefineEdgeAttribute(networkPtr, name.ptr, /* Double */ 5, 1);
				if (!okDef) {
					throw new Error('Failed to define weight attribute in worker');
				}
				const bufPtr = module._CXNetworkGetEdgeAttributeBuffer(networkPtr, name.ptr);
				if (!bufPtr) {
					throw new Error('Failed to get weight buffer in worker');
				}
				for (let i = 0; i < edgeCount; i += 1) {
					const edgeId = module.HEAPU32[(outIndicesPtr / 4) + i] >>> 0;
					module.HEAPF64[(bufPtr / 8) + edgeId] = weights[i] ?? 0;
				}
				module._CXNetworkBumpEdgeAttributeVersion(networkPtr, name.ptr);
			} finally {
				name.free();
			}
		}

		const weightNamePtr = edgeWeightAttribute ? allocCString(module, edgeWeightAttribute) : null;
		const sessionPtr = module._CXLeidenSessionCreate(
			networkPtr,
			weightNamePtr ? weightNamePtr.ptr : 0,
			Number(resolution),
			seed >>> 0,
			maxLevels >>> 0,
			maxPasses >>> 0
		);
		if (weightNamePtr) {
			weightNamePtr.free();
		}
		if (!sessionPtr) {
			throw new Error('Failed to create Leiden session in worker');
		}

		const outName = allocCString(module, outNodeCommunityAttribute);
		return {
			kind: 'leiden',
			module,
			networkPtr,
			sessionPtr,
			outNamePtr: outName.ptr,
			dispose() {
				outName.free();
				module._CXLeidenSessionDestroy(sessionPtr);
				module._CXFreeNetwork(networkPtr);
			},
			cancelRequested: false,
		};
	} catch (e) {
		module._CXFreeNetwork(networkPtr);
		throw e;
	} finally {
		module._free(edgesPtr);
		module._free(outIndicesPtr);
	}
}

function getLeidenProgress(module, sessionPtr) {
	const scratch = module._malloc(64);
	if (!scratch) {
		throw new Error('Failed to allocate progress scratch');
	}
	try {
		const progressCurrentPtr = scratch + 0;
		const progressTotalPtr = scratch + 8;
		const phasePtr = scratch + 16;
		const levelPtr = scratch + 20;
		const maxLevelsPtr = scratch + 24;
		const passPtr = scratch + 28;
		const maxPassesPtr = scratch + 32;
		const visitedPtr = scratch + 36;
		const nodeCountPtr = scratch + 40;
		const communityCountPtr = scratch + 44;

		module._CXLeidenSessionGetProgress(
			sessionPtr,
			progressCurrentPtr,
			progressTotalPtr,
			phasePtr,
			levelPtr,
			maxLevelsPtr,
			passPtr,
			maxPassesPtr,
			visitedPtr,
			nodeCountPtr,
			communityCountPtr
		);

		return {
			progressCurrent: module.HEAPF64[progressCurrentPtr / 8] ?? 0,
			progressTotal: module.HEAPF64[progressTotalPtr / 8] ?? 0,
			phase: module.HEAPU32[phasePtr / 4] ?? 0,
			level: module.HEAPU32[levelPtr / 4] ?? 0,
			maxLevels: module.HEAPU32[maxLevelsPtr / 4] ?? 0,
			pass: module.HEAPU32[passPtr / 4] ?? 0,
			maxPasses: module.HEAPU32[maxPassesPtr / 4] ?? 0,
			visitedThisPass: module.HEAPU32[visitedPtr / 4] ?? 0,
			nodeCount: module.HEAPU32[nodeCountPtr / 4] ?? 0,
			communityCount: module.HEAPU32[communityCountPtr / 4] ?? 0,
		};
	} finally {
		module._free(scratch);
	}
}

function stepLeiden(session, stepOptions) {
	const module = session.module;
	const { budget, timeoutMs, chunkBudget } = parseStepOptions(stepOptions);

	if (timeoutMs == null) {
		const phase = module._CXLeidenSessionStep(session.sessionPtr, budget);
		return { phase, ...getLeidenProgress(module, session.sessionPtr) };
	}

	const deadline = nowMs() + timeoutMs;
	let phase = 0;
	do {
		phase = module._CXLeidenSessionStep(session.sessionPtr, chunkBudget);
		if (phase === 5 || phase === 6) {
			break;
		}
	} while (nowMs() < deadline);
	return { phase, ...getLeidenProgress(module, session.sessionPtr) };
}

function finalizeLeiden(session) {
	const module = session.module;
	const modularityPtr = module._malloc(8);
	const communityCountPtr = module._malloc(4);
	if (!modularityPtr || !communityCountPtr) {
		if (modularityPtr) module._free(modularityPtr);
		if (communityCountPtr) module._free(communityCountPtr);
		throw new Error('Failed to allocate finalize buffers');
	}
	try {
		module.HEAPF64[modularityPtr / 8] = 0;
		module.HEAPU32[communityCountPtr / 4] = 0;
		const okFinalize = module._CXLeidenSessionFinalize(session.sessionPtr, session.outNamePtr, modularityPtr, communityCountPtr);
		if (!okFinalize) {
			throw new Error('Finalize failed');
		}
		const modularity = module.HEAPF64[modularityPtr / 8] ?? 0;
		const communityCount = module.HEAPU32[communityCountPtr / 4] ?? 0;
		const bufPtr = module._CXNetworkGetNodeAttributeBuffer(session.networkPtr, session.outNamePtr);
		if (!bufPtr) {
			throw new Error('Failed to read community buffer');
		}
		const nodeCount = module._CXNetworkNodeCount(session.networkPtr) >>> 0;
		const view = new Uint32Array(module.HEAPU32.buffer, bufPtr, nodeCount);
		const communities = new Uint32Array(nodeCount);
		communities.set(view);
		return { modularity, communityCount, communitiesBuffer: communities.buffer };
	} finally {
		module._free(modularityPtr);
		module._free(communityCountPtr);
	}
}

async function handleRun(session, responder, requestId, options) {
	const yieldMs = Math.max(0, Number(options?.yieldMs) || 0);
	const stepOptions = options?.stepOptions ?? {};
	let last = null;
	for (;;) {
		if (session.cancelRequested) {
			throw new Error('canceled');
		}
		if (session.kind === 'leiden') {
			last = stepLeiden(session, stepOptions);
			responder.progress(requestId, session.sessionId, last);
			if (last.phase === 5) break;
			if (last.phase === 6) throw new Error('failed');
		} else {
			throw new Error(`Unsupported session kind: ${session.kind}`);
		}
		await yieldToLoop(yieldMs);
	}

	if (session.kind === 'leiden') {
		const result = finalizeLeiden(session);
		responder.ok(requestId, { sessionId: session.sessionId, event: 'done', last, result }, [result.communitiesBuffer]);
	}
}

export function installHeliosSessionWorker({ post, onMessage }) {
	const responder = makeResponder(post);
	const sessions = new Map();
	let nextSessionId = 1;

	onMessage(async (message) => {
		const { id, cmd } = message || {};
		if (!id || !cmd) return;
		try {
			const module = await getModule();
			if (cmd === 'create') {
				const { kind, payload } = message;
				if (kind !== 'leiden') {
					throw new Error(`Unsupported session kind: ${kind}`);
				}
				const sessionId = nextSessionId++;
				const session = await handleCreateLeiden(module, payload);
				session.sessionId = sessionId;
				sessions.set(sessionId, session);
				responder.ok(id, { sessionId });
				return;
			}

			if (cmd === 'cancel') {
				const { sessionId } = message;
				const session = sessions.get(sessionId);
				if (session) {
					session.cancelRequested = true;
				}
				responder.ok(id, { sessionId });
				return;
			}

			if (cmd === 'dispose') {
				const { sessionId } = message;
				const session = sessions.get(sessionId);
				if (session) {
					sessions.delete(sessionId);
					session.dispose();
				}
				responder.ok(id, { sessionId });
				return;
			}

			if (cmd === 'run') {
				const { sessionId, options } = message;
				const session = sessions.get(sessionId);
				if (!session) throw new Error('Unknown session');
				await handleRun(session, responder, id, options);
				return;
			}

			throw new Error(`Unknown cmd: ${cmd}`);
		} catch (e) {
			responder.fail(id, e);
		}
	});
}

