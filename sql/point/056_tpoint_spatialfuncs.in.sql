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

/*
 * tpoint_spatialfuncs.sql
 * Spatial functions for temporal points.
 */

CREATE FUNCTION SRID(stbox)
  RETURNS int
  AS 'MODULE_PATHNAME', 'stbox_srid'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION setSRID(stbox, integer)
  RETURNS stbox
  AS 'MODULE_PATHNAME', 'stbox_set_srid'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION transform(stbox, integer)
  RETURNS stbox
  AS 'MODULE_PATHNAME', 'stbox_transform'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION round(stbox, integer DEFAULT 0)
  RETURNS stbox
  AS 'MODULE_PATHNAME', 'stbox_round'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION round(geometry, integer DEFAULT 0)
  RETURNS geometry
  AS 'MODULE_PATHNAME', 'geo_round'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION round(geography, integer DEFAULT 0)
  RETURNS geography
  AS 'MODULE_PATHNAME', 'geo_round'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

/*****************************************************************************/

CREATE FUNCTION SRID(tgeompoint)
  RETURNS integer
  AS 'MODULE_PATHNAME', 'tpoint_srid'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION SRID(tgeogpoint)
  RETURNS integer
  AS 'MODULE_PATHNAME', 'tpoint_srid'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION setSRID(tgeompoint, integer)
  RETURNS tgeompoint
  AS 'MODULE_PATHNAME', 'tpoint_set_srid'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION setSRID(tgeogpoint, integer)
  RETURNS tgeogpoint
  AS 'MODULE_PATHNAME', 'tpoint_set_srid'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION transform(tgeompoint, integer)
  RETURNS tgeompoint
  AS 'MODULE_PATHNAME', 'tpoint_transform'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

-- Gauss Kruger transformation

CREATE FUNCTION transform_gk(tgeompoint)
  RETURNS tgeompoint
  AS 'MODULE_PATHNAME', 'tgeompoint_transform_gk'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION transform_gk(geometry)
  RETURNS geometry
  AS 'MODULE_PATHNAME', 'geometry_transform_gk'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

/*****************************************************************************/

CREATE FUNCTION tgeogpoint(tgeompoint)
  RETURNS tgeogpoint
  AS 'MODULE_PATHNAME', 'tgeompoint_to_tgeogpoint'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION tgeompoint(tgeogpoint)
  RETURNS tgeompoint
  AS 'MODULE_PATHNAME', 'tgeogpoint_to_tgeompoint'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE CAST (tgeompoint AS tgeogpoint) WITH FUNCTION tgeogpoint(tgeompoint);
CREATE CAST (tgeogpoint AS tgeompoint) WITH FUNCTION tgeompoint(tgeogpoint);

CREATE FUNCTION getX(tgeompoint)
  RETURNS tfloat
  AS 'MODULE_PATHNAME', 'tpoint_get_x'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION getX(tgeogpoint)
  RETURNS tfloat
  AS 'MODULE_PATHNAME', 'tpoint_get_x'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION getY(tgeompoint)
  RETURNS tfloat
  AS 'MODULE_PATHNAME', 'tpoint_get_y'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION getY(tgeogpoint)
  RETURNS tfloat
  AS 'MODULE_PATHNAME', 'tpoint_get_y'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION getZ(tgeompoint)
  RETURNS tfloat
  AS 'MODULE_PATHNAME', 'tpoint_get_z'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION getZ(tgeogpoint)
  RETURNS tfloat
  AS 'MODULE_PATHNAME', 'tpoint_get_z'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION round(tgeompoint, int DEFAULT 0)
  RETURNS tgeompoint
  AS 'MODULE_PATHNAME', 'tpoint_round'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION round(tgeogpoint, int DEFAULT 0)
  RETURNS tgeogpoint
  AS 'MODULE_PATHNAME', 'tpoint_round'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION trajectory(tgeompoint)
  RETURNS geometry
  AS 'MODULE_PATHNAME', 'tpoint_trajectory'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION trajectory(tgeogpoint)
  RETURNS geography
  AS 'MODULE_PATHNAME', 'tpoint_trajectory'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

