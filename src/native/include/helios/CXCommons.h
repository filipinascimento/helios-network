//
//  CXCommons.h
//  CXNetwork
//
//  Created by Filipi Nascimento Silva on 11/11/12.
//  Copyright (c) 2012 Filipi Nascimento Silva. All rights reserved.
//

#ifndef CXNetwork_CXCommons_h
#define CXNetwork_CXCommons_h

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <float.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include <ctype.h>
#include <stdarg.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE 
#endif


#if __LP64__ || _WIN64 || NS_BUILD_32_LIKE_64 || CX_64BITS
#define CX_BUILD_64BITS 1
#endif


#if CX_USE_LIBDISPATCH
#include <dispatch/dispatch.h>
#define CX_ENABLE_PARALLELISM 1
#elif _OPENMP //CX_USE_LIBDISPATCH
#include <omp.h>
#define CX_USE_OPENMP 1
#define CX_ENABLE_PARALLELISM 1
#endif //_OPENMP

#define kCXDefaultParallelBlocks 1024

#if !defined(CX_INLINE)
#if defined(__EMSCRIPTEN__)
#define CX_INLINE static inline
#elif defined(__GNUC__)
#define CX_INLINE static __inline__ __attribute__((always_inline))
#elif defined(__MWERKS__) || defined(__cplusplus)
#define CX_INLINE static inline
#elif defined(_MSC_VER)
#define CX_INLINE static __inline
#elif defined(__WIN32__)
#define CX_INLINE static __inline__
#endif
#endif

#if !defined(CX_STATIC_INLINE)
#if defined(__GNUC__)
#define CX_STATIC_INLINE static __inline__
#elif defined(__MWERKS__) || defined(__cplusplus)
#define CX_STATIC_INLINE static inline
#elif defined(_MSC_VER)
#define CX_STATIC_INLINE static __inline
#elif defined(__WIN32__)
#define CX_STATIC_INLINE static __inline__
#endif
#endif

#ifdef __cplusplus
#define CX_EXTERN extern "C" __attribute__((visibility ("default")))
#else
#define CX_EXTERN extern __attribute__((visibility ("default")))
#endif


#if defined(__GNUC__)
#define CXLikely(x)       __builtin_expect((x),1)
#define CXUnlikely(x)     __builtin_expect((x),0)
#define CXExpecting(x,y)  __builtin_expect((x),(y))
#else
#define CXLikely(x)       x
#define CXUnlikely(x)     x
#define CXExpecting(x,y)  x
#endif

#if !defined(CXMIN)
#define CXMIN(A,B)	((A) < (B) ? (A) : (B))
#endif

#if !defined(CXMAX)
#define CXMAX(A,B)	((A) > (B) ? (A) : (B))
#endif

#if !defined(CXABS)
#define CXABS(A)	((A) < 0 ? (-(A)) : (A))
#endif

#define __SHORT_FILE_ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define CXLog(...) do {fprintf(stderr,"#%s:%d[%s]: ", __SHORT_FILE_, __LINE__, __func__);fprintf(stderr,__VA_ARGS__);fprintf(stderr,"\n");} while (0)

#if DEBUG
#define CXDebugLog(...) do {fprintf(stderr,"#%s:%d[%s]: ", __SHORT_FILE_, __LINE__, __func__);fprintf(stderr,__VA_ARGS__);fprintf(stderr,"\n");} while (0)
#else
#define CXDebugLog(...)
#endif


#if CX_BENCHMARK_FUNCTIONS
#include <mach/mach_time.h>
/** Converts Mach absolute time stamps to seconds. */
CX_INLINE double CXTimeSubtract( uint64_t endTime, uint64_t startTime ){
    uint64_t difference = endTime - startTime;
    static double conversion = 0.0;
    
    if( conversion == 0.0 )
    {
        mach_timebase_info_data_t info;
        kern_return_t err = mach_timebase_info( &info );
        
		//Convert the timebase into seconds
        if( err == 0  )
			conversion = 1e-9 * (double) info.numer / (double) info.denom;
    }
    
    return conversion * (double) difference;
}

/** Declares scratch variables used by the benchmarking helpers. */
#define CX_BenchmarkPrepare(prefix) static uint64_t _CX_Benchmark_##prefix##_start; static double _CX_Benchmark_##prefix##_elapsed
/** Captures the start timestamp for a benchmark section. */
#define CX_BenchmarkStart(prefix)   _CX_Benchmark_##prefix##_start = mach_absolute_time()
/** Captures the stop timestamp and stores the elapsed seconds. */
#define CX_BenchmarkStop(prefix)    _CX_Benchmark_##prefix##_elapsed = CXTimeSubtract(mach_absolute_time(),_CX_Benchmark_##prefix##_start)
/** Logs the elapsed time using CXLog. */
#define CX_BenchmarkPrint(prefix)   CXLog("Function finished in %g s.",_CX_Benchmark_##prefix##_elapsed)
/** Accessor macro returning the elapsed time in seconds. */
#define CX_BenchmarkTime(prefix)    _CX_Benchmark_##prefix##_elapsed

#else

#define CX_BenchmarkPrepare(prefix)
#define CX_BenchmarkStart(prefix)
#define CX_BenchmarkStop(prefix)
#define CX_BenchmarkPrint(prefix)
#define CX_BenchmarkTime(prefix)

#endif //CX_BENCHMARK_FUNCTIONS


