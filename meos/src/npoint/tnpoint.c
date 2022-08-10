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
 * @brief Basic functions for temporal network points.
 */

#include "npoint/tnpoint.h"

/* C */
#include <assert.h>
/* MobilityDB */
#include <meos.h>
#include <meos_internal.h>
#include "general/temporal_parser.h"
#include "general/temporal_util.h"
#include "general/lifting.h"
#include "point/tpoint_spatialfuncs.h"
#include "npoint/tnpoint_static.h"
#include "npoint/tnpoint_parser.h"

/*****************************************************************************
 * Input/output functions
 *****************************************************************************/


/*****************************************************************************
 * Cast functions
 *****************************************************************************/

/**
 * @brief Cast a temporal network point as a temporal geometric point.
 */
TInstant *
tnpointinst_tgeompointinst(const TInstant *inst)
{
  Npoint *np = DatumGetNpointP(tinstant_value(inst));
  GSERIALIZED *geom = npoint_geom(np);
  TInstant *result = 
    tinstant_make(PointerGetDatum(geom), T_TGEOMPOINT, inst->t);
  pfree(geom);
  return result;
}

/**
 * @brief Cast a temporal network point as a temporal geometric point.
 */
TInstantSet *
tnpointinstset_tgeompointinstset(const TInstantSet *is)
{
  TInstant **instants = palloc(sizeof(TInstant *) * is->count);
  for (int i = 0; i < is->count; i++)
  {
    const TInstant *inst = tinstantset_inst_n(is, i);
    instants[i] = tnpointinst_tgeompointinst(inst);
  }
  TInstantSet *result = tinstantset_make_free(instants, is->count, MERGE_NO);
  return result;
}

/**
 * @brief Cast a temporal network point as a temporal geometric point.
 */
TSequence *
tnpointseq_tgeompointseq(const TSequence *seq)
{
  TInstant **instants = palloc(sizeof(TInstant *) * seq->count);
  Npoint *np = DatumGetNpointP(tinstant_value(tsequence_inst_n(seq, 0)));
  /* We are sure line is not empty */
  GSERIALIZED *line = route_geom(np->rid);
  int srid = gserialized_get_srid(line);
  LWLINE *lwline = (LWLINE *) lwgeom_from_gserialized(line);
  for (int i = 0; i < seq->count; i++)
  {
    const TInstant *inst = tsequence_inst_n(seq, i);
    np = DatumGetNpointP(tinstant_value(inst));
    POINTARRAY *opa = lwline_interpolate_points(lwline, np->pos, 0);
    LWGEOM *lwpoint;
    assert(opa->npoints <= 1);
    lwpoint = lwpoint_as_lwgeom(lwpoint_construct(srid, NULL, opa));
    Datum point = PointerGetDatum(geo_serialize(lwpoint));
    instants[i] = tinstant_make(point, T_TGEOMPOINT, inst->t);
    pfree(DatumGetPointer(point));
  }
  TSequence *result = tsequence_make_free(instants, seq->count,
    seq->period.lower_inc, seq->period.upper_inc,
    MOBDB_FLAGS_GET_LINEAR(seq->flags), false);
  pfree(DatumGetPointer(line));
  return result;
}

/**
 * @brief Cast a temporal network point as a temporal geometric point.
 */
TSequenceSet *
tnpointseqset_tgeompointseqset(const TSequenceSet *ss)
{
  TSequence **sequences = palloc(sizeof(TSequence *) * ss->count);
  for (int i = 0; i < ss->count; i++)
  {
    const TSequence *seq = tsequenceset_seq_n(ss, i);
    sequences[i] = tnpointseq_tgeompointseq(seq);
  }
  TSequenceSet *result = tsequenceset_make_free(sequences, ss->count, false);
  return result;
}

/**
 * @brief Cast a temporal network point as a temporal geometric point.
 */
Temporal *
tnpoint_tgeompoint(const Temporal *temp)
{
  Temporal *result;
  ensure_valid_tempsubtype(temp->subtype);
  if (temp->subtype == TINSTANT)
    result = (Temporal *) tnpointinst_tgeompointinst((TInstant *) temp);
  else if (temp->subtype == TINSTANTSET)
    result = (Temporal *) tnpointinstset_tgeompointinstset((TInstantSet *) temp);
  else if (temp->subtype == TSEQUENCE)
    result = (Temporal *) tnpointseq_tgeompointseq((TSequence *) temp);
  else /* temp->subtype == TSEQUENCESET */
    result = (Temporal *) tnpointseqset_tgeompointseqset((TSequenceSet *) temp);
  return result;
}

