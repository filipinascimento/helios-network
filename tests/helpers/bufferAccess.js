function primeAttribute(network, scope, name) {
	const meta = network._ensureAttributeMetadata?.(scope, name);
	if (!meta) {
		return;
	}
	network._attributePointers?.(scope, name, meta);
}

export function withNodeBuffer(network, name, fn) {
	primeAttribute(network, 'node', name);
	return network.withBufferAccess(() => fn(network.getNodeAttributeBuffer(name)));
}

export function withEdgeBuffer(network, name, fn) {
	primeAttribute(network, 'edge', name);
	return network.withBufferAccess(() => fn(network.getEdgeAttributeBuffer(name)));
}

export function withNetworkBuffer(network, name, fn) {
	primeAttribute(network, 'network', name);
	return network.withBufferAccess(() => fn(network.getNetworkAttributeBuffer(name)));
}