/* Boolean Types */
typedef unsigned char CXBool;
#define CXBoolScan "hhu"

#define CXTrue 1
#define CXFalse 0

/* Capacity Grow*/
#define CXCapacityGrow(count) ((count)*2+1)

/* ATOMIC OPERATIONS */

/**
 * Platform-specific atomic primitives. Each branch provides implementations for
 * the same set of helpers:
 *   - `CXAtomicCompareAndSwap*Barrier`: Performs compare-and-swap with acquire/
 *     release semantics on 32-bit, 64-bit, or pointer-sized values.
 *   - `CXAtomicIncrement*`: Atomically increments the target integer and
 *     returns the new value.
 *   - `CXMemoryBarrier`: Issues a full memory fence.
 */
#if __WIN32__
#include "Windows.h"
CX_INLINE CXBool CXAtomicCompareAndSwap32Barrier(int32_t oldValue, int32_t newValue, volatile int32_t *theValue) {
    int32_t actualOldValue = InterlockedCompareExchange((volatile LONG *)theValue, newValue, oldValue);
    return actualOldValue == oldValue ? CXTrue : CXFalse;
}
#if CX_BUILD_64BITS
CX_INLINE CXBool CXAtomicCompareAndSwap64Barrier(int64_t oldValue, int64_t newValue, volatile int64_t *theValue) {
	int64_t actualOldValue = InterlockedCompareExchange64((volatile LONGLONG *)theValue, newValue, oldValue);
    return actualOldValue == oldValue ? CXTrue : CXFalse;
}
#endif
CX_INLINE CXBool CXAtomicCompareAndSwapPtrBarrier(void* oldValue, void* newValue, void* volatile *theValue) {
	void *actualOldValue = InterlockedCompareExchangePointer((volatile PVOID*)theValue, newValue, (PVOID)oldValue);
	return actualOldValue == oldValue ? CXTrue : CXFalse;
}
CX_INLINE int32_t CXAtomicIncrement32(volatile int32_t *theValue) {
	return (int)InterlockedIncrement((volatile LONG*)theValue);
}

#if CX_BUILD_64BITS
CX_INLINE int64_t CXAtomicIncrement64(volatile int64_t *theValue) {
	return (long long)InterlockedIncrement64((volatile LONGLONG*)theValue);
}
#endif

CX_INLINE void CXMemoryBarrier(void) {
#if _MSC_VER
	MemoryBarrier();
#else
	__sync_synchronize();
#endif
}

#elif __APPLE__
#include <libkern/OSAtomic.h>
#include <stdatomic.h>

CX_INLINE CXBool CXAtomicCompareAndSwap32Barrier(int32_t __oldValue, int32_t __newValue, volatile int32_t *__theValue) {
	return OSAtomicCompareAndSwap32Barrier(__oldValue, __newValue, __theValue);
}
CX_INLINE CXBool CXAtomicCompareAndSwap64Barrier(int64_t __oldValue, int64_t __newValue, volatile int64_t *__theValue) {
	return OSAtomicCompareAndSwap64Barrier(__oldValue, __newValue, __theValue);
}
CX_INLINE CXBool CXAtomicCompareAndSwapPtrBarrier(void* __oldValue, void* __newValue, void* volatile *__theValue) {
	return OSAtomicCompareAndSwapPtrBarrier(__oldValue, __newValue, __theValue);
}
CX_INLINE int32_t CXAtomicIncrement32(volatile int32_t *theValue) {
	return OSAtomicIncrement32(theValue);
}
CX_INLINE int64_t CXAtomicIncrement64(volatile int64_t *theValue) {
	return OSAtomicIncrement64(theValue);
}
CX_INLINE void CXMemoryBarrier(void) {
	OSMemoryBarrier();
}
#else// DEPLOYMENT_TARGET_LINUX
	 // Simply leverage GCC's atomic built-ins (see http://gcc.gnu.org/onlinedocs/gcc-4.1.0/gcc/Atomic-Builtins.html)
CX_INLINE CXBool CXAtomicCompareAndSwap32Barrier(int32_t __oldValue, int32_t __newValue, volatile int32_t *__theValue) {
	return __sync_bool_compare_and_swap(__theValue, __oldValue, __newValue);
}
CX_INLINE CXBool CXAtomicCompareAndSwap64Barrier(int64_t __oldValue, int64_t __newValue, volatile int64_t *__theValue) {
	return __sync_bool_compare_and_swap(__theValue, __oldValue, __newValue);
}
CX_INLINE CXBool CXAtomicCompareAndSwapPtrBarrier(void* __oldValue, void* __newValue, void* volatile *__theValue) {
	return __sync_bool_compare_and_swap(__theValue, __oldValue, __newValue);
}
CX_INLINE int32_t CXAtomicIncrement32(volatile int32_t *theValue) {
	return __sync_fetch_and_add(theValue, 1);
}
CX_INLINE int64_t CXAtomicIncrement64(volatile int64_t *theValue) {
	return __sync_fetch_and_add(theValue, 1);
}
CX_INLINE void CXMemoryBarrier(void) {
	__sync_synchronize();
}
//#else
//#error "Don't know how to perform atomic operations."
#endif



/* Integer definitions */

#if CX_BUILD_64BITS
typedef uint64_t CXUInteger;
typedef int64_t CXInteger;
#define CXUIntegerScan PRIu64
#define CXIntegerScan PRId64
#define CXIntegerMAX INT64_MAX
#define CXUIntegerMAX UINT64_MAX

