import { parentPort } from 'node:worker_threads';
import { installHeliosSessionWorker } from './HeliosSessionWorkerCore.js';

if (!parentPort) {
	throw new Error('worker_threads parentPort is not available');
}

installHeliosSessionWorker({
	post: (message, transfer) => parentPort.postMessage(message, transfer),
	onMessage: (handler) => {
		parentPort.on('message', handler);
	},
});

