#!/usr/bin/env python3

import math
import datetime

_PI2 = 2.0 * math.pi
_EPOCH = datetime.datetime(2000, 1, 1, 12)
_T0 = 2451545.0
_MJD_BASIS = 2400000.5
_Y2000_IN_MJD = _T0 - _MJD_BASIS
_DEG2RAD = 0.017453292519943296
_RAD2DEG = 57.295779513082321
_ASEC360 = 1296000.0
_ASEC2RAD = 4.848136811095359935899141e-6
_ARC = 3600.0 * 180.0 / math.pi     # arcseconds per radian
_C_AUDAY = 173.1446326846693        # speed of light in AU/day
_ERAD = 6378136.6                   # mean earth radius in meters
_AU = 1.4959787069098932e+11        # astronomical unit in meters
_KM_PER_AU = 1.4959787069098932e+8
_ANGVEL = 7.2921150e-5
_SECONDS_PER_DAY = 24.0 * 3600.0
_SOLAR_DAYS_PER_SIDEREAL_DAY = 0.9972695717592592
_MEAN_SYNODIC_MONTH = 29.530588
_EARTH_ORBITAL_PERIOD = 365.256
_REFRACTION_NEAR_HORIZON = 34.0 / 60.0
_SUN_RADIUS_AU  = 4.6505e-3
_MOON_RADIUS_AU = 1.15717e-5
_ASEC180 = 180.0 * 60.0 * 60.0

def _LongitudeOffset(diff):
    offset = diff
    while offset <= -180.0:
        offset += 360.0
    while offset > 180.0:
        offset -= 360.0
    return offset

def _NormalizeLongitude(lon):
    while lon < 0.0:
        lon += 360.0
    while lon >= 360.0:
        lon -= 360.0
    return lon

def VectorLength(vector):
    return math.sqrt(vector.x**2 + vector.y**2 + vector.z**2)

BODY_INVALID = -1
BODY_MERCURY = 0
BODY_VENUS = 1
BODY_EARTH = 2
BODY_MARS = 3
BODY_JUPITER = 4
BODY_SATURN = 5
BODY_URANUS = 6
BODY_NEPTUNE = 7
BODY_PLUTO = 8
BODY_SUN = 9
BODY_MOON = 10

BodyName = [
    'Mercury',
    'Venus',
    'Earth',
    'Mars',
    'Jupiter',
    'Saturn',
    'Uranus',
    'Neptune',
    'Pluto',
    'Sun',
    'Moon',
]

def BodyCode(name):
    return BodyName.index(name)

def _IsSuperiorPlanet(body):
    return body in [BODY_MARS, BODY_JUPITER, BODY_SATURN, BODY_URANUS, BODY_NEPTUNE, BODY_PLUTO]

_PlanetOrbitalPeriod = [
    87.969,
    224.701,
    _EARTH_ORBITAL_PERIOD,
    686.980,
    4332.589,
    10759.22,
    30685.4,
    60189.0,
    90560.0
]

class Error(Exception):
    def __init__(self, message):
        Exception.__init__(self, message)

class EarthNotAllowedError(Error):
    def __init__(self):
        Error.__init__(self, 'The Earth is not allowed as the body.')

class InvalidBodyError(Error):
    def __init__(self):
        Error.__init__(self, 'Invalid astronomical body.')

class BadVectorError(Error):
    def __init__(self):
        Error.__init__(self, 'Vector is too small to have a direction.')

def _SynodicPeriod(body):
    if body == BODY_EARTH:
        raise EarthNotAllowedError()
    if body < 0 or body >= len(_PlanetOrbitalPeriod):
        raise InvalidBodyError()
    if body == BODY_MOON:
        return _MEAN_SYNODIC_MONTH
    return abs(_EARTH_ORBITAL_PERIOD / (_EARTH_ORBITAL_PERIOD/_PlanetOrbitalPeriod[body] - 1.0))

def _AngleBetween(a, b):
    r = VectorLength(a) * VectorLength(b)
    if r < 1.0e-8:
        return BadVectorError()
    dot = (a.x*b.x + a.y*b.y + a.z*b.z) / r
    if dot <= -1.0:
        return 180.0
    if dot >= +1.0:
        return 0.0
    return _RAD2DEG * math.acos(dot)

