#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "CXNetwork.h"
#include "CXNetworkBXNet.h"
#include "CXNetworkXNet.h"

typedef struct {
    PyObject_HEAD
    CXNetworkRef network;
    int owns;
} PyHeliosNetwork;

static PyTypeObject PyHeliosNetworkType;

static int parse_scope(PyObject *obj, CXAttributeScope *out) {
    if (PyLong_Check(obj)) {
        long value = PyLong_AsLong(obj);
        if (value == -1 && PyErr_Occurred()) {
            return -1;
        }
        if (value < CXAttributeScopeNode || value > CXAttributeScopeNetwork) {
            PyErr_SetString(PyExc_ValueError, "Invalid attribute scope");
            return -1;
        }
        *out = (CXAttributeScope)value;
        return 0;
    }
    if (PyUnicode_Check(obj)) {
        const char *value = PyUnicode_AsUTF8(obj);
        if (!value) {
            return -1;
        }
        if (strcmp(value, "node") == 0) {
            *out = CXAttributeScopeNode;
            return 0;
        }
        if (strcmp(value, "edge") == 0) {
            *out = CXAttributeScopeEdge;
            return 0;
        }
        if (strcmp(value, "network") == 0 || strcmp(value, "graph") == 0) {
            *out = CXAttributeScopeNetwork;
            return 0;
        }
    }
    PyErr_SetString(PyExc_ValueError, "Scope must be 'node', 'edge', 'network', or an int enum");
    return -1;
}

static int parse_attribute_type(PyObject *obj, CXAttributeType *out) {
    if (!PyLong_Check(obj)) {
        PyErr_SetString(PyExc_TypeError, "Attribute type must be an integer enum");
        return -1;
    }
    long value = PyLong_AsLong(obj);
    if (value == -1 && PyErr_Occurred()) {
        return -1;
    }
    if (value < 0 || value > 255) {
        PyErr_SetString(PyExc_ValueError, "Invalid attribute type");
        return -1;
    }
    *out = (CXAttributeType)value;
    return 0;
}

static int parse_sort_order(PyObject *obj, CXCategorySortOrder *out) {
    if (PyLong_Check(obj)) {
        long value = PyLong_AsLong(obj);
        if (value == -1 && PyErr_Occurred()) {
            return -1;
        }
        if (value < CX_CATEGORY_SORT_NONE || value > CX_CATEGORY_SORT_NATURAL) {
            PyErr_SetString(PyExc_ValueError, "Invalid category sort order");
            return -1;
        }
        *out = (CXCategorySortOrder)value;
        return 0;
    }
    if (PyUnicode_Check(obj)) {
        const char *value = PyUnicode_AsUTF8(obj);
        if (!value) {
            return -1;
        }
        if (strcmp(value, "none") == 0) {
            *out = CX_CATEGORY_SORT_NONE;
            return 0;
        }
        if (strcmp(value, "frequency") == 0) {
            *out = CX_CATEGORY_SORT_FREQUENCY;
            return 0;
        }
        if (strcmp(value, "alphabetical") == 0) {
            *out = CX_CATEGORY_SORT_ALPHABETICAL;
            return 0;
        }
        if (strcmp(value, "natural") == 0) {
            *out = CX_CATEGORY_SORT_NATURAL;
            return 0;
        }
    }
    PyErr_SetString(PyExc_ValueError, "Sort order must be int or one of: none, frequency, alphabetical, natural");
    return -1;
}

static int parse_dimension_method(PyObject *obj, CXDimensionDifferenceMethod *out) {
    if (obj == NULL || obj == Py_None) {
        *out = CXDimensionLeastSquaresDifferenceMethod;
        return 0;
    }
    if (PyLong_Check(obj)) {
        long value = PyLong_AsLong(obj);
        if (value == -1 && PyErr_Occurred()) {
            return -1;
        }
        if (value < CXDimensionForwardDifferenceMethod || value > CXDimensionLeastSquaresDifferenceMethod) {
            PyErr_SetString(PyExc_ValueError, "Invalid dimension method");
            return -1;
        }
        *out = (CXDimensionDifferenceMethod)value;
        return 0;
    }
    if (PyUnicode_Check(obj)) {
        const char *value = PyUnicode_AsUTF8(obj);
        if (!value) {
            return -1;
        }
        if (strcmp(value, "forward") == 0 || strcmp(value, "fw") == 0) {
            *out = CXDimensionForwardDifferenceMethod;
            return 0;
        }
        if (strcmp(value, "backward") == 0 || strcmp(value, "bk") == 0) {
            *out = CXDimensionBackwardDifferenceMethod;
            return 0;
        }
        if (strcmp(value, "central") == 0 || strcmp(value, "centered") == 0 || strcmp(value, "ce") == 0) {
            *out = CXDimensionCentralDifferenceMethod;
            return 0;
        }
        if (strcmp(value, "leastsquares") == 0 || strcmp(value, "least_squares") == 0 || strcmp(value, "ls") == 0) {
            *out = CXDimensionLeastSquaresDifferenceMethod;
            return 0;
        }
    }
    PyErr_SetString(PyExc_ValueError, "Dimension method must be int or one of: forward, backward, central, leastsquares");
    return -1;
}

static int parse_neighbor_direction(PyObject *obj, CXNeighborDirection *out) {
    if (obj == NULL || obj == Py_None) {
        *out = CXNeighborDirectionBoth;
        return 0;
    }
    if (PyLong_Check(obj)) {
        long value = PyLong_AsLong(obj);
        if (value == -1 && PyErr_Occurred()) {
            return -1;
        }
        if (value < CXNeighborDirectionOut || value > CXNeighborDirectionBoth) {
            PyErr_SetString(PyExc_ValueError, "Invalid neighbor direction");
            return -1;
        }
        *out = (CXNeighborDirection)value;
        return 0;
    }
    if (PyUnicode_Check(obj)) {
        const char *value = PyUnicode_AsUTF8(obj);
        if (!value) {
            return -1;
        }
        if (strcmp(value, "out") == 0 || strcmp(value, "outgoing") == 0) {
            *out = CXNeighborDirectionOut;
            return 0;
        }
        if (strcmp(value, "in") == 0 || strcmp(value, "incoming") == 0) {
            *out = CXNeighborDirectionIn;
            return 0;
        }
        if (strcmp(value, "both") == 0 || strcmp(value, "all") == 0 || strcmp(value, "union") == 0) {
            *out = CXNeighborDirectionBoth;
            return 0;
        }
    }
    PyErr_SetString(PyExc_ValueError, "Direction must be int or one of: out, in, both");
    return -1;
}

static int parse_strength_measure(PyObject *obj, CXStrengthMeasure *out) {
    if (obj == NULL || obj == Py_None) {
        *out = CXStrengthMeasureSum;
        return 0;
    }
    if (PyLong_Check(obj)) {
        long value = PyLong_AsLong(obj);
        if (value == -1 && PyErr_Occurred()) {
            return -1;
        }
        if (value < CXStrengthMeasureSum || value > CXStrengthMeasureMinimum) {
            PyErr_SetString(PyExc_ValueError, "Invalid strength measure");
            return -1;
        }
        *out = (CXStrengthMeasure)value;
        return 0;
    }
    if (PyUnicode_Check(obj)) {
        const char *value = PyUnicode_AsUTF8(obj);
        if (!value) {
            return -1;
        }
        if (strcmp(value, "sum") == 0 || strcmp(value, "total") == 0) {
            *out = CXStrengthMeasureSum;
            return 0;
        }
        if (strcmp(value, "average") == 0 || strcmp(value, "avg") == 0 || strcmp(value, "mean") == 0) {
            *out = CXStrengthMeasureAverage;
            return 0;
        }
        if (strcmp(value, "maximum") == 0 || strcmp(value, "max") == 0) {
            *out = CXStrengthMeasureMaximum;
            return 0;
        }
        if (strcmp(value, "minimum") == 0 || strcmp(value, "min") == 0) {
            *out = CXStrengthMeasureMinimum;
            return 0;
        }
    }
    PyErr_SetString(PyExc_ValueError, "Strength measure must be int or one of: sum, average, maximum, minimum");
    return -1;
}

static int parse_clustering_variant(PyObject *obj, CXClusteringCoefficientVariant *out) {
    if (obj == NULL || obj == Py_None) {
        *out = CXClusteringCoefficientUnweighted;
        return 0;
    }
    if (PyLong_Check(obj)) {
        long value = PyLong_AsLong(obj);
        if (value == -1 && PyErr_Occurred()) {
            return -1;
        }
        if (value < CXClusteringCoefficientUnweighted || value > CXClusteringCoefficientNewman) {
            PyErr_SetString(PyExc_ValueError, "Invalid clustering variant");
            return -1;
        }
        *out = (CXClusteringCoefficientVariant)value;
        return 0;
    }
    if (PyUnicode_Check(obj)) {
        const char *value = PyUnicode_AsUTF8(obj);
        if (!value) {
            return -1;
        }
        if (strcmp(value, "unweighted") == 0 || strcmp(value, "binary") == 0) {
            *out = CXClusteringCoefficientUnweighted;
            return 0;
        }
        if (strcmp(value, "onnela") == 0) {
            *out = CXClusteringCoefficientOnnela;
            return 0;
        }
        if (strcmp(value, "newman") == 0 || strcmp(value, "barrat") == 0 || strcmp(value, "weighted") == 0) {
            *out = CXClusteringCoefficientNewman;
            return 0;
        }
    }
    PyErr_SetString(PyExc_ValueError, "Clustering variant must be int or one of: unweighted, onnela, newman");
    return -1;
}

static int parse_execution_mode(PyObject *obj, CXMeasurementExecutionMode *out) {
    if (obj == NULL || obj == Py_None) {
        *out = CXMeasurementExecutionParallel;
        return 0;
    }
    if (PyLong_Check(obj)) {
        long value = PyLong_AsLong(obj);
        if (value == -1 && PyErr_Occurred()) {
            return -1;
        }
        if (value < CXMeasurementExecutionAuto || value > CXMeasurementExecutionParallel) {
            PyErr_SetString(PyExc_ValueError, "Invalid measurement execution mode");
            return -1;
        }
        *out = (CXMeasurementExecutionMode)value;
        return 0;
    }
    if (PyUnicode_Check(obj)) {
        const char *value = PyUnicode_AsUTF8(obj);
        if (!value) {
            return -1;
        }
        if (strcmp(value, "auto") == 0) {
            *out = CXMeasurementExecutionAuto;
            return 0;
        }
        if (strcmp(value, "single") == 0 || strcmp(value, "single-thread") == 0 || strcmp(value, "singlethread") == 0) {
            *out = CXMeasurementExecutionSingleThread;
            return 0;
        }
        if (strcmp(value, "parallel") == 0 || strcmp(value, "native") == 0) {
            *out = CXMeasurementExecutionParallel;
            return 0;
        }
    }
    PyErr_SetString(PyExc_ValueError, "Execution mode must be int or one of: auto, single-thread, parallel");
    return -1;
}

static int parse_source_nodes(PyObject *obj, CXIndex **outNodes, CXSize *outCount) {
    if (!outNodes || !outCount) {
        PyErr_SetString(PyExc_RuntimeError, "Internal error while parsing source nodes");
        return -1;
    }
    *outNodes = NULL;
    *outCount = 0;

    if (!obj || obj == Py_None) {
        return 0;
    }
    if (PyLong_Check(obj)) {
        unsigned long value = PyLong_AsUnsignedLong(obj);
        if (PyErr_Occurred()) {
            return -1;
        }
        CXIndex *single = (CXIndex *)calloc(1, sizeof(CXIndex));
        if (!single) {
            PyErr_NoMemory();
            return -1;
        }
        single[0] = (CXIndex)value;
        *outNodes = single;
        *outCount = 1;
        return 0;
    }

    PyObject *fast = PySequence_Fast(obj, "source_nodes must be a node id or a sequence of node ids");
    if (!fast) {
        return -1;
    }
    Py_ssize_t pyCount = PySequence_Fast_GET_SIZE(fast);
    if (pyCount <= 0) {
        Py_DECREF(fast);
        return 0;
    }

    CXIndex *nodes = (CXIndex *)calloc((size_t)pyCount, sizeof(CXIndex));
    if (!nodes) {
        Py_DECREF(fast);
        PyErr_NoMemory();
        return -1;
    }
    for (Py_ssize_t i = 0; i < pyCount; i++) {
        PyObject *item = PySequence_Fast_GET_ITEM(fast, i);
        unsigned long value = PyLong_AsUnsignedLong(item);
        if (PyErr_Occurred()) {
            free(nodes);
            Py_DECREF(fast);
            return -1;
        }
        nodes[(size_t)i] = (CXIndex)value;
    }
    Py_DECREF(fast);

    *outNodes = nodes;
    *outCount = (CXSize)pyCount;
    return 0;
}

static PyObject *index_buffer_to_list(const CXIndex *values, CXSize count) {
    PyObject *list = PyList_New((Py_ssize_t)count);
    if (!list) {
        return NULL;
    }
    for (CXSize i = 0; i < count; i++) {
        PyList_SET_ITEM(list, (Py_ssize_t)i, PyLong_FromUnsignedLong((unsigned long)values[i]));
    }
    return list;
}