CX_INLINE CXBool CXAtomicCompareAndSwapIntegerBarrier(CXInteger __oldValue, CXInteger __newValue, volatile CXInteger* __theValue){
	return CXAtomicCompareAndSwap64Barrier(__oldValue, __newValue, __theValue);
}

CX_INLINE int64_t CXAtomicIncrementInteger(volatile CXInteger* theValue) {
	return CXAtomicIncrement64(theValue);
}

#else
typedef uint32_t CXUInteger;
typedef int32_t CXInteger;
#define CXUIntegerScan PRIu32
#define CXIntegerScan PRId32
#define CXIntegerMAX INT32_MAX
#define CXUIntegerMAX UINT32_MAX
CX_INLINE CXBool CXAtomicCompareAndSwapIntegerBarrier(CXInteger __oldValue, CXInteger __newValue, volatile CXInteger* __theValue){
	return CXAtomicCompareAndSwap32Barrier(__oldValue, __newValue, __theValue);
}

CX_INLINE int64_t CXAtomicIncrementInteger(volatile CXInteger* theValue) {
	return CXAtomicIncrement32(theValue);
}

#endif

// TODO: Maybe change to size_t
typedef CXUInteger CXIndex;
#define CXIndexScan CXUIntegerScan
#define CXIndexMAX CXUIntegerMAX

typedef CXIndex CXSize;
#define CXSizeScan CXIndexScan
#define CXSizeMAX CXIndexMAX

typedef float CXFloat;
#define CXFloatScan "g"
typedef double CXDouble;
#define CXDoubleScan "g"

#define CXFloatMIN -FLT_MAX
#define CXFloatMAX FLT_MAX



#if __APPLE__
CX_INLINE void CXRandomSeedDev(){
	srandomdev();
	uint64_t okok = random();
	seed48((unsigned short*)&okok);
}
static int g_seed = 100; 
CX_INLINE int CX_FastrandInt() {
	g_seed = (214013*g_seed+2531011);
	return (g_seed>>16)&0x7FFF;
}
CX_INLINE void CXRandomSeed(CXUInteger seed){srandom((unsigned int)seed);}
CX_INLINE CXUInteger CXRandom() {return random();}
CX_INLINE CXInteger CXRandomInRange(CXInteger start,CXInteger length){return (CXInteger)start+(CXInteger)(random()%(length));}
CX_INLINE CXFloat CXRandomFloat(){return (float)drand48();}

#elif defined(LINUX)
CX_INLINE void CXRandomSeedDev(){
	srandom(time(NULL));
	uint64_t okok = random();
	seed48((unsigned short*)&okok);
}
static int g_seed = 100;
CX_INLINE int CX_FastrandInt() {
	g_seed = (214013*g_seed+2531011);
	return (g_seed>>16)&0x7FFF;
}
CX_INLINE void CXRandomSeed(CXUInteger seed){srandom((unsigned int)seed);}
CX_INLINE CXUInteger CXRandom() {return random();}
CX_INLINE CXInteger CXRandomInRange(CXInteger start,CXInteger length){return (CXInteger)start+(CXInteger)(random()%(length));}
CX_INLINE CXFloat CXRandomFloat(){return (float)drand48();}
#else
#pragma message ("warning: Generic random")
CX_INLINE void CXRandomSeedDev(){srand(time(NULL));}
CX_INLINE void CXRandomSeed(CXUInteger seed){srand((unsigned int)seed);}
CX_INLINE CXUInteger CXRandom() {return rand();}
CX_INLINE CXInteger CXRandomInRange(CXInteger start, CXInteger length){
	uint32_t n = (uint32_t)length;
	uint32_t limit = RAND_MAX - RAND_MAX % n;
	uint32_t rnd;
	do {
		rnd = rand();
	} while (rnd >= limit);
	return start + (rnd % n);
}
CX_INLINE CXFloat CXRandomFloat(){return (float)rand()/(float)(RAND_MAX);}
#endif

typedef char CXChar;
typedef CXChar* CXString;

#define _kCXStringReadlineINITSIZE   112  /* power of 2 minus 16, helps malloc */
#define _kCXStringReadlineDELTASIZE (_kCXStringReadlineINITSIZE + 16)


CX_INLINE CXString CXNewStringReadingLine(FILE *f){
	int     cursize, ch, ix;
	char   *buffer, *temp;
	char* ln;
	
	ln = NULL; /* default */
	if (NULL == (buffer = malloc(_kCXStringReadlineINITSIZE))) return NULL;
	cursize = _kCXStringReadlineINITSIZE;
	
	ix = 0;
	while ((EOF != (ch = getc(f))) && ('\n' != ch)) {
		if (ix >= (cursize - 1)) { /* extend buffer */
			cursize += _kCXStringReadlineDELTASIZE;
			if (NULL == (temp = realloc(buffer, (size_t)cursize))) {
				/* ran out of memory, return partial line */
				buffer[ix] = '\0';
				ln = buffer;
				return NULL;
			}
			buffer = temp;
		}
		buffer[ix++] = ch;
	}
	if ((EOF == ch) && (0 == ix)) {
		free(buffer);
		return NULL;//Check
	}
	
	buffer[ix] = '\0';
	if (NULL == (temp = realloc(buffer, (size_t)ix + 1))) {
		ln = buffer;  /* without reducing it */
	}
	else ln = temp;
	return ln;
}