class _delta_t_entry_t:
    def __init__(self, mjd, dt):
        self.mjd = mjd
        self.dt = dt

_DT = [
_delta_t_entry_t(-72638.0, 38),
_delta_t_entry_t(-65333.0, 26),
_delta_t_entry_t(-58028.0, 21),
_delta_t_entry_t(-50724.0, 21.1),
_delta_t_entry_t(-43419.0, 13.5),
_delta_t_entry_t(-39766.0, 13.7),
_delta_t_entry_t(-36114.0, 14.8),
_delta_t_entry_t(-32461.0, 15.7),
_delta_t_entry_t(-28809.0, 15.6),
_delta_t_entry_t(-25156.0, 13.3),
_delta_t_entry_t(-21504.0, 12.6),
_delta_t_entry_t(-17852.0, 11.2),
_delta_t_entry_t(-14200.0, 11.13),
_delta_t_entry_t(-10547.0, 7.95),
_delta_t_entry_t(-6895.0, 6.22),
_delta_t_entry_t(-3242.0, 6.55),
_delta_t_entry_t(-1416.0, 7.26),
_delta_t_entry_t(410.0, 7.35),
_delta_t_entry_t(2237.0, 5.92),
_delta_t_entry_t(4063.0, 1.04),
_delta_t_entry_t(5889.0, -3.19),
_delta_t_entry_t(7715.0, -5.36),
_delta_t_entry_t(9542.0, -5.74),
_delta_t_entry_t(11368.0, -5.86),
_delta_t_entry_t(13194.0, -6.41),
_delta_t_entry_t(15020.0, -2.70),
_delta_t_entry_t(16846.0, 3.92),
_delta_t_entry_t(18672.0, 10.38),
_delta_t_entry_t(20498.0, 17.19),
_delta_t_entry_t(22324.0, 21.41),
_delta_t_entry_t(24151.0, 23.63),
_delta_t_entry_t(25977.0, 24.02),
_delta_t_entry_t(27803.0, 23.91),
_delta_t_entry_t(29629.0, 24.35),
_delta_t_entry_t(31456.0, 26.76),
_delta_t_entry_t(33282.0, 29.15),
_delta_t_entry_t(35108.0, 31.07),
_delta_t_entry_t(36934.0, 33.150),
_delta_t_entry_t(38761.0, 35.738),
_delta_t_entry_t(40587.0, 40.182),
_delta_t_entry_t(42413.0, 45.477),
_delta_t_entry_t(44239.0, 50.540),
_delta_t_entry_t(44605.0, 51.3808),
_delta_t_entry_t(44970.0, 52.1668),
_delta_t_entry_t(45335.0, 52.9565),
_delta_t_entry_t(45700.0, 53.7882),
_delta_t_entry_t(46066.0, 54.3427),
_delta_t_entry_t(46431.0, 54.8712),
_delta_t_entry_t(46796.0, 55.3222),
_delta_t_entry_t(47161.0, 55.8197),
_delta_t_entry_t(47527.0, 56.3000),
_delta_t_entry_t(47892.0, 56.8553),
_delta_t_entry_t(48257.0, 57.5653),
_delta_t_entry_t(48622.0, 58.3092),
_delta_t_entry_t(48988.0, 59.1218),
_delta_t_entry_t(49353.0, 59.9845),
_delta_t_entry_t(49718.0, 60.7853),
_delta_t_entry_t(50083.0, 61.6287),
_delta_t_entry_t(50449.0, 62.2950),
_delta_t_entry_t(50814.0, 62.9659),
_delta_t_entry_t(51179.0, 63.4673),
_delta_t_entry_t(51544.0, 63.8285),
_delta_t_entry_t(51910.0, 64.0908),
_delta_t_entry_t(52275.0, 64.2998),
_delta_t_entry_t(52640.0, 64.4734),
_delta_t_entry_t(53005.0, 64.5736),
_delta_t_entry_t(53371.0, 64.6876),
_delta_t_entry_t(53736.0, 64.8452),
_delta_t_entry_t(54101.0, 65.1464),
_delta_t_entry_t(54466.0, 65.4573),
_delta_t_entry_t(54832.0, 65.7768),
_delta_t_entry_t(55197.0, 66.0699),
_delta_t_entry_t(55562.0, 66.3246),
_delta_t_entry_t(55927.0, 66.6030),
_delta_t_entry_t(56293.0, 66.9069),
_delta_t_entry_t(56658.0, 67.2810),
_delta_t_entry_t(57023.0, 67.6439),
_delta_t_entry_t(57388.0, 68.1024),
_delta_t_entry_t(57754.0, 68.5927),
_delta_t_entry_t(58119.0, 68.9676),
_delta_t_entry_t(58484.0, 69.2201),
_delta_t_entry_t(58849.0, 69.87),
_delta_t_entry_t(59214.0, 70.39),
_delta_t_entry_t(59580.0, 70.91),
_delta_t_entry_t(59945.0, 71.40),
_delta_t_entry_t(60310.0, 71.88),
_delta_t_entry_t(60675.0, 72.36),
_delta_t_entry_t(61041.0, 72.83),
_delta_t_entry_t(61406.0, 73.32),
_delta_t_entry_t(61680.0, 73.66)
]

