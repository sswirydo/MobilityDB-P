/*****************************************************************************
 *
 * This MobilityDB code is provided under The PostgreSQL License.
 * Copyright (c) 2016-2022, Université libre de Bruxelles and MobilityDB
 * contributors
 *
 * MobilityDB includes portions of PostGIS version 3 source code released
 * under the GNU General Public License (GPLv2 or later).
 * Copyright (c) 2001-2022, PostGIS contributors
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without a written
 * agreement is hereby granted, provided that the above copyright notice and
 * this paragraph and the following two paragraphs appear in all copies.
 *
 * IN NO EVENT SHALL UNIVERSITE LIBRE DE BRUXELLES BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING
 * LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
 * EVEN IF UNIVERSITE LIBRE DE BRUXELLES HAS BEEN ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * UNIVERSITE LIBRE DE BRUXELLES SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFTWARE PROVIDED HEREUNDER IS ON
 * AN "AS IS" BASIS, AND UNIVERSITE LIBRE DE BRUXELLES HAS NO OBLIGATIONS TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS. 
 *
 *****************************************************************************/

/**
 * @file tsequence.h
 * Basic functions for temporal sequences.
 */

#ifndef __TSEQUENCE_H__
#define __TSEQUENCE_H__

#include <postgres.h>
#include <catalog/pg_type.h>
#include <utils/array.h>
#include <utils/rangetypes.h>

#include "temporal.h"

/*****************************************************************************/

/* General functions */

extern const TInstant *tsequence_inst_n(const TSequence *seq, int index);
extern void *tsequence_bbox_ptr(const TSequence *seq);
extern void tsequence_bbox(const TSequence *seq, void *box);
extern int *tsequenceset_make_valid_gaps(const TInstant **instants, int count,
  bool lower_inc, bool upper_inc, bool linear, double maxdist, Interval *maxt,
  int *countsplits);
extern TSequence *tsequence_make1(const TInstant **instants, int count,
  bool lower_inc, bool upper_inc, bool linear, bool normalize);
extern TSequence *tsequence_make(const TInstant **instants,
  int count, bool lower_inc, bool upper_inc, bool linear, bool normalize);
extern TSequence *tsequence_make_free(TInstant **instants,
  int count, bool lower_inc, bool upper_inc, bool linear, bool normalize);
extern TSequence *tsequence_copy(const TSequence *seq);
extern int tsequence_find_timestamp(const TSequence *seq, TimestampTz t);
extern TSequence **tseqarr2_to_tseqarr(TSequence ***sequences,
  int *countseqs, int count, int totalseqs);

/* Append and merge functions */

extern Temporal *tsequence_append_tinstant(const TSequence *seq,
  const TInstant *inst);
extern Temporal *tsequence_merge(const TSequence *seq1, const TSequence *seq2);
extern TSequence **tseqarr_normalize(const TSequence **sequences,
  int count, int *newcount);
extern TSequence **tsequence_merge_array1(const TSequence **sequences,
  int count, int *totalcount);
extern Temporal *tsequence_merge_array(const TSequence **sequences, int count);

/* Synchronization functions */

extern bool synchronize_tsequence_tsequence(const TSequence *seq1,
  const TSequence *seq2, TSequence **sync1, TSequence **sync2,
  bool interpoint);

/* Intersection functions */

extern bool tlinearsegm_intersection_value(const TInstant *inst1,
  const TInstant *inst2, Datum value, Oid basetypid, Datum *inter,
  TimestampTz *t);
extern bool tsegment_intersection(const TInstant *start1,
  const TInstant *end1, bool linear1, const TInstant *start2,
  const TInstant *end2, bool linear2, Datum *inter1, Datum *inter2,
  TimestampTz *t);

extern bool intersection_tsequence_tinstant(const TSequence *seq,
  const TInstant *inst, TInstant **inter1, TInstant **inter2);
extern bool intersection_tinstant_tsequence(const TInstant *inst,
  const TSequence *seq, TInstant **inter1, TInstant **inter2);
extern bool intersection_tsequence_tinstantset(const TSequence *seq,
  const TInstantSet *ti, TInstantSet **inter1, TInstantSet **inter2);
extern bool intersection_tinstantset_tsequence(const TInstantSet *ti,
  const TSequence *seq, TInstantSet **inter1, TInstantSet **inter2);

/* Input/output functions */

extern char *tsequence_to_string(const TSequence *seq, bool component,
  char *(*value_out)(Oid, Datum));
extern void tsequence_write(const TSequence *seq, StringInfo buf);
extern TSequence *tsequence_read(StringInfo buf, Oid basetypid);

/* Constructor functions */

extern TSequence *tsequence_from_base_internal(Datum value, Oid basetypid,
  const Period *p, bool linear);

extern Datum tsequence_from_base(PG_FUNCTION_ARGS);

/* Cast functions */

extern TSequence *tintseq_to_tfloatseq(const TSequence *seq);
extern TSequence *tfloatseq_to_tintseq(const TSequence *seq);

/* Transformation functions */

extern TSequence *tinstant_to_tsequence(const TInstant *inst, bool linear);
extern TSequence *tinstantset_to_tsequence(const TInstantSet *ti, bool linear);
extern TSequence *tsequenceset_to_tsequence(const TSequenceSet *ts);
extern int tstepseq_to_linear1(const TSequence *seq, TSequence **result);
extern TSequenceSet *tstepseq_to_linear(const TSequence *seq);
extern TSequence *tsequence_shift_tscale(const TSequence *seq,
  const Interval *start, const Interval *duration);

/* Accessor functions */