/*****************************************************************************/

/**
 * @brief Cast a temporal geometric point as a temporal network point.
 */
TInstant *
tgeompointinst_tnpointinst(const TInstant *inst)
{
  GSERIALIZED *gs = DatumGetGserializedP(tinstant_value(inst));
  Npoint *np = geom_npoint(gs);
  if (np == NULL)
    return NULL;
  TInstant *result = tinstant_make(PointerGetDatum(np), T_TNPOINT, inst->t);
  pfree(np);
  return result;
}

/**
 * @brief Cast a temporal geometric point as a temporal network point.
 */
TInstantSet *
tgeompointinstset_tnpointinstset(const TInstantSet *is)
{
  TInstant **instants = palloc(sizeof(TInstant *) * is->count);
  for (int i = 0; i < is->count; i++)
  {
    const TInstant *inst = tinstantset_inst_n(is, i);
    TInstant *inst1 = tgeompointinst_tnpointinst(inst);
    if (inst1 == NULL)
    {
      pfree_array((void **) instants, i);
      return NULL;
    }
    instants[i] = inst1;
  }
  TInstantSet *result = tinstantset_make_free(instants, is->count, MERGE_NO);
  return result;
}

/**
 * @brief Cast a temporal geometric point as a temporal network point.
 */
TSequence *
tgeompointseq_tnpointseq(const TSequence *seq)
{
  TInstant **instants = palloc(sizeof(TInstant *) * seq->count);
  for (int i = 0; i < seq->count; i++)
  {
    const TInstant *inst = tsequence_inst_n(seq, i);
    TInstant *inst1 = tgeompointinst_tnpointinst(inst);
    if (inst1 == NULL)
    {
      pfree_array((void **) instants, i);
      return NULL;
    }
    instants[i] = inst1;
  }
  TSequence *result = tsequence_make_free(instants, seq->count,
    seq->period.lower_inc, seq->period.upper_inc,
    MOBDB_FLAGS_GET_LINEAR(seq->flags), true);
  return result;
}

/**
 * @brief Cast a temporal geometric point as a temporal network point.
 */
TSequenceSet *
tgeompointseqset_tnpointseqset(const TSequenceSet *ss)
{
  TSequence **sequences = palloc(sizeof(TSequence *) * ss->count);
  for (int i = 0; i < ss->count; i++)
  {
    const TSequence *seq = tsequenceset_seq_n(ss, i);
    TSequence *seq1 = tgeompointseq_tnpointseq(seq);
    if (seq1 == NULL)
    {
      pfree_array((void **) sequences, i);
      return NULL;
    }
    sequences[i] = seq1;
  }
  TSequenceSet *result = tsequenceset_make_free(sequences, ss->count, true);
  return result;
}

/**
 * @brief Cast a temporal geometric point as a temporal network point.
 */
Temporal *
tgeompoint_tnpoint(const Temporal *temp)
{
  int32_t srid_tpoint = tpoint_srid(temp);
  int32_t srid_ways = get_srid_ways();
  ensure_same_srid(srid_tpoint, srid_ways);
  Temporal *result;
  ensure_valid_tempsubtype(temp->subtype);
  if (temp->subtype == TINSTANT)
    result = (Temporal *) tgeompointinst_tnpointinst((TInstant *) temp);
  else if (temp->subtype == TINSTANTSET)
    result = (Temporal *) tgeompointinstset_tnpointinstset((TInstantSet *) temp);
  else if (temp->subtype == TSEQUENCE)
    result = (Temporal *) tgeompointseq_tnpointseq((TSequence *) temp);
  else /* temp->subtype == TSEQUENCESET */
    result = (Temporal *) tgeompointseqset_tnpointseqset((TSequenceSet *) temp);
  return result;
}

/*****************************************************************************
 * Accessor functions
 *****************************************************************************/

/**
 * @brief Return the network segments covered by the temporal network point.
 */
