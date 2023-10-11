/**
  DISCLAIMER: The below code is proof of concept and is not intended for production use.
              It is still in developped in the context of a master thesis.
*/

#ifndef __PERIODIC_H__
#define __PERIODIC_H__

/* C */
#include <stdbool.h>
#include <stdint.h>
/* PostgreSQL */
#include <postgres.h>
#include "postgres_int_defs.h"
/* PostGIS */
#include <liblwgeom.h>
/* MEOS */
#include "meos.h"
#include "general/temporal.h"
// #include "general/pg_types.h"

// int32 v1_len_; <-- exactly 4 bytes to add at start for variable-length types, do not modify


/*****************************************************************************
 *  PMode
*****************************************************************************/

typedef struct
{
  Interval frequency; // maybe store as a TimestampTz and create Interval when used?
  int32 repetitions;
  // TimestampTz start_date;
  // TimestampTz end_date;
} PMode;


#define DatumGetPmodeP(X) ((PMode*) DatumGetPointer(X))
#define PmodePGetDatum(X) PointerGetDatum(X)
#define PG_GETARG_PMODE_P(n) DatumGetPmodeP(PG_GETARG_DATUM(n))
#define PG_RETURN_PMODE_P(x) return PmodePGetDatum(x)

extern PMode *pmode_in(const char *str);
extern PMode *pmode_parse(const char **str);
extern PMode *pmode_make(Interval *frequency, int32 repetitions);
extern char *pmode_out(const PMode *pmode);


/*****************************************************************************
 *  Structure definition
 * 
 *  Note that below Periodic structures have the same definition 
 *  as Temporal structures in order to enable easy casting between them. 
*****************************************************************************/

typedef struct
{
  int32 vl_len_;        /**< Varlena header (do not touch directly!) */
  uint8 temptype;       /**< Temporal type */
  uint8 subtype;        /**< Temporal subtype */
  int16 flags;          /**< Flags */
  /* variable-length data follows */
} Periodic;

typedef struct
{
  int32 vl_len_;        /**< Varlena header (do not touch directly!) */
  uint8 temptype;       /**< Temporal type */
  uint8 subtype;        /**< Temporal subtype */
  int16 flags;          /**< Flags */
  TimestampTz t;        /**< Timestamp (8 bytes) */
  Datum value;          /**< Base value for types passed by value,
                             first 8 bytes of the base value for values
                             passed by reference. The extra bytes
                             needed are added upon creation. */
  /* variable-length data follows */
} PInstant;

typedef struct
{
  int32 vl_len_;        /**< Varlena header (do not touch directly!) */
  uint8 temptype;       /**< Temporal type */
  uint8 subtype;        /**< Temporal subtype */
  int16 flags;          /**< Flags */
  int32 count;          /**< Number of TInstant elements */
  int32 maxcount;       /**< Maximum number of TInstant elements */
  int16 bboxsize;       /**< Size of the bounding box */
  char padding[6];      /**< Not used */
  Span period;          /**< Time span (24 bytes). All bounding boxes start
                             with a period so actually it is also the begining
                             of the bounding box. The extra bytes needed for
                             the bounding box are added upon creation. */
  /* variable-length data follows */
} PSequence;

#define PSEQUENCE_BBOX_PTR(seq)      ((void *)(&(seq)->period))

typedef struct
{
  int32 vl_len_;        /**< Varlena header (do not touch directly!) */
  uint8 temptype;       /**< Temporal type */
  uint8 subtype;        /**< Temporal subtype */
  int16 flags;          /**< Flags */
  int32 count;          /**< Number of TSequence elements */
  int32 totalcount;     /**< Total number of TInstant elements in all
                             composing TSequence elements */
  int32 maxcount;       /**< Maximum number of TSequence elements */
  int16 bboxsize;       /**< Size of the bounding box */
  int16 padding;        /**< Not used */
  Span period;          /**< Time span (24 bytes). All bounding boxes start
                             with a period so actually it is also the begining
                             of the bounding box. The extra bytes needed for
                             the bounding box are added upon creation. */
  /* variable-length data follows */
} PSequenceSet;

#define PSEQUENCESET_BBOX_PTR(ss)      ((void *)(&(ss)->period))

