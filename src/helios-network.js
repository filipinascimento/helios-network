/**
 * Primary entry that re-exports the Helios network API.
 * Consumers can import directly from `helios-network`.
 */
export {
	default,
	AttributeType,
	CategorySortOrder,
	DimensionDifferenceMethod,
	NeighborDirection,
	StrengthMeasure,
	ClusteringCoefficientVariant,
	MeasurementExecutionMode,
	ConnectedComponentsMode,
	getHeliosModule,
	NodeSelector,
	EdgeSelector,
} from './js/HeliosNetwork.js';
