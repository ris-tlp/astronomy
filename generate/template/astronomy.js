/*
    MIT License

    Copyright (c) 2019 Don Cross <cosinekitty@gmail.com>

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

/**
 * @fileoverview Astronomy calculation library for browser scripting and Node.js.
 * 
 * @author Don Cross <cosinekitty@gmail.com>
 * @license MIT
 */
'use strict';

/**
 * @name Astronomy
 * @namespace Astronomy
 */
(function(Astronomy){
'use strict';
const j2000 = new Date('2000-01-01T12:00:00Z');
const T0 = 2451545.0;
const MJD_BASIS = 2400000.5;            // mjd + MJD_BASIS = jd
const Y2000_IN_MJD = T0 - MJD_BASIS;    // the 2000.0 epoch expressed in MJD
const PI2 = 2 * Math.PI;
const ARC = 3600 * (180 / Math.PI);     // arcseconds per radian
const ERAD = 6378136.6;                 // mean earth radius
const AU = 1.4959787069098932e+11;      // astronomical unit in meters
const C_AUDAY = 173.1446326846693;      // speed of light in AU/day
const ASEC2RAD = 4.848136811095359935899141e-6;
const DEG2RAD = 0.017453292519943296;
const RAD2DEG = 57.295779513082321;
const ASEC180 = 180 * 60 * 60;              // arcseconds per 180 degrees (or pi radians)
const ASEC360 = 2 * ASEC180;                // arcseconds per 360 degrees (or 2*pi radians)
const ANGVEL = 7.2921150e-5;
const AU_KM = 1.4959787069098932e+8;
const au_per_parsec = ASEC180 / Math.PI;    // exact definition of how many AU = one parsec
const sun_mag_at_1_au = -0.17 - 5*Math.log10(au_per_parsec);    // formula from JPL Horizons
const mean_synodic_month = 29.530588;       // average number of days for Moon to return to the same phase
const seconds_per_day = 24 * 3600;
const millis_per_day = seconds_per_day * 1000;
const solar_days_per_sidereal_day = 0.9972695717592592;
const sun_radius_au   = 4.6505e-3;
const refraction_near_horizon = 34 / 60;        // degrees of refractive "lift" seen for objects near horizon
//const earth_radius_au = 4.2588e-5;
const moon_radius_au = 1.15717e-5;
let ob2000;   // lazy-evaluated mean obliquity of the ecliptic at J2000, in radians
let cos_ob2000;
let sin_ob2000;

//========================================================================================
// BEGIN performance measurement

function PerformanceInfo() {
    this.CallCount = {
        search_func: 0,
        search: 0,
        longitude_search: 0,
        longitude_iter: 0
    };
}

PerformanceInfo.prototype.Clone = function() {
    const clone = new PerformanceInfo();
    for (let tag in this.CallCount) {
        clone.CallCount[tag] = this.CallCount[tag];
    }
    return clone;
}

let Perf = new PerformanceInfo();

Astronomy.GetPerformanceMetrics = function() {
    return Perf.Clone();
}

Astronomy.ResetPerformanceMetrics = function() {
    Perf = new PerformanceInfo();
}

// END performance measurement
//========================================================================================

function Frac(x) {
    return x - Math.floor(x);
}

function AngleBetween(a, b) {
    const aa = (a.x*a.x + a.y*a.y + a.z*a.z);
    if (Math.abs(aa) < 1.0e-8)
        throw `AngleBetween: first vector is too short.`;

    const bb = (b.x*b.x + b.y*b.y + b.z*b.z);
    if (Math.abs(bb) < 1.0e-8)
        throw `AngleBetween: second vector is too short.`;

    const dot = (a.x*b.x + a.y*b.y + a.z*b.z) / Math.sqrt(aa * bb);

    if (dot <= -1.0)
        return 180;

    if (dot >= +1.0)
        return 0;

    const angle = RAD2DEG * Math.acos(dot);
    return angle;
}

/**
 * @constant {string[]} Astronomy.Bodies 
 *      An array of strings, each a name of a supported astronomical body.
 *      Not all bodies are valid for all functions, but any string not in this
 *      list is not supported at all.
 */
Astronomy.Bodies = [
    'Sun',
    'Moon',
    'Mercury',
    'Venus',
    'Earth',
    'Mars',
    'Jupiter',
    'Saturn',
    'Uranus',
    'Neptune',
    'Pluto'
];

const Planet = {
    Mercury: { OrbitalPeriod:    87.969 },
    Venus:   { OrbitalPeriod:   224.701 },
    Earth:   { OrbitalPeriod:   365.256 },
    Mars:    { OrbitalPeriod:   686.980 },
    Jupiter: { OrbitalPeriod:  4332.589 },
    Saturn:  { OrbitalPeriod: 10759.22  },
    Uranus:  { OrbitalPeriod: 30685.4   },
    Neptune: { OrbitalPeriod: 60189.0   },
    Pluto:   { OrbitalPeriod: 90560.0   }
};

const vsop = {
    Mercury: $ASTRO_JS_VSOP(Mercury),
    Venus:   $ASTRO_JS_VSOP(Venus),
    Earth:   $ASTRO_JS_VSOP(Earth),
    Mars:    $ASTRO_JS_VSOP(Mars),
    Jupiter: $ASTRO_JS_VSOP(Jupiter),
    Saturn:  $ASTRO_JS_VSOP(Saturn),
    Uranus:  $ASTRO_JS_VSOP(Uranus),
    Neptune: $ASTRO_JS_VSOP(Neptune)
};

const cheb = {
    Pluto:  $ASTRO_JS_CHEBYSHEV(8)
};

const DT = $ASTRO_JS_DELTA_T();

function DeltaT(mjd) {
    // Calculate the difference TT-UT for the given date/time, expressed
    // as a Modified Julian Date.
    // DT[i] = { mjd: 58484.0, dt: 69.34 }
    // Check end ranges. If outside the known bounds, clamp to the closest known value.

    if (mjd <= DT[0].mjd) {
        return DT[0].dt;
    }

    if (mjd >= DT[DT.length-1].mjd) {
        return DT[DT.length-1].dt;
    }

    // Do a binary search to find the pair of indexes this mjd lies between.
    
    let lo = 0;
    let hi = DT.length-2;   // make sure there is always an array element after the one we are looking at
    while (lo <= hi) {
        let c = (lo + hi) >> 1;
        if (mjd < DT[c].mjd) {
            hi = c-1;
        } else if (mjd > DT[c+1].mjd) {
            lo = c+1;
        } else {
            let frac = (mjd - DT[c].mjd) / (DT[c+1].mjd - DT[c].mjd);
            return DT[c].dt + frac*(DT[c+1].dt - DT[c].dt);
        }
    }

    // This should never happen if the binary search algorithm is correct.
    throw `Could not find Delta-T value for MJD=${mjd}`;
}

function TerrestrialTime(ut) {
    return ut + DeltaT(ut + Y2000_IN_MJD)/86400;
}

/**
 * The date and time of an astronomical observation.
 * Objects of this type are used throughout the internals
 * of the Astronomy library, and are included in certain return objects.
 * The constructor is not accessible outside the Astronomy library;
 * outside users should call the {@link Astronomy.MakeTime} function
 * to create a <code>Time</code> object.
 * 
 * @class
 * 
 * @constructor
 * 
 * @memberof Astronomy
 * 
 * @param {(Date|number)} date
 *      A JavaScript Date object or a numeric UTC value expressed in J2000 days.
 * 
 * @property {Date} date  
 *      The JavaScript Date object for the given date and time.
 *      This Date corresponds to the numeric day value stored in the <code>ut</code> property.
 * 
 * @property {number} ut  
 *      Universal Time (UT1/UTC) in fractional days since the J2000 epoch.
 *      Universal Time represents time measured with respect to the Earth's rotation,
 *      tracking mean solar days.
 *      The Astronomy library approximates UT1 and UTC as being the same thing.
 *      This gives sufficient accuracy for the 1-arcminute angular resolution requirement
 *      of this project.
 * 
 * @property {number} tt
 *      Terrestrial Time in fractional days since the J2000 epoch.
 *      TT represents a continuously flowing ephemeris timescale independent of
 *      any variations of the Earth's rotation, and is adjusted from UT
 *      using historical and predictive models of those variations.
 */
function Time(date) {
    const MillisPerDay = 1000 * 3600 * 24;

    if (date instanceof Date) {
        this.date = date;
        this.ut = (date - j2000) / MillisPerDay;
        this.tt = TerrestrialTime(this.ut);
        return;
    }

    if (typeof date === 'number') {
        this.date = new Date(j2000 - (-date)*MillisPerDay);
        this.ut = date;
        this.tt = TerrestrialTime(this.ut);
        return;
    }

    throw 'AstroTime() argument must be a Date object, a Time object, or a numeric UTC Julian date.';
}

/**
 * Formats a <code>Time</code> object as an 
 * <a href="https://en.wikipedia.org/wiki/ISO_8601">ISO 8601</a>
 * date/time string in UTC, to millisecond resolution.
 * Example: 
 * <pre>
 * <code>2018-08-17T17:22:04.050Z</code>
 * </pre>
 * @returns {string}
 */
Time.prototype.toString = function() {
    return this.date.toISOString();
}

/**
 * Returns a new <code>Time</code> object adjusted by the floating point number of days.
 * Does NOT modify the original <code>Time</code> object.
 * @returns {Astronomy.Time}
 */
Time.prototype.AddDays = function(days) {
    // This is slightly wrong, but the error is tiny.
    // We really should be adding to TT, not to UT.
    // But using TT would require creating an inverse function for DeltaT,
    // which would be quite a bit of extra calculation.
    // I estimate the error is in practice on the order of 10^(-7)
    // times the value of 'days'.
    // This is based on a typical drift of 1 second per year between UT and TT.
    return new Time(this.ut + days);
}

function InterpolateTime(time1, time2, fraction) {
    return new Time(time1.ut + fraction*(time2.ut - time1.ut));
}

function AstroTime(date) {
    if (date instanceof Time) {
        return date;
    }
    return new Time(date);
}

/**
 * Given a Date object or a number days since noon (12:00) on January 1, 2000 (UTC),
 * this function creates an {@link Astronomy.Time} object.
 * Given an {@link Astronomy.Time} object, returns the same object unmodified.
 * Use of this function is not required for any of the other exposed functions in this library,
 * because they all guarantee converting date/time parameters to Astronomy.Time
 * as needed. However, it may be convenient for callers who need to understand
 * the difference between UTC and TT (Terrestrial Time). In some use cases,
 * converting once to Astronomy.Time format and passing the result into multiple
 * function calls may be more efficient than passing in native JavaScript Date objects.
 * 
 * @param {(Date | number | Astronomy.Time)} date
 *      A Date object, a number of UTC days since the J2000 epoch (noon on January 1, 2000),
 *      or an Astronomy.Time object. See remarks above.
 * 
 * @returns {Astronomy.Time}
 */
Astronomy.MakeTime = function(date) {
    return AstroTime(date);
}

var nals_t = [
    [ 0,    0,    0,    0,    1],
    [ 0,    0,    2,   -2,    2],
    [ 0,    0,    2,    0,    2],
    [ 0,    0,    0,    0,    2],
    [ 0,    1,    0,    0,    0],
    [ 0,    1,    2,   -2,    2],
    [ 1,    0,    0,    0,    0],
    [ 0,    0,    2,    0,    1],
    [ 1,    0,    2,    0,    2],
    [ 0,   -1,    2,   -2,    2],
    [ 0,    0,    2,   -2,    1],
    [-1,    0,    2,    0,    2],
    [-1,    0,    0,    2,    0],
    [ 1,    0,    0,    0,    1],
    [-1,    0,    0,    0,    1],
    [-1,    0,    2,    2,    2],
    [ 1,    0,    2,    0,    1],
    [-2,    0,    2,    0,    1],
    [ 0,    0,    0,    2,    0],
    [ 0,    0,    2,    2,    2],
    [ 0,   -2,    2,   -2,    2],
    [-2,    0,    0,    2,    0],
    [ 2,    0,    2,    0,    2],
    [ 1,    0,    2,   -2,    2],
    [-1,    0,    2,    0,    1],
    [ 2,    0,    0,    0,    0],
    [ 0,    0,    2,    0,    0],
    [ 0,    1,    0,    0,    1],
    [-1,    0,    0,    2,    1],
    [ 0,    2,    2,   -2,    2],
    [ 0,    0,   -2,    2,    0],
    [ 1,    0,    0,   -2,    1],
    [ 0,   -1,    0,    0,    1],
    [-1,    0,    2,    2,    1],
    [ 0,    2,    0,    0,    0],
    [ 1,    0,    2,    2,    2],
    [-2,    0,    2,    0,    0],
    [ 0,    1,    2,    0,    2],
    [ 0,    0,    2,    2,    1],
    [ 0,   -1,    2,    0,    2],
    [ 0,    0,    0,    2,    1],
    [ 1,    0,    2,   -2,    1],
    [ 2,    0,    2,   -2,    2],
    [-2,    0,    0,    2,    1],
    [ 2,    0,    2,    0,    1],
    [ 0,   -1,    2,   -2,    1],
    [ 0,    0,    0,   -2,    1],
    [-1,   -1,    0,    2,    0],
    [ 2,    0,    0,   -2,    1],
    [ 1,    0,    0,    2,    0],
    [ 0,    1,    2,   -2,    1],
    [ 1,   -1,    0,    0,    0],
    [-2,    0,    2,    0,    2],
    [ 3,    0,    2,    0,    2],
    [ 0,   -1,    0,    2,    0],
    [ 1,   -1,    2,    0,    2],
    [ 0,    0,    0,    1,    0],
    [-1,   -1,    2,    2,    2],
    [-1,    0,    2,    0,    0],
    [ 0,   -1,    2,    2,    2],
    [-2,    0,    0,    0,    1],
    [ 1,    1,    2,    0,    2],
    [ 2,    0,    0,    0,    1],
    [-1,    1,    0,    1,    0],
    [ 1,    1,    0,    0,    0],
    [ 1,    0,    2,    0,    0],
    [-1,    0,    2,   -2,    1],
    [ 1,    0,    0,    0,    2],
    [-1,    0,    0,    1,    0],
    [ 0,    0,    2,    1,    2],
    [-1,    0,    2,    4,    2],
    [-1,    1,    0,    1,    1],
    [ 0,   -2,    2,   -2,    1],
    [ 1,    0,    2,    2,    1],
    [-2,    0,    2,    2,    2],
    [-1,    0,    0,    0,    2],
    [ 1,    1,    2,   -2,    2]
];

var cls_t = [
    [-172064161.0, -174666.0,  33386.0, 92052331.0,  9086.0, 15377.0],
    [ -13170906.0,   -1675.0, -13696.0,  5730336.0, -3015.0, -4587.0],
    [  -2276413.0,    -234.0,   2796.0,   978459.0,  -485.0,  1374.0],
    [   2074554.0,     207.0,   -698.0,  -897492.0,   470.0,  -291.0],
    [   1475877.0,   -3633.0,  11817.0,    73871.0,  -184.0, -1924.0],
    [   -516821.0,    1226.0,   -524.0,   224386.0,  -677.0,  -174.0],
    [    711159.0,      73.0,   -872.0,    -6750.0,     0.0,   358.0],
    [   -387298.0,    -367.0,    380.0,   200728.0,    18.0,   318.0],
    [   -301461.0,     -36.0,    816.0,   129025.0,   -63.0,   367.0],
    [    215829.0,    -494.0,    111.0,   -95929.0,   299.0,   132.0],
    [    128227.0,     137.0,    181.0,   -68982.0,    -9.0,    39.0],
    [    123457.0,      11.0,     19.0,   -53311.0,    32.0,    -4.0],
    [    156994.0,      10.0,   -168.0,    -1235.0,     0.0,    82.0],
    [     63110.0,      63.0,     27.0,   -33228.0,     0.0,    -9.0],
    [    -57976.0,     -63.0,   -189.0,    31429.0,     0.0,   -75.0],
    [    -59641.0,     -11.0,    149.0,    25543.0,   -11.0,    66.0],
    [    -51613.0,     -42.0,    129.0,    26366.0,     0.0,    78.0],
    [     45893.0,      50.0,     31.0,   -24236.0,   -10.0,    20.0],
    [     63384.0,      11.0,   -150.0,    -1220.0,     0.0,    29.0],
    [    -38571.0,      -1.0,    158.0,    16452.0,   -11.0,    68.0],
    [     32481.0,       0.0,      0.0,   -13870.0,     0.0,     0.0],
    [    -47722.0,       0.0,    -18.0,      477.0,     0.0,   -25.0],
    [    -31046.0,      -1.0,    131.0,    13238.0,   -11.0,    59.0],
    [     28593.0,       0.0,     -1.0,   -12338.0,    10.0,    -3.0],
    [     20441.0,      21.0,     10.0,   -10758.0,     0.0,    -3.0],
    [     29243.0,       0.0,    -74.0,     -609.0,     0.0,    13.0],
    [     25887.0,       0.0,    -66.0,     -550.0,     0.0,    11.0],
    [    -14053.0,     -25.0,     79.0,     8551.0,    -2.0,   -45.0],
    [     15164.0,      10.0,     11.0,    -8001.0,     0.0,    -1.0],
    [    -15794.0,      72.0,    -16.0,     6850.0,   -42.0,    -5.0],
    [     21783.0,       0.0,     13.0,     -167.0,     0.0,    13.0],
    [    -12873.0,     -10.0,    -37.0,     6953.0,     0.0,   -14.0],
    [    -12654.0,      11.0,     63.0,     6415.0,     0.0,    26.0],
    [    -10204.0,       0.0,     25.0,     5222.0,     0.0,    15.0],
    [     16707.0,     -85.0,    -10.0,      168.0,    -1.0,    10.0],
    [     -7691.0,       0.0,     44.0,     3268.0,     0.0,    19.0],
    [    -11024.0,       0.0,    -14.0,      104.0,     0.0,     2.0],
    [      7566.0,     -21.0,    -11.0,    -3250.0,     0.0,    -5.0],
    [     -6637.0,     -11.0,     25.0,     3353.0,     0.0,    14.0],
    [     -7141.0,      21.0,      8.0,     3070.0,     0.0,     4.0],
    [     -6302.0,     -11.0,      2.0,     3272.0,     0.0,     4.0],
    [      5800.0,      10.0,      2.0,    -3045.0,     0.0,    -1.0],
    [      6443.0,       0.0,     -7.0,    -2768.0,     0.0,    -4.0],
    [     -5774.0,     -11.0,    -15.0,     3041.0,     0.0,    -5.0],
    [     -5350.0,       0.0,     21.0,     2695.0,     0.0,    12.0],
    [     -4752.0,     -11.0,     -3.0,     2719.0,     0.0,    -3.0],
    [     -4940.0,     -11.0,    -21.0,     2720.0,     0.0,    -9.0],
    [      7350.0,       0.0,     -8.0,      -51.0,     0.0,     4.0],
    [      4065.0,       0.0,      6.0,    -2206.0,     0.0,     1.0],
    [      6579.0,       0.0,    -24.0,     -199.0,     0.0,     2.0],
    [      3579.0,       0.0,      5.0,    -1900.0,     0.0,     1.0],
    [      4725.0,       0.0,     -6.0,      -41.0,     0.0,     3.0],
    [     -3075.0,       0.0,     -2.0,     1313.0,     0.0,    -1.0],
    [     -2904.0,       0.0,     15.0,     1233.0,     0.0,     7.0],
    [      4348.0,       0.0,    -10.0,      -81.0,     0.0,     2.0],
    [     -2878.0,       0.0,      8.0,     1232.0,     0.0,     4.0],
    [     -4230.0,       0.0,      5.0,      -20.0,     0.0,    -2.0],
    [     -2819.0,       0.0,      7.0,     1207.0,     0.0,     3.0],
    [     -4056.0,       0.0,      5.0,       40.0,     0.0,    -2.0],
    [     -2647.0,       0.0,     11.0,     1129.0,     0.0,     5.0],
    [     -2294.0,       0.0,    -10.0,     1266.0,     0.0,    -4.0],
    [      2481.0,       0.0,     -7.0,    -1062.0,     0.0,    -3.0],
    [      2179.0,       0.0,     -2.0,    -1129.0,     0.0,    -2.0],
    [      3276.0,       0.0,      1.0,       -9.0,     0.0,     0.0],
    [     -3389.0,       0.0,      5.0,       35.0,     0.0,    -2.0],
    [      3339.0,       0.0,    -13.0,     -107.0,     0.0,     1.0],
    [     -1987.0,       0.0,     -6.0,     1073.0,     0.0,    -2.0],
    [     -1981.0,       0.0,      0.0,      854.0,     0.0,     0.0],
    [      4026.0,       0.0,   -353.0,     -553.0,     0.0,  -139.0],
    [      1660.0,       0.0,     -5.0,     -710.0,     0.0,    -2.0],
    [     -1521.0,       0.0,      9.0,      647.0,     0.0,     4.0],
    [      1314.0,       0.0,      0.0,     -700.0,     0.0,     0.0],
    [     -1283.0,       0.0,      0.0,      672.0,     0.0,     0.0],
    [     -1331.0,       0.0,      8.0,      663.0,     0.0,     4.0],
    [      1383.0,       0.0,     -2.0,     -594.0,     0.0,    -2.0],
    [      1405.0,       0.0,      4.0,     -610.0,     0.0,     2.0],
    [      1290.0,       0.0,      0.0,     -556.0,     0.0,     0.0]
];

function iau2000b(time) {
    var i, t, el, elp, f, d, om, arg, dp, de, sarg, carg;

    function mod(x) {
        return (x % ASEC360) * ASEC2RAD;
    }

    t = time.tt / 36525;
    el  = mod(485868.249036 + t * 1717915923.2178);
    elp = mod(1287104.79305 + t * 129596581.0481);
    f   = mod(335779.526232 + t * 1739527262.8478);
    d   = mod(1072260.70369 + t * 1602961601.2090);
    om  = mod(450160.398036 - t * 6962890.5431);
    dp = 0;
    de = 0;
    for (i=76; i >= 0; --i) {
        arg = (nals_t[i][0]*el + nals_t[i][1]*elp + nals_t[i][2]*f + nals_t[i][3]*d + nals_t[i][4]*om) % PI2;
        sarg = Math.sin(arg);
        carg = Math.cos(arg);
        dp += (cls_t[i][0] + cls_t[i][1] * t) * sarg + cls_t[i][2] * carg;
        de += (cls_t[i][3] + cls_t[i][4] * t) * carg + cls_t[i][5] * sarg;
    }
    return {
        dpsi: (-0.000135 * ASEC2RAD) + (dp * 1.0e-7 * ASEC2RAD),
        deps: (+0.000388 * ASEC2RAD) + (de * 1.0e-7 * ASEC2RAD)
    };
 }

function nutation_angles(time) {
    var nut = iau2000b(time);
    return { dpsi: nut.dpsi/ASEC2RAD, deps: nut.deps/ASEC2RAD };
}

function mean_obliq(time) {
    var t = time.tt / 36525;
    return (
        (((( -  0.0000000434   * t
             -  0.000000576  ) * t
             +  0.00200340   ) * t
             -  0.0001831    ) * t
             - 46.836769     ) * t + 84381.406
    );
}

var cache_e_tilt;

function e_tilt(time) {
    if (!cache_e_tilt || Math.abs(cache_e_tilt.tt - time.tt) > 1.0e-6) {
        const nut = nutation_angles(time);
        const mean_obl_seconds = mean_obliq(time);
        const mean_ob = mean_obl_seconds / 3600;
        const true_ob = (mean_obl_seconds + nut.deps) / 3600;
        cache_e_tilt = {
            tt: time.tt,
            dpsi: nut.dpsi,
            deps: nut.deps,
            ee: nut.dpsi * Math.cos(mean_ob * DEG2RAD) / 15,
            mobl: mean_ob,
            tobl: true_ob
        };
    }
    return cache_e_tilt;
}

function ecl2equ_vec(time, pos) {
    var obl = e_tilt(time).mobl * DEG2RAD;
    var cos_obl = Math.cos(obl);
    var sin_obl = Math.sin(obl);
    return [
        pos[0],
        pos[1]*cos_obl - pos[2]*sin_obl,
        pos[1]*sin_obl + pos[2]*cos_obl
    ];
}

function CalcMoon(time) {
    const T = time.tt / 36525;

    function DeclareArray1(xmin, xmax) {
        var array = [];
        var i;
        for (i=0; i <= xmax-xmin; ++i) {
            array.push(0);
        }
        return {min:xmin, array:array};
    }

    function DeclareArray2(xmin, xmax, ymin, ymax) {
        var array = [];
        var i;
        for (i=0; i <= xmax-xmin; ++i) {
            array.push(DeclareArray1(ymin, ymax));
        }
        return {min:xmin, array:array};
    }

    function ArrayGet2(a, x, y) {
        var m = a.array[x - a.min];
        return m.array[y - m.min];
    }

    function ArraySet2(a, x, y, v) {
        var m = a.array[x - a.min];
        m.array[y - m.min] = v;
    }

    var S, MAX, ARG, FAC, I, J, T2, DGAM, DLAM, N, GAM1C, SINPI, L0, L, LS, F, D, DL0, DL, DLS, DF, DD, DS;
    var coArray = DeclareArray2(-6, 6, 1, 4);
    var siArray = DeclareArray2(-6, 6, 1, 4);

    function CO(x, y) {
        return ArrayGet2(coArray, x, y);
    }

    function SI(x, y) {
        return ArrayGet2(siArray, x, y);
    }

    function SetCO(x, y, v) {
        return ArraySet2(coArray, x, y, v);
    }

    function SetSI(x, y, v) {
        return ArraySet2(siArray, x, y, v);
    }

    function AddThe(c1, s1, c2, s2, func) {
        return func(c1*c2 - s1*s2, s1*c2 + c1*s2);
    }

    function Sine(phi) {
        return Math.sin(PI2 * phi);
    }

    T2 = T*T;
    DLAM = 0; 
    DS = 0; 
    GAM1C = 0; 
    SINPI = 3422.7000;

    var S1 = Sine(0.19833+0.05611*T); 
    var S2 = Sine(0.27869+0.04508*T);
    var S3 = Sine(0.16827-0.36903*T); 
    var S4 = Sine(0.34734-5.37261*T);
    var S5 = Sine(0.10498-5.37899*T); 
    var S6 = Sine(0.42681-0.41855*T);
    var S7 = Sine(0.14943-5.37511*T);
    DL0 = 0.84*S1+0.31*S2+14.27*S3+ 7.26*S4+ 0.28*S5+0.24*S6;
    DL  = 2.94*S1+0.31*S2+14.27*S3+ 9.34*S4+ 1.12*S5+0.83*S6;
    DLS =-6.40*S1                                   -1.89*S6;
    DF  = 0.21*S1+0.31*S2+14.27*S3-88.70*S4-15.30*S5+0.24*S6-1.86*S7;
    DD  = DL0-DLS;
    DGAM  = (-3332E-9 * Sine(0.59734-5.37261*T)
              -539E-9 * Sine(0.35498-5.37899*T)
               -64E-9 * Sine(0.39943-5.37511*T));

    L0 = PI2*Frac(0.60643382+1336.85522467*T-0.00000313*T2) + DL0/ARC;
    L  = PI2*Frac(0.37489701+1325.55240982*T+0.00002565*T2) + DL /ARC;
    LS = PI2*Frac(0.99312619+  99.99735956*T-0.00000044*T2) + DLS/ARC;
    F  = PI2*Frac(0.25909118+1342.22782980*T-0.00000892*T2) + DF /ARC;
    D  = PI2*Frac(0.82736186+1236.85308708*T-0.00000397*T2) + DD /ARC;
    for (I=1; I<=4; ++I)
    {
        switch (I)
        {
            case 1: ARG=L;  MAX=4; FAC=1.000002208;               break;
            case 2: ARG=LS; MAX=3; FAC=0.997504612-0.002495388*T; break;
            case 3: ARG=F;  MAX=4; FAC=1.000002708+139.978*DGAM;  break;
            case 4: ARG=D;  MAX=6; FAC=1.0;                       break;
        }
        SetCO(0, 1, 1);
        SetCO(1, I, Math.cos(ARG) * FAC);
        SetSI(0, I, 0);
        SetSI(1, I, Math.sin(ARG) * FAC);
        for (J=2; J<=MAX; ++J) {
            AddThe(CO(J-1,I), SI(J-1,I), CO(1,I), SI(1,I), (c, s) => (SetCO(J,I,c), SetSI(J,I,s)));
        }
        for (J=1; J<=MAX; ++J) {
            SetCO(-J, I, CO(J, I));
            SetSI(-J, I, -SI(J, I));
        }
    }

    function Term(p, q, r, s) {
        var result = { x:1, y:0 };
        var I = [ null, p, q, r, s ];
        for (var k=1; k <= 4; ++k)
            if (I[k] !== 0) 
                AddThe(result.x, result.y, CO(I[k], k), SI(I[k], k), (c, s) => (result.x=c, result.y=s));
        return result;
    }

    function AddSol(coeffl, coeffs, coeffg, coeffp, p, q, r, s) {
        var result = Term(p, q, r, s);
        DLAM += coeffl * result.y;
        DS += coeffs * result.y;
        GAM1C += coeffg * result.x;
        SINPI += coeffp * result.x;
    }

    AddSol(    13.902,   14.06,-0.001,   0.2607,0, 0, 0, 4);
    AddSol(     0.403,   -4.01,+0.394,   0.0023,0, 0, 0, 3);
    AddSol(  2369.912, 2373.36,+0.601,  28.2333,0, 0, 0, 2);
    AddSol(  -125.154, -112.79,-0.725,  -0.9781,0, 0, 0, 1);
    AddSol(     1.979,    6.98,-0.445,   0.0433,1, 0, 0, 4);
    AddSol(   191.953,  192.72,+0.029,   3.0861,1, 0, 0, 2);
    AddSol(    -8.466,  -13.51,+0.455,  -0.1093,1, 0, 0, 1);
    AddSol( 22639.500,22609.07,+0.079, 186.5398,1, 0, 0, 0);
    AddSol(    18.609,    3.59,-0.094,   0.0118,1, 0, 0,-1);
    AddSol( -4586.465,-4578.13,-0.077,  34.3117,1, 0, 0,-2);
    AddSol(    +3.215,    5.44,+0.192,  -0.0386,1, 0, 0,-3);
    AddSol(   -38.428,  -38.64,+0.001,   0.6008,1, 0, 0,-4);
    AddSol(    -0.393,   -1.43,-0.092,   0.0086,1, 0, 0,-6);
    AddSol(    -0.289,   -1.59,+0.123,  -0.0053,0, 1, 0, 4);
    AddSol(   -24.420,  -25.10,+0.040,  -0.3000,0, 1, 0, 2);
    AddSol(    18.023,   17.93,+0.007,   0.1494,0, 1, 0, 1);
    AddSol(  -668.146, -126.98,-1.302,  -0.3997,0, 1, 0, 0);
    AddSol(     0.560,    0.32,-0.001,  -0.0037,0, 1, 0,-1);
    AddSol(  -165.145, -165.06,+0.054,   1.9178,0, 1, 0,-2);
    AddSol(    -1.877,   -6.46,-0.416,   0.0339,0, 1, 0,-4);
    AddSol(     0.213,    1.02,-0.074,   0.0054,2, 0, 0, 4);
    AddSol(    14.387,   14.78,-0.017,   0.2833,2, 0, 0, 2);
    AddSol(    -0.586,   -1.20,+0.054,  -0.0100,2, 0, 0, 1);
    AddSol(   769.016,  767.96,+0.107,  10.1657,2, 0, 0, 0);
    AddSol(    +1.750,    2.01,-0.018,   0.0155,2, 0, 0,-1);
    AddSol(  -211.656, -152.53,+5.679,  -0.3039,2, 0, 0,-2);
    AddSol(    +1.225,    0.91,-0.030,  -0.0088,2, 0, 0,-3);
    AddSol(   -30.773,  -34.07,-0.308,   0.3722,2, 0, 0,-4);
    AddSol(    -0.570,   -1.40,-0.074,   0.0109,2, 0, 0,-6);
    AddSol(    -2.921,  -11.75,+0.787,  -0.0484,1, 1, 0, 2);
    AddSol(    +1.267,    1.52,-0.022,   0.0164,1, 1, 0, 1);
    AddSol(  -109.673, -115.18,+0.461,  -0.9490,1, 1, 0, 0);
    AddSol(  -205.962, -182.36,+2.056,  +1.4437,1, 1, 0,-2);
    AddSol(     0.233,    0.36, 0.012,  -0.0025,1, 1, 0,-3);
    AddSol(    -4.391,   -9.66,-0.471,   0.0673,1, 1, 0,-4);

    AddSol(     0.283,    1.53,-0.111,  +0.0060,1,-1, 0,+4);
    AddSol(    14.577,   31.70,-1.540,  +0.2302,1,-1, 0, 2);
    AddSol(   147.687,  138.76,+0.679,  +1.1528,1,-1, 0, 0);
    AddSol(    -1.089,    0.55,+0.021,   0.0   ,1,-1, 0,-1);
    AddSol(    28.475,   23.59,-0.443,  -0.2257,1,-1, 0,-2);
    AddSol(    -0.276,   -0.38,-0.006,  -0.0036,1,-1, 0,-3);
    AddSol(     0.636,    2.27,+0.146,  -0.0102,1,-1, 0,-4);
    AddSol(    -0.189,   -1.68,+0.131,  -0.0028,0, 2, 0, 2);
    AddSol(    -7.486,   -0.66,-0.037,  -0.0086,0, 2, 0, 0);
    AddSol(    -8.096,  -16.35,-0.740,   0.0918,0, 2, 0,-2);
    AddSol(    -5.741,   -0.04, 0.0  ,  -0.0009,0, 0, 2, 2);
    AddSol(     0.255,    0.0 , 0.0  ,   0.0   ,0, 0, 2, 1);
    AddSol(  -411.608,   -0.20, 0.0  ,  -0.0124,0, 0, 2, 0);
    AddSol(     0.584,    0.84, 0.0  ,  +0.0071,0, 0, 2,-1);
    AddSol(   -55.173,  -52.14, 0.0  ,  -0.1052,0, 0, 2,-2);
    AddSol(     0.254,    0.25, 0.0  ,  -0.0017,0, 0, 2,-3);
    AddSol(    +0.025,   -1.67, 0.0  ,  +0.0031,0, 0, 2,-4);
    AddSol(     1.060,    2.96,-0.166,   0.0243,3, 0, 0,+2);
    AddSol(    36.124,   50.64,-1.300,   0.6215,3, 0, 0, 0);
    AddSol(   -13.193,  -16.40,+0.258,  -0.1187,3, 0, 0,-2);
    AddSol(    -1.187,   -0.74,+0.042,   0.0074,3, 0, 0,-4);
    AddSol(    -0.293,   -0.31,-0.002,   0.0046,3, 0, 0,-6);
    AddSol(    -0.290,   -1.45,+0.116,  -0.0051,2, 1, 0, 2);
    AddSol(    -7.649,  -10.56,+0.259,  -0.1038,2, 1, 0, 0);
    AddSol(    -8.627,   -7.59,+0.078,  -0.0192,2, 1, 0,-2);
    AddSol(    -2.740,   -2.54,+0.022,   0.0324,2, 1, 0,-4);
    AddSol(     1.181,    3.32,-0.212,   0.0213,2,-1, 0,+2);
    AddSol(     9.703,   11.67,-0.151,   0.1268,2,-1, 0, 0);
    AddSol(    -0.352,   -0.37,+0.001,  -0.0028,2,-1, 0,-1);
    AddSol(    -2.494,   -1.17,-0.003,  -0.0017,2,-1, 0,-2);
    AddSol(     0.360,    0.20,-0.012,  -0.0043,2,-1, 0,-4);
    AddSol(    -1.167,   -1.25,+0.008,  -0.0106,1, 2, 0, 0);
    AddSol(    -7.412,   -6.12,+0.117,   0.0484,1, 2, 0,-2);
    AddSol(    -0.311,   -0.65,-0.032,   0.0044,1, 2, 0,-4);
    AddSol(    +0.757,    1.82,-0.105,   0.0112,1,-2, 0, 2);
    AddSol(    +2.580,    2.32,+0.027,   0.0196,1,-2, 0, 0);
    AddSol(    +2.533,    2.40,-0.014,  -0.0212,1,-2, 0,-2);
    AddSol(    -0.344,   -0.57,-0.025,  +0.0036,0, 3, 0,-2);
    AddSol(    -0.992,   -0.02, 0.0  ,   0.0   ,1, 0, 2, 2);
    AddSol(   -45.099,   -0.02, 0.0  ,  -0.0010,1, 0, 2, 0);
    AddSol(    -0.179,   -9.52, 0.0  ,  -0.0833,1, 0, 2,-2);
    AddSol(    -0.301,   -0.33, 0.0  ,   0.0014,1, 0, 2,-4);
    AddSol(    -6.382,   -3.37, 0.0  ,  -0.0481,1, 0,-2, 2);
    AddSol(    39.528,   85.13, 0.0  ,  -0.7136,1, 0,-2, 0);
    AddSol(     9.366,    0.71, 0.0  ,  -0.0112,1, 0,-2,-2);
    AddSol(     0.202,    0.02, 0.0  ,   0.0   ,1, 0,-2,-4);

    AddSol(     0.415,    0.10, 0.0  ,  0.0013,0, 1, 2, 0);
    AddSol(    -2.152,   -2.26, 0.0  , -0.0066,0, 1, 2,-2);
    AddSol(    -1.440,   -1.30, 0.0  , +0.0014,0, 1,-2, 2);
    AddSol(     0.384,   -0.04, 0.0  ,  0.0   ,0, 1,-2,-2);
    AddSol(    +1.938,   +3.60,-0.145, +0.0401,4, 0, 0, 0);
    AddSol(    -0.952,   -1.58,+0.052, -0.0130,4, 0, 0,-2);
    AddSol(    -0.551,   -0.94,+0.032, -0.0097,3, 1, 0, 0);
    AddSol(    -0.482,   -0.57,+0.005, -0.0045,3, 1, 0,-2);
    AddSol(     0.681,    0.96,-0.026,  0.0115,3,-1, 0, 0);
    AddSol(    -0.297,   -0.27, 0.002, -0.0009,2, 2, 0,-2);
    AddSol(     0.254,   +0.21,-0.003,  0.0   ,2,-2, 0,-2);
    AddSol(    -0.250,   -0.22, 0.004,  0.0014,1, 3, 0,-2);
    AddSol(    -3.996,    0.0 , 0.0  , +0.0004,2, 0, 2, 0);
    AddSol(     0.557,   -0.75, 0.0  , -0.0090,2, 0, 2,-2);
    AddSol(    -0.459,   -0.38, 0.0  , -0.0053,2, 0,-2, 2);
    AddSol(    -1.298,    0.74, 0.0  , +0.0004,2, 0,-2, 0);
    AddSol(     0.538,    1.14, 0.0  , -0.0141,2, 0,-2,-2);
    AddSol(     0.263,    0.02, 0.0  ,  0.0   ,1, 1, 2, 0);
    AddSol(     0.426,   +0.07, 0.0  , -0.0006,1, 1,-2,-2);
    AddSol(    -0.304,   +0.03, 0.0  , +0.0003,1,-1, 2, 0);
    AddSol(    -0.372,   -0.19, 0.0  , -0.0027,1,-1,-2, 2);
    AddSol(    +0.418,    0.0 , 0.0  ,  0.0   ,0, 0, 4, 0);
    AddSol(    -0.330,   -0.04, 0.0  ,  0.0   ,3, 0, 2, 0);

    function ADDN(coeffn, p, q, r, s) {
        return coeffn * Term(p, q, r, s).y;
    }

    N = 0;
    N += ADDN(-526.069, 0, 0,1,-2); 
    N += ADDN(  -3.352, 0, 0,1,-4);
    N += ADDN( +44.297,+1, 0,1,-2); 
    N += ADDN(  -6.000,+1, 0,1,-4);
    N += ADDN( +20.599,-1, 0,1, 0); 
    N += ADDN( -30.598,-1, 0,1,-2);
    N += ADDN( -24.649,-2, 0,1, 0); 
    N += ADDN(  -2.000,-2, 0,1,-2);
    N += ADDN( -22.571, 0,+1,1,-2); 
    N += ADDN( +10.985, 0,-1,1,-2);

    DLAM += (
        +0.82*Sine(0.7736  -62.5512*T)+0.31*Sine(0.0466 -125.1025*T)
        +0.35*Sine(0.5785  -25.1042*T)+0.66*Sine(0.4591+1335.8075*T)
        +0.64*Sine(0.3130  -91.5680*T)+1.14*Sine(0.1480+1331.2898*T)
        +0.21*Sine(0.5918+1056.5859*T)+0.44*Sine(0.5784+1322.8595*T)
        +0.24*Sine(0.2275   -5.7374*T)+0.28*Sine(0.2965   +2.6929*T)
        +0.33*Sine(0.3132   +6.3368*T)
    );

    S = F + DS/ARC;

    var lat_seconds = (1.000002708 + 139.978*DGAM)*(18518.511+1.189+GAM1C)*Math.sin(S) - 6.24*Math.sin(3*S) + N;

    return {
        geo_eclip_lon: PI2 * Frac((L0+DLAM/ARC) / PI2),
        geo_eclip_lat: (Math.PI / (180 * 3600)) * lat_seconds,
        distance_au: (ARC * (ERAD / AU)) / (0.999953253 * SINPI)
    };
}

function precession(tt1, pos1, tt2) {
    var xx, yx, zx, xy, yy, zy, xz, yz, zz;
    var eps0 = 84381.406;
    var t, psia, omegaa, chia, sa, ca, sb, cb, sc, cc, sd, cd;

    if ((tt1 !== 0) && (tt2 !== 0))
        throw 'One of (tt1, tt2) must be 0.';

    t = (tt2 - tt1) / 36525;
    if (tt2 === 0)
        t = -t;

    psia   = (((((-    0.0000000951  * t
                 +    0.000132851 ) * t
                 -    0.00114045  ) * t
                 -    1.0790069   ) * t
                 + 5038.481507    ) * t);

    omegaa = (((((+    0.0000003337  * t
                 -    0.000000467 ) * t
                 -    0.00772503  ) * t
                 +    0.0512623   ) * t
                 -    0.025754    ) * t + eps0);

    chia   = (((((-    0.0000000560  * t
                 +    0.000170663 ) * t
                 -    0.00121197  ) * t
                 -    2.3814292   ) * t
                 +   10.556403    ) * t);

    eps0 = eps0 * ASEC2RAD;
    psia = psia * ASEC2RAD;
    omegaa = omegaa * ASEC2RAD;
    chia = chia * ASEC2RAD;

    sa = Math.sin(eps0);
    ca = Math.cos(eps0);
    sb = Math.sin(-psia);
    cb = Math.cos(-psia);
    sc = Math.sin(-omegaa);
    cc = Math.cos(-omegaa);
    sd = Math.sin(chia);
    cd = Math.cos(chia);

    xx =  cd * cb - sb * sd * cc;
    yx =  cd * sb * ca + sd * cc * cb * ca - sa * sd * sc;
    zx =  cd * sb * sa + sd * cc * cb * sa + ca * sd * sc;
    xy = -sd * cb - sb * cd * cc;
    yy = -sd * sb * ca + cd * cc * cb * ca - sa * cd * sc;
    zy = -sd * sb * sa + cd * cc * cb * sa + ca * cd * sc;
    xz =  sb * sc;
    yz = -sc * cb * ca - sa * cc;
    zz = -sc * cb * sa + cc * ca;

    if (tt2 == 0)
    { 
        // Perform rotation from epoch to J2000.0.
        return [
            xx * pos1[0] + xy * pos1[1] + xz * pos1[2],
            yx * pos1[0] + yy * pos1[1] + yz * pos1[2],
            zx * pos1[0] + zy * pos1[1] + zz * pos1[2]
        ];
    }

    // Perform rotation from J2000.0 to epoch.
    return [
        xx * pos1[0] + yx * pos1[1] + zx * pos1[2],
        xy * pos1[0] + yy * pos1[1] + zy * pos1[2],
        xz * pos1[0] + yz * pos1[1] + zz * pos1[2]
    ]; 
}

function era(time) {    // Earth Rotation Angle
    const thet1 = 0.7790572732640 + 0.00273781191135448 * time.ut;
    const thet3 = time.ut % 1;
    let theta = 360 * ((thet1 + thet3) % 1);
    if (theta < 0) {
        theta += 360;
    }
    return theta;
}

function sidereal_time(time) {          // calculates Greenwich Apparent Sidereal Time (GAST)
    const t = time.tt / 36525;
    let eqeq = 15 * e_tilt(time).ee;    // Replace with eqeq=0 to get GMST instead of GAST (if we ever need it) 
    const theta = era(time);
    const st = (eqeq + 0.014506 +
        (((( -    0.0000000368   * t
            -    0.000029956  ) * t
            -    0.00000044   ) * t
            +    1.3915817    ) * t
            + 4612.156534     ) * t);

    let gst = ((st/3600 + theta) % 360) / 15;
    if (gst < 0) {
        gst += 24;
    }
    return gst;
}

function terra(observer, st) {
    const erad_km = ERAD / 1000;
    const df = 1 - 0.003352819697896;    // flattening of the Earth
    const df2 = df * df;
    const phi = observer.latitude * DEG2RAD;
    const sinphi = Math.sin(phi);
    const cosphi = Math.cos(phi);
    const c = 1 / Math.sqrt(cosphi*cosphi + df2*sinphi*sinphi);
    const s = df2 * c;
    const ht_km = observer.height / 1000;
    const ach = erad_km*c + ht_km;
    const ash = erad_km*s + ht_km;
    const stlocl = (15*st + observer.longitude) * DEG2RAD;
    const sinst = Math.sin(stlocl);
    const cosst = Math.cos(stlocl);
    return {
        pos: [ach*cosphi*cosst/AU_KM, ach*cosphi*sinst/AU_KM, ash*sinphi/AU_KM],
        vel: [-ANGVEL*ach*cosphi*sinst*86400, ANGVEL*ach*cosphi*cosst*86400, 0]
    };
}

function nutation(time, direction, pos) {
    const tilt = e_tilt(time);
    const oblm = tilt.mobl * DEG2RAD;
    const oblt = tilt.tobl * DEG2RAD;
    const psi = tilt.dpsi * ASEC2RAD;
    const cobm = Math.cos(oblm);
    const sobm = Math.sin(oblm);
    const cobt = Math.cos(oblt);
    const sobt = Math.sin(oblt);
    const cpsi = Math.cos(psi);
    const spsi = Math.sin(psi);

    const xx = cpsi;
    const yx = -spsi * cobm;
    const zx = -spsi * sobm;
    const xy = spsi * cobt;
    const yy = cpsi * cobm * cobt + sobm * sobt;
    const zy = cpsi * sobm * cobt - cobm * sobt;
    const xz = spsi * sobt;
    const yz = cpsi * cobm * sobt - sobm * cobt;
    const zz = cpsi * sobm * sobt + cobm * cobt; 

    if (direction === 0) {
        // forward rotation
        return [
            xx * pos[0] + yx * pos[1] + zx * pos[2],
            xy * pos[0] + yy * pos[1] + zy * pos[2],
            xz * pos[0] + yz * pos[1] + zz * pos[2]
        ];
    }

    // inverse rotation
    return [
        xx * pos[0] + xy * pos[1] + xz * pos[2],
        yx * pos[0] + yy * pos[1] + yz * pos[2],
        zx * pos[0] + zy * pos[1] + zz * pos[2]
    ];
}

function geo_pos(time, observer) {
    const gast = sidereal_time(time);
    const pos1 = terra(observer, gast).pos;
    const pos2 = nutation(time, -1, pos1);
    const pos3 = precession(time.tt, pos2, 0);
    return pos3;
}

function vector2radec(pos)
{
    const xyproj = pos[0]*pos[0] + pos[1]*pos[1];
    const dist = Math.sqrt(xyproj + pos[2]*pos[2]);
    if (xyproj === 0)
    {
        if (pos[2] === 0)
            throw 'Indeterminate sky coordinates';

        if (pos[2] < 0)
            return { ra:0, dec:-90, dist:dist };

        return { ra:0, dec:+90, dist:dist };
    }

    let ra = Math.atan2(pos[1], pos[0]) / (DEG2RAD * 15);
    if (ra < 0) {
        ra += 24;
    }
    let dec = Math.atan2(pos[2], Math.sqrt(xyproj)) / DEG2RAD;
    return { ra:ra, dec:dec, dist:dist };
}

function spin(angle, pos1) {
    const angr = angle * DEG2RAD;
    const cosang = Math.cos(angr);
    const sinang = Math.sin(angr);
    const xx = cosang;
    const yx = sinang;
    const zx = 0;
    const xy = -sinang;
    const yy = cosang;
    const zy = 0;
    const xz = 0;
    const yz = 0;
    const zz = 1;
    let pos2 = [
        xx*pos1[0] + yx*pos1[1] + zx*pos1[2],
        xy*pos1[0] + yy*pos1[1] + zy*pos1[2],
        xz*pos1[0] + yz*pos1[1] + zz*pos1[2]
    ];
    return pos2;
}

function ter2cel(time, vec1) {
    const gast = sidereal_time(time);
    let vec2 = spin(-15 * gast, vec1);
    return vec2;
}

function refract(zd_obs, location) {
    if (zd_obs < 0.1 || zd_obs > 91.0) {
        return 0.0;
    }
    let pressure = 1010 * Math.exp(-location.height / 9100);
    let celsius = 10;
    let kelvin = 273 + celsius;
    let h = 90 - zd_obs;
    let angle = (h + 7.31/(h + 4.4));
    let r = 0.016667 / Math.tan(angle * DEG2RAD);
    let refr = r * (0.28 * pressure / kelvin);
    return refr;
}

/**
 * Given a date and time, a geographic location of an observer on the Earth, and
 * equatorial coordinates (right ascension and declination) of a celestial object,
 * returns horizontal coordinates (azimuth and altitude angles) for that object
 * as seen by that observer.
 * 
 * @param {(Date|Number|Astronomy.Time)} date
 *      The date and time for which to find horizontal coordinates.
 * 
 * @param {Astronomy.Observer} location
 *      The location of the observer for which to find horizontal coordinates.
 * 
 * @param {Number} ra
 *      Right ascension in sidereal hours of the celestial object, 
 *      referred to the mean equinox of date for the J2000 epoch.
 * 
 * @param {Number} dec
 *      Declination in degrees of the celestial object, 
 *      referred to the mean equator of date for the J2000 epoch.
 *      Positive values are north of the celestial equator and negative values are south.
 * 
 * @returns {Astronomy.HorizontalCoordinates}
 */
Astronomy.Horizon = function(date, location, ra, dec, refraction) {     // based on NOVAS equ2hor()
    let time = AstroTime(date);

    const sinlat = Math.sin(location.latitude * DEG2RAD);
    const coslat = Math.cos(location.latitude * DEG2RAD);
    const sinlon = Math.sin(location.longitude * DEG2RAD);
    const coslon = Math.cos(location.longitude * DEG2RAD);
    const sindc = Math.sin(dec * DEG2RAD);
    const cosdc = Math.cos(dec * DEG2RAD);
    const sinra = Math.sin(ra * 15 * DEG2RAD);
    const cosra = Math.cos(ra * 15 * DEG2RAD);
    let uze = [coslat*coslon, coslat*sinlon, sinlat];
    let une = [-sinlat*coslon, -sinlat*sinlon, coslat];
    let uwe = [sinlon, -coslon, 0];

    let uz = ter2cel(time, uze);
    let un = ter2cel(time, une);
    let uw = ter2cel(time, uwe);

    let p = [cosdc*cosra, cosdc*sinra, sindc];

    const pz = p[0]*uz[0] + p[1]*uz[1] + p[2]*uz[2];
    const pn = p[0]*un[0] + p[1]*un[1] + p[2]*un[2];
    const pw = p[0]*uw[0] + p[1]*uw[1] + p[2]*uw[2];

    let proj = Math.sqrt(pn*pn + pw*pw);
    let az = 0;
    if (proj > 0) {
        az = -Math.atan2(pw, pn) * RAD2DEG;
        if (az < 0) az += 360;
        if (az >= 360) az -= 360;
    }
    let zd = Math.atan2(proj, pz) * RAD2DEG;
    let out_ra = ra;
    let out_dec = dec;

    if (refraction) {
        let zd1, refr, j;
        let zd0 = zd;

        if (refraction === 'novas') {
            do {
                zd1 = zd;
                refr = refract(zd, location);
                zd = zd0 - refr;
            } while (Math.abs(zd - zd1) > 3.0e-5);
        } else if (refraction === 'normal' || refraction === 'jplhor') {
            // http://extras.springer.com/1999/978-1-4471-0555-8/chap4/horizons/horizons.pdf
            // JPL Horizons says it uses refraction algorithm from 
            // Meeus "Astronomical Algorithms", 1991, p. 101-102.
            // I found the following Go implementation:
            // https://github.com/soniakeys/meeus/blob/master/v3/refraction/refract.go
            // This is a translation from the function "Saemundsson" there.
            // I found experimentally that JPL Horizons clamps the angle to 1 degree below the horizon.
            // This is important because the 'refr' formula below goes crazy near hd = -5.11.
            let hd = Math.max(-1, 90 - zd);
            refr = (1.02 / Math.tan((hd+10.3/(hd+5.11))*DEG2RAD)) / 60;

            if (refraction === 'normal' && zd > 91) {
                // In "normal" mode we gradually reduce refraction toward the nadir
                // so that we never get an altitude angle less than -90 degrees.
                // When horizon angle is -1 degrees, zd = 91, and the factor is exactly 1.
                // As zd approaches 180 (the nadir), the fraction approaches 0 linearly.
                refr *= (180 - zd) / 89;
            }

            zd -= refr;
        } else {
            throw 'If specified, refraction must be one of: "normal", "jplhor", "novas".';
        }

        if (refr > 0.0 && zd > 3.0e-4) {
            const sinzd = Math.sin(zd * DEG2RAD);
            const coszd = Math.cos(zd * DEG2RAD);
            const sinzd0 = Math.sin(zd0 * DEG2RAD);
            const coszd0 = Math.cos(zd0 * DEG2RAD);
            var pr = [];
            for (j=0; j<3; ++j) {
                pr.push(((p[j] - coszd0 * uz[j]) / sinzd0)*sinzd + uz[j]*coszd);
            }
            proj = Math.sqrt(pr[0]*pr[0] + pr[1]*pr[1]);
            if (proj > 0) {
                out_ra = Math.atan2(pr[1], pr[0]) * RAD2DEG / 15;
                if (out_ra < 0) {
                    out_ra += 24;
                }
                if (out_ra >= 24) {
                    out_ra -= 24;
                }
            } else {
                out_ra = 0;
            }
            out_dec = Math.atan2(pr[2], proj) * RAD2DEG;
        }
    }

    return { azimuth:az, altitude:90-zd, ra:out_ra, dec:out_dec };
}

/**
 * Represents the geographic location of an observer on the surface of the Earth.
 * 
 * @class
 * @constructor
 * @memberof Astronomy
 * 
 * @property {Number} latitude_degrees 
 *      The observer's geographic latitude in degrees north of the Earth's equator.
 *      The value is negative for observers south of the equator.
 *      Must be in the range -90 to +90.
 * 
 * @property {Number} longitude_degrees
 *      The observer's geographic longitude in degrees east of the prime meridian 
 *      passing through Greenwich, England.
 *      The value is negative for observers west of the prime meridian.
 *      The value should be kept in the range -180 to +180 to minimize floating point errors.
 * 
 * @property {Number} height_in_meters
 *      The observer's elevation above mean sea level, expressed in meters.
 */
function Observer(latitude_degrees, longitude_degrees, height_in_meters) {
    this.latitude = latitude_degrees;
    this.longitude = longitude_degrees;
    this.height = height_in_meters;
}

/**
 * Creates an {@link Astronomy.Observer} object.
 * 
 * @param {Number} latitude_degrees 
 *      The observer's geographic latitude in degrees north of the Earth's equator.
 *      The value is negative for observers south of the equator.
 *      Must be in the range -90 to +90.
 * 
 * @param {Number} longitude_degrees
 *      The observer's geographic longitude in degrees east of the prime meridian 
 *      passing through Greenwich, England.
 *      The value is negative for observers west of the prime meridian.
 *      The value should be kept in the range -180 to +180 to minimize floating point errors.
 * 
 * @param {Number} height_in_meters
 *      The observer's elevation above mean sea level, expressed in meters.
 *      If omitted, the elevation is assumed to be 0 meters.
 */
Astronomy.MakeObserver = function(latitude_degrees, longitude_degrees, height_in_meters) {
    return new Observer(latitude_degrees, longitude_degrees, height_in_meters || 0);
}

/**
 * Returns apparent geocentric true ecliptic coordinates of date for the Sun.
 */
Astronomy.SunPosition = function(date) {
    // Correct for light travel time from the Sun.
    // This is really the same as correcting for aberration.
    // Otherwise season calculations (equinox, solstice) will all be early by about 8 minutes!
    const time = AstroTime(date).AddDays(-1 / C_AUDAY);

    // Get heliocentric cartesian coordinates of Earth in J2000.
    const earth2000 = CalcVsop(vsop.Earth, time);

    // Convert to geocentric location of the Sun.
    const sun2000 = [-earth2000.x, -earth2000.y, -earth2000.z];

    // Convert to equator-of-date equatorial cartesian coordinates.
    const stemp = precession(0, sun2000, time.tt);
    const sun_ofdate = nutation(time, 0, stemp);

    // Convert to ecliptic coordinates of date.
    const true_obliq = DEG2RAD * e_tilt(time).tobl;
    const cos_ob = Math.cos(true_obliq);
    const sin_ob = Math.sin(true_obliq);

    const gx = sun_ofdate[0];
    const gy = sun_ofdate[1];
    const gz = sun_ofdate[2];

    const sun_ecliptic = RotateEquatorialToEcliptic(gx, gy, gz, cos_ob, sin_ob);
    return sun_ecliptic;
}

/**
 * Returns equatorial coordinates (right ascension and declination)
 * in two different systems: J2000 and true-equator-of-date.
 */
Astronomy.SkyPos = function(gc_vector, observer) {     // based on NOVAS place()
    const gc_observer = observer ? geo_pos(gc_vector.t, observer) : [0,0,0];        // vector from geocenter to observer
    const j2000_vector = [
        gc_vector.x - gc_observer[0], 
        gc_vector.y - gc_observer[1], 
        gc_vector.z - gc_observer[2] ];

    let j2000_radec = vector2radec(j2000_vector);

    let pos7 = precession(0, j2000_vector, gc_vector.t.tt);
    let ofdate_vector = nutation(gc_vector.t, 0, pos7);
    let ofdate_radec = vector2radec(ofdate_vector);

    let sky = {
        t: gc_vector.t,
        j2000: j2000_radec,
        ofdate: ofdate_radec
    }
    return sky;
}

function RotateEquatorialToEcliptic(gx, gy, gz, cos_ob, sin_ob) {
    // Rotate equatorial vector to obtain ecliptic vector.
    const ex =  gx;
    const ey =  gy*cos_ob + gz*sin_ob;
    const ez = -gy*sin_ob + gz*cos_ob;

    const xyproj = Math.sqrt(ex*ex + ey*ey);
    let elon = 0;
    if (xyproj > 0) {
        elon = RAD2DEG * Math.atan2(ey, ex);
        if (elon < 0) elon += 360;
    }
    let elat = RAD2DEG * Math.atan2(ez, xyproj);

    return { ex:ex, ey:ey, ez:ez, elat:elat, elon:elon };
}

/**
 * Given J2000 equatorial Cartesian coordinates, 
 * returns J2000 ecliptic latitude, longitude, and cartesian coordinates.
 * You can call {@link Astronomy.GeoVector} and use its (x, y, z) return values
 * to pass into this function.
 */
Astronomy.Ecliptic = function(gx, gy, gz) {
    // Based on NOVAS functions equ2ecl() and equ2ecl_vec().
    if (ob2000 === undefined) {
        // Lazy-evaluate and keep the mean obliquity of the ecliptic at J2000.
        // This way we don't need to crunch the numbers more than once.
        ob2000 = DEG2RAD * e_tilt(AstroTime(j2000)).mobl;
        cos_ob2000 = Math.cos(ob2000);
        sin_ob2000 = Math.sin(ob2000);
    }    

    return RotateEquatorialToEcliptic(gx, gy, gz, cos_ob2000, sin_ob2000);
}

/**
 * Calculates the geocentric Cartesian coordinates for the Moon in the J2000 equatorial system.
 * Based on the Nautical Almanac Office's <i>Improved Lunar Ephemeris</i> of 1954,
 * which in turn derives from E. W. Brown's lunar theories.
 * Adapted from Turbo Pascal code from the book 
 * <a href="https://www.springer.com/us/book/9783540672210">Astronomy on the Personal Computer</a> 
 * by Montenbruck and Pfleger.
 * 
 * @param {Date|Number|Astronomy.Time} date
 *      The date and time for which to calculate the Moon's geocentric position.
 * 
 * @returns {Astronomy.Vector}
 */
Astronomy.GeoMoon = function(date) {
    var time = AstroTime(date);
    var moon = CalcMoon(time);

    // Convert geocentric ecliptic spherical coords to cartesian coords.
    var dist_cos_lat = moon.distance_au * Math.cos(moon.geo_eclip_lat);    
    var gepos = [
        dist_cos_lat * Math.cos(moon.geo_eclip_lon),
        dist_cos_lat * Math.sin(moon.geo_eclip_lon),
        moon.distance_au * Math.sin(moon.geo_eclip_lat)
    ];

    // Convert ecliptic coordinates to equatorial coordinates, both in mean equinox of date.
    var mpos1 = ecl2equ_vec(time, gepos);

    // Convert from mean equinox of date to J2000...
    var mpos2 = precession(time.tt, mpos1, 0);

    return { t:time, x:mpos2[0], y:mpos2[1], z:mpos2[2] };
}

function CalcVsop(model, time) {
    var spher = [], eclip, r_coslat;
    var t = time.tt / 365250;   // millennia since 2000
    var formula, series, term, tpower, sum, coord;
    for (formula of model) {
        tpower = 1;
        coord = 0;
        for (series of formula) {
            sum = 0;
            for (term of series) {
                sum += term[0] * Math.cos(term[1] + (t * term[2]));
            }
            coord += tpower * sum;
            tpower *= t;
        }
        spher.push(coord);
    }

    // Convert spherical coordinates to ecliptic cartesian coordinates.
    r_coslat = spher[2] * Math.cos(spher[1]);
    eclip = [
        r_coslat * Math.cos(spher[0]),
        r_coslat * Math.sin(spher[0]),
        spher[2] * Math.sin(spher[1])
    ];

    // Convert ecliptic cartesian coordinates to equatorial cartesian coordinates.
    return {
        t: time,
        x: eclip[0] + 0.000000440360*eclip[1] - 0.000000190919*eclip[2],
        y: -0.000000479966*eclip[0] + 0.917482137087*eclip[1] - 0.397776982902*eclip[2],
        z: 0.397776982902*eclip[1] + 0.917482137087*eclip[2]
    };
}

function ChebScale(t_min, t_max, t) {
    return (2*t - (t_max + t_min)) / (t_max - t_min);
}

function CalcChebyshev(model, time) {
    var record, x, k, d, sum, p0, p1, p2, pos;

    // Search for a record that overlaps the Julian Date 'jd'.
    for (record of model) {
        x = ChebScale(record.tt, record.tt + record.ndays, time.tt);
        if (-1 <= x && x <= +1) {
            pos = [];
            for (d=0; d < 3; ++d) {
                p0 = 1;
                sum = record.coeff[0][d];
                p1 = x;
                sum += record.coeff[1][d] * p1;
                for (k=2; k < record.coeff.length; ++k) {
                    p2 = (2 * x * p1) - p0;
                    sum += record.coeff[k][d] * p2;
                    p0 = p1;
                    p1 = p2;
                }
                pos.push(sum - record.coeff[0][d]/2);
            }
            return { t:time, x:pos[0], y:pos[1], z:pos[2] };
        }
    }
    throw `Cannot extrapolate Chebyshev model for given Terrestrial Time: ${time.tt}`;
}

Astronomy.HelioVector = function(body, date) {
    var time = AstroTime(date);
    if (body in vsop) {
        return CalcVsop(vsop[body], time);
    }
    if (body in cheb) {
        return CalcChebyshev(cheb[body], time);
    }
    if (body === 'Sun') {
        return { t:time, x:0, y:0, z:0 };
    }
    if (body === 'Moon') {
        var e = CalcVsop(vsop.Earth, time);
        var m = Astronomy.GeoMoon(time);
        return { t:time, x:e.x+m.x, y:e.y+m.y, z:e.z+m.z };
    }
    throw `Astronomy.HelioVector: Unknown body "${body}"`;
};

Astronomy.GeoVector = function(body, date) {
    const time = AstroTime(date);
    if (body === 'Moon') {
        return Astronomy.GeoMoon(time);
    }
    if (body === 'Earth') {
        return { t:time, x:0, y:0, z:0 };
    }

    const e = CalcVsop(vsop.Earth, time);

    // Correct for light-travel time, to get position of body as seen from Earth's center.
    let h, geo, ltravel, dt;
    let ltime = time;
    for (let iter=0; iter < 10; ++iter) {
        h = Astronomy.HelioVector(body, ltime);
        geo = { t:time, x:h.x-e.x, y:h.y-e.y, z:h.z-e.z, iter:iter };
        geo.dist = Math.sqrt(geo.x*geo.x + geo.y*geo.y + geo.z*geo.z);
        if (body === 'Sun') {
            return geo;     // The Sun's heliocentric coordinates are always (0,0,0). No need to correct.
        }
        ltravel = geo.dist / C_AUDAY;
        let ltime2 = time.AddDays(-ltravel);
        dt = Math.abs(ltime2.tt - ltime.tt);
        if (dt < 1.0e-9) {
            return geo;
        }
        ltime = ltime2;
    }
    throw `Light-travel time solver did not converge: dt=${dt}`;
}

function QuadInterp(tm, dt, fa, fm, fb) {
    let Q = (fb + fa)/2 - fm;
    let R = (fb - fa)/2;
    let S = fm;
    let x;

    if (Q == 0) {
        // This is a line, not a parabola.
        if (R == 0) {
            // This is a HORIZONTAL line... can't make progress!
            return null;
        }
        x = -S / R;
        if (x < -1 || x > +1) return null;  // out of bounds        
    } else {
        // It really is a parabola. Find roots x1, x2.
        let u = R*R - 4*Q*S;
        if (u <= 0) return null;
        let ru = Math.sqrt(u);
        let x1 = (-R + ru) / (2 * Q);
        let x2 = (-R - ru) / (2 * Q);

        if (-1 <= x1 && x1 <= +1) {
            if (-1 <= x2 && x2 <= +1) return null;
            x = x1;
        } else if (-1 <= x2 && x2 <= +1) {
            x = x2;
        } else {
            return null;
        }
    }

    let t = tm + x*dt;
    let df_dt = (2*Q*x + R) / dt;
    return { x:x, t:t, df_dt:df_dt };
}

/**
 * A continuous function of time used in a call to the <code>Search</code> function.
 * 
 * @callback ContinuousFunction
 * @param {Astronomy.Time} t        The time at which to evaluate the function.
 * @returns {Number}
 */

/**
 * Search for next time <i>t</i> (such that <i>t</i> is between <code>t1</code> and <code>t2</code>)
 * that <code>func(t)</code> crosses from a negative value to a non-negative value.
 * The given function must have "smooth" behavior over the entire inclusive range [<code>t1</code>, <code>t2</code>],
 * meaning that it behaves like a continuous differentiable function.
 * It is not required that <code>t1</code> &lt; <code>t2</code>; <code>t1</code> &gt; <code>t2</code> 
 * allows searching backward in time.
 * Note: <code>t1</code> and <code>t2</code> must be chosen such that there is no possibility
 * of more than one zero-crossing (ascending or descending), or it is possible
 * that the "wrong" event will be found (i.e. not the first event after t1)
 * or even that the function will return null, indicating that no event was found.
 * 
 * @param {ContinuousFunction} func
 * @param {*} t1 
 * @param {*} t2 
 * @param {*} options 
 */
function Search(func, t1, t2, options) {
    const dt_tolerance_seconds = (options && options.dt_tolerance_seconds) || 1;

    function f(t) {
        ++Perf.CallCount.search_func;
        return func(t);
    }

    const dt_days = dt_tolerance_seconds / seconds_per_day;

    ++Perf.CallCount.search;

    let f1 = (options && options.init_f1) || f(t1);
    let f2 = (options && options.init_f2) || f(t2);
    let fmid;

    let iter = 0;
    let iter_limit = (options && options.iter_limit) || 20;
    let calc_fmid = true;
    while (true) {
        if (++iter > iter_limit)
            throw `Excessive iteration in Search()`;

        let tmid = InterpolateTime(t1, t2, 0.5);
        let dt = tmid.ut - t1.ut;

        if (Math.abs(dt) < dt_days) {
            // We are close enough to the event to stop the search.
            return tmid;
        }

        if (calc_fmid)
            fmid = f(tmid);
        else
            calc_fmid = true;   // we already have the correct value of fmid from the previous loop

        // Quadratic interpolation:
        // Try to find a parabola that passes through the 3 points we have sampled:
        // (t1,f1), (tmid,fmid), (t2,f2).
        let q = QuadInterp(tmid.ut, t2.ut - tmid.ut, f1, fmid, f2);

        // Did we find an approximate root-crossing?
        if (q) {
            // Evaluate the function at our candidate solution.
            let tq = AstroTime(q.t);
            let fq = f(tq);

            if (q.df_dt !== 0) {
                if (Math.abs(fq / q.df_dt) < dt_days) {
                    // The estimated time error is small enough that we can quit now.
                    return tq;
                }
    
                // Try guessing a tighter boundary with the interpolated root at the center.
                let dt_guess = 1.2 * Math.abs(fq / q.df_dt);
                if (dt_guess < dt/10) {
                    let tleft = tq.AddDays(-dt_guess);
                    let tright = tq.AddDays(+dt_guess);
                    if ((tleft.ut - t1.ut)*(tleft.ut - t2.ut) < 0) {
                        if ((tright.ut - t1.ut)*(tright.ut - t2.ut) < 0) {
                            let fleft = f(tleft);
                            let fright = f(tright);
                            if (fleft<0 && fright>=0) {
                                f1 = fleft;
                                f2 = fright;
                                t1 = tleft;
                                t2 = tright;
                                fmid = fq;
                                calc_fmid = false;
                                continue;
                            }
                        }
                    }
                }
            }
        }

        if (f1<0 && fmid>=0) {
            t2 = tmid;
            f2 = fmid;
            continue;
        } 
        
        if (fmid<0 && f2>=0) {
            t1 = tmid;
            f1 = fmid;
            continue;
        }

        // Either there is no ascending zero-crossing in this range
        // or the search window is too wide.
        return null;
    }
}

function LongitudeOffset(diff) {
    let offset = diff;
    while (offset <= -180) offset += 360;
    while (offset > 180) offset -= 360;
    return offset;
}

function NormalizeLongitude(lon) {
    while (lon < 0) lon += 360;
    while (lon >= 360) lon -= 360;
    return lon;
}

Astronomy.SearchSunLongitude = function(targetLon, dateStart, limitDays) {
    function sun_offset(t) {
        let pos = Astronomy.SunPosition(t);
        return LongitudeOffset(pos.elon - targetLon);
    }
    let t1 = AstroTime(dateStart);
    let t2 = t1.AddDays(limitDays);
    return Search(sun_offset, t1, t2);
}

Astronomy.LongitudeFromSun = function(body, date) {
    // This function calculates the ecliptic longitude
    // difference between the given body and the Sun 
    // as seen from the Earth at a given moment in time.
    // The returned value ranges [0, 360) degreees.

    if (body === 'Earth')
        throw 'The Earth does not have a longitude as seen from itself.';

    const t = AstroTime(date);    
    let gb = Astronomy.GeoVector(body, t);
    const eb = Astronomy.Ecliptic(gb.x, gb.y, gb.z);

    let gs = Astronomy.GeoVector('Sun', t);
    const es = Astronomy.Ecliptic(gs.x, gs.y, gs.z);

    return NormalizeLongitude(eb.elon - es.elon);
}

Astronomy.AngleFromSun = function(body, date) {
    // This function returns the full angle seen from
    // the Earth, between the given body and the Sun.
    // Unlike LongitudeFromSun, this function does not
    // project the body's "shadow" onto the ecliptic.

    if (body == 'Earth')
        throw 'The Earth does not have an angle as seen from itself.';

    const t = AstroTime(date);
    let sv = Astronomy.GeoVector('Sun', date);
    let bv = Astronomy.GeoVector(body, date);
    let angle = AngleBetween(sv, bv);
    return angle;
}

Astronomy.EclipticLongitude = function(body, date) {    // heliocentric ecliptic longitude in J2000
    // This function calculates the ecliptic longitude of the body
    // as seen from the center of the Sun at the given date.

    let hv = Astronomy.HelioVector(body, date);
    let eclip = Astronomy.Ecliptic(hv.x, hv.y, hv.z);
    return eclip.elon;
}

function VisualMagnitude(body, phase, helio_dist, geo_dist) {
    // For Mercury and Venus, see:  https://iopscience.iop.org/article/10.1086/430212
    let c0, c1=0, c2=0, c3=0;
    switch (body) {
    case 'Mercury':     c0 = -0.60; c1 = +4.98; c2 = -4.88; c3 = +3.02; break;
    case 'Venus':
        if (phase < 163.6) {
            c0 = -4.47; c1 = +1.03; c2 = +0.57; c3 = +0.13;
        } else {
            c0 = 0.98; c1 = -1.02;
        }
        break;
    case 'Mars':        c0 = -1.52; c1 = +1.60;                         break;
    case 'Jupiter':     c0 = -9.40; c1 = +0.50;                         break;
    case 'Uranus':      c0 = -7.19; c1 = +0.25;                         break;
    case 'Neptune':     c0 = -6.87;                                     break;
    case 'Pluto':       c0 = -1.00; c1 = +4.00;                         break;
    default: throw `VisualMagnitude: unsupported body ${body}`;
    }

    const x = phase / 100;
    let mag = c0 + x*(c1 + x*(c2 + x*c3));
    mag += 5*Math.log10(helio_dist * geo_dist);
    return mag;
}

function SaturnMagnitude(phase, helio_dist, geo_dist, gc, time) {
    // Based on formulas by Paul Schlyter found here:
    // http://www.stjarnhimlen.se/comp/ppcomp.html#15

    // We must handle Saturn's rings as a major component of its visual magnitude.
    // Find geocentric ecliptic coordinates of Saturn.
    const eclip = Astronomy.Ecliptic(gc.x, gc.y, gc.z);
    const ir = DEG2RAD * 28.06;   // tilt of Saturn's rings to the ecliptic, in radians
    const Nr = DEG2RAD * (169.51 + (3.82e-5 * time.tt));    // ascending node of Saturn's rings, in radians

    // Find tilt of Saturn's rings, as seen from Earth.
    const lat = DEG2RAD * eclip.elat;
    const lon = DEG2RAD * eclip.elon;
    const tilt = Math.asin(Math.sin(lat)*Math.cos(ir) - Math.cos(lat)*Math.sin(ir)*Math.sin(lon-Nr));
    const sin_tilt = Math.sin(Math.abs(tilt));

    let mag = -9.0 + 0.044*phase;
    mag += sin_tilt*(-2.6 + 1.2*sin_tilt);
    mag += 5*Math.log10(helio_dist * geo_dist);
    return { mag:mag, ring_tilt:RAD2DEG*tilt };
}

function MoonMagnitude(phase, helio_dist, geo_dist) {
    // https://astronomy.stackexchange.com/questions/10246/is-there-a-simple-analytical-formula-for-the-lunar-phase-brightness-curve
    let rad = phase * DEG2RAD;
    let rad2 = rad * rad;
    let rad4 = rad2 * rad2;
    let mag = -12.717 + 1.49*Math.abs(rad) + 0.0431*rad4;

    const moon_mean_distance_au = 385000.6 / AU_KM;
    let geo_au = geo_dist / moon_mean_distance_au;
    mag += 5*Math.log10(helio_dist * geo_au);
    return mag;
}

/**
 * Contains information about the apparent brightness and sunlit phase of a celestial object.
 * 
 * @class
 * @constructor
 * @memberof Astronomy
 * 
 * @property {Astronomy.Time} time 
 *      The date and time pertaining to the other calculated values in this object.
 * 
 * @property {Number} mag 
 *      The <a href="https://en.wikipedia.org/wiki/Apparent_magnitude">apparent visual magnitude</a> of the celestial body.
 * 
 * @property {Number} phase
 *      The angle in degrees as seen from the center of the celestial body between the Sun and the Earth.
 *      The value is always in the range 0 to 180.
 *      The phase angle provides a measure of what fraction of the body's face appears 
 *      illuminated by the Sun as seen from the Earth.
 *      When the observed body is the Sun, the <code>phase</code> property is set to 0,
 *      although this has no physical meaning because the Sun emits, rather than reflects, light.
 *      To calculate the illuminated fraction, use the formula $f = \frac{1}{2} \left( 1 + cos(\phi) \right)$
 *      where $f$ is the illuminated fraction and $\phi$ is the phase angle.
 *      When the phase is near 0 degrees, the body appears "full".
 *      When it is 90 degrees, the body appears "half full". 
 *      And when it is 180 degrees, the body appears "new" and is very difficult to see
 *      because it is both dim and lost in the Sun's glare as seen from the Earth.
 * 
 * @property {Number} helio_dist 
 *      The distance between the center of the Sun and the center of the body in 
 *      <a href="https://en.wikipedia.org/wiki/Astronomical_unit">Astronomical Units</a> (AU).
 * 
 * @property {Number} geo_dist 
 *      The distance between the center of the Earth and the center of the body in AU.
 * 
 * @property {Astronomy.Vector} gc 
 *      Geocentric coordinates: the 3D vector from the center of the Earth to the center of the body.
 *      The components are in expressed in AU and are oriented with respect to the J2000 equatorial plane.
 * 
 * @property {Astronomy.Vector} hc
 *      Heliocentric coordinates: The 3D vector from the center of the Sun to the center of the body.
 *      Like <code>gc</code>, <code>hc</code> is expressed in AU and oriented with respect
 *      to the J2000 equatorial plane.
 * 
 * @property {Number | null} ring_tilt 
 *      For Saturn, this is the angular tilt of the planet's rings in degrees away
 *      from the line of sight from the Earth. When the value is near 0, the rings
 *      appear edge-on from the Earth and are therefore difficult to see.
 *      When <code>ring_tilt</code> approaches its maximum value (about 27 degrees),
 *      the rings appear widest and brightest from the Earth.
 *      Unlike the <a href="https://ssd.jpl.nasa.gov/horizons.cgi">JPL Horizons</a> online tool, 
 *      this library includes the effect of the ring tilt angle in the calculated value 
 *      for Saturn's visual magnitude.
 *      For all bodies other than Saturn, the value of <code>ring_tilt</code> is <code>null</code>.
 */
function IlluminationInfo(time, mag, phase, helio_dist, geo_dist, gc, hc, ring_tilt) {
    this.time = time;
    this.mag = mag;
    this.phase = phase;
    this.helio_dist = helio_dist;
    this.geo_dist = geo_dist;
    this.gc = gc;
    this.hc = hc;
    this.ring_tilt = ring_tilt;
}

/**
 * Calculates the phase angle, visual maginitude, 
 * and other values relating to the body's illumination
 * at the given date and time, as seen from the Earth.
 * 
 * @param {string} body
 *      The name of the celestial body being observed.
 *      Not allowed to be <code>"Earth"</code>.
 * 
 * @param {Date | Number | Astronomy.Time} date
 *      The date and time for which to calculate the illumination data for the given body.
 * 
 * @returns {Astronomy.IlluminationInfo}
 */
Astronomy.Illumination = function(body, date) {
    // Calculates phase angle, distance, and visual maginitude of the body.
    // For the Sun, the phase angle returned is always 0.
    // For other bodies, the phase angle is defined as the
    // angle in degrees as seen by the center of the body
    // of the separation between the centers of the Sun and Earth.

    if (body === 'Earth')
        throw `The illumination of the Earth is not defined.`;

    const time = AstroTime(date);
    const earth = CalcVsop(vsop.Earth, time);
    let phase;
    let hc;         // vector from Sun to body
    let gc;         // vector from Earth to body
    let geo_dist;   // distance from body to center of Earth
    let helio_dist; // distance from body to center of Sun
    let mag;        // visual magnitude

    if (body === 'Sun') {
        gc = { x:(-earth.x), y:(-earth.y), z:(-earth.z) };
        hc = { x:0, y:0, z:0 };
        phase = 0;
    } else {
        if (body === 'Moon') {
            // For extra numeric precision, use geocentric moon formula directly.
            gc = Astronomy.GeoMoon(time);
            hc = { x:(earth.x + gc.x), y:(earth.y + gc.y), z:(earth.z + gc.z) };
        } else {
            // For planets, heliocentric vector is most direct to calculate.
            hc = Astronomy.HelioVector(body, date);
            gc = { x:(hc.x - earth.x), y:(hc.y - earth.y), z:(hc.z - earth.z) };
        }
        phase = AngleBetween(gc, hc);
    }

    geo_dist = Math.sqrt(gc.x*gc.x + gc.y*gc.y + gc.z*gc.z);
    helio_dist = Math.sqrt(hc.x*hc.x + hc.y*hc.y + hc.z*hc.z);
    let ring_tilt = null;   // only reported for Saturn

    if (body === 'Sun') {
        mag = sun_mag_at_1_au + 5*Math.log10(geo_dist);
    } else if (body === 'Moon') {
        mag = MoonMagnitude(phase, helio_dist, geo_dist);
    } else if (body === 'Saturn') {
        const saturn = SaturnMagnitude(phase, helio_dist, geo_dist, gc, time);
        mag = saturn.mag;
        ring_tilt = saturn.ring_tilt;
    } else {
        mag = VisualMagnitude(body, phase, helio_dist, geo_dist);
    }

    return new IlluminationInfo(time, mag, phase, helio_dist, geo_dist, gc, hc, ring_tilt);
}

function SynodicPeriod(body) {
    if (body === 'Earth')
        throw 'The Earth does not have a synodic period as seen from itself.';

    if (body === 'Moon')
        return mean_synodic_month;

    // Calculate the synodic period of the planet from its and the Earth's sidereal periods.
    // The sidereal period of a planet is how long it takes to go around the Sun in days, on average.
    // The synodic period of a planet is how long it takes between consecutive oppositions
    // or conjunctions, on average.

    let planet = Planet[body];
    if (!planet)
        throw `Not a valid planet name: ${body}`;

    // See here for explanation of the formula:
    // https://en.wikipedia.org/wiki/Elongation_(astronomy)#Elongation_period

    const Te = Planet.Earth.OrbitalPeriod;
    const Tp = planet.OrbitalPeriod;
    const synodicPeriod = Math.abs(Te / (Te/Tp - 1));

    return synodicPeriod;
}

Astronomy.SearchRelativeLongitude = function(body, targetRelLon, startDate) {
    const planet = Planet[body];
    if (!planet)
        throw `Cannot search relative longitude because body is not a planet: ${body}`;

    if (body === 'Earth')
        throw 'Cannot search relative longitude for the Earth (it is always 0)';

    // Determine whether the Earth "gains" (+1) on the planet or "loses" (-1)
    // as both race around the Sun.
    const direction = (planet.OrbitalPeriod > Planet.Earth.OrbitalPeriod) ? +1 : -1;

    function offset(t) {
        const plon = Astronomy.EclipticLongitude(body, t);
        const elon = Astronomy.EclipticLongitude('Earth', t);
        const diff = direction * (elon - plon);
        return LongitudeOffset(diff - targetRelLon);
    }

    let syn = SynodicPeriod(body);
    let time = AstroTime(startDate);

    // Iterate until we converge on the desired event.
    // Calculate the error angle, which will be a negative number of degrees,
    // meaning we are "behind" the target elongation in time.
    let error_angle = offset(time);
    if (error_angle > 0) error_angle -= 360;    // force searching forward in time

    ++Perf.CallCount.longitude_search;

    for (let iter=0; iter < 100; ++iter) {
        ++Perf.CallCount.longitude_iter;

        // Estimate how many days in the future (positive) or past (negative)
        // we have to go to get closer to the target elongation.
        let day_adjust = (-error_angle/360) * syn;
        time = time.AddDays(day_adjust);
        if (Math.abs(day_adjust) * seconds_per_day < 1)
            return time;

        let prev_angle = error_angle;
        error_angle = offset(time);

        if (Math.abs(prev_angle) < 30) {
            // Improve convergence for Mercury/Mars (eccentric orbits) 
            // by adjusting the synodic period to more closely match the
            // variable speed of both planets in this part of their respective orbits.
            if (prev_angle !== error_angle) {
                let ratio = prev_angle / (prev_angle - error_angle);
                if (ratio > 0.5 && ratio < 2.0)
                    syn *= ratio;
            }
        }
    }

    throw `Relative longitude search failed to converge for ${body} near ${time.toString()} (error_angle = ${error_angle}).`;
}

Astronomy.MoonPhase = function(date) {
    // This function helps us determine the moon's phase.
    // It returns a value anywhere from 0 to 360 indicating the difference
    // in ecliptic longitude between the center of the Sun and the
    // center of the Moon, as seen from the center of the Earth.
    // Examples of what certain return values mean:
    //   0 = new moon
    //  90 = first quarter
    // 180 = full moon
    // 270 = third quarter.

    return Astronomy.LongitudeFromSun('Moon', date);
}

Astronomy.SearchMoonPhase = function(targetLon, dateStart, limitDays) {
    function moon_offset(t) {
        let mlon = Astronomy.MoonPhase(t);
        return LongitudeOffset(mlon - targetLon);
    }

    // To avoid discontinuities in the moon_offset function causing problems,
    // we need to approximate when that function will next return 0.
    // We probe it with the start time and take advantage of the fact
    // that every lunar phase repeats roughly every 29.5 days.
    // There is a surprising uncertainty in the quarter timing,
    // due to the eccentricity of the moon's orbit.
    // I have seen up to 0.826 days away from the simple prediction.
    // To be safe, we take the predicted time of the event and search
    // +/-0.9 days around it (a 1.8-day wide window).
    // But we must return null if the final result goes beyond limitDays after dateStart.
    const uncertainty = 0.9;

    let ta = AstroTime(dateStart);
    let ya = moon_offset(ta);
    if (ya > 0) ya -= 360;  // force searching forward in time, not backward
    let est_dt = -(mean_synodic_month*ya)/360;
    let dt1 = est_dt - uncertainty;
    if (dt1 > limitDays) return null;   // not possible for moon phase to occur within the specified window
    let dt2 = Math.min(limitDays, est_dt + uncertainty);
    let t1 = ta.AddDays(dt1);
    let t2 = ta.AddDays(dt2);
    return Search(moon_offset, t1, t2);
}

Astronomy.SearchMoonQuarter = function(dateStart) {
    // Determine what the next quarter phase will be.
    let phaseStart = Astronomy.MoonPhase(dateStart);
    let quarterStart = Math.floor(phaseStart / 90);
    let quarter = (quarterStart + 1) % 4;
    let time = Astronomy.SearchMoonPhase(90 * quarter, dateStart, 10);
    return time && { quarter:quarter, time:time };
}

Astronomy.NextMoonQuarter = function(mq) {
    // Skip 6 days past the previous found moon quarter to find the next one.
    // This is less than the minimum possible increment.
    // So far I have seen the interval well contained by the range (6.5, 8.3) days.
    let date = new Date(mq.time.date.getTime() + 6*millis_per_day);
    return Astronomy.SearchMoonQuarter(date);
}

Astronomy.SearchRiseSet = function(body, observer, direction, dateStart, limitDays) {
    // We calculate the apparent angular radius of the Sun and Moon, 
    // but treat all other bodies as points.
    let body_radius_au = { Sun:sun_radius_au, Moon:moon_radius_au }[body] || 0;

    function peak_altitude(t) {
        // Return the angular altitude above or below the horizon
        // of the highest part (the peak) of the given object.
        // This is defined as the apparent altitude of the center of the body plus
        // the body's angular radius.
        // The 'direction' variable in the enclosing function controls
        // whether the angle is measured positive above the horizon or
        // positive below the horizon, depending on whether the caller
        // wants rise times or set times, respectively.

        const pos = Astronomy.GeoVector(body, t);
        const sky = Astronomy.SkyPos(pos, observer);
        const hor = Astronomy.Horizon(t, observer, sky.ofdate.ra, sky.ofdate.dec);
        
        const alt = hor.altitude + RAD2DEG*(body_radius_au / sky.ofdate.dist) + refraction_near_horizon;
        return direction * alt;
    }

    // See if object is currently above/below the horizon.
    // If we are looking for next rise time and the object is below the horizon,
    // we use the current time as the lower time bound and the next culmination
    // as the upper bound.
    // If the object is above the horizon, we search for the next bottom and use it
    // as the lower bound and the next culmination after that bottom as the upper bound.
    // The same logic applies for finding set times, only we swap the hour angles.
    // The peak_altitude() function already considers the 'direction' parameter.

    let ha_before, ha_after;
    if (direction === +1) {
        ha_before = 12;     // reaching the minimum altitude (bottom) comes BEFORE the object rises.
        ha_after = 0;       // reaching the maximum altitude (culmination) comes AFTER the object rises.
    } else if (direction === -1) {
        ha_before = 0;      // reaching culmination comes BEFORE the object sets.
        ha_after = 12;      // reaching bottom comes AFTER the object sets.
    } else {
        throw `Astronomy.SearchRiseSet: Invalid direction parameter ${direction} -- must be +1 or -1`;
    }

    let time_start = AstroTime(dateStart);
    let time_before;
    let evt_before, evt_after;
    let alt_before = peak_altitude(time_start);
    let alt_after;
    if (alt_before > 0) {
        // We are past the sought event, so we have to wait for the next "before" event (culm/bottom).
        evt_before = Astronomy.SearchHourAngle(body, observer, ha_before, time_start);
        time_before = evt_before.time;
        alt_before = peak_altitude(time_before);
    } else {
        // We are before or at the sought event, so we find the next "after" event (bottom/culm),
        // and use the current time as the "before" event.
        time_before = time_start;
    }
    evt_after = Astronomy.SearchHourAngle(body, observer, ha_after, time_before);
    alt_after = peak_altitude(evt_after.time);

    let iter = 0;
    while (true) {
        ++iter;

        if (alt_before <= 0 && alt_after > 0) {
            // Search between evt_before and evt_after for the desired event.
            let tx = Search(peak_altitude, time_before, evt_after.time, {init_f1:alt_before, init_f2:alt_after});
            if (tx) 
                return tx;
        }

        // If we didn't find the desired event, use time_after to find the next before-event.
        evt_before = Astronomy.SearchHourAngle(body, observer, ha_before, evt_after.time);
        evt_after = Astronomy.SearchHourAngle(body, observer, ha_after, evt_before.time);
        if (evt_before.time.ut >= time_start.ut + limitDays)
            return null;

        time_before = evt_before.time;
        alt_before = peak_altitude(evt_before.time);
        alt_after = peak_altitude(evt_after.time);
    }
}

Astronomy.SearchHourAngle = function(body, observer, hourAngle, dateStart) {
    // Find the next time the given body is seen to reach the specified hour angle
    // by the specified observer.
    // Providing hourAngle=0 finds the next maximum altitude event (culmination).
    // Providing hourAngle=12 finds the next minimum altitude event.
    let time = AstroTime(dateStart);
    let iter = 0;

    while (true) {
        ++iter;

        // Calculate Greenwich Apparent Sidereal Time (GAST) at the given time.
        let gast = sidereal_time(time);

        // Find the equator-of-date (RA,DEC) for the body.
        let pos = Astronomy.GeoVector(body, time);
        let sky = Astronomy.SkyPos(pos, observer);

        // Calculate the adjustment needed in sidereal time to bring
        // the hour angle to the desired value.
        let delta_sidereal_hours = ((hourAngle + sky.ofdate.ra - observer.longitude/15) - gast) % 24;
        if (iter === 1) {
            // On the first iteration, always search forward in time.
            if (delta_sidereal_hours < 0)
                delta_sidereal_hours += 24;
        } else {
            // On subsequent iterations, we make the smallest possible adjustment,
            // either forward or backward in time.
            if (delta_sidereal_hours < -12)
                delta_sidereal_hours += 24;
            else if (delta_sidereal_hours > +12)
                delta_sidereal_hours -= 24;
        }

        // If the error is tolerable (less than 0.1 seconds), stop searching.
        if (Math.abs(delta_sidereal_hours) * 3600 < 0.1) {
            const hor = Astronomy.Horizon(time, observer, sky.ofdate.ra, sky.ofdate.dec, 'normal');
            return { time:time, pos:pos, sky:sky, hor:hor, iter:iter };
        }

        // We need to loop another time to get more accuracy.
        // Update the terrestrial time adjusting by sidereal time.
        let delta_days = (delta_sidereal_hours / 24) * solar_days_per_sidereal_day;
        time = time.AddDays(delta_days);
    }
}

Astronomy.Seasons = function(year) {
    function find(targetLon, month, day) {
        let startDate = new Date(Date.UTC(year, month-1, day));
        let time = Astronomy.SearchSunLongitude(targetLon, startDate, 4);
        if (!time)
            throw `Cannot find season change near ${startDate.toISOString()}`;
        return time;
    }

    if (year instanceof Date) {
        year = year.getUTCFullYear();
    }
    let mar_equinox  = find(  0,  3, 19);
    let jun_solstice = find( 90,  6, 19);
    let sep_equinox  = find(180,  9, 21);
    let dec_solstice = find(270, 12, 20);

    return {
        mar_equinox:  mar_equinox,
        jun_solstice: jun_solstice,
        sep_equinox:  sep_equinox,
        dec_solstice: dec_solstice
    };
}

/**
 * Represents the visibility of a planet or the Moon relative to the Sun.
 * Includes angular separation from the Sun and whether visibility is
 * best in the morning or the evening.
 * 
 * @class
 * 
 * @constructor
 * 
 * @memberof Astronomy
 * 
 * @property {Astronomy.Time} time  When the event occurs.
 * @property {string}  visibility   
 *      Either <code>"morning"</code> or <code>"evening"</code>, 
 *      indicating when the body is most easily seen.
 * @property {number}  elongation   
 *      The angle in degrees, as seen from the center of the Earth, 
 *      of the apparent separation between the body and the Sun.
 *      This angle is measured in 3D space and is not projected onto the ecliptic plane.
 * @property {number}  relative_longitude 
 *      The angle in degrees, as seen from the Sun, between the
 *      observed body and the Earth. This value is always between
 *      0 and 180. More precisely, <code>relative_longitude</code> is the absolute
 *      value of the difference between the heliocentric ecliptic longitudes of
 *      the centers of the observed body and the Earth.
 * 
 * @see Astronomy.Elongation
 */
function ElongationEvent(time, visibility, elongation, relative_longitude) {
    this.time = time;
    this.visibility = visibility;
    this.elongation = elongation;
    this.relative_longitude = relative_longitude;
}

/**
 * Calculates the absolute value of the angle between the centers 
 * of the given body and the Sun as seen from the center of the Earth at the given date.
 * The angle is measured along the plane of the Earth's orbit (i.e. the ecliptic) 
 * and ranges [0, 180] degrees. This function is helpful for determining how easy 
 * it is to view Mercury or Venus away from the Sun's glare on a given date.
 * The function also determines whether the object is visible in the morning or evening; 
 * this is more important the smaller the elongation is.
 * 
 * @param {string} body
 *      The name of the observed body. Not allowed to be <code>"Earth"</code>.
 * 
 * @returns {Astronomy.ElongationEvent}
 */
Astronomy.Elongation = function(body, date) {    
    let time = AstroTime(date);

    let lon = Astronomy.LongitudeFromSun(body, time);
    let vis;
    if (lon > 180) {
        vis = 'morning';
        lon = 360 - lon;
    } else {
        vis = 'evening';
    }
    let angle = Astronomy.AngleFromSun(body, time);
    return new ElongationEvent(time, vis, angle, lon);
}

/** 
 * Searches for the next maximum elongation event for Mercury or Venus 
 * that occurs after the given start date. Calling with other values 
 * of <code>body</code> will result in an exception.
 * Maximum elongation occurs when the body has the greatest
 * angular separation from the Sun, as seen from the Earth.
 * Returns an <code>ElongationEvent</code> object containing the date and time of the next
 * maximum elongation, the elongation in degrees, and whether
 * the body is visible in the morning or evening.
 * 
 * @param {string} body     either "Mercury" or "Venus"
 * @param {Date} startDate  the date and time after which to search for the next maximum elongation event
 * 
 * @returns {Astronomy.ElongationEvent}
 */
Astronomy.SearchMaxElongation = function(body, startDate) {

    const dt = 0.01;

    function neg_slope(t) {
        // The slope de/dt goes from positive to negative at the maximum elongation event.
        // But Search() is designed for functions that ascend through zero.
        // So this function returns the negative slope.
        const t1 = t.AddDays(-dt/2);
        const t2 = t.AddDays(+dt/2);
        let e1 = Astronomy.AngleFromSun(body, t1);
        let e2 = Astronomy.AngleFromSun(body, t2);
        let m = (e1-e2)/dt;
        return m;
    }

    let startTime = AstroTime(startDate);

    const table = {
        Mercury : { s1:50.0, s2:85.0 },
        Venus :   { s1:40.0, s2:50.0 }
    };

    const planet = table[body];
    if (!planet)
        throw 'SearchMaxElongation works for Mercury and Venus only.';

    let iter = 0;
    while (++iter <= 2) {
        // Find current heliocentric relative longitude between the
        // inferior planet and the Earth.
        let plon = Astronomy.EclipticLongitude(body, startTime);
        let elon = Astronomy.EclipticLongitude('Earth', startTime);
        let rlon = LongitudeOffset(plon - elon);    // clamp to (-180, +180]

        // The slope function is not well-behaved when rlon is near 0 degrees or 180 degrees
        // because there is a cusp there that causes a discontinuity in the derivative.
        // So we need to guard against searching near such times.

        let t1, t2;
        let rlon_lo, rlon_hi, adjust_days;
        if (rlon >= -planet.s1 && rlon < +planet.s1 ) {
            // Seek to the window [+s1, +s2].
            adjust_days = 0;
            // Search forward for the time t1 when rel lon = +s1.
            rlon_lo = +planet.s1;
            // Search forward for the time t2 when rel lon = +s2.
            rlon_hi = +planet.s2;
        } else if (rlon >= +planet.s2 || rlon < -planet.s2 ) {
            // Seek to the next search window at [-s2, -s1].
            adjust_days = 0;
            // Search forward for the time t1 when rel lon = -s2.
            rlon_lo = -planet.s2;
            // Search forward for the time t2 when rel lon = -s1.
            rlon_hi = -planet.s1;
        } else if (rlon >= 0) {
            // rlon must be in the middle of the window [+s1, +s2].
            // Search BACKWARD for the time t1 when rel lon = +s1.
            adjust_days = -SynodicPeriod(body) / 4;
            rlon_lo = +planet.s1;
            rlon_hi = +planet.s2;
            // Search forward from t1 to find t2 such that rel lon = +s2.
        } else {
            // rlon must be in the middle of the window [-s2, -s1].
            // Search BACKWARD for the time t1 when rel lon = -s2.
            adjust_days = -SynodicPeriod(body) / 4;
            rlon_lo = -planet.s2;
            // Search forward from t1 to find t2 such that rel lon = -s1.
            rlon_hi = -planet.s1;
        }

        let t_start = startTime.AddDays(adjust_days);
        t1 = Astronomy.SearchRelativeLongitude(body, rlon_lo, t_start);
        t2 = Astronomy.SearchRelativeLongitude(body, rlon_hi, t1);

        // Now we have a time range [t1,t2] that brackets a maximum elongation event.
        // Confirm the bracketing.
        let m1 = neg_slope(t1);
        if (m1 >= 0) 
            throw `SearchMaxElongation: internal error: m1 = ${m1}`;

        let m2 = neg_slope(t2);
        if (m2 <= 0) 
            throw `SearchMaxElongation: internal error: m2 = ${m2}`;

        // Use the generic search algorithm to home in on where the slope crosses from negative to positive.
        let tx = Search(neg_slope, t1, t2, {init_f1:m1, init_f2:m2, dt_tolerance_seconds:10});
        if (!tx) 
            throw `SearchMaxElongation: failed search iter ${iter} (t1=${t1.toString()}, t2=${t2.toString()})`;

        if (tx.tt >= startTime.tt)
            return Astronomy.Elongation(body, tx);

        // This event is in the past (earlier than startDate).
        // We need to search forward from t2 to find the next possible window.
        // We never need to search more than twice.
        startTime = t2.AddDays(1);
    }

    throw `SearchMaxElongation: failed to find event after 2 tries.`;
}

/**
 * Searches for the date and time Venus will next appear brightest as seen from the Earth.
 * 
 * @param {string} body
 *      Currently only <code>"Venus"</code> is supported.
 *      Mercury's peak magnitude occurs at superior conjunction, when it is virtually impossible to see from Earth,
 *      so peak magnitude events have little practical value for this planet.
 *      The Moon reaches peak magnitude very close to full moon, which can be found using 
 *      {@link Astronomy.SearchMoonQuarter} or {@link Astronomy.SearchMoonPhase}.
 *      The other planets reach peak magnitude very close to opposition, 
 *      which can be found using {@link Astronomy.SearchRelativeLongitude}.
 * 
 * @param {(Date | Number | Astronomy.Time)} startDate
 *      The date and time after which to find the next peak magnitude event.
 * 
 * @returns {Astronomy.IlluminationInfo}
 */
Astronomy.SearchPeakMagnitude = function(body, startDate) {
    if (body !== 'Venus')
        throw 'SearchPeakMagnitude currently works for Venus only.';

    const dt = 0.01;

    function slope(t) {
        // The Search() function finds a transition from negative to positive values.
        // The derivative of magnitude y with respect to time t (dy/dt)
        // is negative as an object gets brighter, because the magnitude numbers
        // get smaller. At peak magnitude dy/dt = 0, then as the object gets dimmer,
        // dy/dt > 0.
        const t1 = t.AddDays(-dt/2);
        const t2 = t.AddDays(+dt/2);
        const y1 = Astronomy.Illumination(body, t1).mag;
        const y2 = Astronomy.Illumination(body, t2).mag;
        const m = (y2-y1) / dt;
        return m;
    }

    let startTime = AstroTime(startDate);

    // s1 and s2 are relative longitudes within which peak magnitude of Venus can occur.
    const s1 = 10.0;
    const s2 = 30.0;

    let iter = 0;
    while (++iter <= 2) {
        // Find current heliocentric relative longitude between the
        // inferior planet and the Earth.
        let plon = Astronomy.EclipticLongitude(body, startTime);
        let elon = Astronomy.EclipticLongitude('Earth', startTime);
        let rlon = LongitudeOffset(plon - elon);    // clamp to (-180, +180]

        // The slope function is not well-behaved when rlon is near 0 degrees or 180 degrees
        // because there is a cusp there that causes a discontinuity in the derivative.
        // So we need to guard against searching near such times.

        let t1, t2;
        let rlon_lo, rlon_hi, adjust_days;
        if (rlon >= -s1 && rlon < +s1 ) {
            // Seek to the window [+s1, +s2].
            adjust_days = 0;
            // Search forward for the time t1 when rel lon = +s1.
            rlon_lo = +s1;
            // Search forward for the time t2 when rel lon = +s2.
            rlon_hi = +s2;
        } else if (rlon >= +s2 || rlon < -s2 ) {
            // Seek to the next search window at [-s2, -s1].
            adjust_days = 0;
            // Search forward for the time t1 when rel lon = -s2.
            rlon_lo = -s2;
            // Search forward for the time t2 when rel lon = -s1.
            rlon_hi = -s1;
        } else if (rlon >= 0) {
            // rlon must be in the middle of the window [+s1, +s2].
            // Search BACKWARD for the time t1 when rel lon = +s1.
            adjust_days = -SynodicPeriod(body) / 4;
            rlon_lo = +s1;
            rlon_hi = +s2;
            // Search forward from t1 to find t2 such that rel lon = +s2.
        } else {
            // rlon must be in the middle of the window [-s2, -s1].
            // Search BACKWARD for the time t1 when rel lon = -s2.
            adjust_days = -SynodicPeriod(body) / 4;
            rlon_lo = -s2;
            // Search forward from t1 to find t2 such that rel lon = -s1.
            rlon_hi = -s1;
        }

        let t_start = startTime.AddDays(adjust_days);
        t1 = Astronomy.SearchRelativeLongitude(body, rlon_lo, t_start);
        t2 = Astronomy.SearchRelativeLongitude(body, rlon_hi, t1);

        // Now we have a time range [t1,t2] that brackets a maximum magnitude event.
        // Confirm the bracketing.
        let m1 = slope(t1);
        if (m1 >= 0) 
            throw `SearchPeakMagnitude: internal error: m1 = ${m1}`;

        let m2 = slope(t2);
        if (m2 <= 0) 
            throw `SearchPeakMagnitude: internal error: m2 = ${m2}`;

        // Use the generic search algorithm to home in on where the slope crosses from negative to positive.
        let tx = Search(slope, t1, t2, {init_f1:m1, init_f2:m2, dt_tolerance_seconds:10});
        if (!tx) 
            throw `SearchPeakMagnitude: failed search iter ${iter} (t1=${t1.toString()}, t2=${t2.toString()})`;

        if (tx.tt >= startTime.tt)
            return Astronomy.Illumination(body, tx);

        // This event is in the past (earlier than startDate).
        // We need to search forward from t2 to find the next possible window.
        // We never need to search more than twice.
        startTime = t2.AddDays(1);
    }

    throw `SearchPeakMagnitude: failed to find event after 2 tries.`;
}

})(typeof exports==='undefined' ? (this.Astronomy={}) : exports);