// todo
// #if MEOS
//   #define DatumGetPeriodicP(X)       ((Periodic *) DatumGetPointer(X))
// #else
//   #define DatumGetPeriodicP(X)       ((Periodic *) PG_DETOAST_DATUM(X))
// #endif /* MEOS */

#define DatumGetPeriodicP(X)       ((Periodic *) DatumGetPointer(X))
#define PG_GETARG_PERIODIC_P(X)    ((Periodic *) PG_GETARG_VARLENA_P(X))
#define PG_GETARG_PINSTANT_P(X)    ((PInstant *) PG_GETARG_VARLENA_P(X))
#define PG_GETARG_PSEQUENCE_P(X)    ((PSequence *) PG_GETARG_VARLENA_P(X))
#define PG_GETARG_PSEQUENCESET_P(X)    ((PSequenceSet *) PG_GETARG_VARLENA_P(X))


typedef enum
{
  P_NONE  =  0,
  P_DAY   =  1,
  P_WEEK  =  2,
  P_MONTH =  3,
  P_YEAR  =  4,
} perType;


/*****************************************************************************
  FLAGS
*****************************************************************************/

/* The following flags are only used for Periodic */  
#define MEOS_FLAG_PERIODIC    0x0700  // 0001, 0010, 0011, 0100 

#define MEOS_FLAGS_GET_PERIODIC(flags)    (((flags) & MEOS_FLAG_PERIODIC) >> 8)
#define MEOS_FLAGS_SET_PERIODIC(flags, value) ((flags) = (((flags) & ~MEOS_FLAG_PERIODIC) | ((value & 0x07) << 8)))

#define MEOS_FLAGS_GET_PER_DAY(flags)     ((bool) (MEOS_FLAGS_GET_PERIODIC((flags)) == P_DAY))
#define MEOS_FLAGS_GET_PER_WEEK(flags)    ((bool) (MEOS_FLAGS_GET_PERIODIC((flags)) == P_WEEK))
#define MEOS_FLAGS_GET_PER_MONTH(flags)   ((bool) (MEOS_FLAGS_GET_PERIODIC((flags)) == P_MONTH))
#define MEOS_FLAGS_GET_PER_YEAR(flags)    ((bool) (MEOS_FLAGS_GET_PERIODIC((flags)) == P_YEAR))

/*****************************************************************************
 *  Input
*****************************************************************************/

Periodic *periodic_in(const char *str, meosType temptype);
// Periodic *periodic_parse(const char **str, meosType temptype);


/*****************************************************************************
 *  Output
*****************************************************************************/

char *periodic_out(const Periodic *per, int maxdd);
char *pinstant_out(const PInstant *pinst, int maxdd);
char *psequence_out(const PSequence *pseq, int maxdd);
char *psequenceset_out(const PSequenceSet *pss, int maxdd);
char *pinstant_to_string(const PInstant *pinst, const perType ptype, int maxdd, outfunc value_out);
char *psequence_to_string(const PSequence *pseq, const perType ptype, int maxdd, bool component, outfunc value_out);
char *psequenceset_to_string(const PSequenceSet *pss, const perType ptype, int maxdd, outfunc value_out);

/*****************************************************************************
 *  Copy
*****************************************************************************/

Periodic *periodic_copy(const Periodic *per);
PInstant *pinstant_copy(const PInstant *pinst);
PSequence *psequence_copy(const PSequence *pseq);
PSequenceSet *psequenceset_copy(const PSequenceSet *pss);

/*****************************************************************************
 *  Periodic type
*****************************************************************************/

#define MEOS_PERTYPE_STR_MAXLEN 6

Periodic *periodic_set_pertype(const Periodic *per, perType ptype);
char *periodic_get_pertype(const Periodic *per);

/*****************************************************************************
 *  Casting
*****************************************************************************/

Periodic *tint_to_pint(Temporal *temp);
Temporal *pint_to_tint(Periodic *temp);


/*****************************************************************************
 *  Operations
*****************************************************************************/

// Temporal* periodic_generate(Periodic* per, PMode* pmode);
Periodic* temporal_make_periodic(Temporal* temp, PMode* pmode);


/*****************************************************************************
 *  Other
*****************************************************************************/

char *format_timestamptz(TimestampTz tstz, const char* fmt);



#endif /* __PERIODIC_H__ */