def _DeltaT(mjd):
    if mjd <= _DT[0].mjd:
        return _DT[0].dt
    if mjd >= _DT[-1].mjd:
        return _DT[-1].dt
    # Do a binary search to find the pair of indexes this mjd lies between.
    lo = 0
    hi = len(_DT) - 2   # Make sure there is always an array element after the one we are looking at.
    while True:
        if lo > hi:
            # This should never happen unless there is a bug in the binary search.
            raise Error('Could not find delta-t value.')
        c = (lo + hi) // 2
        if mjd < _DT[c].mjd:
            hi = c-1
        elif mjd > _DT[c+1].mjd:
            lo = c+1
        else:
            frac = (mjd - _DT[c].mjd) / (_DT[c+1].mjd - _DT[c].mjd)
            return _DT[c].dt + frac*(_DT[c+1].dt - _DT[c].dt)

def _TerrestrialTime(ut):
    return ut + _DeltaT(ut + _Y2000_IN_MJD) / 86400.0

class Time:
    def __init__(self, ut):
        self.ut = ut
        self.tt = _TerrestrialTime(ut)

    @staticmethod
    def Make(year, month, day, hour, minute, second):
        micro = round((second % 1) * 1000000)
        second = math.floor(second - micro/1000000)
        d = datetime.datetime(year, month, day, hour, minute, second, micro)
        ut = (d - _EPOCH).total_seconds() / 86400
        return Time(ut)

    @staticmethod
    def Now():
        ut = (datetime.datetime.utcnow() - _EPOCH).total_seconds() / 86400.0
        return Time(ut)

    def AddDays(self, days):
        return Time(self.ut + days)

    def __str__(self):
        millis = round(self.ut * 86400000.0)
        n = _EPOCH + datetime.timedelta(milliseconds=millis)
        return '{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{:03d}Z'.format(n.year, n.month, n.day, n.hour, n.minute, n.second, math.floor(n.microsecond / 1000))

    def Utc(self):
        return _EPOCH + datetime.timedelta(days=self.ut)


class Observer:
    def __init__(self, latitude, longitude, height=0):
        self.latitude = latitude
        self.longitude = longitude
        self.height = height

_nals_t = [
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
]

_cls_t = [
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
]

class _iau2000b:
    def __init__(self, time):
        t = time.tt / 36525
        el  = ((485868.249036 + t * 1717915923.2178) % ASEC360) * ASEC2RAD
        elp = ((1287104.79305 + t * 129596581.0481)  % ASEC360) * ASEC2RAD
        f   = ((335779.526232 + t * 1739527262.8478) % ASEC360) * ASEC2RAD
        d   = ((1072260.70369 + t * 1602961601.2090) % ASEC360) * ASEC2RAD
        om  = ((450160.398036 - t * 6962890.5431)    % ASEC360) * ASEC2RAD
        dp = 0
        de = 0
        i = 76
        while i >= 0:
            arg = (_nals_t[i][0]*el + _nals_t[i][1]*elp + _nals_t[i][2]*f + _nals_t[i][3]*d + _nals_t[i][4]*om) % _PI2
            sarg = math.sin(arg)
            carg = math.cos(arg)
            dp += (_cls_t[i][0] + _cls_t[i][1] * t)*sarg + _cls_t[i][2]*carg
            de += (_cls_t[i][3] + _cls_t[i][4] * t)*carg + _cls_t[i][5]*sarg
            i -= 1
        self.dpsi = -0.000135 + (dp * 1.0e-7)
        self.deps = +0.000388 + (de * 1.0e-7)

