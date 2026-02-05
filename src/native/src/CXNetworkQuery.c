#include "CXNetwork.h"

#include <errno.h>
#if defined(_WIN32)
#include "musl-regex/regex.h"
#else
#include <regex.h>
#endif

typedef enum {
	CXQueryTokenEof = 0,
	CXQueryTokenLParen,
	CXQueryTokenRParen,
	CXQueryTokenAnd,
	CXQueryTokenOr,
	CXQueryTokenNot,
	CXQueryTokenIdent,
	CXQueryTokenString,
	CXQueryTokenNumber,
	CXQueryTokenEq,
	CXQueryTokenNe,
	CXQueryTokenLt,
	CXQueryTokenLte,
	CXQueryTokenGt,
	CXQueryTokenGte,
	CXQueryTokenIn,
	CXQueryTokenRegexMatch,
	CXQueryTokenComma,
	CXQueryTokenLBracket,
	CXQueryTokenRBracket,
	CXQueryTokenDot,
	CXQueryTokenDollar
} CXQueryTokenType;

typedef struct {
	CXQueryTokenType type;
	const char *start;
	size_t length;
	double number;
	char *string;
} CXQueryToken;

typedef struct {
	const char *input;
	size_t length;
	size_t pos;
	CXQueryToken current;
	CXBool hasError;
	const char *errorMessage;
	size_t errorOffset;
} CXQueryParser;

typedef enum {
	CXQueryValueNumber = 0,
	CXQueryValueString = 1,
	CXQueryValueList = 2,
	CXQueryValueRegex = 3
} CXQueryValueType;

typedef enum {
	CXQueryOpEq = 0,
	CXQueryOpNe,
	CXQueryOpLt,
	CXQueryOpLte,
	CXQueryOpGt,
	CXQueryOpGte,
	CXQueryOpIn,
	CXQueryOpRegex
} CXQueryOperator;

typedef enum {
	CXQueryQualifierSelf = 0,
	CXQueryQualifierSrc,
	CXQueryQualifierDst,
	CXQueryQualifierAny,
	CXQueryQualifierBoth,
	CXQueryQualifierNeighborAny,
	CXQueryQualifierNeighborBoth
} CXQueryQualifier;

typedef enum {
	CXQueryAccessNone = 0,
	CXQueryAccessAny,
	CXQueryAccessAll,
	CXQueryAccessIndex,
	CXQueryAccessMin,
	CXQueryAccessMax,
	CXQueryAccessAvg,
	CXQueryAccessMedian,
	CXQueryAccessStd,
	CXQueryAccessAbs,
	CXQueryAccessDot
} CXQueryAccessMode;

typedef struct {
	char *name;
	CXQueryQualifier qualifier;
	CXQueryAccessMode accessMode;
	CXSize accessIndex;
	char *dotName;
	CXAttributeRef dotAttribute;
	double *dotVector;
	CXSize dotCount;
	CXQueryOperator op;
	CXQueryValueType valueType;
	double numberValue;
	char *stringValue;
	double *numberList;
	char **stringList;
	size_t listCount;
	char *regexPattern;
	regex_t regex;
	CXBool regexCompiled;
	CXAttributeRef attribute;
	CXAttributeScope scope;
} CXQueryPredicate;

typedef enum {
	CXQueryExprPredicate = 0,
	CXQueryExprNot,
	CXQueryExprBinary
} CXQueryExprType;

typedef enum {
	CXQueryBinaryAnd = 0,
	CXQueryBinaryOr
} CXQueryBinaryOp;

typedef struct CXQueryExpr {
	CXQueryExprType type;
	union {
		CXQueryPredicate predicate;
		struct {
			struct CXQueryExpr *expr;
		} notExpr;
		struct {
			CXQueryBinaryOp op;
			struct CXQueryExpr *left;
			struct CXQueryExpr *right;
		} binary;
	} data;
} CXQueryExpr;

static char g_query_error_message[256] = {0};
static CXSize g_query_error_offset = 0;

static void CXQueryClearError(void) {
	g_query_error_message[0] = '\0';
	g_query_error_offset = 0;
}

static void CXQuerySetError(const char *message, size_t offset) {
	if (!message) {
		message = "Unknown query error";
	}
	strncpy(g_query_error_message, message, sizeof(g_query_error_message) - 1);
	g_query_error_message[sizeof(g_query_error_message) - 1] = '\0';
	g_query_error_offset = (CXSize)offset;
}

const CXString CXNetworkQueryLastErrorMessage(void) {
	return g_query_error_message;
}

CXSize CXNetworkQueryLastErrorOffset(void) {
	return g_query_error_offset;
}

static void CXQuerySkipWhitespace(CXQueryParser *parser) {
	while (parser->pos < parser->length && isspace((unsigned char)parser->input[parser->pos])) {
		parser->pos++;
	}
}