CX_INLINE CXSize CXStringScan(CXString* restrict scannedString, const CXString restrict scanString){
	CXSize scanStringLength = strlen(scanString);
	if(strncmp(*scannedString,scanString,scanStringLength)==0){
		(*scannedString)+= scanStringLength;
		return scanStringLength;
	}else{
		return 0;
	}
}

CX_INLINE CXSize CXStringScanCharacters(CXString* restrict scannedString, CXChar scanCharacter){
	CXString scannedStringTemp = *scannedString;
	CXSize scannedCount = 0;
	while (scannedStringTemp[0]==scanCharacter) {
		scannedStringTemp++;
		scannedCount++;
	}
	*scannedString = scannedStringTemp;
	return scannedCount;
}

CX_INLINE CXString CXNewStringScanningUpToCharacter(CXString* restrict scannedString, CXChar stopCharacter){
	CXString scannedStringTemp = *scannedString;
	CXSize scannedCount = 0;
	CXSize capacity = 1;
	CXString returnString = (CXString)calloc(1,sizeof(CXChar));
	while (scannedStringTemp[0]!=stopCharacter&&scannedStringTemp[0]) {
		if(capacity<scannedCount+2){
			capacity=CXCapacityGrow(scannedCount+2);
			returnString = (CXString)realloc(returnString,capacity);
		}
		returnString[scannedCount] = scannedStringTemp[0];
		scannedStringTemp++;
		scannedCount++;
	}
	returnString[scannedCount] = '\0';
	*scannedString = scannedStringTemp;
	return returnString;
}


CX_INLINE CXString CXNewStringFromString(const CXString theString){
	CXString newString = malloc(sizeof(char) * (strlen(theString)+1));
	strcpy(newString,theString);
	return newString;
}

#ifdef __EMSCRIPTEN__
// Emscripten does have this function, but it is not declared in stdio.h
extern int vasprintf(char **strp, const char *fmt, va_list ap);
#endif

CX_INLINE CXString CXNewStringFromFormat(const CXString format, ...){
	va_list arglist;
	va_start(arglist,format);
	CXString returnedString = NULL;
	vasprintf(&returnedString,format,arglist);
	va_end(arglist);
	return returnedString;
}

CX_INLINE CXString CXNewStringScanningUpToCharactersInList(CXString* restrict scannedString, const CXString restrict stopCharacters){
	CXString scannedStringTemp = *scannedString;
	CXSize scannedCount = 0;
	CXSize capacity = 1;
	CXString returnString = (CXString)calloc(1,sizeof(CXChar));
	CXSize stopCharactersSize = strlen(stopCharacters);
	while (scannedStringTemp[0]) {
		CXIndex stopIndex=0;
		CXBool foundStopCharacter = CXFalse;
		for (stopIndex=0; stopIndex<stopCharactersSize; stopIndex++) {
			if(scannedStringTemp[0]==stopCharacters[stopIndex]){
				foundStopCharacter = CXTrue;
			};
		}
		
		if(foundStopCharacter){
			break;
		}
		
		if(capacity<scannedCount+2){
			capacity=CXCapacityGrow(scannedCount+2);
			returnString = (CXString)realloc(returnString,capacity);
		}
		returnString[scannedCount] = scannedStringTemp[0];
		scannedStringTemp++;
		scannedCount++;
	}
	returnString[scannedCount] = '\0';
	*scannedString = scannedStringTemp;
	return returnString;
}


CX_INLINE CXSize CXStringScanUpToCharactersInList(CXString* restrict scannedString, const CXString restrict stopCharacters){
	CXString scannedStringTemp = *scannedString;
	CXSize scannedCount = 0;
	CXSize stopCharactersSize = strlen(stopCharacters);
	while (scannedStringTemp[0]) {
		CXIndex stopIndex=0;
		CXBool foundStopCharacter = CXFalse;
		for (stopIndex=0; stopIndex<stopCharactersSize; stopIndex++) {
			if(scannedStringTemp[0]==stopCharacters[stopIndex]){
				foundStopCharacter = CXTrue;
			};
		}
		
		if(foundStopCharacter){
			break;
		}
		
		scannedStringTemp++;
		scannedCount++;
	}
	*scannedString = scannedStringTemp;
	return scannedCount;
}

CX_INLINE CXString CXNewStringScanningUpToString(CXString* restrict scannedString, const CXString restrict stopString){
	CXString scannedStringTemp = *scannedString;
	CXSize scannedCount = 0;
	CXSize capacity = 1;
	CXString returnString = (CXString)calloc(1,sizeof(CXChar));
	while (scannedStringTemp[0]) {
		CXString checkScannedString = scannedStringTemp;
		CXString checkStopString = stopString;
		while (checkScannedString[0]==checkStopString[0]&&checkScannedString[0]&&checkStopString[0]){
			checkScannedString++;
			checkStopString++;
		}
		if(checkStopString[0]){
			if(capacity<scannedCount+2){
				capacity=CXCapacityGrow(scannedCount+2);
				returnString = (CXString)realloc(returnString,capacity);
			}
			returnString[scannedCount] = scannedStringTemp[0];
			scannedStringTemp++;
			scannedCount++;
		}else{
			break;
		}
	}
	returnString[scannedCount] = '\0';
	*scannedString = scannedStringTemp;
	return returnString;
}