def _mean_obliq(tt):
    t = tt / 36525
    asec = (
        (((( -  0.0000000434   * t
             -  0.000000576  ) * t
             +  0.00200340   ) * t
             -  0.0001831    ) * t
             - 46.836769     ) * t + 84381.406
    )
    return asec / 3600.0

class _e_tilt:
    def __init__(self, time):
        e = _iau2000b(time)
        self.dpsi = e.dpsi
        self.deps = e.deps
        self.mobl = _mean_obliq(time.tt)
        self.tobl = self.mobl + (e.deps / 3600.0)
        self.tt = time.tt
        self.ee = e.dpsi * math.cos(self.mobl * _DEG2RAD) / 15.0

def _ecl2equ_vec(time, ecl, equ):
    obl = _mean_obliq(time.tt) * _DEG2RAD
    cos_obl = math.cos(obl)
    sin_obl = math.sin(obl)
    return [
        ecl[0],
        ecl[1]*cos_obl - ecl[2]*sin_obl,
        ecl[1]*sin_obl + ecl[2]*cos_obl
    ]

def _precession(tt1, pos1, tt2):
    eps0 = 84381.406
    if tt1 != 0 and tt2 != 0:
        raise Error('One of (tt1, tt2) must be zero.')
    t = (tt2 - tt1) / 36525
    if tt2 == 0:
        t = -t

    psia  = (((((-    0.0000000951  * t
                 +    0.000132851 ) * t
                 -    0.00114045  ) * t
                 -    1.0790069   ) * t
                 + 5038.481507    ) * t)

    omegaa = (((((+   0.0000003337  * t
                 -    0.000000467 ) * t
                 -    0.00772503  ) * t
                 +    0.0512623   ) * t
                 -    0.025754    ) * t + eps0)

    chia  = (((((-    0.0000000560  * t
                 +    0.000170663 ) * t
                 -    0.00121197  ) * t
                 -    2.3814292   ) * t
                 +   10.556403    ) * t)

    eps0 *= _ASEC2RAD
    psia *= _ASEC2RAD
    omegaa *= _ASEC2RAD
    chia *= _ASEC2RAD

    sa = math.sin(eps0)
    ca = math.cos(eps0)
    sb = math.sin(-psia)
    cb = math.cos(-psia)
    sc = math.sin(-omegaa)
    cc = math.cos(-omegaa)
    sd = math.sin(chia)
    cd = math.cos(chia)

    xx =  cd * cb - sb * sd * cc
    yx =  cd * sb * ca + sd * cc * cb * ca - sa * sd * sc
    zx =  cd * sb * sa + sd * cc * cb * sa + ca * sd * sc
    xy = -sd * cb - sb * cd * cc
    yy = -sd * sb * ca + cd * cc * cb * ca - sa * cd * sc
    zy = -sd * sb * sa + cd * cc * cb * sa + ca * cd * sc
    xz =  sb * sc
    yz = -sc * cb * ca - sa * cc
    zz = -sc * cb * sa + cc * ca
    if tt2 == 0.0:
        # Perform rotation from other epoch to J2000.0.
        return [
            xx * pos1[0] + xy * pos1[1] + xz * pos1[2],
            yx * pos1[0] + yy * pos1[1] + yz * pos1[2],
            zx * pos1[0] + zy * pos1[1] + zz * pos1[2]
        ]

    # Perform rotation from J2000.0 to other epoch.
    return [
        xx * pos1[0] + yx * pos1[1] + zx * pos1[2],
        xy * pos1[0] + yy * pos1[1] + zy * pos1[2],
        xz * pos1[0] + yz * pos1[1] + zz * pos1[2]
    ]

class Equatorial:
    def __init__(self, ra, dec, dist):
        self.ra = ra
        self.dec = dec
        self.dist = dist