static CXAttributeRef get_attribute_for_scope(CXNetworkRef network, CXAttributeScope scope, const char *name) {
    switch (scope) {
        case CXAttributeScopeNode:
            return CXNetworkGetNodeAttribute(network, name);
        case CXAttributeScopeEdge:
            return CXNetworkGetEdgeAttribute(network, name);
        case CXAttributeScopeNetwork:
            return CXNetworkGetNetworkAttribute(network, name);
        default:
            return NULL;
    }
}

static PyObject *Network_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) {
    PyHeliosNetwork *self = (PyHeliosNetwork *)type->tp_alloc(type, 0);
    if (!self) {
        return NULL;
    }
    self->network = NULL;
    self->owns = 1;
    return (PyObject *)self;
}

static int Network_init(PyHeliosNetwork *self, PyObject *args, PyObject *kwargs) {
    static const char *kwlist[] = {"directed", "node_capacity", "edge_capacity", NULL};
    int directed = 0;
    Py_ssize_t node_capacity = CXNetwork_INITIAL_NODE_CAPACITY;
    Py_ssize_t edge_capacity = CXNetwork_INITIAL_EDGE_CAPACITY;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|pnn", (char **)kwlist, &directed, &node_capacity, &edge_capacity)) {
        return -1;
    }
    if (node_capacity <= 0 || edge_capacity <= 0) {
        PyErr_SetString(PyExc_ValueError, "Capacities must be positive");
        return -1;
    }

    self->network = CXNewNetworkWithCapacity(directed ? CXTrue : CXFalse, (CXSize)node_capacity, (CXSize)edge_capacity);
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create Helios network");
        return -1;
    }
    return 0;
}

