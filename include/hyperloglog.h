/*
 * hyperloglog.h - API for HyperLogLog probabilistic cardinality approximation.
 */

/*
 * Copyright (c) 2017, Quarantainenet Holding B.V.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the company nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HYPERLOGLOG_H
#define HYPERLOGLOG_H

#include "byte_slice.h"
#include "includes.h"

#define HLL_P 14 /* The greater is P, the smaller the error. */
#define HLL_REGISTERS (1 << HLL_P) /* With P=14, 16384 registers. */
#define HLL_P_MASK (HLL_REGISTERS - 1) /* Mask to index register. */
#define HLL_BITS 6 /* Enough to count up to 63 leading zeroes. */
#define HLL_DENSE_SIZE (((HLL_REGISTERS * HLL_BITS + 7) / 8) + 1)

#define HLL_DENSE 0 /* Dense encoding. */
#define HLL_SPARSE 1 /* Sparse encoding. */

#define C_OK 1
#define C_ERR 0

typedef struct {
	uint64_t card; /**< Cached cardinality */
	uint8_t encoding; /**< HLL_DENSE or HLL_SPARSE. */
	byte_slice_t registers; /**< The hyperloglog data */
	bool registers_owned; /**< Whether the registers are allocated by the hll or not */
} hll;

/** Initialize the hyperloglog state in 'hll'.
 *
 * This will allocate the necesary memory for the hyperloglog data from the heap.
 *
 * \param hll The hyperloglog to initialize
 */
extern void hllInit(hll* hll);

/** Initialize an HLL object with hyperloglog data kept elsewhere
 *
 * \note The data being referred to must be in the dense representation.
 * \note The registers data must have a size of `HLL_SIZE`
 *
 * \remark This buffer is not owned by the HLL object which is fine because
 *         dense encodings never need to (re)allocate this buffer.
 * \remark Supplying a fully zeroed memory slice allows one to create a new
 *         hyperloglog in the dense representation.
 *
 * \param hll       The hyperloglog to initialize
 * \param registers The hyperloglog data
 */
extern void hllInitFromBuffer(hll* hll, byte_slice_t registers);

/** Add a value to the hyperlolog
 *
 * \param hll  The hyperloglog to add the value to
 * \param hash A 64-bit collision resistent hash of the value to add
 * \returns A boolean indicating whether something was changed (whether it is probably a new value)
 */
extern int hllAdd(hll* hll, uint64_t hash);

/** Return the approximated cardinality of the set based on the harmonic mean of the hyperloglog
 *
 * \param hll     The hyperloglog that is to be used for the cardinality approximation
 * \param invalid If the sparse representation of the HLL object is not valid, the integer
 *                pointed by 'invalid' is set to non-zero, otherwise it is left untouched.
 * \returns The approximate cardinality
 */
extern uint64_t hllCount(hll* hll, int* invalid);

/** Convert the 'hll' to the "dense" representation.
 *
 * \param hll The hyperloglog to convert
 * \returns 'C_OK' if the 'hll' is in the "dense" representation after the call or 'C_ERR' if the conversion failed.
 */
extern int hllSparseToDense(hll* hll);

/** Destroy the hll object
 *
 * When done with a hyperlog one should always destroy it to prevent possible resource leakage.
 *
 * \note If the hyperloglog was initialized using `hllInitFromBuffer` then the caller is responsible
 *       for cleaning up the registers data.
 *
 * \param hll The hyperloglog to destroy
 */
extern void hllDestroy(hll* hll);

#endif /* HYPERLOGLOG_H */