Nsegment **
tnpointinst_positions(const TInstant *inst)
{
  Nsegment **result = palloc(sizeof(Nsegment *));
  Npoint *np = DatumGetNpointP(tinstant_value(inst));
  result[0] = nsegment_make(np->rid, np->pos, np->pos);
  return result;
}

/**
 * @brief Return the network segments covered by the temporal network point.
 */
Nsegment **
tnpointinstset_positions(const TInstantSet *is, int *count)
{
  int count1;
  /* The following function removes duplicate values */
  Datum *values = tinstantset_values(is, &count1);
  Nsegment **result = palloc(sizeof(Nsegment *) * count1);
  for (int i = 0; i < count1; i++)
  {
    Npoint *np = DatumGetNpointP(values[i]);
    result[i] = nsegment_make(np->rid, np->pos, np->pos);
  }
  pfree(values);
  *count = count1;
  return result;
}

/**
 * Return the network segments covered by the temporal network point.
 */
Nsegment **
tnpointseq_step_positions(const TSequence *seq, int *count)
{
  int count1;
  /* The following function removes duplicate values */
  Datum *values = tsequence_values(seq, &count1);
  Nsegment **result = palloc(sizeof(Nsegment *) * count1);
  for (int i = 0; i < count1; i++)
  {
    Npoint *np = DatumGetNpointP(values[i]);
    result[i] = nsegment_make(np->rid, np->pos, np->pos);
  }
  pfree(values);
  *count = count1;
  return result;
}

/**
 * Return the network segments covered by the temporal network point.
 */
Nsegment *
tnpointseq_linear_positions(const TSequence *seq)
{
  const TInstant *inst = tsequence_inst_n(seq, 0);
  Npoint *np = DatumGetNpointP(tinstant_value(inst));
  int64 rid = np->rid;
  double minPos = np->pos, maxPos = np->pos;
  for (int i = 1; i < seq->count; i++)
  {
    inst = tsequence_inst_n(seq, i);
    np = DatumGetNpointP(tinstant_value(inst));
    minPos = Min(minPos, np->pos);
    maxPos = Max(maxPos, np->pos);
  }
  return nsegment_make(rid, minPos, maxPos);
}

/**
 * @brief Return the network segments covered by the temporal network point.
 */
Nsegment **
tnpointseq_positions(const TSequence *seq, int *count)
{
  if (MOBDB_FLAGS_GET_LINEAR(seq->flags))
  {
    Nsegment **result = palloc(sizeof(Nsegment *));
    result[0] = tnpointseq_linear_positions(seq);
    *count = 1;
    return result;
  }
  else
    return tnpointseq_step_positions(seq, count);
}

/**
 * Return the network segments covered by the temporal network point.
 */
Nsegment **
tnpointseqset_linear_positions(const TSequenceSet *ss, int *count)
{
  Nsegment **segments = palloc(sizeof(Nsegment *) * ss->count);
  for (int i = 0; i < ss->count; i++)
  {
    const TSequence *seq = tsequenceset_seq_n(ss, i);
    segments[i] = tnpointseq_linear_positions(seq);
  }
  Nsegment **result = segments;
  int count1 = ss->count;
  if (count1 > 1)
    result = nsegmentarr_normalize(segments, &count1);
  *count = count1;
  return result;
}

/**
 * Return the network segments covered by the temporal network point.
 */
Nsegment **
tnpointseqset_step_positions(const TSequenceSet *ss, int *count)
{
  /* The following function removes duplicate values */
  int newcount;
  Datum *values = tsequenceset_values(ss, &newcount);
  Nsegment **result = palloc(sizeof(Nsegment *) * newcount);
  for (int i = 0; i < newcount; i++)
  {
    Npoint *np = DatumGetNpointP(values[i]);
    result[i] = nsegment_make(np->rid, np->pos, np->pos);
  }
  pfree(values);
  *count = newcount;
  return result;
}

/**
 * @brief Return the network segments covered by the temporal network point.
 */
Nsegment **
tnpointseqset_positions(const TSequenceSet *ss, int *count)
{
  Nsegment **result;
  if (MOBDB_FLAGS_GET_LINEAR(ss->flags))
    result = tnpointseqset_linear_positions(ss, count);
  else
    result = tnpointseqset_step_positions(ss, count);
  return result;
}