CX_INLINE CXSize CXStringScanUpToString(CXString* restrict scannedString, const CXString restrict stopString){
	CXString scannedStringTemp = *scannedString;
	CXSize scannedCount = 0;
	while (scannedStringTemp[0]) {
		CXString checkScannedString = scannedStringTemp;
		CXString checkStopString = stopString;
		while (checkScannedString[0]==checkStopString[0]&&checkScannedString[0]&&checkStopString[0]){
			checkScannedString++;
			checkStopString++;
		}
		if(checkStopString[0]){
			scannedStringTemp++;
			scannedCount++;
		}else{
			break;
		}
	}
	*scannedString = scannedStringTemp;
	return scannedCount;
}


CX_INLINE CXSize CXStringScanIndex(CXString* restrict scannedString, CXIndex* restrict scannedIndex){
	CXString scannedStringTemp;
	CXSize scannedCount = 0;
	CXIndex scannedValue = strtol(*scannedString, &scannedStringTemp, 10);
	scannedCount = (CXSize)(scannedStringTemp-(*scannedString));
	if(scannedCount){
		*scannedIndex = scannedValue;
		*scannedString = scannedStringTemp;
	}
	return scannedCount;
}


CX_INLINE CXInteger CXStringScanInteger(CXString* restrict scannedString, CXInteger* restrict scannedInteger){
	CXString scannedStringTemp;
	CXSize scannedCount = 0;
	CXInteger scannedValue = strtol(*scannedString, &scannedStringTemp, 10);
	scannedCount = (CXSize)(scannedStringTemp-(*scannedString));
	if(scannedCount){
		*scannedInteger = scannedValue;
		*scannedString = scannedStringTemp;
	}
	return scannedCount;
}

CX_INLINE CXSize CXStringScanFloat(CXString* restrict scannedString, float* restrict scannedFloat){
	CXString scannedStringTemp;
	CXSize scannedCount = 0;
	float scannedValue = strtof(*scannedString, &scannedStringTemp);
	scannedCount = (CXSize)(scannedStringTemp-(*scannedString));
	if(scannedCount){
		*scannedFloat = scannedValue;
		*scannedString = scannedStringTemp;
	}
	return scannedCount;
}
CX_INLINE CXSize CXStringScanDouble(CXString* restrict scannedString, double* restrict scannedDouble){
	CXString scannedStringTemp;
	CXSize scannedCount = 0;
	double scannedValue = strtod(*scannedString, &scannedStringTemp);
	scannedCount = (CXSize)(scannedStringTemp-(*scannedString));
	if(scannedCount){
		*scannedDouble = scannedValue;
		*scannedString = scannedStringTemp;
	}
	return scannedCount;
}


CX_INLINE CXSize CXStringScanStrictIndex(CXString* restrict scannedString, CXIndex* restrict scannedIndex){
	CXSize scannedCount = 0;
	if(!isspace(**scannedString)){
		CXString scannedStringTemp;
		CXIndex scannedValue = strtol(*scannedString, &scannedStringTemp, 10);
		scannedCount = (CXSize)(scannedStringTemp-(*scannedString));
		if(scannedCount){
			*scannedIndex = scannedValue;
			*scannedString = scannedStringTemp;
		}
	}
	return scannedCount;
}


CX_INLINE CXInteger CXStringScanStrictInteger(CXString* restrict scannedString, CXInteger* restrict scannedInteger){
	CXSize scannedCount = 0;
	if(!isspace(**scannedString)){
		CXString scannedStringTemp;
		CXInteger scannedValue = strtol(*scannedString, &scannedStringTemp, 10);
		scannedCount = (CXSize)(scannedStringTemp-(*scannedString));
		if(scannedCount){
			*scannedInteger = scannedValue;
			*scannedString = scannedStringTemp;
		}
	}
	return scannedCount;
}

CX_INLINE CXSize CXStringScanStrictFloat(CXString* restrict scannedString, float* restrict scannedFloat){
	CXSize scannedCount = 0;
	if(!isspace(**scannedString)){
		CXString scannedStringTemp;
		float scannedValue = strtof(*scannedString, &scannedStringTemp);
		scannedCount = (CXSize)(scannedStringTemp-(*scannedString));
		if(scannedCount){
			*scannedFloat = scannedValue;
			*scannedString = scannedStringTemp;
		}
	}
	return scannedCount;
}

CX_INLINE CXSize CXStringScanStrictDouble(CXString* restrict scannedString, double* restrict scannedDouble){
	CXSize scannedCount = 0;
	if(!isspace(**scannedString)){
		CXString scannedStringTemp;
		double scannedValue = strtod(*scannedString, &scannedStringTemp);
		scannedCount = (CXSize)(scannedStringTemp-(*scannedString));
		if(scannedCount){
			*scannedDouble = scannedValue;
			*scannedString = scannedStringTemp;
		}
	}
	return scannedCount;
}


CX_INLINE CXSize CXStringScanCharactersList(CXString* scannedString, const CXString restrict charactersList){
	CXString scannedStringTemp = *scannedString;
	CXSize scannedCount = 0;
	CXSize charListIndex = 0;
	CXSize charListCount = strlen(charactersList);
	CXSize notfoundChar = 0;
	while(charListIndex<charListCount&&notfoundChar<charListCount){
		CXChar scanCharacter = charactersList[charListIndex];
		charListIndex = (charListIndex+1)%charListCount;
		notfoundChar++;
		while (scannedStringTemp[0]==scanCharacter) {
			scannedStringTemp++;
			scannedCount++;
			notfoundChar=0;
		}
	}
	*scannedString = scannedStringTemp;
	return scannedCount;
}