static CXBool CXQueryIsIdentStart(char c) {
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

static CXBool CXQueryIsIdentChar(char c) {
	return CXQueryIsIdentStart(c) || (c >= '0' && c <= '9');
}

static CXBool CXQueryMatchKeyword(const CXQueryToken *token, const char *keyword) {
	size_t len = strlen(keyword);
	if (token->length != len) {
		return CXFalse;
	}
	for (size_t i = 0; i < len; i++) {
		char a = token->start[i];
		char b = keyword[i];
		if (a >= 'A' && a <= 'Z') {
			a = (char)(a - 'A' + 'a');
		}
		if (b >= 'A' && b <= 'Z') {
			b = (char)(b - 'A' + 'a');
		}
		if (a != b) {
			return CXFalse;
		}
	}
	return CXTrue;
}

static void CXQueryTokenFree(CXQueryToken *token) {
	if (token->string) {
		free(token->string);
		token->string = NULL;
	}
}

static char *CXQueryCopyTokenString(const CXQueryToken *token) {
	char *text = (char *)malloc(token->length + 1);
	if (!text) {
		return NULL;
	}
	memcpy(text, token->start, token->length);
	text[token->length] = '\0';
	return text;
}

static CXQueryToken CXQueryReadString(CXQueryParser *parser) {
	CXQueryToken token = {0};
	token.type = CXQueryTokenString;
	parser->pos++; // skip opening quote
	size_t start = parser->pos;
	char *buffer = NULL;
	size_t bufferLen = 0;
	while (parser->pos < parser->length) {
		char c = parser->input[parser->pos];
		if (c == '"') {
			parser->pos++;
			if (buffer) {
				char *next = (char *)realloc(buffer, bufferLen + 1);
				if (!next) {
					free(buffer);
					token.type = CXQueryTokenEof;
					return token;
				}
				buffer = next;
				buffer[bufferLen] = '\0';
			}
			token.start = parser->input + start;
			token.length = parser->pos - start - 1;
			token.string = buffer;
			return token;
		}
		if (c == '\\') {
			parser->pos++;
			if (parser->pos >= parser->length) {
				break;
			}
			char esc = parser->input[parser->pos];
			char decoded = esc;
			switch (esc) {
				case 'n': decoded = '\n'; break;
				case 't': decoded = '\t'; break;
				case 'r': decoded = '\r'; break;
				case '\\': decoded = '\\'; break;
				case '"': decoded = '"'; break;
				default: break;
			}
			char *next = (char *)realloc(buffer, bufferLen + 1);
			if (!next) {
				free(buffer);
				token.type = CXQueryTokenEof;
				return token;
			}
			buffer = next;
			buffer[bufferLen++] = decoded;
			parser->pos++;
			continue;
		}
		char *next = (char *)realloc(buffer, bufferLen + 1);
		if (!next) {
			free(buffer);
			token.type = CXQueryTokenEof;
			return token;
		}
		buffer = next;
		buffer[bufferLen++] = c;
		parser->pos++;
	}
	if (buffer) {
		free(buffer);
	}
	token.type = CXQueryTokenEof;
	return token;
}

static CXQueryToken CXQueryNextToken(CXQueryParser *parser) {
	CXQuerySkipWhitespace(parser);
	CXQueryToken token = {0};
	token.start = parser->input + parser->pos;
	token.length = 1;
	token.string = NULL;
	if (parser->pos >= parser->length) {
		token.type = CXQueryTokenEof;
		return token;
	}
	char c = parser->input[parser->pos];
	if (c == '(') {
		parser->pos++;
		token.type = CXQueryTokenLParen;
		return token;
	}
	if (c == ')') {
		parser->pos++;
		token.type = CXQueryTokenRParen;
		return token;
	}
	if (c == '[') {
		parser->pos++;
		token.type = CXQueryTokenLBracket;
		return token;
	}
	if (c == ']') {
		parser->pos++;
		token.type = CXQueryTokenRBracket;
		return token;
	}
	if (c == ',') {
		parser->pos++;
		token.type = CXQueryTokenComma;
		return token;
	}
	if (c == '.') {
		parser->pos++;
		token.type = CXQueryTokenDot;
		return token;
	}
	if (c == '$') {
		parser->pos++;
		token.type = CXQueryTokenDollar;
		return token;
	}
	if (c == '=' && parser->pos + 1 < parser->length && parser->input[parser->pos + 1] == '~') {
		parser->pos += 2;
		token.type = CXQueryTokenRegexMatch;
		token.length = 2;
		return token;
	}
	if (c == '=' && parser->pos + 1 < parser->length && parser->input[parser->pos + 1] == '=') {
		parser->pos += 2;
		token.type = CXQueryTokenEq;
		token.length = 2;
		return token;
	}
	if (c == '!' && parser->pos + 1 < parser->length && parser->input[parser->pos + 1] == '=') {
		parser->pos += 2;
		token.type = CXQueryTokenNe;
		token.length = 2;
		return token;
	}
	if (c == '<') {
		if (parser->pos + 1 < parser->length && parser->input[parser->pos + 1] == '=') {
			parser->pos += 2;
			token.type = CXQueryTokenLte;
			token.length = 2;
			return token;
		}
		parser->pos++;
		token.type = CXQueryTokenLt;
		return token;
	}
	if (c == '>') {
		if (parser->pos + 1 < parser->length && parser->input[parser->pos + 1] == '=') {
			parser->pos += 2;
			token.type = CXQueryTokenGte;
			token.length = 2;
			return token;
		}
		parser->pos++;
		token.type = CXQueryTokenGt;
		return token;
	}
	if (c == '"') {
		token = CXQueryReadString(parser);
		if (token.type == CXQueryTokenEof) {
			parser->hasError = CXTrue;
			parser->errorMessage = "Unterminated string literal";
			parser->errorOffset = parser->pos;
		}
		return token;
	}
	if (c == '-' || (c >= '0' && c <= '9')) {
		char *endptr = NULL;
		errno = 0;
		double value = strtod(parser->input + parser->pos, &endptr);
		if (endptr == parser->input + parser->pos) {
			parser->pos++;
			token.type = CXQueryTokenEof;
			return token;
		}
		size_t consumed = (size_t)(endptr - (parser->input + parser->pos));
		parser->pos += consumed;
		token.type = CXQueryTokenNumber;
		token.length = consumed;
		token.number = value;
		return token;
	}
	if (CXQueryIsIdentStart(c)) {
		size_t start = parser->pos;
		parser->pos++;
		while (parser->pos < parser->length && CXQueryIsIdentChar(parser->input[parser->pos])) {
			parser->pos++;
		}
		token.start = parser->input + start;
		token.length = parser->pos - start;
		if (CXQueryMatchKeyword(&token, "and")) {
			token.type = CXQueryTokenAnd;
		} else if (CXQueryMatchKeyword(&token, "or")) {
			token.type = CXQueryTokenOr;
		} else if (CXQueryMatchKeyword(&token, "not")) {
			token.type = CXQueryTokenNot;
		} else if (CXQueryMatchKeyword(&token, "in")) {
			token.type = CXQueryTokenIn;
		} else {
			token.type = CXQueryTokenIdent;
		}
		return token;
	}
	parser->pos++;
	token.type = CXQueryTokenEof;
	return token;
}

static void CXQueryAdvance(CXQueryParser *parser) {
	CXQueryTokenFree(&parser->current);
	parser->current = CXQueryNextToken(parser);
}

static CXBool CXQueryConsume(CXQueryParser *parser, CXQueryTokenType type, const char *message) {
	if (parser->current.type == type) {
		CXQueryAdvance(parser);
		return CXTrue;
	}
	parser->hasError = CXTrue;
	parser->errorMessage = message;
	parser->errorOffset = parser->current.start ? (size_t)(parser->current.start - parser->input) : parser->pos;
	return CXFalse;
}

static CXQueryExpr *CXQueryParseExpression(CXQueryParser *parser);

static CXQueryExpr *CXQueryNewExpr(CXQueryExprType type) {
	CXQueryExpr *expr = (CXQueryExpr *)calloc(1, sizeof(CXQueryExpr));
	if (!expr) {
		return NULL;
	}
	expr->type = type;
	return expr;
}

static CXQueryExpr *CXQueryParsePrimary(CXQueryParser *parser);

static CXQueryExpr *CXQueryParseNot(CXQueryParser *parser) {
	if (parser->current.type == CXQueryTokenNot) {
		CXQueryAdvance(parser);
		CXQueryExpr *expr = CXQueryNewExpr(CXQueryExprNot);
		if (!expr) {
			parser->hasError = CXTrue;
			parser->errorMessage = "Out of memory";
			parser->errorOffset = parser->pos;
			return NULL;
		}
		expr->data.notExpr.expr = CXQueryParseNot(parser);
		return expr;
	}
	return CXQueryParsePrimary(parser);
}

static CXQueryExpr *CXQueryParseAnd(CXQueryParser *parser) {
	CXQueryExpr *expr = CXQueryParseNot(parser);
	while (expr && parser->current.type == CXQueryTokenAnd) {
		CXQueryAdvance(parser);
		CXQueryExpr *right = CXQueryParseNot(parser);
		CXQueryExpr *node = CXQueryNewExpr(CXQueryExprBinary);
		if (!node) {
			parser->hasError = CXTrue;
			parser->errorMessage = "Out of memory";
			parser->errorOffset = parser->pos;
			return expr;
		}
		node->data.binary.op = CXQueryBinaryAnd;
		node->data.binary.left = expr;
		node->data.binary.right = right;
		expr = node;
	}
	return expr;
}

static CXQueryExpr *CXQueryParseOr(CXQueryParser *parser) {
	CXQueryExpr *expr = CXQueryParseAnd(parser);
	while (expr && parser->current.type == CXQueryTokenOr) {
		CXQueryAdvance(parser);
		CXQueryExpr *right = CXQueryParseAnd(parser);
		CXQueryExpr *node = CXQueryNewExpr(CXQueryExprBinary);
		if (!node) {
			parser->hasError = CXTrue;
			parser->errorMessage = "Out of memory";
			parser->errorOffset = parser->pos;
			return expr;
		}
		node->data.binary.op = CXQueryBinaryOr;
		node->data.binary.left = expr;
		node->data.binary.right = right;
		expr = node;
	}
	return expr;
}

static CXQueryExpr *CXQueryParseExpression(CXQueryParser *parser) {
	return CXQueryParseOr(parser);
}

static CXBool CXQueryParseQualifier(CXQueryParser *parser, CXQueryQualifier *outQualifier) {
	if (parser->current.type != CXQueryTokenIdent) {
		parser->hasError = CXTrue;
		parser->errorMessage = "Expected qualifier after '$'";
		parser->errorOffset = parser->current.start ? (size_t)(parser->current.start - parser->input) : parser->pos;
		return CXFalse;
	}
	if (CXQueryMatchKeyword(&parser->current, "src")) {
		*outQualifier = CXQueryQualifierSrc;
	} else if (CXQueryMatchKeyword(&parser->current, "dst")) {
		*outQualifier = CXQueryQualifierDst;
	} else if (CXQueryMatchKeyword(&parser->current, "any")) {
		*outQualifier = CXQueryQualifierAny;
	} else if (CXQueryMatchKeyword(&parser->current, "both")) {
		*outQualifier = CXQueryQualifierBoth;
	} else {
		parser->hasError = CXTrue;
		parser->errorMessage = "Unknown qualifier";
		parser->errorOffset = parser->current.start ? (size_t)(parser->current.start - parser->input) : parser->pos;
		return CXFalse;
	}
	CXQueryAdvance(parser);
	return CXTrue;
}

static CXBool CXQueryParsePredicate(CXQueryParser *parser, CXQueryPredicate *outPredicate) {
	memset(outPredicate, 0, sizeof(*outPredicate));
	outPredicate->qualifier = CXQueryQualifierSelf;

	if (parser->current.type == CXQueryTokenDollar) {
		CXQueryAdvance(parser);
		CXQueryQualifier qualifier = CXQueryQualifierSelf;
		if (!CXQueryParseQualifier(parser, &qualifier)) {
			return CXFalse;
		}
		if (!CXQueryConsume(parser, CXQueryTokenDot, "Expected '.' after qualifier")) {
			return CXFalse;
		}
		if (qualifier == CXQueryQualifierAny || qualifier == CXQueryQualifierBoth) {
			if (parser->current.type == CXQueryTokenIdent && CXQueryMatchKeyword(&parser->current, "neighbor")) {
				CXQueryAdvance(parser);
				if (!CXQueryConsume(parser, CXQueryTokenDot, "Expected '.' after neighbor")) {
					return CXFalse;
				}
				outPredicate->qualifier = (qualifier == CXQueryQualifierAny)
					? CXQueryQualifierNeighborAny
					: CXQueryQualifierNeighborBoth;
			} else {
				outPredicate->qualifier = qualifier;
			}
		} else {
			outPredicate->qualifier = qualifier;
		}
	}

	if (parser->current.type != CXQueryTokenIdent) {
		parser->hasError = CXTrue;
		parser->errorMessage = "Expected attribute name";
		parser->errorOffset = parser->current.start ? (size_t)(parser->current.start - parser->input) : parser->pos;
		return CXFalse;
	}
	outPredicate->name = CXQueryCopyTokenString(&parser->current);
	if (!outPredicate->name) {
		parser->hasError = CXTrue;
		parser->errorMessage = "Out of memory";
		parser->errorOffset = parser->pos;
		return CXFalse;
	}
	CXQueryAdvance(parser);

	if (parser->current.type == CXQueryTokenLBracket) {
		CXQueryAdvance(parser);
		if (parser->current.type != CXQueryTokenNumber) {
			parser->hasError = CXTrue;
			parser->errorMessage = "Expected numeric index";
			parser->errorOffset = parser->current.start ? (size_t)(parser->current.start - parser->input) : parser->pos;
			return CXFalse;
		}
		if (parser->current.number < 0) {
			parser->hasError = CXTrue;
			parser->errorMessage = "Index must be >= 0";
			parser->errorOffset = parser->current.start ? (size_t)(parser->current.start - parser->input) : parser->pos;
			return CXFalse;
		}
		outPredicate->accessMode = CXQueryAccessIndex;
		outPredicate->accessIndex = (CXSize)parser->current.number;
		CXQueryAdvance(parser);
		if (!CXQueryConsume(parser, CXQueryTokenRBracket, "Expected ']' after index")) {
			return CXFalse;
		}
	}

	if (parser->current.type == CXQueryTokenDot) {
		CXQueryAdvance(parser);
		if (parser->current.type != CXQueryTokenIdent) {
			parser->hasError = CXTrue;
			parser->errorMessage = "Expected accessor name after '.'";
			parser->errorOffset = parser->current.start ? (size_t)(parser->current.start - parser->input) : parser->pos;
			return CXFalse;
		}
		if (outPredicate->accessMode != CXQueryAccessNone) {
			parser->hasError = CXTrue;
			parser->errorMessage = "Only one vector accessor is allowed";
			parser->errorOffset = parser->current.start ? (size_t)(parser->current.start - parser->input) : parser->pos;
			return CXFalse;
		}
		if (CXQueryMatchKeyword(&parser->current, "any")) {
			outPredicate->accessMode = CXQueryAccessAny;
		} else if (CXQueryMatchKeyword(&parser->current, "all")) {
			outPredicate->accessMode = CXQueryAccessAll;
		} else if (CXQueryMatchKeyword(&parser->current, "min")) {
			outPredicate->accessMode = CXQueryAccessMin;
		} else if (CXQueryMatchKeyword(&parser->current, "max")) {
			outPredicate->accessMode = CXQueryAccessMax;
		} else if (CXQueryMatchKeyword(&parser->current, "avg")) {
			outPredicate->accessMode = CXQueryAccessAvg;
		} else if (CXQueryMatchKeyword(&parser->current, "median")) {
			outPredicate->accessMode = CXQueryAccessMedian;
		} else if (CXQueryMatchKeyword(&parser->current, "std")) {
			outPredicate->accessMode = CXQueryAccessStd;
		} else if (CXQueryMatchKeyword(&parser->current, "abs")) {
			outPredicate->accessMode = CXQueryAccessAbs;
		} else if (CXQueryMatchKeyword(&parser->current, "dot")) {
			outPredicate->accessMode = CXQueryAccessDot;
		} else {
			parser->hasError = CXTrue;
			parser->errorMessage = "Unknown accessor";
			parser->errorOffset = parser->current.start ? (size_t)(parser->current.start - parser->input) : parser->pos;
			return CXFalse;
		}
		CXQueryAdvance(parser);
		if (outPredicate->accessMode == CXQueryAccessDot) {
			if (!CXQueryConsume(parser, CXQueryTokenLParen, "Expected '(' after dot")) {
				return CXFalse;
			}
			if (parser->current.type == CXQueryTokenIdent) {
				outPredicate->dotName = CXQueryCopyTokenString(&parser->current);
				if (!outPredicate->dotName) {
					parser->hasError = CXTrue;
					parser->errorMessage = "Out of memory";
					parser->errorOffset = parser->pos;
					return CXFalse;
				}
				CXQueryAdvance(parser);
			} else if (parser->current.type == CXQueryTokenLBracket) {
				CXQueryAdvance(parser);
				while (parser->current.type != CXQueryTokenRBracket) {
					if (parser->current.type != CXQueryTokenNumber) {
						parser->hasError = CXTrue;
						parser->errorMessage = "Expected numeric literal in dot vector";
						parser->errorOffset = parser->current.start ? (size_t)(parser->current.start - parser->input) : parser->pos;
						return CXFalse;
					}
					double *next = realloc(outPredicate->dotVector, sizeof(double) * (outPredicate->dotCount + 1));
					if (!next) {
						parser->hasError = CXTrue;
						parser->errorMessage = "Out of memory";
						parser->errorOffset = parser->pos;
						return CXFalse;
					}
					outPredicate->dotVector = next;
					outPredicate->dotVector[outPredicate->dotCount++] = parser->current.number;
					CXQueryAdvance(parser);
					if (parser->current.type == CXQueryTokenComma) {
						CXQueryAdvance(parser);
					}
				}
				CXQueryAdvance(parser);
			} else {
				parser->hasError = CXTrue;
				parser->errorMessage = "Expected attribute name or vector literal in dot()";
				parser->errorOffset = parser->current.start ? (size_t)(parser->current.start - parser->input) : parser->pos;
				return CXFalse;
			}
			if (!CXQueryConsume(parser, CXQueryTokenRParen, "Expected ')' after dot attribute")) {
				return CXFalse;
			}
		}
	}

	switch (parser->current.type) {
		case CXQueryTokenEq: outPredicate->op = CXQueryOpEq; break;
		case CXQueryTokenNe: outPredicate->op = CXQueryOpNe; break;
		case CXQueryTokenLt: outPredicate->op = CXQueryOpLt; break;
		case CXQueryTokenLte: outPredicate->op = CXQueryOpLte; break;
		case CXQueryTokenGt: outPredicate->op = CXQueryOpGt; break;
		case CXQueryTokenGte: outPredicate->op = CXQueryOpGte; break;
		case CXQueryTokenIn: outPredicate->op = CXQueryOpIn; break;
		case CXQueryTokenRegexMatch: outPredicate->op = CXQueryOpRegex; break;
		default:
			parser->hasError = CXTrue;
			parser->errorMessage = "Expected comparison operator";
			parser->errorOffset = parser->current.start ? (size_t)(parser->current.start - parser->input) : parser->pos;
			return CXFalse;
	}
	CXQueryAdvance(parser);

	if (outPredicate->op == CXQueryOpIn) {
		if (parser->current.type != CXQueryTokenLParen) {
			parser->hasError = CXTrue;
			parser->errorMessage = "Expected '(' after IN";
			parser->errorOffset = parser->current.start ? (size_t)(parser->current.start - parser->input) : parser->pos;
			return CXFalse;
		}
		CXQueryAdvance(parser);
		while (parser->current.type != CXQueryTokenRParen) {
			if (parser->current.type == CXQueryTokenNumber) {
				if (outPredicate->stringList) {
					parser->hasError = CXTrue;
					parser->errorMessage = "IN list cannot mix strings and numbers";
					parser->errorOffset = parser->current.start ? (size_t)(parser->current.start - parser->input) : parser->pos;
					return CXFalse;
				}
				double *next = realloc(outPredicate->numberList, sizeof(double) * (outPredicate->listCount + 1));
				if (!next) {
					parser->hasError = CXTrue;
					parser->errorMessage = "Out of memory";
					parser->errorOffset = parser->pos;
					return CXFalse;
				}
				outPredicate->numberList = next;
				outPredicate->numberList[outPredicate->listCount++] = parser->current.number;
				outPredicate->valueType = CXQueryValueList;
				CXQueryAdvance(parser);
			} else if (parser->current.type == CXQueryTokenString) {
				if (outPredicate->numberList) {
					parser->hasError = CXTrue;
					parser->errorMessage = "IN list cannot mix strings and numbers";
					parser->errorOffset = parser->current.start ? (size_t)(parser->current.start - parser->input) : parser->pos;
					return CXFalse;
				}
				char *text = NULL;
				if (parser->current.string) {
					text = parser->current.string;
					parser->current.string = NULL;
				} else {
					text = CXQueryCopyTokenString(&parser->current);
				}
				if (!text) {
					parser->hasError = CXTrue;
					parser->errorMessage = "Out of memory";
					parser->errorOffset = parser->pos;
					return CXFalse;
				}
				char **next = realloc(outPredicate->stringList, sizeof(char *) * (outPredicate->listCount + 1));
				if (!next) {
					free(text);
					parser->hasError = CXTrue;
					parser->errorMessage = "Out of memory";
					parser->errorOffset = parser->pos;
					return CXFalse;
				}
				outPredicate->stringList = next;
				outPredicate->stringList[outPredicate->listCount++] = text;
				outPredicate->valueType = CXQueryValueList;
				CXQueryAdvance(parser);
			} else {
				parser->hasError = CXTrue;
				parser->errorMessage = "Expected literal in IN list";
				parser->errorOffset = parser->current.start ? (size_t)(parser->current.start - parser->input) : parser->pos;
				return CXFalse;
			}
			if (parser->current.type == CXQueryTokenComma) {
				CXQueryAdvance(parser);
			}
		}
		CXQueryAdvance(parser);
		if (outPredicate->listCount == 0) {
			parser->hasError = CXTrue;
			parser->errorMessage = "IN list cannot be empty";
			parser->errorOffset = parser->pos;
			return CXFalse;
		}
		return CXTrue;
	}

	if (outPredicate->op == CXQueryOpRegex) {
		if (parser->current.type == CXQueryTokenString) {
			outPredicate->valueType = CXQueryValueRegex;
			if (parser->current.string) {
				outPredicate->regexPattern = parser->current.string;
				parser->current.string = NULL;
			} else {
				outPredicate->regexPattern = CXQueryCopyTokenString(&parser->current);
			}
			if (!outPredicate->regexPattern) {
				parser->hasError = CXTrue;
				parser->errorMessage = "Out of memory";
				parser->errorOffset = parser->pos;
				return CXFalse;
			}
			CXQueryAdvance(parser);
			return CXTrue;
		}
		parser->hasError = CXTrue;
		parser->errorMessage = "Expected string literal for regex";
		parser->errorOffset = parser->current.start ? (size_t)(parser->current.start - parser->input) : parser->pos;
		return CXFalse;
	}

	if (parser->current.type == CXQueryTokenNumber) {
		outPredicate->valueType = CXQueryValueNumber;
		outPredicate->numberValue = parser->current.number;
		CXQueryAdvance(parser);
		return CXTrue;
	}
	if (parser->current.type == CXQueryTokenString) {
		outPredicate->valueType = CXQueryValueString;
		if (parser->current.string) {
			outPredicate->stringValue = parser->current.string;
			parser->current.string = NULL;
		} else {
			outPredicate->stringValue = CXQueryCopyTokenString(&parser->current);
		}
		if (!outPredicate->stringValue) {
			parser->hasError = CXTrue;
			parser->errorMessage = "Out of memory";
			parser->errorOffset = parser->pos;
			return CXFalse;
		}
		CXQueryAdvance(parser);
		return CXTrue;
	}

	parser->hasError = CXTrue;
	parser->errorMessage = "Expected literal value";
	parser->errorOffset = parser->current.start ? (size_t)(parser->current.start - parser->input) : parser->pos;
	return CXFalse;
}

static CXQueryExpr *CXQueryParsePrimary(CXQueryParser *parser) {
	if (parser->current.type == CXQueryTokenLParen) {
		CXQueryAdvance(parser);
		CXQueryExpr *expr = CXQueryParseExpression(parser);
		if (!CXQueryConsume(parser, CXQueryTokenRParen, "Expected ')'")) {
			return expr;
		}
		return expr;
	}
	CXQueryExpr *expr = CXQueryNewExpr(CXQueryExprPredicate);
	if (!expr) {
		parser->hasError = CXTrue;
		parser->errorMessage = "Out of memory";
		parser->errorOffset = parser->pos;
		return NULL;
	}
	if (!CXQueryParsePredicate(parser, &expr->data.predicate)) {
		free(expr);
		return NULL;
	}
	return expr;
}

static void CXQueryFreeExpr(CXQueryExpr *expr) {
	if (!expr) {
		return;
	}
	switch (expr->type) {
		case CXQueryExprPredicate:
			free(expr->data.predicate.name);
			free(expr->data.predicate.stringValue);
			if (expr->data.predicate.stringList) {
				for (size_t i = 0; i < expr->data.predicate.listCount; i++) {
					free(expr->data.predicate.stringList[i]);
				}
				free(expr->data.predicate.stringList);
			}
			free(expr->data.predicate.numberList);
			free(expr->data.predicate.regexPattern);
			free(expr->data.predicate.dotName);
			free(expr->data.predicate.dotVector);
			if (expr->data.predicate.regexCompiled) {
				regfree(&expr->data.predicate.regex);
			}
			break;
		case CXQueryExprNot:
			CXQueryFreeExpr(expr->data.notExpr.expr);
			break;
		case CXQueryExprBinary:
			CXQueryFreeExpr(expr->data.binary.left);
			CXQueryFreeExpr(expr->data.binary.right);
			break;
		default:
			break;
	}
	free(expr);
}

static CXBool CXQueryDecodeCategoryId(const void *data, int32_t *outId) {
	if (!outId) {
		return CXFalse;
	}
	uintptr_t raw = (uintptr_t)data;
	if (raw == 0) {
		return CXFalse;
	}
	if (raw == 1u) {
		*outId = -1;
		return CXTrue;
	}
	*outId = (int32_t)(uint32_t)(raw - 2u);
	return CXTrue;
}

static CXBool CXQueryResolveAttribute(CXNetworkRef network, CXAttributeScope scope, CXQueryPredicate *predicate, const char **outError) {
	CXAttributeRef attr = NULL;
	if (scope == CXAttributeScopeNode) {
		attr = CXNetworkGetNodeAttribute(network, predicate->name);
	} else if (scope == CXAttributeScopeEdge) {
		attr = CXNetworkGetEdgeAttribute(network, predicate->name);
	} else {
		attr = CXNetworkGetNetworkAttribute(network, predicate->name);
	}
	if (!attr) {
		*outError = "Attribute not found";
		return CXFalse;
	}
	predicate->attribute = attr;
	predicate->scope = scope;
	return CXTrue;
}

static CXBool CXQueryAttributeIsNumeric(const CXAttributeRef attr) {
	if (!attr) {
		return CXFalse;
	}
	switch (attr->type) {
		case CXBooleanAttributeType:
		case CXFloatAttributeType:
		case CXDoubleAttributeType:
		case CXIntegerAttributeType:
		case CXUnsignedIntegerAttributeType:
		case CXBigIntegerAttributeType:
		case CXUnsignedBigIntegerAttributeType:
		case CXDataAttributeCategoryType:
			return CXTrue;
		default:
			return CXFalse;
	}
}

static CXBool CXQueryResolveAttributeByName(CXNetworkRef network, CXAttributeScope scope, const char *name, CXAttributeRef *outAttr) {
	if (!network || !name || !outAttr) {
		return CXFalse;
	}
	CXAttributeRef attr = NULL;
	if (scope == CXAttributeScopeNode) {
		attr = CXNetworkGetNodeAttribute(network, name);
	} else if (scope == CXAttributeScopeEdge) {
		attr = CXNetworkGetEdgeAttribute(network, name);
	} else {
		attr = CXNetworkGetNetworkAttribute(network, name);
	}
	if (!attr) {
		return CXFalse;
	}
	*outAttr = attr;
	return CXTrue;
}

static CXBool CXQueryBindAttributes(CXNetworkRef network, CXQueryExpr *expr, CXAttributeScope selfScope, const char **outError) {
	if (!expr) {
		return CXTrue;
	}
	switch (expr->type) {
		case CXQueryExprPredicate: {
			CXQueryPredicate *pred = &expr->data.predicate;
			CXAttributeScope scope = selfScope;
			switch (pred->qualifier) {
				case CXQueryQualifierSelf:
					scope = selfScope;
					break;
				case CXQueryQualifierSrc:
				case CXQueryQualifierDst:
				case CXQueryQualifierAny:
				case CXQueryQualifierBoth:
				case CXQueryQualifierNeighborAny:
				case CXQueryQualifierNeighborBoth:
					scope = CXAttributeScopeNode;
					break;
				default:
					break;
			}
			if (!CXQueryResolveAttribute(network, scope, pred, outError)) {
				return CXFalse;
			}
			if (pred->op == CXQueryOpRegex) {
				if (pred->attribute->type != CXStringAttributeType) {
					*outError = "Regex queries are only supported for string attributes";
					return CXFalse;
				}
				if (!pred->regexPattern) {
					*outError = "Missing regex pattern";
					return CXFalse;
				}
				if (regcomp(&pred->regex, pred->regexPattern, REG_EXTENDED | REG_NOSUB) != 0) {
					*outError = "Invalid regex pattern";
					return CXFalse;
				}
				pred->regexCompiled = CXTrue;
			}
			if (pred->op == CXQueryOpIn && pred->valueType == CXQueryValueList) {
				if (pred->attribute->type == CXStringAttributeType) {
					if (!pred->stringList || pred->listCount == 0) {
						*outError = "IN list cannot be empty";
						return CXFalse;
					}
				} else if (pred->attribute->type == CXDataAttributeCategoryType) {
					if (!pred->stringList || pred->listCount == 0) {
						*outError = "IN list cannot be empty";
						return CXFalse;
					}
					if (!pred->attribute->categoricalDictionary) {
						*outError = "Categorical dictionary is missing";
						return CXFalse;
					}
					double *ids = malloc(sizeof(double) * pred->listCount);
					if (!ids) {
						*outError = "Out of memory";
						return CXFalse;
					}
					for (size_t i = 0; i < pred->listCount; i++) {
						void *encoded = CXStringDictionaryEntryForKey(pred->attribute->categoricalDictionary, pred->stringList[i]);
						int32_t id = 0;
						if (!encoded || !CXQueryDecodeCategoryId(encoded, &id)) {
							free(ids);
							*outError = "Category label not found";
							return CXFalse;
						}
						ids[i] = (double)id;
					}
					for (size_t i = 0; i < pred->listCount; i++) {
						free(pred->stringList[i]);
					}
					free(pred->stringList);
					pred->stringList = NULL;
					pred->numberList = ids;
				} else {
					if (!pred->numberList || pred->listCount == 0) {
						*outError = "IN list cannot be empty";
						return CXFalse;
					}
				}
			}
			return CXTrue;
		}
		case CXQueryExprNot:
			return CXQueryBindAttributes(network, expr->data.notExpr.expr, selfScope, outError);
		case CXQueryExprBinary:
			if (!CXQueryBindAttributes(network, expr->data.binary.left, selfScope, outError)) {
				return CXFalse;
			}
			return CXQueryBindAttributes(network, expr->data.binary.right, selfScope, outError);
		default:
			return CXTrue;
	}
}

static CXBool CXQueryGetNumericValueAt(CXAttributeRef attr, CXIndex index, CXSize dim, double *outValue) {
	if (!attr || !outValue || !attr->data) {
		return CXFalse;
	}
	if (index >= attr->capacity) {
		return CXFalse;
	}
	uint8_t *base = attr->data + (size_t)index * attr->stride + (size_t)dim * attr->elementSize;
	switch (attr->type) {
		case CXBooleanAttributeType: {
			uint8_t value = *(uint8_t *)base;
			*outValue = value ? 1.0 : 0.0;
			return CXTrue;
		}
		case CXFloatAttributeType: {
			*outValue = *(float *)base;
			return CXTrue;
		}
		case CXDoubleAttributeType: {
			*outValue = *(double *)base;
			return CXTrue;
		}
		case CXIntegerAttributeType: {
			*outValue = (double)(*(int32_t *)base);
			return CXTrue;
		}
		case CXUnsignedIntegerAttributeType: {
			*outValue = (double)(*(uint32_t *)base);
			return CXTrue;
		}
		case CXBigIntegerAttributeType: {
			*outValue = (double)(*(int64_t *)base);
			return CXTrue;
		}
		case CXUnsignedBigIntegerAttributeType: {
			*outValue = (double)(*(uint64_t *)base);
			return CXTrue;
		}
		case CXDataAttributeCategoryType: {
			*outValue = (double)(*(int32_t *)base);
			return CXTrue;
		}
		default:
			return CXFalse;
	}
}

static const char *CXQueryGetStringValueAt(CXAttributeRef attr, CXIndex index, CXSize dim) {
	if (!attr || !attr->data || index >= attr->capacity) {
		return NULL;
	}
	uint8_t *base = attr->data + (size_t)index * attr->stride;
	if (attr->type == CXStringAttributeType) {
		CXString *strings = (CXString *)base;
		return strings[dim];
	}
	return NULL;
}

static int CXQueryCompareDouble(const void *lhs, const void *rhs);
static CXBool CXQueryComputeNumericAccessor(const CXQueryPredicate *predicate, CXIndex index, double *outValue);
static CXBool CXQueryComparePredicateAt(CXQueryPredicate *predicate, CXIndex index, CXSize dim) {
	if (!predicate || !predicate->attribute) {
		return CXFalse;
	}
	if (predicate->op == CXQueryOpRegex) {
		if (predicate->attribute->type != CXStringAttributeType || !predicate->regexCompiled) {
			return CXFalse;
		}
		const char *value = CXQueryGetStringValueAt(predicate->attribute, index, dim);
		if (!value) {
			return CXFalse;
		}
		return regexec(&predicate->regex, value, 0, NULL, 0) == 0 ? CXTrue : CXFalse;
	}
	if (predicate->op == CXQueryOpIn && predicate->valueType == CXQueryValueList) {
		if (predicate->attribute->type == CXStringAttributeType && predicate->stringList) {
			const char *value = CXQueryGetStringValueAt(predicate->attribute, index, dim);
			if (!value) {
				return CXFalse;
			}
			for (size_t i = 0; i < predicate->listCount; i++) {
				if (strcmp(value, predicate->stringList[i]) == 0) {
					return CXTrue;
				}
			}
			return CXFalse;
		}
		double value = 0.0;
		if (!CXQueryGetNumericValueAt(predicate->attribute, index, dim, &value) || !predicate->numberList) {
			return CXFalse;
		}
		for (size_t i = 0; i < predicate->listCount; i++) {
			if (value == predicate->numberList[i]) {
				return CXTrue;
			}
		}
		return CXFalse;
	}
	if (predicate->valueType == CXQueryValueString) {
		if (predicate->attribute->type == CXStringAttributeType) {
			const char *value = CXQueryGetStringValueAt(predicate->attribute, index, dim);
			if (!value) {
				return CXFalse;
			}
			int cmp = strcmp(value, predicate->stringValue);
			if (predicate->op == CXQueryOpEq) {
				return cmp == 0 ? CXTrue : CXFalse;
			}
			if (predicate->op == CXQueryOpNe) {
				return cmp != 0 ? CXTrue : CXFalse;
			}
			return CXFalse;
		}
		if (predicate->attribute->type == CXDataAttributeCategoryType) {
			int32_t id = 0;
			if (!predicate->attribute->categoricalDictionary) {
				return CXFalse;
			}
			void *encoded = CXStringDictionaryEntryForKey(predicate->attribute->categoricalDictionary, predicate->stringValue);
			if (!encoded || !CXQueryDecodeCategoryId(encoded, &id)) {
				return CXFalse;
			}
			double numeric = 0.0;
			if (!CXQueryGetNumericValueAt(predicate->attribute, index, dim, &numeric)) {
				return CXFalse;
			}
			double target = (double)id;
			switch (predicate->op) {
				case CXQueryOpEq: return numeric == target ? CXTrue : CXFalse;
				case CXQueryOpNe: return numeric != target ? CXTrue : CXFalse;
				case CXQueryOpLt: return numeric < target ? CXTrue : CXFalse;
				case CXQueryOpLte: return numeric <= target ? CXTrue : CXFalse;
				case CXQueryOpGt: return numeric > target ? CXTrue : CXFalse;
				case CXQueryOpGte: return numeric >= target ? CXTrue : CXFalse;
				default: return CXFalse;
			}
		}
		return CXFalse;
	}
	double value = 0.0;
	if (!CXQueryGetNumericValueAt(predicate->attribute, index, dim, &value)) {
		return CXFalse;
	}
	double target = predicate->numberValue;
	switch (predicate->op) {
		case CXQueryOpEq: return value == target ? CXTrue : CXFalse;
		case CXQueryOpNe: return value != target ? CXTrue : CXFalse;
		case CXQueryOpLt: return value < target ? CXTrue : CXFalse;
		case CXQueryOpLte: return value <= target ? CXTrue : CXFalse;
		case CXQueryOpGt: return value > target ? CXTrue : CXFalse;
		case CXQueryOpGte: return value >= target ? CXTrue : CXFalse;
		default: return CXFalse;
	}
}

static CXBool CXQueryComputeNumericAccessor(const CXQueryPredicate *predicate, CXIndex index, double *outValue) {
	if (!predicate || !predicate->attribute || !outValue) {
		return CXFalse;
	}
	CXAttributeRef attr = predicate->attribute;
	CXSize dimension = attr->dimension > 0 ? attr->dimension : 1;
	if (dimension <= 1) {
		return CXQueryGetNumericValueAt(attr, index, 0, outValue);
	}
	switch (predicate->accessMode) {
		case CXQueryAccessMin: {
			double value = 0.0;
			if (!CXQueryGetNumericValueAt(attr, index, 0, &value)) {
				return CXFalse;
			}
			double minValue = value;
			for (CXSize dim = 1; dim < dimension; dim++) {
				if (!CXQueryGetNumericValueAt(attr, index, dim, &value)) {
					return CXFalse;
				}
				if (value < minValue) {
					minValue = value;
				}
			}
			*outValue = minValue;
			return CXTrue;
		}
		case CXQueryAccessMax: {
			double value = 0.0;
			if (!CXQueryGetNumericValueAt(attr, index, 0, &value)) {
				return CXFalse;
			}
			double maxValue = value;
			for (CXSize dim = 1; dim < dimension; dim++) {
				if (!CXQueryGetNumericValueAt(attr, index, dim, &value)) {
					return CXFalse;
				}
				if (value > maxValue) {
					maxValue = value;
				}
			}
			*outValue = maxValue;
			return CXTrue;
		}
		case CXQueryAccessAvg: {
			double total = 0.0;
			for (CXSize dim = 0; dim < dimension; dim++) {
				double value = 0.0;
				if (!CXQueryGetNumericValueAt(attr, index, dim, &value)) {
					return CXFalse;
				}
				total += value;
			}
			*outValue = total / (double)dimension;
			return CXTrue;
		}
		case CXQueryAccessMedian: {
			double *values = (double *)malloc(sizeof(double) * dimension);
			if (!values) {
				return CXFalse;
			}
			for (CXSize dim = 0; dim < dimension; dim++) {
				if (!CXQueryGetNumericValueAt(attr, index, dim, &values[dim])) {
					free(values);
					return CXFalse;
				}
			}
			qsort(values, dimension, sizeof(double), CXQueryCompareDouble);
			if (dimension % 2 == 0) {
				CXSize mid = dimension / 2;
				*outValue = (values[mid - 1] + values[mid]) / 2.0;
			} else {
				*outValue = values[dimension / 2];
			}
			free(values);
			return CXTrue;
		}
		case CXQueryAccessStd: {
			double total = 0.0;
			for (CXSize dim = 0; dim < dimension; dim++) {
				double value = 0.0;
				if (!CXQueryGetNumericValueAt(attr, index, dim, &value)) {
					return CXFalse;
				}
				total += value;
			}
			double mean = total / (double)dimension;
			double variance = 0.0;
			for (CXSize dim = 0; dim < dimension; dim++) {
				double value = 0.0;
				if (!CXQueryGetNumericValueAt(attr, index, dim, &value)) {
					return CXFalse;
				}
				double diff = value - mean;
				variance += diff * diff;
			}
			variance /= (double)dimension;
			*outValue = sqrt(variance);
			return CXTrue;
		}
		case CXQueryAccessAbs: {
			double sumSquares = 0.0;
			for (CXSize dim = 0; dim < dimension; dim++) {
				double value = 0.0;
				if (!CXQueryGetNumericValueAt(attr, index, dim, &value)) {
					return CXFalse;
				}
				sumSquares += value * value;
			}
			*outValue = sqrt(sumSquares);
			return CXTrue;
		}
		case CXQueryAccessDot: {
			if (!predicate->dotAttribute) {
				if (!predicate->dotVector || predicate->dotCount != dimension) {
					return CXFalse;
				}
			}
			double total = 0.0;
			for (CXSize dim = 0; dim < dimension; dim++) {
				double a = 0.0;
				double b = 0.0;
				if (!CXQueryGetNumericValueAt(attr, index, dim, &a)) {
					return CXFalse;
				}
				if (predicate->dotAttribute) {
					if (!CXQueryGetNumericValueAt(predicate->dotAttribute, index, dim, &b)) {
						return CXFalse;
					}
				} else {
					b = predicate->dotVector[dim];
				}
				total += a * b;
			}
			*outValue = total;
			return CXTrue;
		}
		default:
			break;
	}
	return CXFalse;
}

static int CXQueryCompareDouble(const void *lhs, const void *rhs) {
	const double a = *(const double *)lhs;
	const double b = *(const double *)rhs;
	if (a < b) {
		return -1;
	}
	if (a > b) {
		return 1;
	}
	return 0;
}

static CXBool CXQueryComparePredicate(CXQueryPredicate *predicate, CXIndex index) {
	if (!predicate || !predicate->attribute) {
		return CXFalse;
	}
	if (predicate->accessMode == CXQueryAccessIndex) {
		return CXQueryComparePredicateAt(predicate, index, predicate->accessIndex);
	}
	if (predicate->accessMode == CXQueryAccessAny) {
		CXSize dimension = predicate->attribute->dimension > 0 ? predicate->attribute->dimension : 1;
		for (CXSize dim = 0; dim < dimension; dim++) {
			if (CXQueryComparePredicateAt(predicate, index, dim)) {
				return CXTrue;
			}
		}
		return CXFalse;
	}
	if (predicate->accessMode == CXQueryAccessAll) {
		CXSize dimension = predicate->attribute->dimension > 0 ? predicate->attribute->dimension : 1;
		for (CXSize dim = 0; dim < dimension; dim++) {
			if (!CXQueryComparePredicateAt(predicate, index, dim)) {
				return CXFalse;
			}
		}
		return CXTrue;
	}
	if (predicate->accessMode != CXQueryAccessNone) {
		double value = 0.0;
		if (!CXQueryComputeNumericAccessor(predicate, index, &value)) {
			return CXFalse;
		}
		double target = predicate->numberValue;
		switch (predicate->op) {
			case CXQueryOpEq: return value == target ? CXTrue : CXFalse;
			case CXQueryOpNe: return value != target ? CXTrue : CXFalse;
			case CXQueryOpLt: return value < target ? CXTrue : CXFalse;
			case CXQueryOpLte: return value <= target ? CXTrue : CXFalse;
			case CXQueryOpGt: return value > target ? CXTrue : CXFalse;
			case CXQueryOpGte: return value >= target ? CXTrue : CXFalse;
			case CXQueryOpIn: {
				for (size_t i = 0; i < predicate->listCount; i++) {
					if (value == predicate->numberList[i]) {
						return CXTrue;
					}
				}
				return CXFalse;
			}
			default:
				return CXFalse;
		}
	}
	CXSize dimension = predicate->attribute->dimension > 0 ? predicate->attribute->dimension : 1;
	for (CXSize dim = 0; dim < dimension; dim++) {
		if (CXQueryComparePredicateAt(predicate, index, dim)) {
			return CXTrue;
		}
	}
	return CXFalse;
}

static CXBool CXQueryEvaluateNodePredicate(CXNetworkRef network, CXQueryPredicate *predicate, CXIndex nodeIndex) {
	if (!network || !predicate) {
		return CXFalse;
	}
	switch (predicate->qualifier) {
		case CXQueryQualifierSelf:
			return CXQueryComparePredicate(predicate, nodeIndex);
		case CXQueryQualifierNeighborAny:
		case CXQueryQualifierNeighborBoth: {
			CXBool anyMatch = CXFalse;
			CXBool allMatch = CXTrue;
			CXBool hasNeighbor = CXFalse;
			CXNodeRecord *record = &network->nodes[nodeIndex];
			CXNeighborContainer *containers[2] = { &record->outNeighbors, &record->inNeighbors };
			for (size_t c = 0; c < 2; c++) {
				CXNeighborContainer *container = containers[c];
				CXNeighborFOR(neighborNode, edgeIndex, container) {
					(void)edgeIndex;
					hasNeighbor = CXTrue;
					CXBool match = CXQueryComparePredicate(predicate, neighborNode);
					if (match) {
						anyMatch = CXTrue;
					} else {
						allMatch = CXFalse;
					}
					if (predicate->qualifier == CXQueryQualifierNeighborAny && anyMatch) {
						return CXTrue;
					}
				}
			}
			if (!hasNeighbor) {
				return CXFalse;
			}
			return predicate->qualifier == CXQueryQualifierNeighborBoth ? (allMatch ? CXTrue : CXFalse) : (anyMatch ? CXTrue : CXFalse);
		}
		default:
			return CXFalse;
	}
}

static CXBool CXQueryEvaluateEdgePredicate(CXNetworkRef network, CXQueryPredicate *predicate, CXIndex edgeIndex) {
	if (!network || !predicate) {
		return CXFalse;
	}
	CXEdge edge = network->edges[edgeIndex];
	switch (predicate->qualifier) {
		case CXQueryQualifierSelf:
			return CXQueryComparePredicate(predicate, edgeIndex);
		case CXQueryQualifierSrc:
			return CXQueryComparePredicate(predicate, edge.from);
		case CXQueryQualifierDst:
			return CXQueryComparePredicate(predicate, edge.to);
		case CXQueryQualifierAny: {
			CXBool a = CXQueryComparePredicate(predicate, edge.from);
			if (a) {
				return CXTrue;
			}
			return CXQueryComparePredicate(predicate, edge.to);
		}
		case CXQueryQualifierBoth: {
			CXBool a = CXQueryComparePredicate(predicate, edge.from);
			if (!a) {
				return CXFalse;
			}
			return CXQueryComparePredicate(predicate, edge.to);
		}
		default:
			return CXFalse;
	}
}

static CXBool CXQueryEvaluateNodeExpr(CXNetworkRef network, CXQueryExpr *expr, CXIndex nodeIndex) {
	if (!expr) {
		return CXFalse;
	}
	switch (expr->type) {
		case CXQueryExprPredicate:
			return CXQueryEvaluateNodePredicate(network, &expr->data.predicate, nodeIndex);
		case CXQueryExprNot:
			return CXQueryEvaluateNodeExpr(network, expr->data.notExpr.expr, nodeIndex) ? CXFalse : CXTrue;
		case CXQueryExprBinary: {
			if (expr->data.binary.op == CXQueryBinaryAnd) {
				if (!CXQueryEvaluateNodeExpr(network, expr->data.binary.left, nodeIndex)) {
					return CXFalse;
				}
				return CXQueryEvaluateNodeExpr(network, expr->data.binary.right, nodeIndex);
			}
			if (CXQueryEvaluateNodeExpr(network, expr->data.binary.left, nodeIndex)) {
				return CXTrue;
			}
			return CXQueryEvaluateNodeExpr(network, expr->data.binary.right, nodeIndex);
		}
		default:
			return CXFalse;
	}
}

static CXBool CXQueryEvaluateEdgeExpr(CXNetworkRef network, CXQueryExpr *expr, CXIndex edgeIndex) {
	if (!expr) {
		return CXFalse;
	}
	switch (expr->type) {
		case CXQueryExprPredicate:
			return CXQueryEvaluateEdgePredicate(network, &expr->data.predicate, edgeIndex);
		case CXQueryExprNot:
			return CXQueryEvaluateEdgeExpr(network, expr->data.notExpr.expr, edgeIndex) ? CXFalse : CXTrue;
		case CXQueryExprBinary: {
			if (expr->data.binary.op == CXQueryBinaryAnd) {
				if (!CXQueryEvaluateEdgeExpr(network, expr->data.binary.left, edgeIndex)) {
					return CXFalse;
				}
				return CXQueryEvaluateEdgeExpr(network, expr->data.binary.right, edgeIndex);
			}
			if (CXQueryEvaluateEdgeExpr(network, expr->data.binary.left, edgeIndex)) {
				return CXTrue;
			}
			return CXQueryEvaluateEdgeExpr(network, expr->data.binary.right, edgeIndex);
		}
		default:
			return CXFalse;
	}
}

static CXQueryExpr *CXQueryParse(const CXString query, CXQueryParser *parser) {
	parser->input = query ? query : "";
	parser->length = query ? strlen(query) : 0;
	parser->pos = 0;
	parser->hasError = CXFalse;
	parser->errorMessage = NULL;
	parser->errorOffset = 0;
	parser->current.type = CXQueryTokenEof;
	parser->current.start = parser->input;
	parser->current.length = 0;
	parser->current.number = 0.0;
	parser->current.string = NULL;
	CXQueryAdvance(parser);
	CXQueryExpr *expr = CXQueryParseExpression(parser);
	if (!parser->hasError && parser->current.type != CXQueryTokenEof) {
		parser->hasError = CXTrue;
		parser->errorMessage = "Unexpected token";
		parser->errorOffset = parser->current.start ? (size_t)(parser->current.start - parser->input) : parser->pos;
	}
	return expr;
}

static CXBool CXQueryValidateQualifierForScope(CXQueryPredicate *predicate, CXAttributeScope scope, const char **outError) {
	if (scope == CXAttributeScopeNode) {
		switch (predicate->qualifier) {
			case CXQueryQualifierSelf:
			case CXQueryQualifierNeighborAny:
			case CXQueryQualifierNeighborBoth:
				return CXTrue;
			default:
				*outError = "Node queries only support $any.neighbor/$both.neighbor qualifiers";
				return CXFalse;
		}
	}
	if (scope == CXAttributeScopeEdge) {
		switch (predicate->qualifier) {
			case CXQueryQualifierSelf:
			case CXQueryQualifierSrc:
			case CXQueryQualifierDst:
			case CXQueryQualifierAny:
			case CXQueryQualifierBoth:
				return CXTrue;
			default:
				*outError = "Edge queries do not support neighbor qualifiers";
				return CXFalse;
		}
	}
	return CXTrue;
}

static CXBool CXQueryValidateQualifiers(CXQueryExpr *expr, CXAttributeScope scope, const char **outError) {
	if (!expr) {
		return CXTrue;
	}
	switch (expr->type) {
		case CXQueryExprPredicate:
			return CXQueryValidateQualifierForScope(&expr->data.predicate, scope, outError);
		case CXQueryExprNot:
			return CXQueryValidateQualifiers(expr->data.notExpr.expr, scope, outError);
		case CXQueryExprBinary:
			if (!CXQueryValidateQualifiers(expr->data.binary.left, scope, outError)) {
				return CXFalse;
			}
			return CXQueryValidateQualifiers(expr->data.binary.right, scope, outError);
		default:
			return CXTrue;
	}
}

static CXBool CXQueryBindPredicateConstraints(CXNetworkRef network, CXQueryExpr *expr, CXAttributeScope selfScope, const char **outError) {
	if (!expr) {
		return CXTrue;
	}
	switch (expr->type) {
		case CXQueryExprPredicate: {
			CXQueryPredicate *pred = &expr->data.predicate;
			if (pred->accessMode == CXQueryAccessIndex) {
				if (pred->attribute->dimension > 0 && pred->accessIndex >= pred->attribute->dimension) {
					*outError = "Vector index out of range";
					return CXFalse;
				}
			}
			if (pred->accessMode == CXQueryAccessAny || pred->accessMode == CXQueryAccessAll) {
				if (pred->accessMode == CXQueryAccessAny && pred->attribute->dimension <= 1) {
					*outError = "Accessor requires a vector attribute";
					return CXFalse;
				}
				if (pred->accessMode == CXQueryAccessAll && pred->attribute->dimension <= 1) {
					*outError = "Accessor requires a vector attribute";
					return CXFalse;
				}
			}
			if (pred->accessMode == CXQueryAccessMin ||
				pred->accessMode == CXQueryAccessMax ||
				pred->accessMode == CXQueryAccessAvg ||
				pred->accessMode == CXQueryAccessMedian ||
				pred->accessMode == CXQueryAccessStd ||
				pred->accessMode == CXQueryAccessAbs ||
				pred->accessMode == CXQueryAccessDot) {
				if (!CXQueryAttributeIsNumeric(pred->attribute)) {
					*outError = "Accessor requires a numeric attribute";
					return CXFalse;
				}
				if (pred->attribute->dimension <= 1) {
					*outError = "Accessor requires a vector attribute";
					return CXFalse;
				}
			}
			if (pred->accessMode == CXQueryAccessDot) {
				if (!pred->dotName) {
					if (!pred->dotVector || pred->dotCount == 0) {
						*outError = "dot() requires a target attribute or vector";
						return CXFalse;
					}
				}
				if (pred->dotName) {
					if (!CXQueryResolveAttributeByName(network, selfScope, pred->dotName, &pred->dotAttribute)) {
						*outError = "dot() attribute not found";
						return CXFalse;
					}
					if (!CXQueryAttributeIsNumeric(pred->dotAttribute)) {
						*outError = "dot() requires a numeric attribute";
						return CXFalse;
					}
					if (pred->dotAttribute->dimension != pred->attribute->dimension) {
						*outError = "dot() attributes must have matching dimensions";
						return CXFalse;
					}
				} else {
					if (pred->dotCount != pred->attribute->dimension) {
						*outError = "dot() vector must match attribute dimension";
						return CXFalse;
					}
				}
			}
			if (pred->op == CXQueryOpRegex && pred->valueType != CXQueryValueRegex) {
				*outError = "Regex operator requires a string literal";
				return CXFalse;
			}
			if (pred->op == CXQueryOpIn && pred->valueType != CXQueryValueList) {
				*outError = "IN operator requires a list";
				return CXFalse;
			}
			if (pred->accessMode != CXQueryAccessNone &&
				pred->accessMode != CXQueryAccessIndex &&
				pred->accessMode != CXQueryAccessAny &&
				pred->accessMode != CXQueryAccessAll) {
				if (pred->op == CXQueryOpRegex) {
					*outError = "Regex cannot be used with numeric accessors";
					return CXFalse;
				}
				if (pred->valueType == CXQueryValueString) {
					*outError = "String comparisons cannot be used with numeric accessors";
					return CXFalse;
				}
			}
			if (pred->valueType == CXQueryValueString && pred->op != CXQueryOpEq && pred->op != CXQueryOpNe) {
				*outError = "String comparisons only support == or !=";
				return CXFalse;
			}
			return CXTrue;
		}
		case CXQueryExprNot:
			return CXQueryBindPredicateConstraints(network, expr->data.notExpr.expr, selfScope, outError);
		case CXQueryExprBinary:
			if (!CXQueryBindPredicateConstraints(network, expr->data.binary.left, selfScope, outError)) {
				return CXFalse;
			}
			return CXQueryBindPredicateConstraints(network, expr->data.binary.right, selfScope, outError);
		default:
			return CXTrue;
	}
}

static CXBool CXQuerySelectNodes(CXNetworkRef network, const CXString query, CXNodeSelectorRef selector) {
	if (!network || !selector) {
		CXQuerySetError("Network or selector missing", 0);
		return CXFalse;
	}
	CXQueryParser parser = {0};
	CXQueryExpr *expr = CXQueryParse(query, &parser);
	if (parser.hasError || !expr) {
		CXQuerySetError(parser.errorMessage, parser.errorOffset);
		CXQueryTokenFree(&parser.current);
		CXQueryFreeExpr(expr);
		return CXFalse;
	}
	const char *error = NULL;
	if (!CXQueryValidateQualifiers(expr, CXAttributeScopeNode, &error)) {
		CXQuerySetError(error, 0);
		CXQueryFreeExpr(expr);
		CXQueryTokenFree(&parser.current);
		return CXFalse;
	}
	if (!CXQueryBindAttributes(network, expr, CXAttributeScopeNode, &error)) {
		CXQuerySetError(error, 0);
		CXQueryFreeExpr(expr);
		CXQueryTokenFree(&parser.current);
		return CXFalse;
	}
	if (!CXQueryBindPredicateConstraints(network, expr, CXAttributeScopeNode, &error)) {
		CXQuerySetError(error, 0);
		CXQueryFreeExpr(expr);
		CXQueryTokenFree(&parser.current);
		return CXFalse;
	}

	CXIndex *matches = NULL;
	CXSize matchCount = 0;
	CXSize matchCapacity = 0;
	for (CXIndex idx = 0; idx < network->nodeCapacity; idx++) {
		if (!network->nodeActive[idx]) {
			continue;
		}
		if (CXQueryEvaluateNodeExpr(network, expr, idx)) {
			CXGrowArrayAddElement(idx, sizeof(CXIndex), matchCount, matchCapacity, matches);
		}
	}
	CXIndex dummy = 0;
	const CXIndex *input = matches ? matches : &dummy;
	if (!CXNodeSelectorFillFromArray(selector, input, matchCount)) {
		free(matches);
		CXQuerySetError("Failed to populate selector", 0);
		CXQueryFreeExpr(expr);
		CXQueryTokenFree(&parser.current);
		return CXFalse;
	}
	free(matches);
	CXQueryFreeExpr(expr);
	CXQueryTokenFree(&parser.current);
	return CXTrue;
}

static CXBool CXQuerySelectEdges(CXNetworkRef network, const CXString query, CXEdgeSelectorRef selector) {
	if (!network || !selector) {
		CXQuerySetError("Network or selector missing", 0);
		return CXFalse;
	}
	CXQueryParser parser = {0};
	CXQueryExpr *expr = CXQueryParse(query, &parser);
	if (parser.hasError || !expr) {
		CXQuerySetError(parser.errorMessage, parser.errorOffset);
		CXQueryTokenFree(&parser.current);
		CXQueryFreeExpr(expr);
		return CXFalse;
	}
	const char *error = NULL;
	if (!CXQueryValidateQualifiers(expr, CXAttributeScopeEdge, &error)) {
		CXQuerySetError(error, 0);
		CXQueryFreeExpr(expr);
		CXQueryTokenFree(&parser.current);
		return CXFalse;
	}
	if (!CXQueryBindAttributes(network, expr, CXAttributeScopeEdge, &error)) {
		CXQuerySetError(error, 0);
		CXQueryFreeExpr(expr);
		CXQueryTokenFree(&parser.current);
		return CXFalse;
	}
	if (!CXQueryBindPredicateConstraints(network, expr, CXAttributeScopeEdge, &error)) {
		CXQuerySetError(error, 0);
		CXQueryFreeExpr(expr);
		CXQueryTokenFree(&parser.current);
		return CXFalse;
	}

	CXIndex *matches = NULL;
	CXSize matchCount = 0;
	CXSize matchCapacity = 0;
	for (CXIndex idx = 0; idx < network->edgeCapacity; idx++) {
		if (!network->edgeActive[idx]) {
			continue;
		}
		if (CXQueryEvaluateEdgeExpr(network, expr, idx)) {
			CXGrowArrayAddElement(idx, sizeof(CXIndex), matchCount, matchCapacity, matches);
		}
	}
	CXIndex dummy = 0;
	const CXIndex *input = matches ? matches : &dummy;
	if (!CXEdgeSelectorFillFromArray(selector, input, matchCount)) {
		free(matches);
		CXQuerySetError("Failed to populate selector", 0);
		CXQueryFreeExpr(expr);
		CXQueryTokenFree(&parser.current);
		return CXFalse;
	}
	free(matches);
	CXQueryFreeExpr(expr);
	CXQueryTokenFree(&parser.current);
	return CXTrue;
}

CXBool CXNetworkSelectNodesByQuery(CXNetworkRef network, const CXString query, CXNodeSelectorRef selector) {
	CXQueryClearError();
	return CXQuerySelectNodes(network, query, selector);
}

CXBool CXNetworkSelectEdgesByQuery(CXNetworkRef network, const CXString query, CXEdgeSelectorRef selector) {
	CXQueryClearError();
	return CXQuerySelectEdges(network, query, selector);
}