extern int tsequence_values(const TSequence *seq, Datum *result);
extern ArrayType *tsequence_values_array(const TSequence *seq);
extern RangeType *tfloatseq_range(const TSequence *seq);
extern int tfloatseq_ranges(const TSequence *seq, RangeType **result);
extern ArrayType *tfloatseq_ranges_array(const TSequence *seq);
extern PeriodSet *tsequence_get_time(const TSequence *seq);
extern const TInstant *tsequence_min_instant(const TSequence *seq);
extern Datum tsequence_min_value(const TSequence *seq);
extern Datum tsequence_max_value(const TSequence *seq);
extern Datum tsequence_duration(const TSequence *seq);
extern void tsequence_period(const TSequence *seq, Period *p);
extern int tsequence_segments(const TSequence *seq, TSequence **result);
extern ArrayType *tsequence_segments_array(const TSequence *seq);
extern const TInstant **tsequence_instants(const TSequence *seq, int *count);
extern ArrayType *tsequence_instants_array(const TSequence *seq);
extern TimestampTz tsequence_start_timestamp(const TSequence *seq);
extern TimestampTz tsequence_end_timestamp(const TSequence *seq);
extern int tsequence_timestamps(const TSequence *seq, TimestampTz *result);
extern ArrayType *tsequence_timestamps_array(const TSequence *seq);

/* Ever/always comparison operators */

extern bool tsequence_ever_eq(const TSequence *seq, Datum value);
extern bool tsequence_always_eq(const TSequence *seq, Datum value);
extern bool tsequence_ever_lt(const TSequence *seq, Datum value);
extern bool tsequence_ever_le(const TSequence *seq, Datum value);
extern bool tsequence_always_lt(const TSequence *seq, Datum value);
extern bool tsequence_always_le(const TSequence *seq, Datum value);

/* Restriction Functions */

extern int tsequence_restrict_value1(const TSequence *seq, Datum value,
  bool atfunc, TSequence **result);
extern TSequenceSet *tsequence_restrict_value(const TSequence *seq,
  Datum value, bool atfunc);
extern int tsequence_at_values1(const TSequence *seq, const Datum *values,
  int count, TSequence **result);
extern TSequenceSet *tsequence_restrict_values(const TSequence *seq,
  const Datum *values, int count, bool atfunc);
extern int tnumberseq_restrict_range2(const TSequence *seq,
  const RangeType *range, bool atfunc, TSequence **result);
extern TSequenceSet *tnumberseq_restrict_range(const TSequence *seq,
  const RangeType *range, bool atfunc);
extern int tnumberseq_restrict_ranges1(const TSequence *seq,
  RangeType **normranges, int count, bool atfunc, bool bboxtest,
  TSequence **result);
extern TSequenceSet *tnumberseq_restrict_ranges(const TSequence *seq,
  RangeType **normranges, int count, bool atfunc, bool bboxtest);
extern TSequenceSet *tsequence_restrict_minmax(const TSequence *seq,
  bool min, bool atfunc);

extern Datum tsegment_value_at_timestamp(const TInstant *inst1,
  const TInstant *inst2, bool linear, TimestampTz t);
extern bool tsequence_value_at_timestamp(const TSequence *seq, TimestampTz t,
  Datum *result);
extern bool tsequence_value_at_timestamp_inc(const TSequence *seq, TimestampTz t,
  Datum *result);

extern TInstant *tsegment_at_timestamp(const TInstant *inst1,
  const TInstant *inst2, bool linear, TimestampTz t);
extern TInstant *tsequence_at_timestamp(const TSequence *seq, TimestampTz t);
extern int tsequence_minus_timestamp1(const TSequence *seq, TimestampTz t,
  TSequence **result);
extern TSequenceSet *tsequence_minus_timestamp(const TSequence *seq,
  TimestampTz t);
extern TInstantSet *tsequence_at_timestampset(const TSequence *seq,
  const TimestampSet *ts);
extern int tsequence_minus_timestampset1(const TSequence *seq,
  const TimestampSet *ts, TSequence **result);
extern TSequenceSet *tsequence_minus_timestampset(const TSequence *seq,
  const TimestampSet *ts);
extern TSequence *tsequence_at_period(const TSequence *seq, const Period *p);
extern int tsequence_minus_period1(const TSequence *seq, const Period *p,
  TSequence **result);
extern TSequenceSet *tsequence_minus_period(const TSequence *seq, const Period *p);
extern int tsequence_at_periodset(const TSequence *seq, const PeriodSet *ps,
  TSequence **result);
extern int tsequence_minus_periodset(const TSequence *seq, const PeriodSet *ps,
  int from, TSequence **result);
extern TSequenceSet *tsequence_restrict_periodset(const TSequence *seq,
  const PeriodSet *ps, bool atfunc);

/* Intersects functions */

extern bool tsequence_intersects_timestamp(const TSequence *seq, TimestampTz t);
extern bool tsequence_intersects_timestampset(const TSequence *seq,
  const TimestampSet *t);
extern bool tsequence_intersects_period(const TSequence *seq, const Period *p);
extern bool tsequence_intersects_periodset(const TSequence *seq,
  const PeriodSet *ps);

/* Local aggregate functions */

extern double tnumberseq_integral(const TSequence *seq);
extern double tnumberseq_twavg(const TSequence *seq);

/* Comparison functions */

extern bool tsequence_eq(const TSequence *seq1, const TSequence *seq2);
extern int tsequence_cmp(const TSequence *seq1, const TSequence *seq2);

/* Function for defining hash index */

extern uint32 tsequence_hash(const TSequence *seq);

/*****************************************************************************/

#endif