/**
 * @brief Return the network segments covered by the temporal network point.
 */
Nsegment **
tnpoint_positions(const Temporal *temp, int *count)
{
  Nsegment **result;
  ensure_valid_tempsubtype(temp->subtype);
  if (temp->subtype == TINSTANT)
  {
    result = tnpointinst_positions((TInstant *) temp);
    *count = 1;
  }
  else if (temp->subtype == TINSTANTSET)
    result = tnpointinstset_positions((TInstantSet *) temp, count);
  else if (temp->subtype == TSEQUENCE)
    result = tnpointseq_positions((TSequence *) temp, count);
  else /* temp->subtype == TSEQUENCESET */
    result = tnpointseqset_positions((TSequenceSet *) temp, count);
  return result;
}

/*****************************************************************************/

/**
 * @brief Return the route of the temporal network point.
 */
int64
tnpointinst_route(const TInstant *inst)
{
  Npoint *np = DatumGetNpointP(tinstant_value(inst));
  return np->rid;
}

/**
 * @brief Return the route of a temporal network point.
 */
int64
tnpoint_route(const Temporal *temp)
{
  if (temp->subtype != TINSTANT && temp->subtype != TSEQUENCE)
    elog(ERROR, "Input must be a temporal instant or a temporal sequence");

  const TInstant *inst = (temp->subtype == TINSTANT) ?
    (const TInstant *) temp : tsequence_inst_n((const TSequence *) temp, 0);
  Npoint *np = DatumGetNpointP(tinstant_value(inst));
  return np->rid;
}

/**
 * @brief Return the array of routes of a temporal network point
 */
int64 *
tnpointinst_routes(const TInstant *inst)
{
  Npoint *np = DatumGetNpointP(tinstant_value(inst));
  int64 *result = palloc(sizeof(int64));
  result[0]= np->rid;
  return result;
}

/**
 * @brief Return the array of routes of a temporal network point
 */
int64 *
tnpointinstset_routes(const TInstantSet *is)
{
  int64 *result = palloc(sizeof(int64) * is->count);
  for (int i = 0; i < is->count; i++)
  {
    const TInstant *inst = tinstantset_inst_n(is, i);
    Npoint *np = DatumGetNpointP(tinstant_value(inst));
    result[i] = np->rid;
  }
  return result;
}

/**
 * @brief Return the array of routes of a temporal network point
 */
int64 *
tnpointseq_routes(const TSequence *seq)
{
  const TInstant *inst = tsequence_inst_n(seq, 0);
  Npoint *np = DatumGetNpointP(tinstant_value(inst));
  int64 *result = palloc(sizeof(int64));
  result[0]= np->rid;
  return result;
}

/**
 * @brief Return the array of routes of a temporal network point
 */
int64 *
tnpointseqset_routes(const TSequenceSet *ss)
{
  int64 *result = palloc(sizeof(int64) * ss->count);
  for (int i = 0; i < ss->count; i++)
  {
    const TSequence *seq = tsequenceset_seq_n(ss, i);
    const TInstant *inst = tsequence_inst_n(seq, 0);
    Npoint *np = DatumGetNpointP(tinstant_value(inst));
    result[i] = np->rid;
  }
  return result;
}

/**
 * @brief Return the array of routes of a temporal network point
 */
int64 *
tnpoint_routes(const Temporal *temp, int *count)
{
  int64 *result;
  ensure_valid_tempsubtype(temp->subtype);
  if (temp->subtype == TINSTANT)
  {
    result = tnpointinst_routes((TInstant *) temp);
    *count = 1;
  }
  else if (temp->subtype == TINSTANTSET)
  {
    result = tnpointinstset_routes((TInstantSet *) temp);
    *count = ((TInstantSet *) temp)->count;
  }
  else if (temp->subtype == TSEQUENCE)
  {
    result = tnpointseq_routes((TSequence *) temp);
    *count = 1;
  }
  else /* temp->subtype == TSEQUENCESET */
  {
    result = tnpointseqset_routes((TSequenceSet *) temp);
    *count = ((TSequenceSet *) temp)->count;
  }
  return result;
}

/*****************************************************************************/