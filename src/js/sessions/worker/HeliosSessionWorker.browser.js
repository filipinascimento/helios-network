import { installHeliosSessionWorker } from './HeliosSessionWorkerCore.js';

installHeliosSessionWorker({
	post: (message, transfer) => globalThis.postMessage(message, transfer),
	onMessage: (handler) => {
		globalThis.onmessage = (event) => handler(event.data);
	},
});