def _vector2radec(pos):
    xyproj = pos[0]*pos[0] + pos[1]*pos[1]
    dist = math.sqrt(xyproj + pos[2]*pos[2])
    if xyproj == 0.0:
        if pos[2] == 0.0:
            # Indeterminate coordinates: pos vector has zero length.
            raise Error('Cannot convert vector to polar coordinates')
        ra = 0.0
        if pos[2] < 0.0:
            dec = -90.0
        else:
            dec = +90.0
    else:
        ra = math.atan2(pos[1], pos[0]) / (_DEG2RAD * 15)
        if ra < 0:
            ra += 24
        dec = _RAD2DEG * math.atan2(pos[2], math.sqrt(xyproj))
    return Equatorial(ra, dec, dist)


def _nutation(time, direction, inpos):
    tilt = _e_tilt(time)
    oblm = tilt.mobl * _DEG2RAD
    oblt = tilt.tobl * _DEG2RAD
    psi = tilt.dpsi * _ASEC2RAD
    cobm = math.cos(oblm)
    sobm = math.sin(oblm)
    cobt = math.cos(oblt)
    sobt = math.sin(oblt)
    cpsi = math.cos(psi)
    spsi = math.sin(psi)

    xx = cpsi
    yx = -spsi * cobm
    zx = -spsi * sobm
    xy = spsi * cobt
    yy = cpsi * cobm * cobt + sobm * sobt
    zy = cpsi * sobm * cobt - cobm * sobt
    xz = spsi * sobt
    yz = cpsi * cobm * sobt - sobm * cobt
    zz = cpsi * sobm * sobt + cobm * cobt

    if direction == 0:
        # forward rotation
        return [
            xx * inpos[0] + yx * inpos[1] + zx * inpos[2],
            xy * inpos[0] + yy * inpos[1] + zy * inpos[2],
            xz * inpos[0] + yz * inpos[1] + zz * inpos[2]
        ]

    # inverse rotation
    return [
        xx * inpos[0] + xy * inpos[1] + xz * inpos[2],
        yx * inpos[0] + yy * inpos[1] + yz * inpos[2],
        zx * inpos[0] + zy * inpos[1] + zz * inpos[2]
    ]

def _era(time):        # Earth Rotation Angle
    thet1 = 0.7790572732640 + 0.00273781191135448 * time.ut
    thet3 = time.ut % 1.0
    theta = 360.0 * ((thet1 + thet3) % 1.0)
    if theta < 0.0:
        theta += 360.0
    return theta


def _sidereal_time(time):
    t = time.tt / 36525.0
    eqeq = 15.0 * _e_tilt(time).ee    # Replace with eqeq=0 to get GMST instead of GAST (if we ever need it)
    theta = _era(time)
    st = (eqeq + 0.014506 +
        (((( -    0.0000000368   * t
            -    0.000029956  ) * t
            -    0.00000044   ) * t
            +    1.3915817    ) * t
            + 4612.156534     ) * t)
    gst = ((st/3600.0 + theta) % 360.0) / 15.0
    if gst < 0.0:
        gst += 24.0
    return gst


def _terra(observer, st):
    erad_km = _ERAD / 1000.0
    df = 1.0 - 0.003352819697896    # flattening of the Earth
    df2 = df * df
    phi = observer.latitude * _DEG2RAD
    sinphi = math.sin(phi)
    cosphi = math.cos(phi)
    c = 1.0 / math.sqrt(cosphi*cosphi + df2*sinphi*sinphi)
    s = df2 * c
    ht_km = observer.height / 1000.0
    ach = erad_km*c + ht_km
    ash = erad_km*s + ht_km
    stlocl = (15.0*st + observer.longitude) * _DEG2RAD
    sinst = math.sin(stlocl)
    cosst = math.cos(stlocl)
    return [
        ach * cosphi * cosst / _KM_PER_AU,
        ach * cosphi * sinst / _KM_PER_AU,
        ash * sinphi / _KM_PER_AU
    ]

def _geo_pos(time, observer):
    gast = _sidereal_time(time)
    pos1 = _terra(observer, gast)
    pos2 = _nutation(time, -1, pos1)
    outpos = _precession(time.tt, pos2, 0.0)
    return outpos