CX_INLINE CXBool _CXStringIsInSet(CXChar readChar, CXString const charSet){
	if(readChar=='\0'){
		return CXFalse;
	}
	CXString curCharSet = charSet;
	CXBool charFound = CXFalse;
	while (*curCharSet&&!(charFound=((*(curCharSet++))==readChar))) {}
	return charFound;
}

CX_INLINE void CXStringTrim(CXString restrict theString, CXString const trimCharacters){
	if(theString && theString[0]){
		CXString curString = theString;
		CXIndex stringIndex = strlen(curString);
		while(stringIndex&& *curString &&_CXStringIsInSet(curString[stringIndex - 1],trimCharacters)){
			curString[--stringIndex] = 0;
		}
		while(*curString && _CXStringIsInSet(*curString,trimCharacters)) ++curString, --stringIndex;
		memmove(theString, curString, stringIndex + 1);
	}
}

CX_INLINE void CXStringTrimSpaces(CXString restrict theString) {
	if(theString && theString[0]){
		CXString curString = theString;
		CXIndex stringIndex = strlen(curString);
		while(isspace(curString[stringIndex - 1])) curString[--stringIndex] = 0;
		while(*curString && isspace(*curString)) ++curString, --stringIndex;
		memmove(theString, curString, stringIndex + 1);
	}
}

/** Natural-order string compare: returns -1, 0, 1; numeric runs compare by value. */
CX_INLINE int CXStringCompareNatural(const CXString left, const CXString right){
	if(left == NULL && right == NULL){
		return 0;
	}
	if(left == NULL){
		return -1;
	}
	if(right == NULL){
		return 1;
	}
	const unsigned char* a = (const unsigned char*)left;
	const unsigned char* b = (const unsigned char*)right;
	while(*a != '\0' || *b != '\0'){
		if(isdigit(*a) && isdigit(*b)){
			const unsigned char* a_start = a;
			const unsigned char* b_start = b;
			while(*a == '0'){
				a++;
			}
			while(*b == '0'){
				b++;
			}
			const unsigned char* a_sig = a;
			const unsigned char* b_sig = b;
			while(isdigit(*a)){
				a++;
			}
			while(isdigit(*b)){
				b++;
			}
			const unsigned char* a_end = a;
			const unsigned char* b_end = b;
			size_t a_sig_len = (size_t)(a_end - a_sig);
			size_t b_sig_len = (size_t)(b_end - b_sig);
			if(a_sig_len == 0){
				a_sig_len = 1;
				a_sig = a_end - 1;
			}
			if(b_sig_len == 0){
				b_sig_len = 1;
				b_sig = b_end - 1;
			}
			if(a_sig_len != b_sig_len){
				return a_sig_len < b_sig_len ? -1 : 1;
			}
			int digit_compare = memcmp(a_sig, b_sig, a_sig_len);
			if(digit_compare != 0){
				return digit_compare < 0 ? -1 : 1;
			}
			size_t a_len = (size_t)(a_end - a_start);
			size_t b_len = (size_t)(b_end - b_start);
			if(a_len != b_len){
				return a_len < b_len ? -1 : 1;
			}
			continue;
		}
		if(*a != *b){
			return *a < *b ? -1 : 1;
		}
		if(*a == '\0'){
			break;
		}
		a++;
		b++;
	}
	return 0;
}

CX_INLINE void CXStringDestroy(CXString theString){
	free(theString);
}


CX_INLINE CXString CXNewStringByRemovingFileExtension(const CXString theString) {
	CXString newString, lastExtensionSeparator, lastPathSeparator;
#if __WIN32__
#pragma message ("warning: Windows may need to use other separators, check this. FIXME")
// #warning "Windows may need to use other separators, check this. FIXME"
	char extensionSeparator = '.';
	char pathSeparator = '\\';
#else
	char extensionSeparator = '.';
	char pathSeparator = '/';
#endif
	if (theString == NULL){
		return NULL;
	}
	
	if ((newString = calloc(sizeof(char),strlen(theString) + 1)) == NULL){
		return NULL;
	}
	
	strcpy(newString, theString);
	
	lastExtensionSeparator = strrchr(newString, extensionSeparator);
	lastPathSeparator = (pathSeparator == 0) ? NULL : strrchr(newString, pathSeparator);
	
	if(lastExtensionSeparator != NULL){
		if (lastPathSeparator != NULL) {
			if (lastPathSeparator < lastExtensionSeparator) {
				*lastExtensionSeparator = '\0';
			}
		}else{
			*lastExtensionSeparator = '\0';
		}
	}
	return newString;
}