static void Network_dealloc(PyHeliosNetwork *self) {
    if (self->owns && self->network) {
        CXFreeNetwork(self->network);
        self->network = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *Network_is_directed(PyHeliosNetwork *self, void *closure) {
    (void)closure;
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    return PyBool_FromLong(self->network->isDirected ? 1 : 0);
}

static PyObject *Network_node_count(PyHeliosNetwork *self, PyObject *args) {
    (void)args;
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    return PyLong_FromSize_t((size_t)CXNetworkNodeCount(self->network));
}

static PyObject *Network_edge_count(PyHeliosNetwork *self, PyObject *args) {
    (void)args;
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    return PyLong_FromSize_t((size_t)CXNetworkEdgeCount(self->network));
}

static PyObject *Network_node_capacity(PyHeliosNetwork *self, PyObject *args) {
    (void)args;
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    return PyLong_FromSize_t((size_t)CXNetworkNodeCapacity(self->network));
}

static PyObject *Network_edge_capacity(PyHeliosNetwork *self, PyObject *args) {
    (void)args;
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    return PyLong_FromSize_t((size_t)CXNetworkEdgeCapacity(self->network));
}

static PyObject *Network_add_nodes(PyHeliosNetwork *self, PyObject *args) {
    Py_ssize_t count = 0;
    if (!PyArg_ParseTuple(args, "n", &count)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    if (count <= 0) {
        PyErr_SetString(PyExc_ValueError, "Count must be positive");
        return NULL;
    }
    CXIndex *indices = (CXIndex *)calloc((size_t)count, sizeof(CXIndex));
    if (!indices) {
        PyErr_NoMemory();
        return NULL;
    }
    CXBool ok = CXNetworkAddNodes(self->network, (CXSize)count, indices);
    if (!ok) {
        free(indices);
        PyErr_SetString(PyExc_RuntimeError, "Failed to add nodes");
        return NULL;
    }
    PyObject *list = PyList_New((Py_ssize_t)count);
    for (Py_ssize_t i = 0; i < count; i++) {
        PyList_SET_ITEM(list, i, PyLong_FromUnsignedLong((unsigned long)indices[i]));
    }
    free(indices);
    return list;
}

static PyObject *Network_remove_nodes(PyHeliosNetwork *self, PyObject *args) {
    PyObject *seq = NULL;
    if (!PyArg_ParseTuple(args, "O", &seq)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    PyObject *fast = PySequence_Fast(seq, "Expected a sequence of node indices");
    if (!fast) {
        return NULL;
    }
    Py_ssize_t count = PySequence_Fast_GET_SIZE(fast);
    CXIndex *indices = (CXIndex *)calloc((size_t)count, sizeof(CXIndex));
    if (!indices) {
        Py_DECREF(fast);
        PyErr_NoMemory();
        return NULL;
    }
    for (Py_ssize_t i = 0; i < count; i++) {
        PyObject *item = PySequence_Fast_GET_ITEM(fast, i);
        unsigned long value = PyLong_AsUnsignedLong(item);
        if (PyErr_Occurred()) {
            free(indices);
            Py_DECREF(fast);
            return NULL;
        }
        indices[i] = (CXIndex)value;
    }
    CXBool ok = CXNetworkRemoveNodes(self->network, indices, (CXSize)count);
    free(indices);
    Py_DECREF(fast);
    if (!ok) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to remove nodes");
        return NULL;
    }
    Py_RETURN_TRUE;
}

static PyObject *Network_add_edges(PyHeliosNetwork *self, PyObject *args) {
    PyObject *seq = NULL;
    if (!PyArg_ParseTuple(args, "O", &seq)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    PyObject *fast = PySequence_Fast(seq, "Expected a sequence of edge tuples");
    if (!fast) {
        return NULL;
    }
    Py_ssize_t count = PySequence_Fast_GET_SIZE(fast);
    CXEdge *edges = (CXEdge *)calloc((size_t)count, sizeof(CXEdge));
    if (!edges) {
        Py_DECREF(fast);
        PyErr_NoMemory();
        return NULL;
    }
    for (Py_ssize_t i = 0; i < count; i++) {
        PyObject *item = PySequence_Fast_GET_ITEM(fast, i);
        PyObject *edge_tuple = PySequence_Fast(item, "Edge must be a 2-item sequence");
        if (!edge_tuple) {
            free(edges);
            Py_DECREF(fast);
            return NULL;
        }
        if (PySequence_Fast_GET_SIZE(edge_tuple) != 2) {
            Py_DECREF(edge_tuple);
            free(edges);
            Py_DECREF(fast);
            PyErr_SetString(PyExc_ValueError, "Edge must have 2 endpoints");
            return NULL;
        }
        PyObject *from_obj = PySequence_Fast_GET_ITEM(edge_tuple, 0);
        PyObject *to_obj = PySequence_Fast_GET_ITEM(edge_tuple, 1);
        unsigned long from = PyLong_AsUnsignedLong(from_obj);
        unsigned long to = PyLong_AsUnsignedLong(to_obj);
        Py_DECREF(edge_tuple);
        if (PyErr_Occurred()) {
            free(edges);
            Py_DECREF(fast);
            return NULL;
        }
        edges[i].from = (CXUInteger)from;
        edges[i].to = (CXUInteger)to;
    }
    CXIndex *indices = (CXIndex *)calloc((size_t)count, sizeof(CXIndex));
    if (!indices) {
        free(edges);
        Py_DECREF(fast);
        PyErr_NoMemory();
        return NULL;
    }
    CXBool ok = CXNetworkAddEdges(self->network, edges, (CXSize)count, indices);
    free(edges);
    if (!ok) {
        free(indices);
        Py_DECREF(fast);
        PyErr_SetString(PyExc_RuntimeError, "Failed to add edges");
        return NULL;
    }
    PyObject *list = PyList_New((Py_ssize_t)count);
    for (Py_ssize_t i = 0; i < count; i++) {
        PyList_SET_ITEM(list, i, PyLong_FromUnsignedLong((unsigned long)indices[i]));
    }
    free(indices);
    Py_DECREF(fast);
    return list;
}

static PyObject *Network_remove_edges(PyHeliosNetwork *self, PyObject *args) {
    PyObject *seq = NULL;
    if (!PyArg_ParseTuple(args, "O", &seq)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    PyObject *fast = PySequence_Fast(seq, "Expected a sequence of edge indices");
    if (!fast) {
        return NULL;
    }
    Py_ssize_t count = PySequence_Fast_GET_SIZE(fast);
    CXIndex *indices = (CXIndex *)calloc((size_t)count, sizeof(CXIndex));
    if (!indices) {
        Py_DECREF(fast);
        PyErr_NoMemory();
        return NULL;
    }
    for (Py_ssize_t i = 0; i < count; i++) {
        PyObject *item = PySequence_Fast_GET_ITEM(fast, i);
        unsigned long value = PyLong_AsUnsignedLong(item);
        if (PyErr_Occurred()) {
            free(indices);
            Py_DECREF(fast);
            return NULL;
        }
        indices[i] = (CXIndex)value;
    }
    CXBool ok = CXNetworkRemoveEdges(self->network, indices, (CXSize)count);
    free(indices);
    Py_DECREF(fast);
    if (!ok) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to remove edges");
        return NULL;
    }
    Py_RETURN_TRUE;
}

static PyObject *Network_is_node_active(PyHeliosNetwork *self, PyObject *args) {
    unsigned long index = 0;
    if (!PyArg_ParseTuple(args, "k", &index)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    return PyBool_FromLong(CXNetworkIsNodeActive(self->network, (CXIndex)index) ? 1 : 0);
}

static PyObject *Network_is_edge_active(PyHeliosNetwork *self, PyObject *args) {
    unsigned long index = 0;
    if (!PyArg_ParseTuple(args, "k", &index)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    return PyBool_FromLong(CXNetworkIsEdgeActive(self->network, (CXIndex)index) ? 1 : 0);
}

static PyObject *Network_node_indices(PyHeliosNetwork *self, PyObject *args) {
    (void)args;
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    CXSize count = CXNetworkNodeCount(self->network);
    CXIndex *indices = (CXIndex *)calloc((size_t)count, sizeof(CXIndex));
    if (!indices) {
        PyErr_NoMemory();
        return NULL;
    }
    CXSize written = CXNetworkWriteActiveNodes(self->network, indices, count);
    PyObject *list = PyList_New((Py_ssize_t)written);
    for (CXSize i = 0; i < written; i++) {
        PyList_SET_ITEM(list, (Py_ssize_t)i, PyLong_FromUnsignedLong((unsigned long)indices[i]));
    }
    free(indices);
    return list;
}

static PyObject *Network_edge_indices(PyHeliosNetwork *self, PyObject *args) {
    (void)args;
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    CXSize count = CXNetworkEdgeCount(self->network);
    CXIndex *indices = (CXIndex *)calloc((size_t)count, sizeof(CXIndex));
    if (!indices) {
        PyErr_NoMemory();
        return NULL;
    }
    CXSize written = CXNetworkWriteActiveEdges(self->network, indices, count);
    PyObject *list = PyList_New((Py_ssize_t)written);
    for (CXSize i = 0; i < written; i++) {
        PyList_SET_ITEM(list, (Py_ssize_t)i, PyLong_FromUnsignedLong((unsigned long)indices[i]));
    }
    free(indices);
    return list;
}

static PyObject *Network_edge_endpoints(PyHeliosNetwork *self, PyObject *args) {
    unsigned long index = 0;
    if (!PyArg_ParseTuple(args, "k", &index)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    CXEdge *edges = CXNetworkEdgesBuffer(self->network);
    if (!edges) {
        PyErr_SetString(PyExc_RuntimeError, "Edge buffer is not available");
        return NULL;
    }
    CXEdge edge = edges[index];
    return Py_BuildValue("kk", (unsigned long)edge.from, (unsigned long)edge.to);
}

static PyObject *neighbor_container_to_dict(CXNeighborContainer *container) {
    CXSize count = container ? CXNeighborContainerCount(container) : 0;
    CXIndex *nodes = NULL;
    CXIndex *edges = NULL;
    if (count > 0) {
        nodes = (CXIndex *)calloc((size_t)count, sizeof(CXIndex));
        edges = (CXIndex *)calloc((size_t)count, sizeof(CXIndex));
        if (!nodes || !edges) {
            free(nodes);
            free(edges);
            PyErr_NoMemory();
            return NULL;
        }
        CXNeighborContainerGetNodes(container, nodes, count);
        CXNeighborContainerGetEdges(container, edges, count);
    }

    PyObject *nodes_list = index_buffer_to_list(nodes, count);
    PyObject *edges_list = index_buffer_to_list(edges, count);
    free(nodes);
    free(edges);
    if (!nodes_list || !edges_list) {
        Py_XDECREF(nodes_list);
        Py_XDECREF(edges_list);
        return NULL;
    }

    PyObject *result = PyDict_New();
    if (!result) {
        Py_DECREF(nodes_list);
        Py_DECREF(edges_list);
        return NULL;
    }
    PyDict_SetItemString(result, "nodes", nodes_list);
    PyDict_SetItemString(result, "edges", edges_list);
    Py_DECREF(nodes_list);
    Py_DECREF(edges_list);
    return result;
}

static PyObject *Network_out_neighbors(PyHeliosNetwork *self, PyObject *args) {
    unsigned long node = 0;
    if (!PyArg_ParseTuple(args, "k", &node)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    CXNeighborContainer *container = CXNetworkOutNeighbors(self->network, (CXIndex)node);
    if (!container) {
        PyErr_SetString(PyExc_IndexError, "Node index is out of range");
        return NULL;
    }
    return neighbor_container_to_dict(container);
}

static PyObject *Network_in_neighbors(PyHeliosNetwork *self, PyObject *args) {
    unsigned long node = 0;
    if (!PyArg_ParseTuple(args, "k", &node)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    CXNeighborContainer *container = CXNetworkInNeighbors(self->network, (CXIndex)node);
    if (!container) {
        PyErr_SetString(PyExc_IndexError, "Node index is out of range");
        return NULL;
    }
    return neighbor_container_to_dict(container);
}

static PyObject *neighbor_selectors_to_dict(CXNodeSelectorRef nodeSelector, CXEdgeSelectorRef edgeSelector) {
    CXSize nodeCount = nodeSelector ? CXNodeSelectorCount(nodeSelector) : 0;
    CXSize edgeCount = edgeSelector ? CXEdgeSelectorCount(edgeSelector) : 0;
    CXIndex *nodeData = nodeSelector ? CXNodeSelectorData(nodeSelector) : NULL;
    CXIndex *edgeData = edgeSelector ? CXEdgeSelectorData(edgeSelector) : NULL;

    PyObject *nodes_list = index_buffer_to_list(nodeData, nodeCount);
    PyObject *edges_list = index_buffer_to_list(edgeData, edgeCount);
    if (!nodes_list || !edges_list) {
        Py_XDECREF(nodes_list);
        Py_XDECREF(edges_list);
        return NULL;
    }
    PyObject *result = PyDict_New();
    if (!result) {
        Py_DECREF(nodes_list);
        Py_DECREF(edges_list);
        return NULL;
    }
    PyDict_SetItemString(result, "nodes", nodes_list);
    PyDict_SetItemString(result, "edges", edges_list);
    Py_DECREF(nodes_list);
    Py_DECREF(edges_list);
    return result;
}

static PyObject *Network_neighbors(PyHeliosNetwork *self, PyObject *args, PyObject *kwargs) {
    static const char *kwlist[] = {"source_nodes", "direction", "include_source_nodes", NULL};
    PyObject *source_nodes_obj = NULL;
    PyObject *direction_obj = NULL;
    int include_source_nodes = 1;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|Op", (char **)kwlist, &source_nodes_obj, &direction_obj, &include_source_nodes)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }

    CXNeighborDirection direction = CXNeighborDirectionBoth;
    if (parse_neighbor_direction(direction_obj, &direction) != 0) {
        return NULL;
    }

    CXIndex *sources = NULL;
    CXSize source_count = 0;
    if (parse_source_nodes(source_nodes_obj, &sources, &source_count) != 0) {
        return NULL;
    }

    CXNodeSelectorRef nodeSelector = CXNodeSelectorCreate(0);
    CXEdgeSelectorRef edgeSelector = CXEdgeSelectorCreate(0);
    if (!nodeSelector || !edgeSelector) {
        free(sources);
        if (nodeSelector) {
            CXNodeSelectorDestroy(nodeSelector);
        }
        if (edgeSelector) {
            CXEdgeSelectorDestroy(edgeSelector);
        }
        PyErr_NoMemory();
        return NULL;
    }

    CXBool ok = CXNetworkCollectNeighbors(
        self->network,
        sources,
        source_count,
        direction,
        include_source_nodes ? CXTrue : CXFalse,
        nodeSelector,
        edgeSelector
    );
    free(sources);
    if (!ok) {
        CXNodeSelectorDestroy(nodeSelector);
        CXEdgeSelectorDestroy(edgeSelector);
        PyErr_SetString(PyExc_RuntimeError, "Failed to collect neighbors");
        return NULL;
    }

    PyObject *result = neighbor_selectors_to_dict(nodeSelector, edgeSelector);
    CXNodeSelectorDestroy(nodeSelector);
    CXEdgeSelectorDestroy(edgeSelector);
    return result;
}

static PyObject *Network_neighbors_at_level(PyHeliosNetwork *self, PyObject *args, PyObject *kwargs) {
    static const char *kwlist[] = {"source_nodes", "level", "direction", "include_source_nodes", NULL};
    PyObject *source_nodes_obj = NULL;
    Py_ssize_t level = 0;
    PyObject *direction_obj = NULL;
    int include_source_nodes = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "On|Op", (char **)kwlist, &source_nodes_obj, &level, &direction_obj, &include_source_nodes)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    if (level < 0) {
        PyErr_SetString(PyExc_ValueError, "level must be >= 0");
        return NULL;
    }

    CXNeighborDirection direction = CXNeighborDirectionBoth;
    if (parse_neighbor_direction(direction_obj, &direction) != 0) {
        return NULL;
    }

    CXIndex *sources = NULL;
    CXSize source_count = 0;
    if (parse_source_nodes(source_nodes_obj, &sources, &source_count) != 0) {
        return NULL;
    }

    CXNodeSelectorRef nodeSelector = CXNodeSelectorCreate(0);
    CXEdgeSelectorRef edgeSelector = CXEdgeSelectorCreate(0);
    if (!nodeSelector || !edgeSelector) {
        free(sources);
        if (nodeSelector) {
            CXNodeSelectorDestroy(nodeSelector);
        }
        if (edgeSelector) {
            CXEdgeSelectorDestroy(edgeSelector);
        }
        PyErr_NoMemory();
        return NULL;
    }

    CXBool ok = CXNetworkCollectNeighborsAtLevel(
        self->network,
        sources,
        source_count,
        direction,
        (CXSize)level,
        include_source_nodes ? CXTrue : CXFalse,
        nodeSelector,
        edgeSelector
    );
    free(sources);
    if (!ok) {
        CXNodeSelectorDestroy(nodeSelector);
        CXEdgeSelectorDestroy(edgeSelector);
        PyErr_SetString(PyExc_RuntimeError, "Failed to collect neighbors at level");
        return NULL;
    }

    PyObject *result = neighbor_selectors_to_dict(nodeSelector, edgeSelector);
    CXNodeSelectorDestroy(nodeSelector);
    CXEdgeSelectorDestroy(edgeSelector);
    return result;
}

static PyObject *Network_neighbors_up_to_level(PyHeliosNetwork *self, PyObject *args, PyObject *kwargs) {
    static const char *kwlist[] = {"source_nodes", "max_level", "direction", "include_source_nodes", NULL};
    PyObject *source_nodes_obj = NULL;
    Py_ssize_t max_level = 0;
    PyObject *direction_obj = NULL;
    int include_source_nodes = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "On|Op", (char **)kwlist, &source_nodes_obj, &max_level, &direction_obj, &include_source_nodes)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    if (max_level < 0) {
        PyErr_SetString(PyExc_ValueError, "max_level must be >= 0");
        return NULL;
    }

    CXNeighborDirection direction = CXNeighborDirectionBoth;
    if (parse_neighbor_direction(direction_obj, &direction) != 0) {
        return NULL;
    }

    CXIndex *sources = NULL;
    CXSize source_count = 0;
    if (parse_source_nodes(source_nodes_obj, &sources, &source_count) != 0) {
        return NULL;
    }

    CXNodeSelectorRef nodeSelector = CXNodeSelectorCreate(0);
    CXEdgeSelectorRef edgeSelector = CXEdgeSelectorCreate(0);
    if (!nodeSelector || !edgeSelector) {
        free(sources);
        if (nodeSelector) {
            CXNodeSelectorDestroy(nodeSelector);
        }
        if (edgeSelector) {
            CXEdgeSelectorDestroy(edgeSelector);
        }
        PyErr_NoMemory();
        return NULL;
    }

    CXBool ok = CXNetworkCollectNeighborsUpToLevel(
        self->network,
        sources,
        source_count,
        direction,
        (CXSize)max_level,
        include_source_nodes ? CXTrue : CXFalse,
        nodeSelector,
        edgeSelector
    );
    free(sources);
    if (!ok) {
        CXNodeSelectorDestroy(nodeSelector);
        CXEdgeSelectorDestroy(edgeSelector);
        PyErr_SetString(PyExc_RuntimeError, "Failed to collect neighbors up to level");
        return NULL;
    }

    PyObject *result = neighbor_selectors_to_dict(nodeSelector, edgeSelector);
    CXNodeSelectorDestroy(nodeSelector);
    CXEdgeSelectorDestroy(edgeSelector);
    return result;
}

static PyObject *Network_edges_with_indices(PyHeliosNetwork *self, PyObject *args) {
    (void)args;
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    CXSize count = CXNetworkEdgeCount(self->network);
    CXIndex *indices = (CXIndex *)calloc((size_t)count, sizeof(CXIndex));
    if (!indices) {
        PyErr_NoMemory();
        return NULL;
    }
    CXSize written = CXNetworkWriteActiveEdges(self->network, indices, count);
    CXEdge *edges = CXNetworkEdgesBuffer(self->network);
    if (!edges) {
        free(indices);
        PyErr_SetString(PyExc_RuntimeError, "Edge buffer is not available");
        return NULL;
    }
    PyObject *list = PyList_New((Py_ssize_t)written);
    for (CXSize i = 0; i < written; i++) {
        CXIndex edge_index = indices[i];
        CXEdge edge = edges[edge_index];
        PyObject *tuple = Py_BuildValue("k(kk)", (unsigned long)edge_index, (unsigned long)edge.from, (unsigned long)edge.to);
        PyList_SET_ITEM(list, (Py_ssize_t)i, tuple);
    }
    free(indices);
    return list;
}

static PyObject *Network_define_attribute(PyHeliosNetwork *self, PyObject *args) {
    PyObject *scope_obj = NULL;
    const char *name = NULL;
    PyObject *type_obj = NULL;
    Py_ssize_t dimension = 1;
    if (!PyArg_ParseTuple(args, "OsOn", &scope_obj, &name, &type_obj, &dimension)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    if (!name || name[0] == '\0') {
        PyErr_SetString(PyExc_ValueError, "Attribute name is required");
        return NULL;
    }
    if (dimension <= 0) {
        PyErr_SetString(PyExc_ValueError, "Dimension must be positive");
        return NULL;
    }
    CXAttributeScope scope = CXAttributeScopeNode;
    if (parse_scope(scope_obj, &scope) != 0) {
        return NULL;
    }
    CXAttributeType type = CXStringAttributeType;
    if (parse_attribute_type(type_obj, &type) != 0) {
        return NULL;
    }

    CXBool ok = CXFalse;
    switch (scope) {
        case CXAttributeScopeNode:
            ok = CXNetworkDefineNodeAttribute(self->network, name, type, (CXSize)dimension);
            break;
        case CXAttributeScopeEdge:
            ok = CXNetworkDefineEdgeAttribute(self->network, name, type, (CXSize)dimension);
            break;
        case CXAttributeScopeNetwork:
            ok = CXNetworkDefineNetworkAttribute(self->network, name, type, (CXSize)dimension);
            break;
        default:
            break;
    }
    if (!ok) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to define attribute");
        return NULL;
    }
    Py_RETURN_TRUE;
}

static PyObject *Network_list_attributes(PyHeliosNetwork *self, PyObject *args) {
    PyObject *scope_obj = NULL;
    if (!PyArg_ParseTuple(args, "O", &scope_obj)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    CXAttributeScope scope = CXAttributeScopeNode;
    if (parse_scope(scope_obj, &scope) != 0) {
        return NULL;
    }
    CXStringDictionaryRef dict = NULL;
    switch (scope) {
        case CXAttributeScopeNode:
            dict = self->network->nodeAttributes;
            break;
        case CXAttributeScopeEdge:
            dict = self->network->edgeAttributes;
            break;
        case CXAttributeScopeNetwork:
            dict = self->network->networkAttributes;
            break;
        default:
            break;
    }
    if (!dict) {
        return PyList_New(0);
    }
    PyObject *list = PyList_New(0);
    CXStringDictionaryFOR(entry, dict) {
        PyObject *name = PyUnicode_FromString(entry->key ? entry->key : "");
        PyList_Append(list, name);
        Py_DECREF(name);
    }
    return list;
}

static PyObject *Network_attribute_info(PyHeliosNetwork *self, PyObject *args) {
    PyObject *scope_obj = NULL;
    const char *name = NULL;
    if (!PyArg_ParseTuple(args, "Os", &scope_obj, &name)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    CXAttributeScope scope = CXAttributeScopeNode;
    if (parse_scope(scope_obj, &scope) != 0) {
        return NULL;
    }
    CXAttributeRef attr = get_attribute_for_scope(self->network, scope, name);
    if (!attr) {
        PyErr_SetString(PyExc_KeyError, "Attribute not found");
        return NULL;
    }

    PyObject *dict = PyDict_New();
    PyDict_SetItemString(dict, "type", PyLong_FromLong((long)attr->type));
    PyDict_SetItemString(dict, "dimension", PyLong_FromSize_t((size_t)attr->dimension));
    PyDict_SetItemString(dict, "element_size", PyLong_FromSize_t((size_t)attr->elementSize));
    PyDict_SetItemString(dict, "stride", PyLong_FromSize_t((size_t)attr->stride));
    PyDict_SetItemString(dict, "capacity", PyLong_FromSize_t((size_t)attr->capacity));
    PyDict_SetItemString(dict, "version", PyLong_FromUnsignedLongLong((unsigned long long)attr->version));
    PyDict_SetItemString(dict, "uses_javascript_shadow", PyBool_FromLong(attr->usesJavascriptShadow ? 1 : 0));
    return dict;
}

static PyObject *Network_attribute_buffer(PyHeliosNetwork *self, PyObject *args) {
    PyObject *scope_obj = NULL;
    const char *name = NULL;
    if (!PyArg_ParseTuple(args, "Os", &scope_obj, &name)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    CXAttributeScope scope = CXAttributeScopeNode;
    if (parse_scope(scope_obj, &scope) != 0) {
        return NULL;
    }
    CXAttributeRef attr = get_attribute_for_scope(self->network, scope, name);
    if (!attr) {
        PyErr_SetString(PyExc_KeyError, "Attribute not found");
        return NULL;
    }
    if (attr->type == CXDataAttributeMultiCategoryType || attr->type == CXJavascriptAttributeType) {
        PyErr_SetString(PyExc_TypeError, "Attribute does not expose a raw buffer");
        return NULL;
    }
    if (!attr->data) {
        Py_RETURN_NONE;
    }
    size_t size = (size_t)attr->capacity * (size_t)attr->stride;
    return PyMemoryView_FromMemory((char *)attr->data, (Py_ssize_t)size, PyBUF_WRITE);
}

static int write_numeric_value(CXAttributeRef attr, uint8_t *dst, PyObject *value, CXSize dimension) {
    for (CXSize i = 0; i < dimension; i++) {
        PyObject *item = value;
        if (dimension > 1) {
            item = PySequence_Fast_GET_ITEM(value, (Py_ssize_t)i);
        }
        if (!item) {
            PyErr_SetString(PyExc_ValueError, "Invalid attribute value");
            return -1;
        }
        switch (attr->type) {
            case CXBooleanAttributeType: {
                int v = PyObject_IsTrue(item);
                if (v < 0) {
                    return -1;
                }
                uint8_t *ptr = (uint8_t *)(dst + i * attr->elementSize);
                *ptr = (uint8_t)(v ? 1 : 0);
                break;
            }
            case CXFloatAttributeType: {
                double v = PyFloat_AsDouble(item);
                if (PyErr_Occurred()) {
                    return -1;
                }
                float *ptr = (float *)(dst + i * attr->elementSize);
                *ptr = (float)v;
                break;
            }
            case CXDoubleAttributeType: {
                double v = PyFloat_AsDouble(item);
                if (PyErr_Occurred()) {
                    return -1;
                }
                double *ptr = (double *)(dst + i * attr->elementSize);
                *ptr = v;
                break;
            }
            case CXIntegerAttributeType: {
                long long v = PyLong_AsLongLong(item);
                if (PyErr_Occurred()) {
                    return -1;
                }
                int32_t *ptr = (int32_t *)(dst + i * attr->elementSize);
                *ptr = (int32_t)v;
                break;
            }
            case CXUnsignedIntegerAttributeType: {
                unsigned long long v = PyLong_AsUnsignedLongLong(item);
                if (PyErr_Occurred()) {
                    return -1;
                }
                uint32_t *ptr = (uint32_t *)(dst + i * attr->elementSize);
                *ptr = (uint32_t)v;
                break;
            }
            case CXBigIntegerAttributeType: {
                long long v = PyLong_AsLongLong(item);
                if (PyErr_Occurred()) {
                    return -1;
                }
                int64_t *ptr = (int64_t *)(dst + i * attr->elementSize);
                *ptr = (int64_t)v;
                break;
            }
            case CXUnsignedBigIntegerAttributeType: {
                unsigned long long v = PyLong_AsUnsignedLongLong(item);
                if (PyErr_Occurred()) {
                    return -1;
                }
                uint64_t *ptr = (uint64_t *)(dst + i * attr->elementSize);
                *ptr = (uint64_t)v;
                break;
            }
            case CXDataAttributeCategoryType: {
                long long v = PyLong_AsLongLong(item);
                if (PyErr_Occurred()) {
                    return -1;
                }
                int32_t *ptr = (int32_t *)(dst + i * attr->elementSize);
                *ptr = (int32_t)v;
                break;
            }
            default:
                PyErr_SetString(PyExc_TypeError, "Unsupported attribute type");
                return -1;
        }
    }
    return 0;
}

static PyObject *Network_set_attribute_value(PyHeliosNetwork *self, PyObject *args) {
    PyObject *scope_obj = NULL;
    const char *name = NULL;
    unsigned long index = 0;
    PyObject *value = NULL;
    if (!PyArg_ParseTuple(args, "OskO", &scope_obj, &name, &index, &value)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    CXAttributeScope scope = CXAttributeScopeNode;
    if (parse_scope(scope_obj, &scope) != 0) {
        return NULL;
    }
    CXAttributeRef attr = get_attribute_for_scope(self->network, scope, name);
    if (!attr) {
        PyErr_SetString(PyExc_KeyError, "Attribute not found");
        return NULL;
    }
    if (index >= attr->capacity) {
        PyErr_SetString(PyExc_IndexError, "Attribute index out of range");
        return NULL;
    }
    if (!attr->data) {
        PyErr_SetString(PyExc_RuntimeError, "Attribute buffer is not allocated");
        return NULL;
    }

    CXSize dimension = attr->dimension == 0 ? 1 : attr->dimension;
    PyObject *sequence = value;
    if (dimension > 1) {
        sequence = PySequence_Fast(value, "Expected a sequence for multi-dimensional attribute");
        if (!sequence) {
            return NULL;
        }
        if (PySequence_Fast_GET_SIZE(sequence) != (Py_ssize_t)dimension) {
            Py_DECREF(sequence);
            PyErr_SetString(PyExc_ValueError, "Attribute value has wrong dimension");
            return NULL;
        }
    }

    uint8_t *dst = (uint8_t *)attr->data + (size_t)index * (size_t)attr->stride;
    if (attr->type == CXStringAttributeType) {
        CXString *strings = (CXString *)dst;
        for (CXSize i = 0; i < dimension; i++) {
            PyObject *item = sequence;
            if (dimension > 1) {
                item = PySequence_Fast_GET_ITEM(sequence, (Py_ssize_t)i);
            }
            if (item == Py_None) {
                if (strings[i]) {
                    free(strings[i]);
                }
                strings[i] = NULL;
                continue;
            }
            const char *text = NULL;
            if (PyUnicode_Check(item)) {
                text = PyUnicode_AsUTF8(item);
            } else if (PyBytes_Check(item)) {
                text = PyBytes_AsString(item);
            } else {
                if (dimension > 1) {
                    Py_DECREF(sequence);
                }
                PyErr_SetString(PyExc_TypeError, "String attribute expects str or bytes");
                return NULL;
            }
            if (!text) {
                if (dimension > 1) {
                    Py_DECREF(sequence);
                }
                return NULL;
            }
            char *dup = strdup(text);
            if (!dup) {
                if (dimension > 1) {
                    Py_DECREF(sequence);
                }
                PyErr_NoMemory();
                return NULL;
            }
            if (strings[i]) {
                free(strings[i]);
            }
            strings[i] = dup;
        }
        if (dimension > 1) {
            Py_DECREF(sequence);
        }
        Py_RETURN_TRUE;
    }

    if (write_numeric_value(attr, dst, sequence, dimension) != 0) {
        if (dimension > 1) {
            Py_DECREF(sequence);
        }
        return NULL;
    }
    if (dimension > 1) {
        Py_DECREF(sequence);
    }
    Py_RETURN_TRUE;
}

static PyObject *Network_get_attribute_value(PyHeliosNetwork *self, PyObject *args) {
    PyObject *scope_obj = NULL;
    const char *name = NULL;
    unsigned long index = 0;
    if (!PyArg_ParseTuple(args, "Osk", &scope_obj, &name, &index)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    CXAttributeScope scope = CXAttributeScopeNode;
    if (parse_scope(scope_obj, &scope) != 0) {
        return NULL;
    }
    CXAttributeRef attr = get_attribute_for_scope(self->network, scope, name);
    if (!attr) {
        PyErr_SetString(PyExc_KeyError, "Attribute not found");
        return NULL;
    }
    if (index >= attr->capacity) {
        PyErr_SetString(PyExc_IndexError, "Attribute index out of range");
        return NULL;
    }
    if (!attr->data) {
        Py_RETURN_NONE;
    }

    CXSize dimension = attr->dimension == 0 ? 1 : attr->dimension;
    uint8_t *src = (uint8_t *)attr->data + (size_t)index * (size_t)attr->stride;
    if (attr->type == CXStringAttributeType) {
        CXString *strings = (CXString *)src;
        if (dimension == 1) {
            if (!strings[0]) {
                Py_RETURN_NONE;
            }
            return PyUnicode_FromString(strings[0]);
        }
        PyObject *tuple = PyTuple_New((Py_ssize_t)dimension);
        for (CXSize i = 0; i < dimension; i++) {
            if (!strings[i]) {
                Py_INCREF(Py_None);
                PyTuple_SET_ITEM(tuple, (Py_ssize_t)i, Py_None);
            } else {
                PyTuple_SET_ITEM(tuple, (Py_ssize_t)i, PyUnicode_FromString(strings[i]));
            }
        }
        return tuple;
    }

    if (dimension == 1) {
        switch (attr->type) {
            case CXBooleanAttributeType: {
                uint8_t value = *(uint8_t *)src;
                return PyBool_FromLong(value ? 1 : 0);
            }
            case CXFloatAttributeType: {
                float value = *(float *)src;
                return PyFloat_FromDouble((double)value);
            }
            case CXDoubleAttributeType: {
                double value = *(double *)src;
                return PyFloat_FromDouble(value);
            }
            case CXIntegerAttributeType: {
                int32_t value = *(int32_t *)src;
                return PyLong_FromLong((long)value);
            }
            case CXUnsignedIntegerAttributeType: {
                uint32_t value = *(uint32_t *)src;
                return PyLong_FromUnsignedLong((unsigned long)value);
            }
            case CXBigIntegerAttributeType: {
                int64_t value = *(int64_t *)src;
                return PyLong_FromLongLong((long long)value);
            }
            case CXUnsignedBigIntegerAttributeType: {
                uint64_t value = *(uint64_t *)src;
                return PyLong_FromUnsignedLongLong((unsigned long long)value);
            }
            case CXDataAttributeCategoryType: {
                int32_t value = *(int32_t *)src;
                return PyLong_FromLong((long)value);
            }
            default:
                PyErr_SetString(PyExc_TypeError, "Unsupported attribute type");
                return NULL;
        }
    }

    PyObject *tuple = PyTuple_New((Py_ssize_t)dimension);
    for (CXSize i = 0; i < dimension; i++) {
        uint8_t *item_ptr = src + i * attr->elementSize;
        PyObject *value = NULL;
        switch (attr->type) {
            case CXBooleanAttributeType:
                value = PyBool_FromLong(*(uint8_t *)item_ptr ? 1 : 0);
                break;
            case CXFloatAttributeType:
                value = PyFloat_FromDouble((double)(*(float *)item_ptr));
                break;
            case CXDoubleAttributeType:
                value = PyFloat_FromDouble(*(double *)item_ptr);
                break;
            case CXIntegerAttributeType:
                value = PyLong_FromLong((long)(*(int32_t *)item_ptr));
                break;
            case CXUnsignedIntegerAttributeType:
                value = PyLong_FromUnsignedLong((unsigned long)(*(uint32_t *)item_ptr));
                break;
            case CXBigIntegerAttributeType:
                value = PyLong_FromLongLong((long long)(*(int64_t *)item_ptr));
                break;
            case CXUnsignedBigIntegerAttributeType:
                value = PyLong_FromUnsignedLongLong((unsigned long long)(*(uint64_t *)item_ptr));
                break;
            case CXDataAttributeCategoryType:
                value = PyLong_FromLong((long)(*(int32_t *)item_ptr));
                break;
            default:
                PyErr_SetString(PyExc_TypeError, "Unsupported attribute type");
                Py_DECREF(tuple);
                return NULL;
        }
        PyTuple_SET_ITEM(tuple, (Py_ssize_t)i, value);
    }
    return tuple;
}

static PyObject *Network_select_nodes(PyHeliosNetwork *self, PyObject *args) {
    const char *query = NULL;
    if (!PyArg_ParseTuple(args, "s", &query)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    CXNodeSelectorRef selector = CXNodeSelectorCreate(0);
    if (!selector) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to allocate selector");
        return NULL;
    }
    CXBool ok = CXNetworkSelectNodesByQuery(self->network, query, selector);
    if (!ok) {
        const char *message = CXNetworkQueryLastErrorMessage();
        CXSize offset = CXNetworkQueryLastErrorOffset();
        CXNodeSelectorDestroy(selector);
        if (!message || message[0] == '\0') {
            PyErr_Format(PyExc_ValueError, "Query failed at %zu", (size_t)offset);
        } else {
            PyErr_Format(PyExc_ValueError, "Query failed at %zu: %s", (size_t)offset, message);
        }
        return NULL;
    }
    CXSize count = CXNodeSelectorCount(selector);
    CXIndex *data = CXNodeSelectorData(selector);
    PyObject *list = PyList_New((Py_ssize_t)count);
    if (!list) {
        CXNodeSelectorDestroy(selector);
        return NULL;
    }
    for (CXSize i = 0; i < count; i++) {
        PyList_SET_ITEM(list, (Py_ssize_t)i, PyLong_FromUnsignedLong((unsigned long)data[i]));
    }
    CXNodeSelectorDestroy(selector);
    return list;
}

static PyObject *Network_select_edges(PyHeliosNetwork *self, PyObject *args) {
    const char *query = NULL;
    if (!PyArg_ParseTuple(args, "s", &query)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    CXEdgeSelectorRef selector = CXEdgeSelectorCreate(0);
    if (!selector) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to allocate selector");
        return NULL;
    }
    CXBool ok = CXNetworkSelectEdgesByQuery(self->network, query, selector);
    if (!ok) {
        const char *message = CXNetworkQueryLastErrorMessage();
        CXSize offset = CXNetworkQueryLastErrorOffset();
        CXEdgeSelectorDestroy(selector);
        if (!message || message[0] == '\0') {
            PyErr_Format(PyExc_ValueError, "Query failed at %zu", (size_t)offset);
        } else {
            PyErr_Format(PyExc_ValueError, "Query failed at %zu: %s", (size_t)offset, message);
        }
        return NULL;
    }
    CXSize count = CXEdgeSelectorCount(selector);
    CXIndex *data = CXEdgeSelectorData(selector);
    PyObject *list = PyList_New((Py_ssize_t)count);
    if (!list) {
        CXEdgeSelectorDestroy(selector);
        return NULL;
    }
    for (CXSize i = 0; i < count; i++) {
        PyList_SET_ITEM(list, (Py_ssize_t)i, PyLong_FromUnsignedLong((unsigned long)data[i]));
    }
    CXEdgeSelectorDestroy(selector);
    return list;
}

static PyObject *Network_save_xnet(PyHeliosNetwork *self, PyObject *args) {
    const char *path = NULL;
    if (!PyArg_ParseTuple(args, "s", &path)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    CXBool ok = CXNetworkWriteXNet(self->network, path);
    if (!ok) {
        PyErr_SetString(PyExc_IOError, "Failed to write XNet file");
        return NULL;
    }
    Py_RETURN_TRUE;
}

static PyObject *Network_save_bxnet(PyHeliosNetwork *self, PyObject *args) {
    const char *path = NULL;
    if (!PyArg_ParseTuple(args, "s", &path)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    CXBool ok = CXNetworkWriteBXNet(self->network, path);
    if (!ok) {
        PyErr_SetString(PyExc_IOError, "Failed to write BXNet file");
        return NULL;
    }
    Py_RETURN_TRUE;
}

static PyObject *Network_save_zxnet(PyHeliosNetwork *self, PyObject *args) {
    const char *path = NULL;
    int compression = 6;
    if (!PyArg_ParseTuple(args, "s|i", &path, &compression)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    CXBool ok = CXNetworkWriteZXNet(self->network, path, compression);
    if (!ok) {
        PyErr_SetString(PyExc_IOError, "Failed to write ZXNet file");
        return NULL;
    }
    Py_RETURN_TRUE;
}

static PyObject *Network_categorize_attribute(PyHeliosNetwork *self, PyObject *args, PyObject *kwargs) {
    static const char *kwlist[] = {"scope", "name", "sort_order", "missing_label", NULL};
    PyObject *scope_obj = NULL;
    const char *name = NULL;
    PyObject *sort_obj = NULL;
    PyObject *missing_obj = Py_None;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "Os|OO", (char **)kwlist, &scope_obj, &name, &sort_obj, &missing_obj)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    CXAttributeScope scope = CXAttributeScopeNode;
    if (parse_scope(scope_obj, &scope) != 0) {
        return NULL;
    }
    CXCategorySortOrder order = CX_CATEGORY_SORT_NONE;
    if (sort_obj && sort_obj != Py_None) {
        if (parse_sort_order(sort_obj, &order) != 0) {
            return NULL;
        }
    }
    const char *missing_label = NULL;
    if (missing_obj && missing_obj != Py_None) {
        if (!PyUnicode_Check(missing_obj)) {
            PyErr_SetString(PyExc_TypeError, "missing_label must be a string");
            return NULL;
        }
        missing_label = PyUnicode_AsUTF8(missing_obj);
        if (!missing_label) {
            return NULL;
        }
    }
    CXBool ok = CXNetworkCategorizeAttribute(self->network, scope, name, order, missing_label);
    if (!ok) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to categorize attribute");
        return NULL;
    }
    Py_RETURN_TRUE;
}

static PyObject *Network_decategorize_attribute(PyHeliosNetwork *self, PyObject *args, PyObject *kwargs) {
    static const char *kwlist[] = {"scope", "name", "missing_label", NULL};
    PyObject *scope_obj = NULL;
    const char *name = NULL;
    PyObject *missing_obj = Py_None;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "Os|O", (char **)kwlist, &scope_obj, &name, &missing_obj)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    CXAttributeScope scope = CXAttributeScopeNode;
    if (parse_scope(scope_obj, &scope) != 0) {
        return NULL;
    }
    const char *missing_label = NULL;
    if (missing_obj && missing_obj != Py_None) {
        if (!PyUnicode_Check(missing_obj)) {
            PyErr_SetString(PyExc_TypeError, "missing_label must be a string");
            return NULL;
        }
        missing_label = PyUnicode_AsUTF8(missing_obj);
        if (!missing_label) {
            return NULL;
        }
    }
    CXBool ok = CXNetworkDecategorizeAttribute(self->network, scope, name, missing_label);
    if (!ok) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to decategorize attribute");
        return NULL;
    }
    Py_RETURN_TRUE;
}

static PyObject *Network_get_category_dictionary(PyHeliosNetwork *self, PyObject *args) {
    PyObject *scope_obj = NULL;
    const char *name = NULL;
    if (!PyArg_ParseTuple(args, "Os", &scope_obj, &name)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    CXAttributeScope scope = CXAttributeScopeNode;
    if (parse_scope(scope_obj, &scope) != 0) {
        return NULL;
    }
    CXSize count = CXNetworkGetAttributeCategoryDictionaryCount(self->network, scope, name);
    if (count == 0) {
        return PyDict_New();
    }
    int32_t *ids = (int32_t *)calloc((size_t)count, sizeof(int32_t));
    CXString *labels = (CXString *)calloc((size_t)count, sizeof(CXString));
    if (!ids || !labels) {
        free(ids);
        free(labels);
        PyErr_NoMemory();
        return NULL;
    }
    CXBool ok = CXNetworkGetAttributeCategoryDictionaryEntries(self->network, scope, name, ids, labels, count);
    if (!ok) {
        free(ids);
        free(labels);
        PyErr_SetString(PyExc_RuntimeError, "Failed to fetch category dictionary");
        return NULL;
    }
    PyObject *dict = PyDict_New();
    for (CXSize i = 0; i < count; i++) {
        if (!labels[i]) {
            continue;
        }
        PyObject *key = PyUnicode_FromString(labels[i]);
        PyObject *val = PyLong_FromLong((long)ids[i]);
        PyDict_SetItem(dict, key, val);
        Py_DECREF(key);
        Py_DECREF(val);
    }
    free(ids);
    free(labels);
    return dict;
}

static PyObject *Network_set_category_dictionary(PyHeliosNetwork *self, PyObject *args, PyObject *kwargs) {
    static const char *kwlist[] = {"scope", "name", "mapping", "remap_existing", NULL};
    PyObject *scope_obj = NULL;
    const char *name = NULL;
    PyObject *mapping = NULL;
    int remap_existing = 1;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OsO|p", (char **)kwlist, &scope_obj, &name, &mapping, &remap_existing)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    CXAttributeScope scope = CXAttributeScopeNode;
    if (parse_scope(scope_obj, &scope) != 0) {
        return NULL;
    }

    PyObject *items = NULL;
    if (PyMapping_Check(mapping)) {
        items = PyMapping_Items(mapping);
    } else {
        items = PySequence_Fast(mapping, "Expected a mapping or sequence of pairs");
    }
    if (!items) {
        return NULL;
    }

    Py_ssize_t count = PySequence_Size(items);
    if (count < 0) {
        Py_DECREF(items);
        return NULL;
    }

    CXString *labels = (CXString *)calloc((size_t)count, sizeof(CXString));
    int32_t *ids = (int32_t *)calloc((size_t)count, sizeof(int32_t));
    if (!labels || !ids) {
        Py_DECREF(items);
        free(labels);
        free(ids);
        PyErr_NoMemory();
        return NULL;
    }

    for (Py_ssize_t i = 0; i < count; i++) {
        PyObject *pair = PySequence_Fast_GET_ITEM(items, i);
        PyObject *pair_seq = PySequence_Fast(pair, "Expected (label, id) pair");
        if (!pair_seq || PySequence_Fast_GET_SIZE(pair_seq) != 2) {
            Py_XDECREF(pair_seq);
            Py_DECREF(items);
            for (Py_ssize_t j = 0; j < count; j++) {
                free(labels[j]);
            }
            free(labels);
            free(ids);
            PyErr_SetString(PyExc_ValueError, "Expected (label, id) pairs");
            return NULL;
        }
        PyObject *label_obj = PySequence_Fast_GET_ITEM(pair_seq, 0);
        PyObject *id_obj = PySequence_Fast_GET_ITEM(pair_seq, 1);
        if (!PyUnicode_Check(label_obj)) {
            Py_DECREF(pair_seq);
            Py_DECREF(items);
            for (Py_ssize_t j = 0; j < count; j++) {
                free(labels[j]);
            }
            free(labels);
            free(ids);
            PyErr_SetString(PyExc_TypeError, "Category label must be a string");
            return NULL;
        }
        const char *label = PyUnicode_AsUTF8(label_obj);
        if (!label) {
            Py_DECREF(pair_seq);
            Py_DECREF(items);
            for (Py_ssize_t j = 0; j < count; j++) {
                free(labels[j]);
            }
            free(labels);
            free(ids);
            return NULL;
        }
        long id_value = PyLong_AsLong(id_obj);
        if (PyErr_Occurred()) {
            Py_DECREF(pair_seq);
            Py_DECREF(items);
            for (Py_ssize_t j = 0; j < count; j++) {
                free(labels[j]);
            }
            free(labels);
            free(ids);
            return NULL;
        }
        labels[i] = strdup(label);
        if (!labels[i]) {
            Py_DECREF(pair_seq);
            Py_DECREF(items);
            for (Py_ssize_t j = 0; j < count; j++) {
                free(labels[j]);
            }
            free(labels);
            free(ids);
            PyErr_NoMemory();
            return NULL;
        }
        ids[i] = (int32_t)id_value;
        Py_DECREF(pair_seq);
    }

    CXBool ok = CXNetworkSetAttributeCategoryDictionary(self->network, scope, name, (const CXString *)labels, ids, (CXSize)count, remap_existing ? CXTrue : CXFalse);
    for (Py_ssize_t i = 0; i < count; i++) {
        free(labels[i]);
    }
    free(labels);
    free(ids);
    Py_DECREF(items);
    if (!ok) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to set category dictionary");
        return NULL;
    }
    Py_RETURN_TRUE;
}

static PyObject *Network_measure_node_dimension(PyHeliosNetwork *self, PyObject *args, PyObject *kwargs) {
    static const char *kwlist[] = {"node", "max_level", "method", "order", NULL};
    Py_ssize_t node = -1;
    Py_ssize_t max_level = 8;
    PyObject *method_obj = NULL;
    Py_ssize_t order = 2;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "n|nOn", (char **)kwlist, &node, &max_level, &method_obj, &order)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    if (node < 0) {
        PyErr_SetString(PyExc_ValueError, "node must be non-negative");
        return NULL;
    }
    if (max_level < 0) {
        PyErr_SetString(PyExc_ValueError, "max_level must be non-negative");
        return NULL;
    }
    if (order < 1) {
        PyErr_SetString(PyExc_ValueError, "order must be >= 1");
        return NULL;
    }

    CXDimensionDifferenceMethod method = CXDimensionLeastSquaresDifferenceMethod;
    if (parse_dimension_method(method_obj, &method) != 0) {
        return NULL;
    }

    CXSize levels = (CXSize)max_level + 1;
    uint32_t *capacity = (uint32_t *)calloc(levels, sizeof(uint32_t));
    float *dimension = (float *)calloc(levels, sizeof(float));
    if (!capacity || !dimension) {
        free(capacity);
        free(dimension);
        PyErr_NoMemory();
        return NULL;
    }

    CXBool ok = CXNetworkMeasureNodeDimension(
        self->network,
        (CXIndex)node,
        (CXSize)max_level,
        method,
        (CXSize)order,
        capacity,
        dimension
    );
    if (!ok) {
        free(capacity);
        free(dimension);
        PyErr_SetString(PyExc_RuntimeError, "Failed to measure node dimension");
        return NULL;
    }

    PyObject *capacity_list = PyList_New((Py_ssize_t)levels);
    PyObject *dimension_list = PyList_New((Py_ssize_t)levels);
    if (!capacity_list || !dimension_list) {
        free(capacity);
        free(dimension);
        Py_XDECREF(capacity_list);
        Py_XDECREF(dimension_list);
        PyErr_NoMemory();
        return NULL;
    }
    for (CXSize i = 0; i < levels; i++) {
        PyList_SET_ITEM(capacity_list, (Py_ssize_t)i, PyLong_FromUnsignedLong((unsigned long)capacity[i]));
        PyList_SET_ITEM(dimension_list, (Py_ssize_t)i, PyFloat_FromDouble((double)dimension[i]));
    }

    free(capacity);
    free(dimension);

    PyObject *result = PyDict_New();
    if (!result) {
        Py_DECREF(capacity_list);
        Py_DECREF(dimension_list);
        return NULL;
    }
    PyObject *max_level_obj = PyLong_FromSsize_t(max_level);
    PyObject *method_obj_out = PyLong_FromLong((long)method);
    PyObject *order_obj = PyLong_FromSsize_t(order);
    if (!max_level_obj || !method_obj_out || !order_obj) {
        Py_XDECREF(max_level_obj);
        Py_XDECREF(method_obj_out);
        Py_XDECREF(order_obj);
        Py_DECREF(result);
        Py_DECREF(capacity_list);
        Py_DECREF(dimension_list);
        PyErr_NoMemory();
        return NULL;
    }
    PyDict_SetItemString(result, "capacity", capacity_list);
    PyDict_SetItemString(result, "dimension", dimension_list);
    PyDict_SetItemString(result, "max_level", max_level_obj);
    PyDict_SetItemString(result, "method", method_obj_out);
    PyDict_SetItemString(result, "order", order_obj);
    Py_DECREF(max_level_obj);
    Py_DECREF(method_obj_out);
    Py_DECREF(order_obj);
    Py_DECREF(capacity_list);
    Py_DECREF(dimension_list);
    return result;
}

static PyObject *Network_measure_dimension(PyHeliosNetwork *self, PyObject *args, PyObject *kwargs) {
    static const char *kwlist[] = {"max_level", "method", "order", "nodes", NULL};
    Py_ssize_t max_level = 8;
    PyObject *method_obj = NULL;
    Py_ssize_t order = 2;
    PyObject *nodes_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|nOnO", (char **)kwlist, &max_level, &method_obj, &order, &nodes_obj)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    if (max_level < 0) {
        PyErr_SetString(PyExc_ValueError, "max_level must be non-negative");
        return NULL;
    }
    if (order < 1) {
        PyErr_SetString(PyExc_ValueError, "order must be >= 1");
        return NULL;
    }

    CXDimensionDifferenceMethod method = CXDimensionLeastSquaresDifferenceMethod;
    if (parse_dimension_method(method_obj, &method) != 0) {
        return NULL;
    }

    CXIndex *nodes = NULL;
    CXSize node_count = 0;
    PyObject *nodes_fast = NULL;
    if (nodes_obj && nodes_obj != Py_None) {
        nodes_fast = PySequence_Fast(nodes_obj, "nodes must be a sequence of node indices");
        if (!nodes_fast) {
            return NULL;
        }
        Py_ssize_t py_count = PySequence_Fast_GET_SIZE(nodes_fast);
        if (py_count < 0) {
            Py_DECREF(nodes_fast);
            return NULL;
        }
        node_count = (CXSize)py_count;
        if (node_count > 0) {
            nodes = (CXIndex *)calloc(node_count, sizeof(CXIndex));
            if (!nodes) {
                Py_DECREF(nodes_fast);
                PyErr_NoMemory();
                return NULL;
            }
            for (Py_ssize_t i = 0; i < py_count; i++) {
                PyObject *item = PySequence_Fast_GET_ITEM(nodes_fast, i);
                unsigned long value = PyLong_AsUnsignedLong(item);
                if (PyErr_Occurred()) {
                    free(nodes);
                    Py_DECREF(nodes_fast);
                    return NULL;
                }
                nodes[i] = (CXIndex)value;
            }
        }
    }

    CXSize levels = (CXSize)max_level + 1;
    float *average_capacity = (float *)calloc(levels, sizeof(float));
    float *global_dimension = (float *)calloc(levels, sizeof(float));
    float *average_node_dimension = (float *)calloc(levels, sizeof(float));
    float *node_dimension_stddev = (float *)calloc(levels, sizeof(float));
    if (!average_capacity || !global_dimension || !average_node_dimension || !node_dimension_stddev) {
        free(nodes);
        Py_XDECREF(nodes_fast);
        free(average_capacity);
        free(global_dimension);
        free(average_node_dimension);
        free(node_dimension_stddev);
        PyErr_NoMemory();
        return NULL;
    }

    CXSize selected_count = CXNetworkMeasureDimension(
        self->network,
        nodes,
        node_count,
        (CXSize)max_level,
        method,
        (CXSize)order,
        average_capacity,
        global_dimension,
        average_node_dimension,
        node_dimension_stddev
    );

    free(nodes);
    Py_XDECREF(nodes_fast);

    PyObject *average_capacity_list = PyList_New((Py_ssize_t)levels);
    PyObject *global_dimension_list = PyList_New((Py_ssize_t)levels);
    PyObject *average_node_dimension_list = PyList_New((Py_ssize_t)levels);
    PyObject *node_dimension_stddev_list = PyList_New((Py_ssize_t)levels);
    if (!average_capacity_list || !global_dimension_list || !average_node_dimension_list || !node_dimension_stddev_list) {
        free(average_capacity);
        free(global_dimension);
        free(average_node_dimension);
        free(node_dimension_stddev);
        Py_XDECREF(average_capacity_list);
        Py_XDECREF(global_dimension_list);
        Py_XDECREF(average_node_dimension_list);
        Py_XDECREF(node_dimension_stddev_list);
        PyErr_NoMemory();
        return NULL;
    }

    for (CXSize i = 0; i < levels; i++) {
        PyList_SET_ITEM(average_capacity_list, (Py_ssize_t)i, PyFloat_FromDouble((double)average_capacity[i]));
        PyList_SET_ITEM(global_dimension_list, (Py_ssize_t)i, PyFloat_FromDouble((double)global_dimension[i]));
        PyList_SET_ITEM(average_node_dimension_list, (Py_ssize_t)i, PyFloat_FromDouble((double)average_node_dimension[i]));
        PyList_SET_ITEM(node_dimension_stddev_list, (Py_ssize_t)i, PyFloat_FromDouble((double)node_dimension_stddev[i]));
    }

    free(average_capacity);
    free(global_dimension);
    free(average_node_dimension);
    free(node_dimension_stddev);

    PyObject *result = PyDict_New();
    if (!result) {
        Py_DECREF(average_capacity_list);
        Py_DECREF(global_dimension_list);
        Py_DECREF(average_node_dimension_list);
        Py_DECREF(node_dimension_stddev_list);
        return NULL;
    }
    PyObject *selected_count_obj = PyLong_FromSize_t((size_t)selected_count);
    PyObject *max_level_obj = PyLong_FromSsize_t(max_level);
    PyObject *method_obj_out = PyLong_FromLong((long)method);
    PyObject *order_obj = PyLong_FromSsize_t(order);
    if (!selected_count_obj || !max_level_obj || !method_obj_out || !order_obj) {
        Py_XDECREF(selected_count_obj);
        Py_XDECREF(max_level_obj);
        Py_XDECREF(method_obj_out);
        Py_XDECREF(order_obj);
        Py_DECREF(result);
        Py_DECREF(average_capacity_list);
        Py_DECREF(global_dimension_list);
        Py_DECREF(average_node_dimension_list);
        Py_DECREF(node_dimension_stddev_list);
        PyErr_NoMemory();
        return NULL;
    }
    PyDict_SetItemString(result, "selected_count", selected_count_obj);
    PyDict_SetItemString(result, "average_capacity", average_capacity_list);
    PyDict_SetItemString(result, "global_dimension", global_dimension_list);
    PyDict_SetItemString(result, "average_node_dimension", average_node_dimension_list);
    PyDict_SetItemString(result, "node_dimension_stddev", node_dimension_stddev_list);
    PyDict_SetItemString(result, "max_level", max_level_obj);
    PyDict_SetItemString(result, "method", method_obj_out);
    PyDict_SetItemString(result, "order", order_obj);
    Py_DECREF(selected_count_obj);
    Py_DECREF(max_level_obj);
    Py_DECREF(method_obj_out);
    Py_DECREF(order_obj);
    Py_DECREF(average_capacity_list);
    Py_DECREF(global_dimension_list);
    Py_DECREF(average_node_dimension_list);
    Py_DECREF(node_dimension_stddev_list);
    return result;
}

static PyObject *float_buffer_to_list(const float *values, CXSize count) {
    PyObject *list = PyList_New((Py_ssize_t)count);
    if (!list) {
        return NULL;
    }
    for (CXSize i = 0; i < count; i++) {
        PyList_SET_ITEM(list, (Py_ssize_t)i, PyFloat_FromDouble((double)values[i]));
    }
    return list;
}

static PyObject *Network_measure_degree(PyHeliosNetwork *self, PyObject *args, PyObject *kwargs) {
    static const char *kwlist[] = {"direction", NULL};
    PyObject *direction_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|O", (char **)kwlist, &direction_obj)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }

    CXNeighborDirection direction = CXNeighborDirectionBoth;
    if (parse_neighbor_direction(direction_obj, &direction) != 0) {
        return NULL;
    }

    float *values = (float *)calloc(self->network->nodeCapacity, sizeof(float));
    if (!values) {
        PyErr_NoMemory();
        return NULL;
    }
    CXBool ok = CXNetworkMeasureDegree(self->network, direction, values);
    if (!ok) {
        free(values);
        PyErr_SetString(PyExc_RuntimeError, "Failed to measure degree");
        return NULL;
    }

    PyObject *values_list = float_buffer_to_list(values, self->network->nodeCapacity);
    free(values);
    if (!values_list) {
        return NULL;
    }

    PyObject *result = PyDict_New();
    if (!result) {
        Py_DECREF(values_list);
        return NULL;
    }
    PyObject *direction_out = PyLong_FromLong((long)direction);
    if (!direction_out) {
        Py_DECREF(values_list);
        Py_DECREF(result);
        return NULL;
    }
    PyDict_SetItemString(result, "values_by_node", values_list);
    PyDict_SetItemString(result, "direction", direction_out);
    Py_DECREF(values_list);
    Py_DECREF(direction_out);
    return result;
}

static PyObject *Network_measure_strength(PyHeliosNetwork *self, PyObject *args, PyObject *kwargs) {
    static const char *kwlist[] = {"edge_weight_attribute", "direction", "measure", NULL};
    const char *edge_weight_attribute = NULL;
    PyObject *direction_obj = NULL;
    PyObject *measure_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|zOO", (char **)kwlist, &edge_weight_attribute, &direction_obj, &measure_obj)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }

    CXNeighborDirection direction = CXNeighborDirectionBoth;
    if (parse_neighbor_direction(direction_obj, &direction) != 0) {
        return NULL;
    }
    CXStrengthMeasure measure = CXStrengthMeasureSum;
    if (parse_strength_measure(measure_obj, &measure) != 0) {
        return NULL;
    }

    float *values = (float *)calloc(self->network->nodeCapacity, sizeof(float));
    if (!values) {
        PyErr_NoMemory();
        return NULL;
    }
    CXBool ok = CXNetworkMeasureStrength(self->network, edge_weight_attribute, direction, measure, values);
    if (!ok) {
        free(values);
        PyErr_SetString(PyExc_RuntimeError, "Failed to measure strength");
        return NULL;
    }

    PyObject *values_list = float_buffer_to_list(values, self->network->nodeCapacity);
    free(values);
    if (!values_list) {
        return NULL;
    }

    PyObject *result = PyDict_New();
    if (!result) {
        Py_DECREF(values_list);
        return NULL;
    }
    PyObject *direction_out = PyLong_FromLong((long)direction);
    PyObject *measure_out = PyLong_FromLong((long)measure);
    if (!direction_out || !measure_out) {
        Py_XDECREF(direction_out);
        Py_XDECREF(measure_out);
        Py_DECREF(values_list);
        Py_DECREF(result);
        return NULL;
    }
    PyDict_SetItemString(result, "values_by_node", values_list);
    PyDict_SetItemString(result, "direction", direction_out);
    PyDict_SetItemString(result, "measure", measure_out);
    Py_DECREF(values_list);
    Py_DECREF(direction_out);
    Py_DECREF(measure_out);
    return result;
}

static PyObject *Network_measure_local_clustering_coefficient(PyHeliosNetwork *self, PyObject *args, PyObject *kwargs) {
    static const char *kwlist[] = {"edge_weight_attribute", "direction", "variant", NULL};
    const char *edge_weight_attribute = NULL;
    PyObject *direction_obj = NULL;
    PyObject *variant_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|zOO", (char **)kwlist, &edge_weight_attribute, &direction_obj, &variant_obj)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }

    CXNeighborDirection direction = CXNeighborDirectionBoth;
    if (parse_neighbor_direction(direction_obj, &direction) != 0) {
        return NULL;
    }
    CXClusteringCoefficientVariant variant = CXClusteringCoefficientUnweighted;
    if (parse_clustering_variant(variant_obj, &variant) != 0) {
        return NULL;
    }

    float *values = (float *)calloc(self->network->nodeCapacity, sizeof(float));
    if (!values) {
        PyErr_NoMemory();
        return NULL;
    }
    CXBool ok = CXNetworkMeasureLocalClusteringCoefficient(
        self->network,
        edge_weight_attribute,
        direction,
        variant,
        values
    );
    if (!ok) {
        free(values);
        PyErr_SetString(PyExc_RuntimeError, "Failed to measure local clustering coefficient");
        return NULL;
    }

    PyObject *values_list = float_buffer_to_list(values, self->network->nodeCapacity);
    free(values);
    if (!values_list) {
        return NULL;
    }
    PyObject *result = PyDict_New();
    if (!result) {
        Py_DECREF(values_list);
        return NULL;
    }
    PyObject *direction_out = PyLong_FromLong((long)direction);
    PyObject *variant_out = PyLong_FromLong((long)variant);
    if (!direction_out || !variant_out) {
        Py_XDECREF(direction_out);
        Py_XDECREF(variant_out);
        Py_DECREF(values_list);
        Py_DECREF(result);
        return NULL;
    }
    PyDict_SetItemString(result, "values_by_node", values_list);
    PyDict_SetItemString(result, "direction", direction_out);
    PyDict_SetItemString(result, "variant", variant_out);
    Py_DECREF(values_list);
    Py_DECREF(direction_out);
    Py_DECREF(variant_out);
    return result;
}

static PyObject *Network_measure_eigenvector_centrality(PyHeliosNetwork *self, PyObject *args, PyObject *kwargs) {
    static const char *kwlist[] = {"edge_weight_attribute", "direction", "max_iterations", "tolerance", "initial", "execution_mode", NULL};
    const char *edge_weight_attribute = NULL;
    PyObject *direction_obj = NULL;
    Py_ssize_t max_iterations = 100;
    double tolerance = 1e-6;
    PyObject *initial_obj = NULL;
    PyObject *execution_mode_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|zOndOO", (char **)kwlist, &edge_weight_attribute, &direction_obj, &max_iterations, &tolerance, &initial_obj, &execution_mode_obj)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }
    if (max_iterations <= 0) {
        PyErr_SetString(PyExc_ValueError, "max_iterations must be positive");
        return NULL;
    }

    CXNeighborDirection direction = CXNeighborDirectionBoth;
    if (parse_neighbor_direction(direction_obj, &direction) != 0) {
        return NULL;
    }
    CXMeasurementExecutionMode execution_mode = CXMeasurementExecutionParallel;
    if (parse_execution_mode(execution_mode_obj, &execution_mode) != 0) {
        return NULL;
    }

    float *initial_values = NULL;
    PyObject *initial_fast = NULL;
    if (initial_obj && initial_obj != Py_None) {
        initial_fast = PySequence_Fast(initial_obj, "initial must be a sequence with node_capacity values");
        if (!initial_fast) {
            return NULL;
        }
        Py_ssize_t initial_count = PySequence_Fast_GET_SIZE(initial_fast);
        if ((CXSize)initial_count != self->network->nodeCapacity) {
            Py_DECREF(initial_fast);
            PyErr_SetString(PyExc_ValueError, "initial sequence length must equal node_capacity");
            return NULL;
        }
        initial_values = (float *)calloc(self->network->nodeCapacity, sizeof(float));
        if (!initial_values) {
            Py_DECREF(initial_fast);
            PyErr_NoMemory();
            return NULL;
        }
        for (Py_ssize_t i = 0; i < initial_count; i++) {
            PyObject *item = PySequence_Fast_GET_ITEM(initial_fast, i);
            double value = PyFloat_AsDouble(item);
            if (PyErr_Occurred()) {
                free(initial_values);
                Py_DECREF(initial_fast);
                return NULL;
            }
            initial_values[i] = (float)value;
        }
    }

    float *values = (float *)calloc(self->network->nodeCapacity, sizeof(float));
    if (!values) {
        free(initial_values);
        Py_XDECREF(initial_fast);
        PyErr_NoMemory();
        return NULL;
    }

    double eigenvalue = 0.0;
    double delta = 0.0;
    CXSize iterations = 0;
    CXBool converged = CXFalse;
    CXBool ok = CXNetworkMeasureEigenvectorCentrality(
        self->network,
        edge_weight_attribute,
        direction,
        execution_mode,
        (CXSize)max_iterations,
        tolerance,
        initial_values,
        values,
        &eigenvalue,
        &delta,
        &iterations,
        &converged
    );

    free(initial_values);
    Py_XDECREF(initial_fast);

    if (!ok) {
        free(values);
        PyErr_SetString(PyExc_RuntimeError, "Failed to measure eigenvector centrality");
        return NULL;
    }

    PyObject *values_list = float_buffer_to_list(values, self->network->nodeCapacity);
    free(values);
    if (!values_list) {
        return NULL;
    }

    PyObject *result = PyDict_New();
    if (!result) {
        Py_DECREF(values_list);
        return NULL;
    }
    PyObject *direction_out = PyLong_FromLong((long)direction);
    PyObject *execution_out = PyLong_FromLong((long)execution_mode);
    PyObject *eigenvalue_out = PyFloat_FromDouble(eigenvalue);
    PyObject *delta_out = PyFloat_FromDouble(delta);
    PyObject *iterations_out = PyLong_FromSize_t((size_t)iterations);
    PyObject *converged_out = PyBool_FromLong(converged ? 1 : 0);
    if (!direction_out || !execution_out || !eigenvalue_out || !delta_out || !iterations_out || !converged_out) {
        Py_XDECREF(direction_out);
        Py_XDECREF(execution_out);
        Py_XDECREF(eigenvalue_out);
        Py_XDECREF(delta_out);
        Py_XDECREF(iterations_out);
        Py_XDECREF(converged_out);
        Py_DECREF(values_list);
        Py_DECREF(result);
        return NULL;
    }
    PyDict_SetItemString(result, "values_by_node", values_list);
    PyDict_SetItemString(result, "direction", direction_out);
    PyDict_SetItemString(result, "execution_mode", execution_out);
    PyDict_SetItemString(result, "eigenvalue", eigenvalue_out);
    PyDict_SetItemString(result, "delta", delta_out);
    PyDict_SetItemString(result, "iterations", iterations_out);
    PyDict_SetItemString(result, "converged", converged_out);
    Py_DECREF(values_list);
    Py_DECREF(direction_out);
    Py_DECREF(execution_out);
    Py_DECREF(eigenvalue_out);
    Py_DECREF(delta_out);
    Py_DECREF(iterations_out);
    Py_DECREF(converged_out);
    return result;
}

static PyObject *Network_measure_betweenness_centrality(PyHeliosNetwork *self, PyObject *args, PyObject *kwargs) {
    static const char *kwlist[] = {"edge_weight_attribute", "source_nodes", "normalize", "accumulate", "initial", "execution_mode", NULL};
    const char *edge_weight_attribute = NULL;
    PyObject *source_nodes_obj = NULL;
    int normalize = 1;
    int accumulate = 0;
    PyObject *initial_obj = NULL;
    PyObject *execution_mode_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|zOppOO", (char **)kwlist, &edge_weight_attribute, &source_nodes_obj, &normalize, &accumulate, &initial_obj, &execution_mode_obj)) {
        return NULL;
    }
    if (!self->network) {
        PyErr_SetString(PyExc_RuntimeError, "Network is not initialized");
        return NULL;
    }

    CXMeasurementExecutionMode execution_mode = CXMeasurementExecutionParallel;
    if (parse_execution_mode(execution_mode_obj, &execution_mode) != 0) {
        return NULL;
    }

    CXIndex *source_nodes = NULL;
    CXSize source_count = 0;
    PyObject *source_fast = NULL;
    if (source_nodes_obj && source_nodes_obj != Py_None) {
        source_fast = PySequence_Fast(source_nodes_obj, "source_nodes must be a sequence");
        if (!source_fast) {
            return NULL;
        }
        Py_ssize_t py_count = PySequence_Fast_GET_SIZE(source_fast);
        if (py_count > 0) {
            source_nodes = (CXIndex *)calloc((size_t)py_count, sizeof(CXIndex));
            if (!source_nodes) {
                Py_DECREF(source_fast);
                PyErr_NoMemory();
                return NULL;
            }
            for (Py_ssize_t i = 0; i < py_count; i++) {
                PyObject *item = PySequence_Fast_GET_ITEM(source_fast, i);
                unsigned long value = PyLong_AsUnsignedLong(item);
                if (PyErr_Occurred()) {
                    free(source_nodes);
                    Py_DECREF(source_fast);
                    return NULL;
                }
                source_nodes[source_count++] = (CXIndex)value;
            }
        }
    }

    float *values = (float *)calloc(self->network->nodeCapacity, sizeof(float));
    if (!values) {
        free(source_nodes);
        Py_XDECREF(source_fast);
        PyErr_NoMemory();
        return NULL;
    }

    PyObject *initial_fast = NULL;
    if (initial_obj && initial_obj != Py_None) {
        initial_fast = PySequence_Fast(initial_obj, "initial must be a sequence with node_capacity values");
        if (!initial_fast) {
            free(values);
            free(source_nodes);
            Py_XDECREF(source_fast);
            return NULL;
        }
        Py_ssize_t initial_count = PySequence_Fast_GET_SIZE(initial_fast);
        if ((CXSize)initial_count != self->network->nodeCapacity) {
            Py_DECREF(initial_fast);
            free(values);
            free(source_nodes);
            Py_XDECREF(source_fast);
            PyErr_SetString(PyExc_ValueError, "initial sequence length must equal node_capacity");
            return NULL;
        }
        for (Py_ssize_t i = 0; i < initial_count; i++) {
            PyObject *item = PySequence_Fast_GET_ITEM(initial_fast, i);
            double value = PyFloat_AsDouble(item);
            if (PyErr_Occurred()) {
                Py_DECREF(initial_fast);
                free(values);
                free(source_nodes);
                Py_XDECREF(source_fast);
                return NULL;
            }
            values[i] = (float)value;
        }
    }

    CXSize processed_sources = CXNetworkMeasureBetweennessCentrality(
        self->network,
        edge_weight_attribute,
        execution_mode,
        source_nodes,
        source_count,
        normalize ? CXTrue : CXFalse,
        accumulate ? CXTrue : CXFalse,
        values
    );

    Py_XDECREF(initial_fast);
    free(source_nodes);
    Py_XDECREF(source_fast);

    PyObject *values_list = float_buffer_to_list(values, self->network->nodeCapacity);
    free(values);
    if (!values_list) {
        return NULL;
    }

    PyObject *result = PyDict_New();
    if (!result) {
        Py_DECREF(values_list);
        return NULL;
    }
    PyObject *processed_out = PyLong_FromSize_t((size_t)processed_sources);
    PyObject *normalize_out = PyBool_FromLong(normalize ? 1 : 0);
    PyObject *accumulate_out = PyBool_FromLong(accumulate ? 1 : 0);
    PyObject *execution_out = PyLong_FromLong((long)execution_mode);
    if (!processed_out || !normalize_out || !accumulate_out || !execution_out) {
        Py_XDECREF(processed_out);
        Py_XDECREF(normalize_out);
        Py_XDECREF(accumulate_out);
        Py_XDECREF(execution_out);
        Py_DECREF(values_list);
        Py_DECREF(result);
        return NULL;
    }
    PyDict_SetItemString(result, "values_by_node", values_list);
    PyDict_SetItemString(result, "processed_sources", processed_out);
    PyDict_SetItemString(result, "normalize", normalize_out);
    PyDict_SetItemString(result, "accumulate", accumulate_out);
    PyDict_SetItemString(result, "execution_mode", execution_out);
    Py_DECREF(values_list);
    Py_DECREF(processed_out);
    Py_DECREF(normalize_out);
    Py_DECREF(accumulate_out);
    Py_DECREF(execution_out);
    return result;
}

static PyMethodDef Network_methods[] = {
    {"node_count", (PyCFunction)Network_node_count, METH_NOARGS, "Return number of active nodes."},
    {"edge_count", (PyCFunction)Network_edge_count, METH_NOARGS, "Return number of active edges."},
    {"node_capacity", (PyCFunction)Network_node_capacity, METH_NOARGS, "Return node capacity."},
    {"edge_capacity", (PyCFunction)Network_edge_capacity, METH_NOARGS, "Return edge capacity."},
    {"add_nodes", (PyCFunction)Network_add_nodes, METH_VARARGS, "Add nodes and return indices."},
    {"remove_nodes", (PyCFunction)Network_remove_nodes, METH_VARARGS, "Remove nodes by indices."},
    {"add_edges", (PyCFunction)Network_add_edges, METH_VARARGS, "Add edges and return indices."},
    {"remove_edges", (PyCFunction)Network_remove_edges, METH_VARARGS, "Remove edges by indices."},
    {"is_node_active", (PyCFunction)Network_is_node_active, METH_VARARGS, "Check node activity."},
    {"is_edge_active", (PyCFunction)Network_is_edge_active, METH_VARARGS, "Check edge activity."},
    {"node_indices", (PyCFunction)Network_node_indices, METH_NOARGS, "Return list of active node indices."},
    {"edge_indices", (PyCFunction)Network_edge_indices, METH_NOARGS, "Return list of active edge indices."},
    {"edge_endpoints", (PyCFunction)Network_edge_endpoints, METH_VARARGS, "Return (source, target) for edge index."},
    {"out_neighbors", (PyCFunction)Network_out_neighbors, METH_VARARGS, "Return outgoing neighbors for one node."},
    {"in_neighbors", (PyCFunction)Network_in_neighbors, METH_VARARGS, "Return incoming neighbors for one node."},
    {"neighbors", (PyCFunction)Network_neighbors, METH_VARARGS | METH_KEYWORDS, "Return one-hop neighbors for source node(s)."},
    {"neighbors_at_level", (PyCFunction)Network_neighbors_at_level, METH_VARARGS | METH_KEYWORDS, "Return neighbors at an exact concentric level."},
    {"neighbors_up_to_level", (PyCFunction)Network_neighbors_up_to_level, METH_VARARGS | METH_KEYWORDS, "Return neighbors up to a concentric level."},
    {"edges_with_indices", (PyCFunction)Network_edges_with_indices, METH_NOARGS, "Return list of (edge_index, (source, target))."},
    {"define_attribute", (PyCFunction)Network_define_attribute, METH_VARARGS, "Define an attribute."},
    {"list_attributes", (PyCFunction)Network_list_attributes, METH_VARARGS, "List attribute names for a scope."},
    {"attribute_info", (PyCFunction)Network_attribute_info, METH_VARARGS, "Get attribute metadata."},
    {"attribute_buffer", (PyCFunction)Network_attribute_buffer, METH_VARARGS, "Get raw attribute buffer as memoryview."},
    {"set_attribute_value", (PyCFunction)Network_set_attribute_value, METH_VARARGS, "Set attribute value."},
    {"get_attribute_value", (PyCFunction)Network_get_attribute_value, METH_VARARGS, "Get attribute value."},
    {"select_nodes", (PyCFunction)Network_select_nodes, METH_VARARGS, "Select nodes by query expression."},
    {"select_edges", (PyCFunction)Network_select_edges, METH_VARARGS, "Select edges by query expression."},
    {"save_xnet", (PyCFunction)Network_save_xnet, METH_VARARGS, "Save network as .xnet."},
    {"save_bxnet", (PyCFunction)Network_save_bxnet, METH_VARARGS, "Save network as .bxnet."},
    {"save_zxnet", (PyCFunction)Network_save_zxnet, METH_VARARGS, "Save network as .zxnet."},
    {"categorize_attribute", (PyCFunction)Network_categorize_attribute, METH_VARARGS | METH_KEYWORDS, "Categorize a string attribute."},
    {"decategorize_attribute", (PyCFunction)Network_decategorize_attribute, METH_VARARGS | METH_KEYWORDS, "Convert categorical attribute to strings."},
    {"get_category_dictionary", (PyCFunction)Network_get_category_dictionary, METH_VARARGS, "Get categorical dictionary as {label: id}."},
    {"set_category_dictionary", (PyCFunction)Network_set_category_dictionary, METH_VARARGS | METH_KEYWORDS, "Set categorical dictionary from mapping or pairs."},
    {"measure_degree", (PyCFunction)Network_measure_degree, METH_VARARGS | METH_KEYWORDS, "Measure node degree values."},
    {"measure_strength", (PyCFunction)Network_measure_strength, METH_VARARGS | METH_KEYWORDS, "Measure node strength values."},
    {"measure_local_clustering_coefficient", (PyCFunction)Network_measure_local_clustering_coefficient, METH_VARARGS | METH_KEYWORDS, "Measure local clustering coefficient values."},
    {"measure_eigenvector_centrality", (PyCFunction)Network_measure_eigenvector_centrality, METH_VARARGS | METH_KEYWORDS, "Measure eigenvector centrality values."},
    {"measure_betweenness_centrality", (PyCFunction)Network_measure_betweenness_centrality, METH_VARARGS | METH_KEYWORDS, "Measure betweenness centrality values."},
    {"measure_node_dimension", (PyCFunction)Network_measure_node_dimension, METH_VARARGS | METH_KEYWORDS, "Measure local multiscale dimension for one node."},
    {"measure_dimension", (PyCFunction)Network_measure_dimension, METH_VARARGS | METH_KEYWORDS, "Measure global multiscale dimension statistics."},
    {NULL, NULL, 0, NULL}
};

static PyGetSetDef Network_getset[] = {
    {"is_directed", (getter)Network_is_directed, NULL, "Whether graph is directed.", NULL},
    {NULL}
};

static PyTypeObject PyHeliosNetworkType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "helios_network.Network",
    .tp_basicsize = sizeof(PyHeliosNetwork),
    .tp_dealloc = (destructor)Network_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = "Helios Network",
    .tp_methods = Network_methods,
    .tp_getset = Network_getset,
    .tp_init = (initproc)Network_init,
    .tp_new = Network_new,
};

static PyObject *Network_FromCXNetwork(CXNetworkRef network) {
    PyHeliosNetwork *obj = PyObject_New(PyHeliosNetwork, &PyHeliosNetworkType);
    if (!obj) {
        return NULL;
    }
    obj->network = network;
    obj->owns = 1;
    return (PyObject *)obj;
}

static PyObject *module_read_xnet(PyObject *self, PyObject *args) {
    (void)self;
    const char *path = NULL;
    if (!PyArg_ParseTuple(args, "s", &path)) {
        return NULL;
    }
    CXNetworkRef network = CXNetworkReadXNet(path);
    if (!network) {
        PyErr_SetString(PyExc_IOError, "Failed to read XNet file");
        return NULL;
    }
    return Network_FromCXNetwork(network);
}

static PyObject *module_read_bxnet(PyObject *self, PyObject *args) {
    (void)self;
    const char *path = NULL;
    if (!PyArg_ParseTuple(args, "s", &path)) {
        return NULL;
    }
    CXNetworkRef network = CXNetworkReadBXNet(path);
    if (!network) {
        PyErr_SetString(PyExc_IOError, "Failed to read BXNet file");
        return NULL;
    }
    return Network_FromCXNetwork(network);
}

static PyObject *module_read_zxnet(PyObject *self, PyObject *args) {
    (void)self;
    const char *path = NULL;
    if (!PyArg_ParseTuple(args, "s", &path)) {
        return NULL;
    }
    CXNetworkRef network = CXNetworkReadZXNet(path);
    if (!network) {
        PyErr_SetString(PyExc_IOError, "Failed to read ZXNet file");
        return NULL;
    }
    return Network_FromCXNetwork(network);
}

static PyMethodDef module_methods[] = {
    {"read_xnet", (PyCFunction)module_read_xnet, METH_VARARGS, "Read .xnet file into a Network."},
    {"read_bxnet", (PyCFunction)module_read_bxnet, METH_VARARGS, "Read .bxnet file into a Network."},
    {"read_zxnet", (PyCFunction)module_read_zxnet, METH_VARARGS, "Read .zxnet file into a Network."},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name = "helios_network._core",
    .m_doc = "Helios Network C core bindings.",
    .m_size = -1,
    .m_methods = module_methods,
};

PyMODINIT_FUNC PyInit__core(void) {
    if (PyType_Ready(&PyHeliosNetworkType) < 0) {
        return NULL;
    }

    PyObject *module = PyModule_Create(&moduledef);
    if (!module) {
        return NULL;
    }

    Py_INCREF(&PyHeliosNetworkType);
    PyModule_AddObject(module, "Network", (PyObject *)&PyHeliosNetworkType);

    PyModule_AddIntConstant(module, "ATTR_STRING", CXStringAttributeType);
    PyModule_AddIntConstant(module, "ATTR_BOOLEAN", CXBooleanAttributeType);
    PyModule_AddIntConstant(module, "ATTR_FLOAT", CXFloatAttributeType);
    PyModule_AddIntConstant(module, "ATTR_INTEGER", CXIntegerAttributeType);
    PyModule_AddIntConstant(module, "ATTR_UNSIGNED_INTEGER", CXUnsignedIntegerAttributeType);
    PyModule_AddIntConstant(module, "ATTR_DOUBLE", CXDoubleAttributeType);
    PyModule_AddIntConstant(module, "ATTR_CATEGORY", CXDataAttributeCategoryType);
    PyModule_AddIntConstant(module, "ATTR_DATA", CXDataAttributeType);
    PyModule_AddIntConstant(module, "ATTR_JAVASCRIPT", CXJavascriptAttributeType);
    PyModule_AddIntConstant(module, "ATTR_BIG_INTEGER", CXBigIntegerAttributeType);
    PyModule_AddIntConstant(module, "ATTR_UNSIGNED_BIG_INTEGER", CXUnsignedBigIntegerAttributeType);
    PyModule_AddIntConstant(module, "ATTR_MULTI_CATEGORY", CXDataAttributeMultiCategoryType);
    PyModule_AddIntConstant(module, "ATTR_UNKNOWN", CXUnknownAttributeType);

    PyModule_AddIntConstant(module, "SCOPE_NODE", CXAttributeScopeNode);
    PyModule_AddIntConstant(module, "SCOPE_EDGE", CXAttributeScopeEdge);
    PyModule_AddIntConstant(module, "SCOPE_NETWORK", CXAttributeScopeNetwork);

    PyModule_AddIntConstant(module, "DENSE_COLOR_U8X4", CXDenseColorFormatU8x4);
    PyModule_AddIntConstant(module, "DENSE_COLOR_U32X4", CXDenseColorFormatU32x4);

    PyModule_AddIntConstant(module, "CATEGORY_SORT_NONE", CX_CATEGORY_SORT_NONE);
    PyModule_AddIntConstant(module, "CATEGORY_SORT_FREQUENCY", CX_CATEGORY_SORT_FREQUENCY);
    PyModule_AddIntConstant(module, "CATEGORY_SORT_ALPHABETICAL", CX_CATEGORY_SORT_ALPHABETICAL);
    PyModule_AddIntConstant(module, "CATEGORY_SORT_NATURAL", CX_CATEGORY_SORT_NATURAL);

    PyModule_AddIntConstant(module, "DIMENSION_METHOD_FORWARD", CXDimensionForwardDifferenceMethod);
    PyModule_AddIntConstant(module, "DIMENSION_METHOD_BACKWARD", CXDimensionBackwardDifferenceMethod);
    PyModule_AddIntConstant(module, "DIMENSION_METHOD_CENTRAL", CXDimensionCentralDifferenceMethod);
    PyModule_AddIntConstant(module, "DIMENSION_METHOD_LEAST_SQUARES", CXDimensionLeastSquaresDifferenceMethod);

    PyModule_AddIntConstant(module, "NEIGHBOR_DIRECTION_OUT", CXNeighborDirectionOut);
    PyModule_AddIntConstant(module, "NEIGHBOR_DIRECTION_IN", CXNeighborDirectionIn);
    PyModule_AddIntConstant(module, "NEIGHBOR_DIRECTION_BOTH", CXNeighborDirectionBoth);

    PyModule_AddIntConstant(module, "STRENGTH_MEASURE_SUM", CXStrengthMeasureSum);
    PyModule_AddIntConstant(module, "STRENGTH_MEASURE_AVERAGE", CXStrengthMeasureAverage);
    PyModule_AddIntConstant(module, "STRENGTH_MEASURE_MAXIMUM", CXStrengthMeasureMaximum);
    PyModule_AddIntConstant(module, "STRENGTH_MEASURE_MINIMUM", CXStrengthMeasureMinimum);

    PyModule_AddIntConstant(module, "CLUSTERING_VARIANT_UNWEIGHTED", CXClusteringCoefficientUnweighted);
    PyModule_AddIntConstant(module, "CLUSTERING_VARIANT_ONNELA", CXClusteringCoefficientOnnela);
    PyModule_AddIntConstant(module, "CLUSTERING_VARIANT_NEWMAN", CXClusteringCoefficientNewman);

    PyModule_AddIntConstant(module, "MEASUREMENT_EXECUTION_AUTO", CXMeasurementExecutionAuto);
    PyModule_AddIntConstant(module, "MEASUREMENT_EXECUTION_SINGLE_THREAD", CXMeasurementExecutionSingleThread);
    PyModule_AddIntConstant(module, "MEASUREMENT_EXECUTION_PARALLEL", CXMeasurementExecutionParallel);

    return module;
}
