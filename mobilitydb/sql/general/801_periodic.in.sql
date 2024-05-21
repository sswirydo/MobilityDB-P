/**
  DISCLAIMER: The below code is proof of concept and is not intended for production use.
              It is still in developped in the context of a master thesis.
*/

/**
  DISCLAIMER: The below code is proof of concept and is not intended for production use.
              It is still in developped in the context of a master thesis.
*/

CREATE TYPE pmode;

/*****************************************************************************
 *  PMode
*****************************************************************************/

CREATE FUNCTION pmode_in(cstring)
  RETURNS pmode
  AS 'MODULE_PATHNAME', 'PMode_in'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION pmode_out(pmode)
  RETURNS cstring
  AS 'MODULE_PATHNAME', 'PMode_out'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE TYPE pmode (
    internallength  = 64, --variable, --FIXME
    input           = pmode_in,
    output          = pmode_out,
    alignment       = double
);

CREATE FUNCTION pmode(interval, integer, boolean, tstzspan) 
  RETURNS pmode
  AS 'MODULE_PATHNAME', 'PMode_constructor'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;



/*****************************************************************************
 *  Periodic
*****************************************************************************/


CREATE TYPE pint;

-- CREATE FUNCTION pint_in(cstring, oid, integer)
CREATE FUNCTION pint_in(cstring)
  RETURNS pint
  AS 'MODULE_PATHNAME', 'Periodic_int_in'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION periodic_out(pint)
  RETURNS cstring
  AS 'MODULE_PATHNAME', 'Periodic_out'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;


CREATE TYPE pint (
    internallength  = variable,
    input           = pint_in,
    output          = periodic_out,
    storage         = extended,
    alignment       = double
);



CREATE TYPE pgeompoint;

-- CREATE FUNCTION pgeompoint_in(cstring, oid, integer)
CREATE FUNCTION pgeompoint_in(cstring)
  RETURNS pgeompoint
  AS 'MODULE_PATHNAME', 'Ppoint_in'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION periodic_out(pgeompoint)
  RETURNS cstring
  AS 'MODULE_PATHNAME', 'Periodic_out'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE TYPE pgeompoint (
  internallength = variable,
  input = pgeompoint_in,
  output = periodic_out,
  storage = extended,
  alignment = double
);

CREATE FUNCTION pgeompointSeq(pgeompoint[], text DEFAULT 'linear',
    lower_inc boolean DEFAULT true, upper_inc boolean DEFAULT true)
  RETURNS pgeompoint
  AS 'MODULE_PATHNAME', 'Psequence_constructor'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;



/*****************************************************************************
 *  Casts
*****************************************************************************/

-- temp solution so we can avoid copying all foo from temporal to periodic
-- todo make periodic as a flag later

CREATE FUNCTION tgeompoint(pgeompoint)
  RETURNS tgeompoint
  AS 'MODULE_PATHNAME', 'Periodic_to_temporal'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION pgeompoint(tgeompoint)
  RETURNS pgeompoint
  AS 'MODULE_PATHNAME', 'Temporal_to_periodic'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE CAST (tgeompoint AS pgeompoint) WITH FUNCTION pgeompoint(tgeompoint);
CREATE CAST (pgeompoint AS tgeompoint) WITH FUNCTION tgeompoint(pgeompoint);

CREATE FUNCTION tint(pint)
  RETURNS tint
  AS 'MODULE_PATHNAME', 'Periodic_to_temporal'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION pint(tint)
  RETURNS pint
  AS 'MODULE_PATHNAME', 'Temporal_to_periodic'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE CAST (pint AS tint) WITH FUNCTION tint(pint);
CREATE CAST (tint AS pint) WITH FUNCTION pint(tint);



/*****************************************************************************
 * Periodic type / flags
*****************************************************************************/

CREATE FUNCTION periodicType(pint)
  RETURNS text
  AS 'MODULE_PATHNAME', 'Periodic_get_type'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION setPeriodicType(pint, text)
  RETURNS pint
  AS 'MODULE_PATHNAME', 'Periodic_set_type'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION periodicType(pgeompoint)
  RETURNS text
  AS 'MODULE_PATHNAME', 'Periodic_get_type'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION setPeriodicType(pgeompoint, text)
  RETURNS pgeompoint
  AS 'MODULE_PATHNAME', 'Periodic_set_type'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;





/*****************************************************************************
 *  Periodicity
*****************************************************************************/


CREATE FUNCTION anchor(pint, pmode)
  RETURNS tint
  AS 'MODULE_PATHNAME', 'Anchor'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION anchor(pgeompoint, pmode)
  RETURNS tgeompoint
  AS 'MODULE_PATHNAME', 'Anchor'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

-- CREATE FUNCTION anchor(pint, pmode, timestamptz)
--   RETURNS tint
--   AS 'MODULE_PATHNAME', 'Anchor'
--   LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

-- CREATE FUNCTION anchor(pint, pmode, timestamptz, timestamptz)
--   RETURNS tint
--   AS 'MODULE_PATHNAME', 'Anchor_end'
--   LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

-- CREATE FUNCTION anchor(pint, pmode, timestamptz, timestamptz, boolean)
--   RETURNS tint
--   AS 'MODULE_PATHNAME', 'Anchor_end_inc'
--   LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;




/*****************************************************************************
 *  Other
*****************************************************************************/

CREATE FUNCTION quick_test(timestamptz, text)
  RETURNS text
  AS 'MODULE_PATHNAME', 'Quick_test'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;