/*****************************************************************************/

CREATE FUNCTION length(tgeompoint)
  RETURNS float
  AS 'MODULE_PATHNAME', 'tpoint_length'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION length(tgeogpoint)
  RETURNS float
  AS 'MODULE_PATHNAME', 'tpoint_length'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION cumulativeLength(tgeompoint)
  RETURNS tfloat
  AS 'MODULE_PATHNAME', 'tpoint_cumulative_length'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION cumulativeLength(tgeogpoint)
  RETURNS tfloat
  AS 'MODULE_PATHNAME', 'tpoint_cumulative_length'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION speed(tgeompoint)
  RETURNS tfloat
  AS 'MODULE_PATHNAME', 'tpoint_speed'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION speed(tgeogpoint)
  RETURNS tfloat
  AS 'MODULE_PATHNAME', 'tpoint_speed'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION twcentroid(tgeompoint)
  RETURNS geometry
  AS 'MODULE_PATHNAME', 'tpoint_twcentroid'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION azimuth(tgeompoint)
  RETURNS tfloat
  AS 'MODULE_PATHNAME', 'tpoint_azimuth'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION azimuth(tgeogpoint)
  RETURNS tfloat
  AS 'MODULE_PATHNAME', 'tpoint_azimuth'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

/*****************************************************************************/

-- The following two functions are meant to be included in PostGIS one day
CREATE FUNCTION bearing(geometry, geometry)
  RETURNS float
  AS 'MODULE_PATHNAME', 'bearing_geo_geo'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION bearing(geography, geography)
  RETURNS float
  AS 'MODULE_PATHNAME', 'bearing_geo_geo'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION bearing(geometry, tgeompoint)
  RETURNS tfloat
  AS 'MODULE_PATHNAME', 'bearing_geo_tpoint'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION bearing(tgeompoint, geometry)
  RETURNS tfloat
  AS 'MODULE_PATHNAME', 'bearing_tpoint_geo'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION bearing(tgeompoint, tgeompoint)
  RETURNS tfloat
  AS 'MODULE_PATHNAME', 'bearing_tpoint_tpoint'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION bearing(geography, tgeogpoint)
  RETURNS tfloat
  AS 'MODULE_PATHNAME', 'bearing_geo_tpoint'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION bearing(tgeogpoint, geography)
  RETURNS tfloat
  AS 'MODULE_PATHNAME', 'bearing_tpoint_geo'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
-- CREATE FUNCTION bearing(tgeogpoint, tgeogpoint)
  -- RETURNS tfloat
  -- AS 'MODULE_PATHNAME', 'bearing_tpoint_tpoint'
  -- LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

/*****************************************************************************/

CREATE FUNCTION isSimple(tgeompoint)
  RETURNS bool
  AS 'MODULE_PATHNAME', 'tpoint_is_simple'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION makeSimple(tgeompoint)
  RETURNS tgeompoint[]
  AS 'MODULE_PATHNAME', 'tpoint_make_simple'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

/*****************************************************************************/

CREATE FUNCTION atGeometry(tgeompoint, geometry)
  RETURNS tgeompoint
  AS 'MODULE_PATHNAME', 'tpoint_at_geometry'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION minusGeometry(tgeompoint, geometry)
  RETURNS tgeompoint
  AS 'MODULE_PATHNAME', 'tpoint_minus_geometry'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION atStbox(tgeompoint, stbox)
  RETURNS tgeompoint
  AS 'MODULE_PATHNAME', 'tpoint_at_stbox'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION atStbox(tgeogpoint, stbox)
  RETURNS tgeogpoint
  AS 'MODULE_PATHNAME', 'tpoint_at_stbox'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION minusStbox(tgeompoint, stbox)
  RETURNS tgeompoint
  AS 'MODULE_PATHNAME', 'tpoint_minus_stbox'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
CREATE FUNCTION minusStbox(tgeogpoint, stbox)
  RETURNS tgeogpoint
  AS 'MODULE_PATHNAME', 'tpoint_minus_stbox'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

/*****************************************************************************/