CX_INLINE CXString CXNewStringFromPathExtension(const CXString theString) {
	CXString newString, lastExtensionSeparator, lastPathSeparator;
#if __WIN32__
#pragma message ("warning: Windows may need to use other separators, check this. FIXME")
// #warning "Windows may need to use other separators, check this. FIXME"
	char extensionSeparator = '.';
	char pathSeparator = '\\';
#else
	char extensionSeparator = '.';
	char pathSeparator = '/';
#endif
	if (theString == NULL){
		return NULL;
	}
	newString = NULL;
	lastExtensionSeparator = strrchr(theString, extensionSeparator);
	lastPathSeparator = (pathSeparator == 0) ? NULL : strrchr(theString, pathSeparator);
	CXSize stringLength = strlen(theString);
	if(lastExtensionSeparator != NULL){
		if (lastPathSeparator != NULL) {
			if (lastPathSeparator < lastExtensionSeparator) {
				CXSize extensionLength = ((theString+stringLength)-(lastExtensionSeparator+1));
				if ((newString = calloc(sizeof(char),extensionLength + 1)) == NULL){
					return NULL;
				}
				strcpy(newString, (lastExtensionSeparator+1));
			}
		}else{
			CXSize extensionLength = ((theString+stringLength)-(lastExtensionSeparator+1));
			if ((newString = calloc(sizeof(char),extensionLength + 1)) == NULL){
				return NULL;
			}
			strcpy(newString, (lastExtensionSeparator+1));
		}
	}
	if(newString==NULL){
		newString = CXNewStringFromString("");
	}
	return newString;
}



CX_INLINE int64_t ipow(int32_t base, uint8_t exp) {
    static const uint8_t highest_bit_set[] = {
        0, 1, 2, 2, 3, 3, 3, 3,
        4, 4, 4, 4, 4, 4, 4, 4,
        5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5,
        6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 255, // anything past 63 is a guaranteed overflow with base > 1
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
    };
	
    uint64_t result = 1;
	
    switch (highest_bit_set[exp]) {
		case 255: // we use 255 as an overflow marker and return 0 on overflow/underflow
			if (base == 1) {
				return 1;
			}
			
			if (base == -1) {
				return 1 - 2 * (exp & 1);
			}
			
			return 0;
		case 6:
			if (exp & 1) result *= base;
			exp >>= 1;
			base *= base;
		case 5:
			if (exp & 1) result *= base;
			exp >>= 1;
			base *= base;
		case 4:
			if (exp & 1) result *= base;
			exp >>= 1;
			base *= base;
		case 3:
			if (exp & 1) result *= base;
			exp >>= 1;
			base *= base;
		case 2:
			if (exp & 1) result *= base;
			exp >>= 1;
			base *= base;
		case 1:
			if (exp & 1) result *= base;
		default:
			return result;
    }
}

CX_INLINE CXUInteger ilog2(uint64_t value){
	
	const CXUInteger tab64[64] = {
		63,  0, 58,  1, 59, 47, 53,  2,
		60, 39, 48, 27, 54, 33, 42,  3,
		61, 51, 37, 40, 49, 18, 28, 20,
		55, 30, 34, 11, 43, 14, 22,  4,
		62, 57, 46, 52, 38, 26, 32, 41,
		50, 36, 17, 19, 29, 10, 13, 21,
		56, 45, 25, 31, 35, 16,  9, 12,
		44, 24, 15,  8, 23,  7,  6,  5};
	
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value |= value >> 32;
    return tab64[((uint64_t)((value - (value >> 1))*0x07EDD5E59A4E28C2)) >> 58];
}

CX_INLINE CXInteger ipow2(CXInteger n){
	return n>=0?(1<<n):0;
}



CX_INLINE uint64_t CXNextPowerOfTwo(uint64_t n){
	n = n - 1;
	n = n | (n >> 1);
	n = n | (n >> 2);
	n = n | (n >> 4);
	n = n | (n >> 8);
	n = n | (n >> 16);
	n = n | (n >> 32);
	n = n + 1;
	return n;
}


typedef struct _CXOperationControl{
	CXBool shouldAbort;
	CXInteger currentProgress;
	CXInteger maxProgress;
	CXInteger maxParallelBlocks;
	CXBool finished;
	void (*updateCallback)(struct _CXOperationControl*);
	void (*streamCallback)(struct _CXOperationControl*,CXIndex,const char* format, ...);
	FILE* defaultStreamFile;
	void* context;
} CXOperationControl;

CX_INLINE CXOperationControl* CXOperationControlCreate(){
	CXOperationControl* operationControl = (CXOperationControl*)malloc(sizeof(CXOperationControl));
	operationControl->shouldAbort=CXFalse;
	operationControl->context=CXFalse;
	operationControl->currentProgress=0;
	operationControl->maxProgress=-1;
	operationControl->maxParallelBlocks = kCXDefaultParallelBlocks;
	operationControl->context = NULL;
	operationControl->updateCallback = NULL;
	operationControl->streamCallback = NULL;
	operationControl->defaultStreamFile = NULL;
	//FIXME: Add callbacks and events support.
	return operationControl;
}

CX_INLINE void CXOperationControlDestroy(CXOperationControl* operationControl){
	free(operationControl);
}


#define __CXSTRINGIFY(x) #x
#define CXTOKENTOSTRING(x) __CXSTRINGIFY(x)


#if CX_USE_LIBDISPATCH
#include <dispatch/dispatch.h>
#define CX_ENABLE_PARALLELISM 1

