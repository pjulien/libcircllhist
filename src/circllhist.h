/** \file circllhist.h */
/*
 * Copyright (c) 2016, Circonus, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name Circonus, Inc. nor the names of its contributors
 *       may be used to endorse or promote products derived from this
 *       software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CIRCLLHIST_H
#define CIRCLLHIST_H

#define DEFAULT_HIST_SIZE 100
#define API_EXPORT(type) extern type
#include <stdlib.h>
#include <sys/types.h>
#include <stdint.h>

typedef struct histogram histogram_t;
typedef struct hist_rollup_config hist_rollup_config_t;
//! A hist_bucket structure represents a histogram bucket with the following
//! dimensions:
//! - (val < -99 || 99 < val) => Invalid bucket
//! - (-10 < val && val < 10) => (-10^-127 .. +10^-127) zero bucket
//! - val > 0 => [ (val/10)*10^exp .. (val+1)/10*10^exp )
//! - val < 0 => ( (val-1)/10*10^exp .. (val/10)*10^exp ]
typedef struct hist_bucket {
  int8_t val; //!< value * 10
  int8_t exp; //!< exponent -128 .. 127
} hist_bucket_t;

////////////////////////////////////////////////////////////////////////////////
// Histogram Buckets

//! Returns the edge of the histogram bucket closer to zero
API_EXPORT(double) hist_bucket_to_double(hist_bucket_t hb);
//! Calculate mid-point of the bucket
API_EXPORT(double) hist_bucket_midpoint(hist_bucket_t in);
//! Get the width of the hist_bucket
API_EXPORT(double) hist_bucket_to_double_bin_width(hist_bucket_t hb);
//! Create the bucket that a value belongs to
API_EXPORT(hist_bucket_t) double_to_hist_bucket(double d);
//! Create the bucket that value * 10^(scale) belongs to
API_EXPORT(hist_bucket_t) int_scale_to_hist_bucket(int64_t value, int scale);

////////////////////////////////////////////////////////////////////////////////
// Creating and Destroying Histograms

//! Create a new histogram
API_EXPORT(histogram_t *) hist_alloc();
//! Create a new histogram with preallocated bins
API_EXPORT(histogram_t *) hist_alloc_nbins(int nbins);
//! Create a fast-histogram
/*! Fast allocations consume 2kb + N * 512b more memory
 *  where N is the number of used exponents.  It allows for O(1) increments for
 *  prexisting keys */
API_EXPORT(histogram_t *) hist_fast_alloc();
//! Create a fast-histogram with preallocated bins
API_EXPORT(histogram_t *) hist_fast_alloc_nbins(int nbins);
//! Free a (fast-) histogram
API_EXPORT(void) hist_free(histogram_t *hist);

////////////////////////////////////////////////////////////////////////////////
// Getting data in and out of histograms

/*! Inserting double values converts from IEEE double to a small static integer
 *  base and can suffer from floating point math skew.  Using the intscale
 *  variant is more precise and significantly faster if you already have
 *  integer measurements. */
//! insert a value into a histogram count times
API_EXPORT(uint64_t) hist_insert(histogram_t *hist, double val, uint64_t count);
//! Remove data from a histogram count times
API_EXPORT(uint64_t) hist_remove(histogram_t *hist, double val, uint64_t count);
//! Insert a single bucket + count into a histogram
//!
//! Updates counts if the bucket exists
//! Handles re-allocation of new buckets if needed
API_EXPORT(uint64_t) hist_insert_raw(histogram_t *hist, hist_bucket_t hb, uint64_t count);
//! Get the number of used buckets in a histogram
API_EXPORT(int) hist_bucket_count(const histogram_t *hist);
//! Same as hist_bucket_count
API_EXPORT(int) hist_num_buckets(const histogram_t *hist);
//! Get the total number of values stored in the histogram
API_EXPORT(uint64_t) hist_sample_count(const histogram_t *hist);
//! Get value+count for bucket at position idx. Valid positions are 0 .. hist_bucket_count()
API_EXPORT(int) hist_bucket_idx(const histogram_t *hist, int idx, double *v, uint64_t *c);
//! Get bucket+count for bucket at position idx. Valid positions are 0 .. hist_bucket_count()
API_EXPORT(int) hist_bucket_idx_bucket(const histogram_t *hist, int idx, hist_bucket_t *b, uint64_t *c);
//! Accumulate bins from each of cnt histograms in src onto tgt
API_EXPORT(int) hist_accumulate(histogram_t *tgt, const histogram_t * const *src, int cnt);
//! Clear data fast. Keeps buckets allocated.
API_EXPORT(void) hist_clear(histogram_t *hist);
//! Insert a value into a histogram value = val * 10^(scale)
API_EXPORT(uint64_t) hist_insert_intscale(histogram_t *hist, int64_t val, int scale, uint64_t count);

////////////////////////////////////////////////////////////////////////////////
// Serialization

//! Serialize histogram to binary data
API_EXPORT(ssize_t) hist_serialize(const histogram_t *h, void *buff, ssize_t len);
API_EXPORT(ssize_t) hist_deserialize(histogram_t *h, const void *buff, ssize_t len);
API_EXPORT(ssize_t) hist_serialize_estimate(const histogram_t *h);
//! Return histogram serialization as base64 encoded string
API_EXPORT(ssize_t) hist_serialize_b64(const histogram_t *h, char *b64_serialized_histo_buff, ssize_t buff_len);
API_EXPORT(ssize_t) hist_deserialize_b64(histogram_t *h, const void *b64_string, ssize_t b64_string_len);
API_EXPORT(ssize_t) hist_serialize_b64_estimate(const histogram_t *h);
//! Compress histogram by squshing together adjacent buckets
//!
//! This compression is lossy. mean/quantiles will be affected by compression.
//! Intended use cases is visualization.
//! \param hist
//! \param mbe the Minimum Bucket Exponent
//! \return the compressed histogram as new value
API_EXPORT(histogram_t *) hist_compress_mbe(histogram_t *h, int8_t mbe);

////////////////////////////////////////////////////////////////////////////////
// Analytics

//! Approximate mean value of all values stored in the histogram
API_EXPORT(double) hist_approx_mean(const histogram_t *);
//! Approximate the sum of all values stored in the histogram
API_EXPORT(double) hist_approx_sum(const histogram_t *);
//! Approiximate n quantiles of all values stored in the histogram
//! \param *q_in array of quantiles to comute
//! \param nq length of quantile array
//! \param *q_out pre-allocated array where results shall be written to
API_EXPORT(int) hist_approx_quantile(const histogram_t *, double *q_in, int nq, double *q_out);

#endif
