/**
 * Primary entry that re-exports the Helios network API.
 * Consumers can import directly from `helios-network`.
 */
export {
	default,
	AttributeType,
	CategorySortOrder,
	DenseColorEncodingFormat,
	DimensionDifferenceMethod,
	NeighborDirection,
	StrengthMeasure,
	ClusteringCoefficientVariant,
	MeasurementExecutionMode,
	getHeliosModule,
	NodeSelector,
	EdgeSelector,
} from './js/HeliosNetwork.js';