#define CXParallelForStart(loopName, indexName, count) \
dispatch_semaphore_t __##loopName##Semaphore = dispatch_semaphore_create(1); \
dispatch_queue_t __##loopName##AsyncQueue = dispatch_queue_create("com.opencx.parallelfor." CXTOKENTOSTRING(__##loopName##AsyncQueue),NULL);\
dispatch_queue_t __##loopName##Queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0);\
dispatch_apply(count, __##loopName##Queue, ^(size_t indexName) {

#define CXParallelForEnd(loopName) });\
dispatch_release(__##loopName##Semaphore);\
dispatch_release(__##loopName##AsyncQueue);


#define CXParallelLoopCriticalRegionStart(loopName) dispatch_semaphore_wait(__##loopName##Semaphore, DISPATCH_TIME_FOREVER);
#define CXParallelLoopCriticalRegionEnd(loopName) dispatch_semaphore_signal(__##loopName##Semaphore);

#define CXParallelLoopSyncCriticalRegionStart(loopName) dispatch_sync(__##loopName##Semaphore, ^{
#define CXParallelLoopSyncCriticalRegionEnd(loopName) });


#elif CX_USE_OPENMP
// #warning "OPENMP ENABLED"
#pragma message ("warning: OPENMP ENABLED")
#include <omp.h>
#define CX_USE_OPENMP 1
#define CX_ENABLE_PARALLELISM 1
#if __WIN32__
#define CXParallelForStart(loopName, indexName, count) \
CXInteger indexName; \
_Pragma("omp parallel for") \
for(indexName=0;indexName<count;indexName++)
#else
#define CXParallelForStart(loopName, indexName, count) \
CXIndex indexName; \
_Pragma("omp parallel for") \
for(indexName=0;indexName<count;indexName++)
#endif

#define CXParallelForEnd(loopName) 

#define CXParallelLoopCriticalRegionStart(loopName) _Pragma("omp critical") {
#define CXParallelLoopCriticalRegionEnd(loopName) }

#define CXParallelLoopSyncCriticalRegionStart(loopName) _Pragma("omp critical") {
#define CXParallelLoopSyncCriticalRegionEnd(loopName) }


#else

#define CX_ENABLE_PARALLELISM 0

#define CXParallelForStart(loopName, indexName, count) \
printf("NOT using OMP\n");\
CXIndex indexName;\
for(indexName=0;indexName<count;indexName++){

#define CXParallelForEnd(loopName) }

#define CXParallelLoopCriticalRegionStart(loopName)
#define CXParallelLoopCriticalRegionEnd(loopName)

#define CXParallelLoopSyncCriticalRegionStart(loopName)
#define CXParallelLoopSyncCriticalRegionEnd(loopName)


#endif //_OPENMP

typedef char* CXBitArray;

#define CXBitArrayMask(index) (1 << ((index) % CHAR_BIT))
#define CXBitArraySlot(index) ((index) / CHAR_BIT)
#define CXBitArraySet(bitArray, index) ((bitArray)[CXBitArraySlot(index)] |= CXBitArrayMask(index))
#define CXBitArrayClear(bitArray, index) ((bitArray)[CXBitArraySlot(index)] &= ~CXBitArrayMask(index))
#define CXBitArrayTest(bitArray, index) ((bitArray)[CXBitArraySlot(index)] & CXBitArrayMask(index))
#define CXBitArrayNSlots(size) ((size + CHAR_BIT - 1) / CHAR_BIT)

CX_INLINE CXBitArray CXNewBitArray(CXSize count){
	return (CXBitArray)calloc(CXBitArrayNSlots(count),1);
}

#define CXBitArrayStatic(variableName,count) char variableName[CXBitArrayNSlots(count)]

CX_INLINE CXBitArray CXReallocBitArray(CXBitArray bitArray,CXSize count){
	return (CXBitArray)realloc(bitArray,CXBitArrayNSlots(count));
}

CX_INLINE CXBitArray CXNewBitArrayClone(const CXBitArray bitArray,CXSize count){
	CXBitArray newBitArray = CXNewBitArray(count);
	memcpy(newBitArray, bitArray, CXBitArrayNSlots(count));
	return newBitArray;
}

CX_INLINE void CXNewBitArrayCopyTo(const CXBitArray fromBitArray,CXSize count, CXBitArray toBitArray){
	memcpy(toBitArray, fromBitArray, CXBitArrayNSlots(count));
}


CX_INLINE void CXGrowBitArray(CXSize newCount, CXSize* count, CXSize* capacity, CXBitArray* bitArray){
	if(*capacity<newCount){
		*capacity = CXCapacityGrow(newCount);
		*bitArray = (CXBitArray)realloc(*bitArray,CXBitArrayNSlots(*capacity));
	}
	if(count){
		*count = newCount;
	}
}


CX_INLINE void CXBitArrayClearAll(CXBitArray bitArray, CXSize count){
	memset(bitArray,0,CXBitArrayNSlots(count));
}

CX_INLINE void CXBitArrayDestroy(CXBitArray bitArray){
	free(bitArray);
}



#define CXGrowArray(newCount, elementSize, count, capacity, array) do{\
if((capacity)<(newCount)){\
(capacity) = CXCapacityGrow(newCount);\
array = realloc((array),(capacity)*(elementSize));\
}\
(count) = (newCount);\
} while (0)

#define CXGrowArrayAddElement(element, elementSize, count, capacity, array) do{\
if((capacity)<(count)+1){\
(capacity) = CXCapacityGrow((count)+1);\
(array) = realloc((array),(capacity)*(elementSize));\
}\
(count)++;\
(array)[count-1]=(element);\
} while (0)




// Network


typedef struct{
	CXUInteger from;
	CXUInteger to;
} CXEdge;


#endif
