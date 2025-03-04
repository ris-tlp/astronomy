/*
    ctest.c  -  Don Cross <cosinekitty.com>

    C langauge unit test for Astronomy Engine project.
    https://github.com/cosinekitty/astronomy
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "astronomy.h"

char *ReadLine(char *s, int n, FILE *f, const char *filename, int lnum)
{
    char *ret = fgets(s, n, f);
    if (ret != NULL)
    {
        /* If a line is longer than the buffer can hold, it can cause confusing problems. */
        /* Detect that and fail immediately, for easier diagnosis. */
        int i;
        int found = 0;
        for (i = 0; !found && s[i] != '\0'; ++i)
            if (s[i] == '\n' || s[i] == '\r')
                found = 1;
        if (!found)
        {
            fprintf(stderr, "C %s(%s line %d): No EOLN character found. Is the line too long for buffer size %d?\n",
                __func__, filename, lnum+1, n);
            exit(1);
        }
    }
    return ret;
}

#define PI          3.14159265358979323846
#define JD_2000     2451545.0

#define SECONDS_PER_DAY     (24 * 3600)
#define MINUTES_PER_DAY     (24 * 60)

#define CHECK(x)        do{if(0 != (error = (x))) goto fail;}while(0)
#define FAIL(...)       do{printf(__VA_ARGS__); error = 1; goto fail;}while(0)
#define FAILRET(...)    do{printf(__VA_ARGS__); return 1;}while(0)

#define FFAIL(...)      do{printf("C %s: ", __func__); printf(__VA_ARGS__); error = 1; goto fail;}while(0)
#define FLNFAIL(...)    do{printf("C %s(%s line %d): ", __func__, filename, lnum); printf(__VA_ARGS__); error = 1; goto fail;}while(0)
#define FPASS()         do{printf("C %s: PASS\n", __func__); error = 0;}while(0)
#define FPASSA(...)     do{printf("C %s: PASS - ", __func__); printf(__VA_ARGS__); error = 0;}while(0)

static int CheckInverse(const char *aname, const char *bname, astro_rotation_t arot, astro_rotation_t brot);
#define CHECK_INVERSE(a,b)   CHECK(CheckInverse(#a, #b, a, b))

int Verbose = 0;
#define DEBUG(...)      do{if(Verbose)printf(__VA_ARGS__);}while(0)
#define FDEBUG(...)     do{if(Verbose){printf("C %s: ", __func__); printf(__VA_ARGS__);}}while(0)

static int CheckStatus(int lnum, const char *varname, astro_status_t status)
{
    if (status != ASTRO_SUCCESS)
        FAILRET("FAILURE at ctest.c[%d]: %s.status = %d\n", lnum, varname, status);
    return 0;
}

static int CheckVector(int lnum, astro_vector_t v)
{
    if (v.status != ASTRO_SUCCESS)
        FAILRET("FAILURE at ctest.c[%d]: vector status = %d\n", lnum, v.status);
    return 0;
}

static int CheckEquator(int lnum, astro_equatorial_t equ)
{
    if (equ.status != ASTRO_SUCCESS)
        FAILRET("FAILURE at ctest.c[%d]: equatorial status = %d\n", lnum, equ.status);
    return 0;
}

#define CHECK_VECTOR(var,expr)   CHECK(CheckVector(__LINE__, ((var) = (expr))))
#define CHECK_EQU(var,expr)      CHECK(CheckEquator(__LINE__, ((var) = (expr))))
#define CHECK_STATUS(expr)       CHECK(CheckStatus(__LINE__, #expr, (expr).status))

static double v(const char *filename, int lnum, double x)
{
    if (!isfinite(x))
    {
        fprintf(stderr, "FATAL(%s line %d): non-numeric result.\n", filename, lnum);
        exit(1);
    }
    return x;
}

#define V(x)        v(__FILE__, __LINE__, (x))
#define ABS(x)      fabs(V(x))

static double DegreeDiff(double a, double b)
{
    double diff = ABS(a - b);
    if (diff > 180.0 && diff < 360.0)
        diff = 360.0 - diff;        /* handle wraparound of range [0, 360).*/
    return diff;
}

typedef struct
{
    int     lnum;
    double  a_value;
    double  b_value;
    double  factor;
    double  diff;
}
maxdiff_column_t;

#define NUM_V_COLUMNS       4
#define NUM_S_COLUMNS       7
#define NUM_J_COLUMNS       8
#define NUM_N_COLUMNS       2
#define NUM_M_COLUMNS       3
#define NUM_DIFF_COLUMNS    (NUM_V_COLUMNS + NUM_S_COLUMNS + NUM_J_COLUMNS + NUM_N_COLUMNS + NUM_M_COLUMNS)

/*-------------------------------------------------------------------------------------------------*/

typedef struct
{
    int size;       /* number of array entries allocated */
    int length;     /* number of array entries that contain valid state vectors */
    astro_state_vector_t *array;
}
state_vector_batch_t;

static state_vector_batch_t EmptyStateVectorBatch()
{
    state_vector_batch_t batch;
    batch.size = 0;
    batch.length = 0;
    batch.array = NULL;
    return batch;
}

static void FreeStateVectorBatch(state_vector_batch_t *batch)
{
    free(batch->array);
    batch->array = NULL;
    batch->size = 0;
    batch->length = 0;
}

static int AppendStateVector(state_vector_batch_t *batch, astro_state_vector_t state)
{
    int error;

    if (state.status != ASTRO_SUCCESS)
        FFAIL("attempt to append state with status = %d\n", state.status);

    if (batch->array == NULL)
    {
        /* This is the first time appending a state vector. */
        const int INIT_SIZE = 100;
        batch->array = calloc((size_t)INIT_SIZE, sizeof(batch->array[0]));
        if (batch->array == NULL)
            FFAIL("failed initial memory allocation!\n");
        batch->size = INIT_SIZE;
        batch->length = 0;
    }
    else if (batch->length == batch->size)
    {
        /* The buffer is full. Allocate a larger one. */
        int longer = 2 * batch->size;
        astro_state_vector_t *bigger = calloc((size_t)longer, sizeof(batch->array[0]));
        if (bigger == NULL)
            FFAIL("failed to increase memory allocation!\n");
        memcpy(bigger, batch->array, (size_t)(batch->size) * sizeof(batch->array[0]));
        free(batch->array);
        batch->array = bigger;
        batch->size = longer;
    }
    batch->array[(batch->length)++] = state;
    error = 0;
fail:
    return error;
}

static int LoadStateVectors(state_vector_batch_t *batch, const char *filename);

/*-------------------------------------------------------------------------------------------------*/


static int Test_AstroTime(void);
static int AstroCheck(void);
static int Diff(double tolerance, const char *a_filename, const char *b_filename);
static int DiffLine(int lnum, const char *aline, const char *bline, maxdiff_column_t column[]);
static int SeasonsTest(void);
static int SeasonsIssue187(void);
static int MoonPhase(void);
static int MoonReverse(void);
static int MoonNodes(void);
static int MoonVector(void);
static int MoonEcliptic(void);
static int RiseSet(void);
static int RiseSetReverse(void);
static int LunarApsis(void);
static int EarthApsis(void);
static int PlanetApsis(void);
static int PlutoCheck(void);
static int ElongationTest(void);
static int MagnitudeTest(void);
static int MoonTest(void);
static int RotationTest(void);
static int TestMaxMag(astro_body_t body, const char *filename);
static const char *ParseJplHorizonsDateTime(const char *text, astro_time_t *time);
static int VectorDiff(astro_vector_t a, astro_vector_t b, double *diff);
static int RefractionTest(void);
static int ConstellationTest(void);
static int LunarEclipseIssue78(void);
static int LunarEclipseTest(void);
static int LunarFractionTest(void);
static int SolarFractionTest(void);
static int GlobalSolarEclipseTest(void);
static int GravitySimulatorTest(void);
static int PlotDeltaT(const char *outFileName);
static double AngleDiff(double alat, double alon, double blat, double blon);
static int LocalSolarEclipseTest(void);
static int LocalSolarEclipseTest1(void);
static int LocalSolarEclipseTest2(void);
static int Transit(void);
static int DistancePlot(astro_body_t body, double ut1, double ut2, const char *filename);
static int GeoidTest(void);
static int JupiterMoonsTest(void);
static int Issue103(void);
static int AberrationTest(void);
static int BaryStateTest(void);
static int HelioStateTest(void);
static int LagrangeTest(void);
static int LagrangeJplAnalysis(void);
static int TopoStateTest(void);
static int Twilight(void);
static int LibrationTest(void);
static int DE405_Check(void);
static int AxisTest(void);
static int SiderealTimeTest(void);
static int DatesIssue250(void);
static int StarRiseSetCulm(void);
static int MapPerformanceTest(void);
static int GeoMoonPerformance(void);
static int NutationPerformance(void);
static int EclipticTest(void);
static int HourAngleTest(void);

typedef int (* unit_test_func_t) (void);

typedef struct
{
    const char *name;
    unit_test_func_t func;
    int exclude;       /* nonzero to exclude the test from the "all" command. Used for "performance" tests. */
}
unit_test_t;

#define EXCLUDE_FROM_AUTOMATED_TESTS 1

static unit_test_t UnitTests[] =
{
    {"aberration",              AberrationTest},
    {"axis",                    AxisTest},
    {"barystate",               BaryStateTest},
    {"check",                   AstroCheck},
    {"constellation",           ConstellationTest},
    {"dates250",                DatesIssue250},
    {"de405",                   DE405_Check},
    {"earth_apsis",             EarthApsis},
    {"ecliptic",                EclipticTest},
    {"elongation",              ElongationTest},
    {"geoid",                   GeoidTest},
    {"global_solar_eclipse",    GlobalSolarEclipseTest},
    {"gravsim",                 GravitySimulatorTest},
    {"heliostate",              HelioStateTest},
    {"hour_angle",              HourAngleTest},
    {"issue_103",               Issue103},
    {"jupiter_moons",           JupiterMoonsTest},
    {"lagrange",                LagrangeTest},
    {"lagrange_jpl",            LagrangeJplAnalysis},
    {"libration",               LibrationTest},
    {"local_solar_eclipse",     LocalSolarEclipseTest},
    {"lunar_eclipse",           LunarEclipseTest},
    {"lunar_eclipse_78",        LunarEclipseIssue78},
    {"lunar_fraction",          LunarFractionTest},
    {"magnitude",               MagnitudeTest},
    {"map",                     MapPerformanceTest,     EXCLUDE_FROM_AUTOMATED_TESTS},
    {"moon",                    MoonTest},
    {"moon_apsis",              LunarApsis},
    {"moon_ecm",                MoonEcliptic},
    {"moon_nodes",              MoonNodes},
    {"moon_performance",        GeoMoonPerformance,     EXCLUDE_FROM_AUTOMATED_TESTS},
    {"moon_phase",              MoonPhase},
    {"moon_reverse",            MoonReverse},
    {"moon_vector",             MoonVector},
    {"nutation",                NutationPerformance,    EXCLUDE_FROM_AUTOMATED_TESTS},
    {"planet_apsis",            PlanetApsis},
    {"pluto",                   PlutoCheck},
    {"refraction",              RefractionTest},
    {"riseset",                 RiseSet},
    {"riseset_reverse",         RiseSetReverse},
    {"rotation",                RotationTest},
    {"seasons",                 SeasonsTest},
    {"seasons187",              SeasonsIssue187},
    {"sidereal",                SiderealTimeTest},
    {"solar_fraction",          SolarFractionTest},
    {"star_risesetculm",        StarRiseSetCulm},
    {"time",                    Test_AstroTime},
    {"topostate",               TopoStateTest},
    {"transit",                 Transit},
    {"twilight",                Twilight}
};

#define NUM_UNIT_TESTS    (sizeof(UnitTests) / sizeof(UnitTests[0]))

int main(int argc, const char *argv[])
{
    int error = 1;
    int i;

    if (argc > 1 && !strcmp(argv[1], "-v"))
    {
        ++argv;
        --argc;
        Verbose = 1;
    }

    if (argc == 1)
    {
        /* List available commands. */
        printf("Run `ctest all` to run all tests.\n");
        printf("Or run `ctest <name>` for a specific named test:\n");
        for (i = 0; i < NUM_UNIT_TESTS; ++i)
            printf("    %s\n", UnitTests[i].name);
        return 1;
    }

    if (argc > 1)
    {
        const char *verb = argv[1];
        if (argc == 2)
        {
            if (!strcmp(verb, "all"))
            {
                for (i=0; i < NUM_UNIT_TESTS; ++i)
                    if (!UnitTests[i].exclude && UnitTests[i].func())
                        FAIL("ctest: Unit test failed: %s\n", UnitTests[i].name);
                goto success;
            }

            for (i=0; i < NUM_UNIT_TESTS; ++i)
            {
                if (!strcmp(verb, UnitTests[i].name))
                {
                    CHECK(UnitTests[i].func());
                    goto success;
                }
            }
        }

        if (argc == 3)
        {
            const char *filename = argv[2];
            if (!strcmp(verb, "dtplot"))
            {
                CHECK(PlotDeltaT(filename));
                goto success;
            }
        }

        if (argc == 5)
        {
            if (!strcmp(verb, "diff"))
            {
                double tolerance = atof(argv[2]);
                const char *c_filename = argv[3];
                const char *js_filename = argv[4];
                CHECK(Diff(tolerance, c_filename, js_filename));
                goto success;
            }
        }

        if (argc == 6)
        {
            if (!strcmp(verb, "distplot"))
            {
                /* ctest distplot name tt1 tt2 */
                astro_body_t body;
                double ut1, ut2;

                body = Astronomy_BodyCode(argv[2]);
                if (body == BODY_INVALID)
                    FAIL("Invalid body name '%s'\n", argv[2]);

                if (1 != sscanf(argv[3], "%lf", &ut1))
                    FAIL("Invalid tt1 value '%s'\n", argv[3]);

                if (1 != sscanf(argv[4], "%lf", &ut2))
                    FAIL("Invalid tt2 value '%s'\n", argv[4]);

                CHECK(DistancePlot(body, ut1, ut2, argv[5]));
                goto success;
            }
        }
    }

    FAIL("ctest: Invalid command line arguments.\n");

success:
    error = 0;

fail:
    Astronomy_Reset();      /* Free memory so valgrind doesn't see any leaks. */
    fflush(stdout);
    fflush(stderr);
    return error;
}


static int CheckTimeFormat(
    astro_time_t time,
    astro_time_format_t format,
    astro_status_t expected_status,
    const char *expected_text)
{
    int error = 1;
    astro_status_t status;
    char text[TIME_TEXT_BYTES];

    status = Astronomy_FormatTime(time, format, text, sizeof(text));
    if (status != expected_status)
        FAIL("C %s(%s): expected status %d, got %d\n", __func__, expected_text, expected_status, status);

    if (strcmp(text, expected_text))
        FAIL("C %s(%s): computed wrong text '%s'\n", __func__, expected_text, text);

    DEBUG("C %s(%s): PASS\n", __func__, expected_text);
    error = 0;
fail:
    return error;
}


static int Test_AstroTime(void)
{
    int error = 1;
    astro_time_t time;
    astro_utc_t utc;
    const double expected_ut = 6910.270978506945;
    const double expected_tt = 6910.271800214368;
    double diff;

    const int year = 2018;
    const int month = 12;
    const int day = 2;
    const int hour = 18;
    const int minute = 30;
    const double second = 12.543;

    time = Astronomy_MakeTime(year, month, day, hour, minute, second);
    FDEBUG("ut=%0.12lf, tt=%0.12lf\n", time.ut, time.tt);

    diff = time.ut - expected_ut;
    if (ABS(diff) > 1.0e-12)
        FFAIL("excessive UT error %lg\n", diff);

    diff = time.tt - expected_tt;
    if (ABS(diff) > 1.0e-12)
        FFAIL("excessive TT error %lg\n", diff);

    utc = Astronomy_UtcFromTime(time);
    if (utc.year != year || utc.month != month || utc.day != day || utc.hour != hour || utc.minute != minute)
    {
        FFAIL("UtcFromTime FAILURE - Expected %04d-%02d-%02dT%02d:%02dZ, found %04d-%02d-%02dT%02d:%02dZ\n",
            year, month, day, hour, minute,
            utc.year, utc.month, utc.day, utc.hour, utc.minute);
    }

    diff = utc.second - second;
    if (ABS(diff) > 2.0e-5)
        FFAIL("excessive UTC second error %lg\n", diff);

    time = Astronomy_MakeTime(2020, 12, 31, 23, 59, 59.4994);
    CHECK(CheckTimeFormat(time, TIME_FORMAT_MILLI,  ASTRO_SUCCESS, "2020-12-31T23:59:59.499Z"));
    CHECK(CheckTimeFormat(time, TIME_FORMAT_SECOND, ASTRO_SUCCESS, "2020-12-31T23:59:59Z"));
    CHECK(CheckTimeFormat(time, TIME_FORMAT_MINUTE, ASTRO_SUCCESS, "2021-01-01T00:00Z"));
    CHECK(CheckTimeFormat(time, TIME_FORMAT_DAY,    ASTRO_SUCCESS, "2020-12-31"));

    time = Astronomy_MakeTime(2020, 12, 31, 23, 59, 59.500);
    CHECK(CheckTimeFormat(time, TIME_FORMAT_MILLI,  ASTRO_SUCCESS, "2020-12-31T23:59:59.500Z"));
    CHECK(CheckTimeFormat(time, TIME_FORMAT_SECOND, ASTRO_SUCCESS, "2021-01-01T00:00:00Z"));
    CHECK(CheckTimeFormat(time, TIME_FORMAT_MINUTE, ASTRO_SUCCESS, "2021-01-01T00:00Z"));
    CHECK(CheckTimeFormat(time, TIME_FORMAT_DAY,    ASTRO_SUCCESS, "2020-12-31"));

    FPASS();
fail:
    return error;
}

static int WriteJupiterMoon(FILE *outfile, int moon_index, astro_state_vector_t moon)
{
    int error;

    CHECK_STATUS(moon);

    fprintf(outfile, "j %d %0.18le %0.18le %0.18le %0.18le %0.18le %0.18le %0.18le %0.18le\n",
        moon_index,
        moon.t.tt, moon.t.ut,
        moon.x,  moon.y,  moon.z,
        moon.vx, moon.vy, moon.vz
    );

fail:
    return error;
}

static int AstroCheck(void)
{
    int error = 1;
    FILE *outfile = NULL;
    const char *filename = "temp/c_check.txt";
    astro_time_t time;
    astro_time_t stop;
    astro_body_t body;
    astro_vector_t pos;
    astro_equatorial_t j2000;
    astro_equatorial_t ofdate;
    astro_horizon_t hor;
    astro_jupiter_moons_t jm;
    astro_spherical_t sphere;
    astro_observer_t observer = Astronomy_MakeObserver(29.0, -81.0, 10.0);
    int b;
    static const astro_body_t bodylist[] =  /* match the order in the JavaScript unit test */
    {
        BODY_SUN, BODY_MERCURY, BODY_VENUS, BODY_EARTH, BODY_MARS,
        BODY_JUPITER, BODY_SATURN, BODY_URANUS, BODY_NEPTUNE, BODY_PLUTO,
        BODY_SSB, BODY_EMB
    };
    static int nbodies = sizeof(bodylist) / sizeof(bodylist[0]);

    outfile = fopen(filename, "wt");
    if (outfile == NULL)
        FFAIL("Cannot open output file: %s\n", filename);

    fprintf(outfile, "o %lf %lf %lf\n", observer.latitude, observer.longitude, observer.height);

    time = Astronomy_MakeTime(1700, 1, 1, 0, 0, 0.0);
    stop = Astronomy_MakeTime(2200, 1, 1, 0, 0, 0.0);
    while (time.tt < stop.tt)
    {
        for (b=0; b < nbodies; ++b)
        {
            body = bodylist[b];
            CHECK_VECTOR(pos, Astronomy_HelioVector(body, time));
            fprintf(outfile, "v %s %0.18le %0.18le %0.18le %0.18le\n", Astronomy_BodyName(body), pos.t.tt, pos.x, pos.y, pos.z);

            if (body != BODY_EARTH && body != BODY_EMB && body != BODY_SSB)
            {
                CHECK_EQU(j2000, Astronomy_Equator(body, &time, observer, EQUATOR_J2000, NO_ABERRATION));
                CHECK_EQU(ofdate, Astronomy_Equator(body, &time, observer, EQUATOR_OF_DATE, ABERRATION));
                hor = Astronomy_Horizon(&time, observer, ofdate.ra, ofdate.dec, REFRACTION_NONE);
                fprintf(outfile, "s %s %0.18le %0.18le %0.18le %0.18le %0.18le %0.18le %0.18le\n",
                    Astronomy_BodyName(body), time.tt, time.ut, j2000.ra, j2000.dec, j2000.dist, hor.azimuth, hor.altitude);
            }
        }

        CHECK_VECTOR(pos, Astronomy_GeoVector(BODY_MOON, time, NO_ABERRATION));
        fprintf(outfile, "v GM %0.18le %0.18le %0.18le %0.18le\n", pos.t.tt, pos.x, pos.y, pos.z);

        CHECK_EQU(j2000, Astronomy_Equator(BODY_MOON, &time, observer, EQUATOR_J2000, NO_ABERRATION));
        CHECK_EQU(ofdate, Astronomy_Equator(BODY_MOON, &time, observer, EQUATOR_OF_DATE, ABERRATION));
        hor = Astronomy_Horizon(&time, observer, ofdate.ra, ofdate.dec, REFRACTION_NONE);
        fprintf(outfile, "s GM %0.18le %0.18le %0.18le %0.18le %0.18le %0.18le %0.18le\n",
            time.tt, time.ut, j2000.ra, j2000.dec, j2000.dist, hor.azimuth, hor.altitude);

        /* Calculate Jupiter's moons for diff purposes only. */
        jm = Astronomy_JupiterMoons(time);
        WriteJupiterMoon(outfile, 0, jm.io);
        WriteJupiterMoon(outfile, 1, jm.europa);
        WriteJupiterMoon(outfile, 2, jm.ganymede);
        WriteJupiterMoon(outfile, 3, jm.callisto);

        /* Nutation calculations. */
        fprintf(outfile, "n %0.18le %0.18le\n", time.psi, time.eps);

        /* Moon's ecliptic coordinates in true equinox of date. */
        sphere = Astronomy_EclipticGeoMoon(time);
        CHECK_STATUS(sphere);
        fprintf(outfile, "m %0.18le %0.18le %0.18le\n", sphere.lat, sphere.lon, sphere.dist);

        time = Astronomy_AddDays(time, 10.0 + PI/100.0);
    }

fail:
    if (outfile != NULL)
        fclose(outfile);
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static double OrbitRange(const char *name)
{
    /* Return the minimum distance from a planet to the Sun, in AU. */
    if (!strcmp(name, "Mercury"))   return 0.307;
    if (!strcmp(name, "Venus"))     return 0.718;
    if (!strcmp(name, "Earth"))     return 0.983;
    if (!strcmp(name, "EMB"))       return 0.983;
    if (!strcmp(name, "Mars"))      return 1.382;
    if (!strcmp(name, "Jupiter"))   return 4.951;
    if (!strcmp(name, "Saturn"))    return 9.014;
    if (!strcmp(name, "Uranus"))    return 18.31;
    if (!strcmp(name, "Neptune"))   return 29.76;
    if (!strcmp(name, "Pluto"))     return 29.73;

    /* GM is geocentric moon */
    if (!strcmp(name, "GM"))        return 0.00243;

    /* For Solar System Barycenter, we use typical distance. */
    if (!strcmp(name, "SSB"))       return 0.005;

    /* The Sun vector is always (0, 0, 0), so range doesn't matter. */
    if (!strcmp(name, "Sun"))       return 1.0;

    /* Jovicentric distances for Jupiter's moons. */
    if (!strcmp(name, "jm0"))   return 0.00282;      /* Io */
    if (!strcmp(name, "jm1"))   return 0.00448;      /* Europa */
    if (!strcmp(name, "jm2"))   return 0.00716;      /* Ganymede */
    if (!strcmp(name, "jm3"))   return 0.01259;      /* Callisto */

    fprintf(stderr, "FATAL(%s): unknown body name '%s'\n", __func__, name);
    exit(1);
}

static double OrbitSpeed(const char *name)
{
    /* Return the orbital speed (AU/day) of Jupiter's moons. */
    if (!strcmp(name, "jm0"))   return 0.0100;      /* Io */
    if (!strcmp(name, "jm1"))   return 0.0079;      /* Europa */
    if (!strcmp(name, "jm2"))   return 0.0063;      /* Ganymede */
    if (!strcmp(name, "jm3"))   return 0.0047;      /* Callisto */

    fprintf(stderr, "FATAL(%s): unknown body name '%s'\n", __func__, name);
    exit(1);
}

static double TopoRange(const char *name)
{
    /* Return the minimum distance from a planet to the Earth, in AU. */
    if (!strcmp(name, "Mercury"))   return 1.0 - 0.307;
    if (!strcmp(name, "Venus"))     return 1.0 - 0.718;
    if (!strcmp(name, "Earth"))     return 1.0;             /* we don't calculate topocentric Earth. */
    if (!strcmp(name, "EMB"))       return 1.0;             /* we don't calculate topocentric EMB */
    if (!strcmp(name, "Mars"))      return 1.382 - 1.0;
    if (!strcmp(name, "Jupiter"))   return 4.951 - 1.0;
    if (!strcmp(name, "Saturn"))    return 9.014 - 1.0;
    if (!strcmp(name, "Uranus"))    return 18.31 - 1.0;
    if (!strcmp(name, "Neptune"))   return 29.76 - 1.0;
    if (!strcmp(name, "Pluto"))     return 29.73 - 1.0;

    /* GM is geocentric moon, so use minimum distance to Earth. */
    if (!strcmp(name, "GM"))        return 0.00243;

    /* For Solar System Barycenter, we use typical distance. */
    if (!strcmp(name, "SSB"))       return 1.0;

    /* The Sun vector is always (0, 0, 0), so range doesn't matter. */
    if (!strcmp(name, "Sun"))       return 1.0;

    fprintf(stderr, "FATAL(%s): unknown body name '%s'\n", __func__, name);
    exit(1);
}


typedef struct
{
    const char *name;   /* the name of the column/field */
    int cos_index;      /* moderate longitude-like differences by the cosine of the indicated latitude-like field (or -1 if N/A) */
    int wrap;           /* the value at which an angular measure can wrap around, or 0 if N/A */
    double range;       /* moderate differences by dividing by this range of possible values (or special codes for 0, -1, -2) */
}
maxdiff_settings_t;

static const maxdiff_settings_t DiffSettings[NUM_DIFF_COLUMNS] =
{
    /* 'v' lines */
    { "helio_tt",       -1,     0,      1.0 },      /*  0 */
    { "helio_x",        -1,     0,      0.0 },      /*  1 */
    { "helio_y",        -1,     0,      0.0 },      /*  2 */
    { "helio_z",        -1,     0,      0.0 },      /*  3 */

    /* 's' lines */
    { "sky_tt",         -1,     0,      1.0 },      /*  4 */
    { "sky_ut",         -1,     0,      1.0 },      /*  5 */
    { "sky_j2000_ra",    7,    24,     24.0 },      /*  6 */
    { "sky_j2000_dec",  -1,     0,    180.0 },      /*  7 */
    { "sky_j2000_dist", -1,     0,     -1.0 },      /*  8 */
    { "sky_hor_az",     10,   360,    360.0 },      /*  9 */
    { "sky_hor_alt",    -1,     0,    180.0 },      /* 10 */

    /* 'j' lines */
    { "jm_tt",          -1,     0,      1.0 },      /* 11 */
    { "jm_ut",          -1,     0,      1.0 },      /* 12 */
    { "jm_x",           -1,     0,      0.0 },      /* 13 */
    { "jm_y",           -1,     0,      0.0 },      /* 14 */
    { "jm_z",           -1,     0,      0.0 },      /* 15 */
    { "jm_vx",          -1,     0,     -2.0 },      /* 16 */
    { "jm_vy",          -1,     0,     -2.0 },      /* 17 */
    { "jm_vz",          -1,     0,     -2.0 },      /* 18 */

    /* 'n' lines: nutation angles */
    { "psi",            -1,     0,     10.0 },      /* 19 */
    { "eps",            -1,     0,     10.0 },      /* 20 */

    /* 'm' lines: EclipticGeoMoon calculations. */
    { "moon_lat",       -1,     0,    180.0 },      /* 21 */
    { "moon_lon",       21,   360,    360.0 },      /* 22 */
    { "moon_dist",      -1,     0,    0.003 }       /* 23 */
};

static int Diff(double tolerance, const char *a_filename, const char *b_filename)
{
    int error = 1;
    int lnum;
    int i;
    FILE *afile = NULL;
    FILE *bfile = NULL;
    char aline[300];
    char bline[300];
    char *aread;
    char *bread;
    maxdiff_column_t columns[NUM_DIFF_COLUMNS];
    double score = 0.0;

    memset(columns, 0, sizeof(columns));

    afile = fopen(a_filename, "rt");
    if (afile == NULL)
        FFAIL("Cannot open input file: %s\n", a_filename);

    bfile = fopen(b_filename, "rt");
    if (bfile == NULL)
        FFAIL("Cannot open input file: %s\n", b_filename);

    lnum = 0;
    for(;;)
    {
        aread = ReadLine(aline, sizeof(aline), afile, a_filename, lnum);
        bread = ReadLine(bline, sizeof(bline), bfile, b_filename, lnum);
        if (aread==NULL && bread==NULL)
            break;      /* normal end of both files */

        if (aread==NULL || bread==NULL)
            FFAIL("Files do not have same number of lines: %s and %s\n", a_filename, b_filename);

        ++lnum;
        CHECK(DiffLine(lnum, aline, bline, columns));
    }

    error = 0;

    printf("First  file: %s\n", a_filename);
    printf("Second file: %s\n", b_filename);
    printf("Tolerance = %0.3le\n\n", tolerance);
    printf("      %10s %23s %23s %10s %10s  %s\n", "lnum", "a_value", "b_value", "factor", "diff", "name");
    for (i = 0; i < NUM_DIFF_COLUMNS; ++i)
    {
        if (columns[i].lnum > 0)
        {
            const char *result = "OK";
            if (columns[i].diff > tolerance)
            {
                result = "FAIL";
                error = 1;
            }

            if (columns[i].diff > score)
                score = columns[i].diff;

            printf("%4s  %10d %23.16le %23.16le %10.5lf %10.3le  %s\n",
                result,
                columns[i].lnum,
                columns[i].a_value,
                columns[i].b_value,
                columns[i].factor,
                columns[i].diff,
                DiffSettings[i].name);
        }
    }

    printf("\nScore = %0.3le\n", score);
    if (error)
        printf("ctest(Diff): EXCEEDED ERROR TOLERANCE.\n");
    printf("----------------------------------------------------------------------------------------------------\n\n");

fail:
    if (afile != NULL) fclose(afile);
    if (bfile != NULL) fclose(bfile);
    return error;
}

static int DiffLine(int lnum, const char *aline, const char *bline, maxdiff_column_t columns[])
{
    int error = 1;
    char abody[10];
    char bbody[10];
    double adata[8];
    double bdata[8];
    double diff;
    int i, na, nb, nrequired = -1;
    int colbase = -1;
    int mindex_a, mindex_b;     /* index of Jupiter's moon: 0..3 */
    maxdiff_column_t *col;

    /* be paranoid: make sure we can't possibly have a fake match. */
    memset(adata, 0xdc, sizeof(adata));
    memset(bdata, 0xce, sizeof(bdata));

    /* Make sure the two data records are the same type. */
    if (aline[0] != bline[0])
        FFAIL("Line %d mismatch record type: '%c' vs '%c'.\n", lnum, aline[0], bline[0]);

    abody[0] = bbody[0] = '\0';     /* default to no body name */

    switch (aline[0])
    {
    case 'o':       /* observer */
        na = sscanf(aline, "o %lf %lf %lf", &adata[0], &adata[1], &adata[2]);
        if (na != 3)
            FFAIL("Bad observer on line %d of first file\n", lnum);
        nb = sscanf(bline, "o %lf %lf %lf", &bdata[0], &bdata[1], &bdata[2]);
        if (nb != 3)
            FFAIL("Bad observer on line %d of second file\n", lnum);
        if (adata[0] != bdata[0] || adata[1] != bdata[1] || adata[2] != bdata[2])
            FFAIL("Observers are not identical on line %d\n", lnum);
        return 0;

    case 'v':       /* heliocentric vector: tt x y z */
        colbase = 0;
        na = sscanf(aline, "v %9[A-Za-z] %lf %lf %lf %lf", abody, &adata[0], &adata[1], &adata[2], &adata[3]);
        nb = sscanf(bline, "v %9[A-Za-z] %lf %lf %lf %lf", bbody, &bdata[0], &bdata[1], &bdata[2], &bdata[3]);
        nrequired = 5;
        break;

    case 's':       /* sky coords: ecliptic and horizontal */
        /* Astronomy_BodyName(body), [0] time.tt, [1] time.ut, [2] j2000.ra, [3] j2000.dec, [4] j2000.dist, [5] hor.azimuth, [6] hor.altitude */
        colbase = NUM_V_COLUMNS;
        na = sscanf(aline, "s %9[A-Za-z] %lf %lf %lf %lf %lf %lf %lf", abody, &adata[0], &adata[1], &adata[2], &adata[3], &adata[4], &adata[5], &adata[6]);
        nb = sscanf(bline, "s %9[A-Za-z] %lf %lf %lf %lf %lf %lf %lf", bbody, &bdata[0], &bdata[1], &bdata[2], &bdata[3], &bdata[4], &bdata[5], &bdata[6]);
        nrequired = 8;
        break;

    case 'j':       /* Jupiter's moons:   j moon[0..3] tt ut x y z vx vy vz */
        colbase = NUM_V_COLUMNS + NUM_S_COLUMNS;
        na = sscanf(aline, "j %d %lf %lf %lf %lf %lf %lf %lf %lf", &mindex_a, &adata[0], &adata[1], &adata[2], &adata[3], &adata[4], &adata[5], &adata[6], &adata[7]);
        nb = sscanf(bline, "j %d %lf %lf %lf %lf %lf %lf %lf %lf", &mindex_b, &bdata[0], &bdata[1], &bdata[2], &bdata[3], &bdata[4], &bdata[5], &bdata[6], &bdata[7]);
        nrequired = 9;
        if (mindex_a < 0 || mindex_a >= 4 || mindex_a != mindex_b)
            FFAIL("Bad Jupiter moon index in line %d: mindex_a=%d, mindex_b=%d\n", lnum, mindex_a, mindex_b);
        snprintf(abody, sizeof(abody), "jm%d", mindex_a);
        snprintf(bbody, sizeof(bbody), "jm%d", mindex_b);
        break;

    case 'n':   /* nutation angles */
        colbase = NUM_V_COLUMNS + NUM_S_COLUMNS + NUM_J_COLUMNS;
        na = sscanf(aline, "n %lf %lf", &adata[0], &adata[1]);
        nb = sscanf(bline, "n %lf %lf", &bdata[0], &bdata[1]);
        nrequired = 2;
        break;

    case 'm':   /* EclipticGeoMoon */
        colbase = NUM_V_COLUMNS + NUM_S_COLUMNS + NUM_J_COLUMNS + NUM_N_COLUMNS;
        na = sscanf(aline, "m %lf %lf %lf", &adata[0], &adata[1], &adata[2]);
        nb = sscanf(bline, "m %lf %lf %lf", &bdata[0], &bdata[1], &bdata[2]);
        nrequired = 3;
        break;

    default:
        FFAIL("Line %d type '%c' is not a valid record type.\n", lnum, aline[0]);
    }

    if (na != nb)
        FFAIL("Line %d mismatch data counts: %d vs %d\n", lnum, na, nb);

    if (na != nrequired)
        FFAIL("Line %d incorrect number of scanned arguments: %d\n", lnum, na);

    if (strcmp(abody, bbody))
        FFAIL("Line %d body mismatch: '%s' vs '%s'\n.", lnum, abody, bbody);

    if (abody[0])
    {
        /* This is one of the record types that contains a body name. */
        /* Therefore, we need to correct the number of numeric data. */
        --nrequired;
    }

    /* Verify all the numeric data are very close. */
    for (i=0; i < nrequired; ++i)
    {
        double factor, denom;
        int ci, w;
        int k = colbase + i;

        /* Life is too short to debug memory corruption errors. */
        if (k < 0 || k >= NUM_DIFF_COLUMNS)
            FFAIL("Internal error on line %d: k=%d\n", lnum, k);

        ci = DiffSettings[k].cos_index;
        w = DiffSettings[k].wrap;

        denom = NAN;
        if (DiffSettings[k].range == -2.0)
            denom = OrbitSpeed(abody);
        else if (DiffSettings[k].range == -1.0)
            denom = TopoRange(abody);
        else if (DiffSettings[k].range == 0.0)
            denom = OrbitRange(abody);
        else if (DiffSettings[k].range > 0.0)
            denom = DiffSettings[k].range;
        else
            FFAIL("Invalid range value: %lf\n", DiffSettings[k].range);

        if (!isfinite(denom) || denom <= 0.0)
            FFAIL("Invalid denominator value: %lf\n", denom);

        factor = V(1.0 / denom);

        diff = ABS(adata[i] - bdata[i]);
        if ((w > 0) && (diff > w / 2))
        {
            /* Tolerate differences in angular values that wrap around at a periodic limit. */
            diff = ABS(w - diff);
        }

        if (ci >= 0)
        {
            /*
                Longitude-like angles (right ascension and azimuth) become less significant as their
                counterpart latitude-like angle gets closer to its poles.
                For example, if horizontal altitude is close to 90 degrees (near the zenith),
                then an azimuth error is not very important.
                So we multiply errors for these types of numbers by the cosine of their counterpart.
                Make sure we calculate identical values regardless of the order of the filenames.
                Therefore, use the arithmetic mean of the two latitude angles.
            */
            factor *= ABS(cos(DEG2RAD * ((adata[ci] + bdata[ci]) / 2.0)));
        }

        diff *= factor;

        /* Remember the worst difference for each different type of calculation. */
        col = &columns[k];
        if (diff > col->diff)
        {
            col->diff = diff;
            col->lnum = lnum;
            col->a_value = adata[i];
            col->b_value = bdata[i];
            col->factor = factor;
        }
    }
    error = 0;

fail:
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int SeasonsTest(void)
{
    const char *filename = "seasons/seasons.txt";
    int error = 1;
    int lnum;
    FILE *infile = NULL;
    char line[200];
    int nscanned, year, month, day, hour, minute;
    int current_year = 0;
    char name[20];
    astro_time_t correct_time;
    astro_time_t calc_time;
    astro_seasons_t seasons;
    double diff_minutes, max_minutes = 0.0;
    int mar_count=0, jun_count=0, sep_count=0, dec_count=0;

    memset(&seasons, 0, sizeof(seasons));

    infile = fopen(filename, "rt");
    if (infile == NULL)
        FFAIL("Cannot open input file: %s\n", filename);

    lnum = 0;
    while (ReadLine(line, sizeof(line), infile, filename, lnum))
    {
        ++lnum;
        /*
            2019-01-03T05:20Z Perihelion
            2019-03-20T21:58Z Equinox
            2019-06-21T15:54Z Solstice
            2019-07-04T22:11Z Aphelion
            2019-09-23T07:50Z Equinox
            2019-12-22T04:19Z Solstice
        */
        nscanned = sscanf(line, "%d-%d-%dT%d:%dZ %10[A-Za-z]", &year, &month, &day, &hour, &minute, name);
        if (nscanned != 6)
            FFAIL("%s line %d : scanned %d, expected 6\n", filename, lnum, nscanned);

        if (year != current_year)
        {
            current_year = year;
            seasons = Astronomy_Seasons(year);
            if (seasons.status != ASTRO_SUCCESS)
                FFAIL("Astronomy_Seasons(%d) returned %d\n", year, seasons.status);
        }

        memset(&calc_time, 0xcd, sizeof(calc_time));
        correct_time = Astronomy_MakeTime(year, month, day, hour, minute, 0.0);
        if (!strcmp(name, "Equinox"))
        {
            switch (month)
            {
            case 3:
                calc_time = seasons.mar_equinox;
                ++mar_count;
                break;
            case 9:
                calc_time = seasons.sep_equinox;
                ++sep_count;
                break;
            default:
                FFAIL("Invalid equinox date in test data: %s line %d\n", filename, lnum);
            }
        }
        else if (!strcmp(name, "Solstice"))
        {
            switch (month)
            {
            case 6:
                calc_time = seasons.jun_solstice;
                ++jun_count;
                break;
            case 12:
                calc_time = seasons.dec_solstice;
                ++dec_count;
                break;
            default:
                FFAIL("Invalid solstice date in test data: %s line %d\n", filename, lnum);
            }
        }
        else if (!strcmp(name, "Aphelion"))
        {
            /* not yet calculated */
            continue;
        }
        else if (!strcmp(name, "Perihelion"))
        {
            /* not yet calculated */
            continue;
        }
        else
            FFAIL("%s line %d: unknown event type '%s'\n", filename, lnum, name);

        /* Verify that the calculated time matches the correct time for this event. */
        diff_minutes = (24.0 * 60.0) * ABS(calc_time.tt - correct_time.tt);
        if (diff_minutes > max_minutes)
            max_minutes = diff_minutes;

        if (diff_minutes > 2.37)
            FFAIL("%s line %d: excessive error (%s): %lf minutes.\n", filename, lnum, name, diff_minutes);
    }

    printf("C %s: verified %d lines from file %s : max error minutes = %0.3lf\n", __func__, lnum, filename, max_minutes);
    printf("C %s: Event counts: mar=%d, jun=%d, sep=%d, dec=%d\n", __func__, mar_count, jun_count, sep_count, dec_count);
    error = 0;

fail:
    if (infile != NULL) fclose(infile);
    return error;
}

static int SeasonsIssue187(void)
{
    int error, year;
    astro_seasons_t seasons;
    char text[TIME_TEXT_BYTES];

    // This is a regression test for:
    // https://github.com/cosinekitty/astronomy/issues/187
    // For years far from the present, the seasons search was sometimes failing.

    for (year = -2000; year <= +9999; ++year)
    {
        seasons = Astronomy_Seasons(year);
        if (seasons.status != ASTRO_SUCCESS)
            FFAIL("Search error %d for year %d.\n", (int)seasons.status, year);

        if (Verbose && ((year > 0) && (year % 1000 == 999)))
        {
            printf("C %s: DEBUG", __func__);
            Astronomy_FormatTime(seasons.mar_equinox, TIME_FORMAT_DAY, text, sizeof(text));
            printf(" %s", text);
            Astronomy_FormatTime(seasons.jun_solstice, TIME_FORMAT_DAY, text, sizeof(text));
            printf(" %s", text);
            Astronomy_FormatTime(seasons.sep_equinox, TIME_FORMAT_DAY, text, sizeof(text));
            printf(" %s", text);
            Astronomy_FormatTime(seasons.dec_solstice, TIME_FORMAT_DAY, text, sizeof(text));
            printf(" %s\n", text);
        }
    }

    FPASS();
fail:
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int MoonPhase(void)
{
    const char *filename = "moonphase/moonphases.txt";
    int error = 1;
    FILE *infile = NULL;
    int lnum, nscanned;
    int quarter, year, month, day, hour, minute;
    int expected_quarter, quarter_count = 0;
    int prev_year = 0;
    double second, expected_elong;
    astro_time_t expected_time, start_time;
    astro_angle_result_t result;
    double degree_error, arcmin, max_arcmin = 0.0;
    double diff_seconds, maxdiff = 0.0;
    const double threshold_seconds = 90.0; /* max tolerable prediction error in seconds */
    astro_moon_quarter_t mq;
    char line[200];

    memset(&mq, 0xcd, sizeof(mq));

    infile = fopen(filename, "rt");
    if (infile == NULL)
        FFAIL("Cannot open input file '%s'\n", filename);

    /*
        0 1800-01-25T03:21:00.000Z
        1 1800-02-01T20:40:00.000Z
        2 1800-02-09T17:26:00.000Z
        3 1800-02-16T15:49:00.000Z
    */
    lnum = 0;
    while (ReadLine(line, sizeof(line), infile, filename, lnum))
    {
        ++lnum;
        nscanned = sscanf(line, "%d %d-%d-%dT%d:%d:%lfZ", &quarter, &year, &month, &day, &hour, &minute, &second);
        if (nscanned != 7)
            FLNFAIL("Invalid data format\n");

        if (quarter < 0 || quarter > 3)
            FLNFAIL("Invalid quarter %d\n", quarter);

        expected_elong = 90.0 * quarter;
        expected_time = Astronomy_MakeTime(year, month, day, hour, minute, second);
        result = Astronomy_MoonPhase(expected_time);
        degree_error = ABS(result.angle - expected_elong);
        if (degree_error > 180.0)
            degree_error = 360 - degree_error;
        arcmin = 60.0 * degree_error;

        if (arcmin > 1.0)
            FLNFAIL("EXCESSIVE ANGULAR ERROR: %lg arcmin\n", arcmin);

        if (arcmin > max_arcmin)
            max_arcmin = arcmin;

        if (year != prev_year)
        {
            prev_year = year;
            /* The test data contains a single year's worth of data for every 10 years. */
            /* Every time we see the year value change, it breaks continuity of the phases. */
            /* Start the search over again. */
            start_time = Astronomy_MakeTime(year, 1, 1, 0, 0, 0.0);
            mq = Astronomy_SearchMoonQuarter(start_time);
            expected_quarter = -1;  /* we have no idea what the quarter should be */
        }
        else
        {
            /* Yet another lunar quarter in the same year. */
            expected_quarter = (1 + mq.quarter) % 4;
            mq = Astronomy_NextMoonQuarter(mq);

            /* Make sure we find the next expected quarter. */
            if (expected_quarter != mq.quarter)
                FLNFAIL("Astronomy_SearchMoonQuarter returned quarter %d, but expected %d\n", mq.quarter, expected_quarter);
        }

        if (mq.status != ASTRO_SUCCESS)
            FLNFAIL("Astronomy_SearchMoonQuarter returned %d\n", mq.status);

        ++quarter_count;

        /* Make sure the time matches what we expect. */
        diff_seconds = ABS(mq.time.tt - expected_time.tt) * (24.0 * 3600.0);
        if (diff_seconds > threshold_seconds)
            FLNFAIL("excessive time error %0.3lf seconds\n", diff_seconds);

        if (diff_seconds > maxdiff)
            maxdiff = diff_seconds;
    }

    printf("C %s: passed %d lines for file %s : max_arcmin = %0.6lf, maxdiff = %0.3lf seconds, %d quarters\n", __func__, lnum, filename, max_arcmin, maxdiff, quarter_count);
    error = 0;

fail:
    if (infile != NULL) fclose(infile);
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int MoonReversePhase(double longitude)
{
    /* Verify that Astronomy_SearchMoonPhase works both forward and backward in time. */

    const int nphases = 5000;   /* approximately 400 years worth */
    int error = 1;
    int i;
    astro_time_t time;
    astro_search_result_t result;
    double *utList = NULL;
    double dtMin = +1000.0;
    double dtMax = -1000.0;
    double diff, maxDiff = 0.0;
    const int nslots = 100;
    int k;
    double ut, ut1, ut2;

    /* Allocate memory to hold times of consecutive moon phases. */
    utList = (double *)calloc(nphases, sizeof(utList[0]));
    if (utList == NULL)
        FFAIL("cannot allocate memory for longitude %0.0lf\n", longitude);

    /* Search forward in time from 1800 to find consecutive new moon events. */
    time = Astronomy_MakeTime(1800, 1, 1, 0, 0, 0.0);
    for (i = 0; i < nphases; ++i)
    {
        result = Astronomy_SearchMoonPhase(longitude, time, +40.0);
        CHECK_STATUS(result);
        utList[i] = result.time.ut;
        if (i > 0)
        {
            /* Verify that consecutive events are reasonably close to the synodic period (29.5 days) apart. */
            double dt = V(utList[i] - utList[i-1]);
            if (dt < dtMin) dtMin = dt;
            if (dt > dtMax) dtMax = dt;
        }
        time = Astronomy_AddDays(result.time, +0.1);
    }

    FDEBUG("longitude %0.0lf: dtMin=%0.3lf days, dtMax=%0.3lf days.\n", longitude, dtMin, dtMax);
    if (dtMin < 29.175 || dtMax > 29.926)
        FFAIL("Time between consecutive new moons is suspicious.\n");

    /* Do a reverse chronological search and make sure the results are consistent with the forward search. */
    time = Astronomy_AddDays(time, 20.0);
    for (i = nphases-1; i >= 0; --i)
    {
        result = Astronomy_SearchMoonPhase(longitude, time, -40.0);
        CHECK_STATUS(result);
        diff = SECONDS_PER_DAY * ABS(result.time.ut - utList[i]);
        if (diff > maxDiff) maxDiff = diff;
        time = Astronomy_AddDays(result.time, -0.1);
    }

    FDEBUG("longitude %0.0lf: Maximum discrepancy in reverse search = %0.3lf seconds.\n", longitude, maxDiff);
    if (maxDiff > 0.165)
        FFAIL("EXCESSIVE DISCREPANCY in reverse search.\n");

    /* Pick a pair of consecutive events from the middle of the list. */
    /* Verify forward and backward searches work correctly from many intermediate times. */
    k = nphases / 2;
    ut1 = utList[k];
    ut2 = utList[k+1];
    for (i = 1; i < nslots; ++i)
    {
        ut = ut1 + ((double)i/nslots)*(ut2 - ut1);
        time = Astronomy_TimeFromDays(ut);

        result = Astronomy_SearchMoonPhase(longitude, time, -40.0);
        CHECK_STATUS(result);
        diff = SECONDS_PER_DAY * ABS(result.time.ut - ut1);
        if (diff > 0.07)
            FFAIL("longitude %0.0lf: backward search error = %0.4le seconds.\n", longitude, diff);

        result = Astronomy_SearchMoonPhase(longitude, time, +40.0);
        CHECK_STATUS(result);
        diff = SECONDS_PER_DAY * ABS(result.time.ut - ut2);
        if (diff > 0.07)
            FFAIL("longitude %0.0lf: forward search error = %0.4le seconds.\n", longitude, diff);
    }

    printf("C %s(%0.0lf): PASS\n", __func__, longitude);
    error = 0;
fail:
    free(utList);
    return error;
}


static int MoonReverse(void)
{
    return
        MoonReversePhase(  0.0) ||
        MoonReversePhase( 90.0) ||
        MoonReversePhase(180.0) ||
        MoonReversePhase(270.0);
}


/*-----------------------------------------------------------------------------------------------------------*/

static int TestElongFile(const char *filename, double targetRelLon)
{
    int error = 1;
    FILE *infile = NULL;
    int lnum;
    char line[100];
    char name[20];
    int year, month, day, hour, minute;
    int nscanned;
    astro_time_t search_date, expected_time;
    astro_body_t body;
    astro_search_result_t search_result;
    double diff_minutes;

    infile = fopen(filename, "rt");
    if (infile == NULL)
        FFAIL("Cannot open input file: %s\n", filename);

    lnum = 0;
    while (ReadLine(line, sizeof(line), infile, filename, lnum))
    {
        ++lnum;

        /* 2018-05-09T00:28Z Jupiter */
        nscanned = sscanf(line, "%d-%d-%dT%d:%dZ %9[A-Za-z]", &year, &month, &day, &hour, &minute, name);
        if (nscanned != 6)
            FLNFAIL("Invalid data format.\n");

        body = Astronomy_BodyCode(name);
        if (body == BODY_INVALID)
            FLNFAIL("Invalid body name '%s'\n", name);

        search_date = Astronomy_MakeTime(year, 1, 1, 0, 0, 0.0);
        expected_time = Astronomy_MakeTime(year, month, day, hour, minute, 0.0);
        search_result = Astronomy_SearchRelativeLongitude(body, targetRelLon, search_date);
        if (search_result.status != ASTRO_SUCCESS)
            FLNFAIL("SearchRelativeLongitude returned %d\n", search_result.status);

        diff_minutes = (24.0 * 60.0) * (search_result.time.tt - expected_time.tt);
        FDEBUG("%-7s error = %6.3lf minutes\n", name, diff_minutes);
        if (ABS(diff_minutes) > 6.8)
            FLNFAIL("EXCESSIVE ERROR\n");
    }

    FPASSA("%d rows of data\n", lnum);
fail:
    if (infile != NULL) fclose(infile);
    return error;
}

typedef struct
{
    astro_body_t        body;
    const char         *searchDate;
    const char         *eventDate;
    double              angle;
    astro_visibility_t  visibility;
}
elong_test_t;

static const elong_test_t ElongTestData[] =
{
    /* Max elongation data obtained from: */
    /* http://www.skycaramba.com/greatest_elongations.shtml */
    { BODY_MERCURY, "2010-01-17T05:22Z", "2010-01-27T05:22Z", 24.80, VISIBLE_MORNING },
    { BODY_MERCURY, "2010-05-16T02:15Z", "2010-05-26T02:15Z", 25.10, VISIBLE_MORNING },
    { BODY_MERCURY, "2010-09-09T17:24Z", "2010-09-19T17:24Z", 17.90, VISIBLE_MORNING },
    { BODY_MERCURY, "2010-12-30T14:33Z", "2011-01-09T14:33Z", 23.30, VISIBLE_MORNING },
    { BODY_MERCURY, "2011-04-27T19:03Z", "2011-05-07T19:03Z", 26.60, VISIBLE_MORNING },
    { BODY_MERCURY, "2011-08-24T05:52Z", "2011-09-03T05:52Z", 18.10, VISIBLE_MORNING },
    { BODY_MERCURY, "2011-12-13T02:56Z", "2011-12-23T02:56Z", 21.80, VISIBLE_MORNING },
    { BODY_MERCURY, "2012-04-08T17:22Z", "2012-04-18T17:22Z", 27.50, VISIBLE_MORNING },
    { BODY_MERCURY, "2012-08-06T12:04Z", "2012-08-16T12:04Z", 18.70, VISIBLE_MORNING },
    { BODY_MERCURY, "2012-11-24T22:55Z", "2012-12-04T22:55Z", 20.60, VISIBLE_MORNING },
    { BODY_MERCURY, "2013-03-21T22:02Z", "2013-03-31T22:02Z", 27.80, VISIBLE_MORNING },
    { BODY_MERCURY, "2013-07-20T08:51Z", "2013-07-30T08:51Z", 19.60, VISIBLE_MORNING },
    { BODY_MERCURY, "2013-11-08T02:28Z", "2013-11-18T02:28Z", 19.50, VISIBLE_MORNING },
    { BODY_MERCURY, "2014-03-04T06:38Z", "2014-03-14T06:38Z", 27.60, VISIBLE_MORNING },
    { BODY_MERCURY, "2014-07-02T18:22Z", "2014-07-12T18:22Z", 20.90, VISIBLE_MORNING },
    { BODY_MERCURY, "2014-10-22T12:36Z", "2014-11-01T12:36Z", 18.70, VISIBLE_MORNING },
    { BODY_MERCURY, "2015-02-14T16:20Z", "2015-02-24T16:20Z", 26.70, VISIBLE_MORNING },
    { BODY_MERCURY, "2015-06-14T17:10Z", "2015-06-24T17:10Z", 22.50, VISIBLE_MORNING },
    { BODY_MERCURY, "2015-10-06T03:20Z", "2015-10-16T03:20Z", 18.10, VISIBLE_MORNING },
    { BODY_MERCURY, "2016-01-28T01:22Z", "2016-02-07T01:22Z", 25.60, VISIBLE_MORNING },
    { BODY_MERCURY, "2016-05-26T08:45Z", "2016-06-05T08:45Z", 24.20, VISIBLE_MORNING },
    { BODY_MERCURY, "2016-09-18T19:27Z", "2016-09-28T19:27Z", 17.90, VISIBLE_MORNING },
    { BODY_MERCURY, "2017-01-09T09:42Z", "2017-01-19T09:42Z", 24.10, VISIBLE_MORNING },
    { BODY_MERCURY, "2017-05-07T23:19Z", "2017-05-17T23:19Z", 25.80, VISIBLE_MORNING },
    { BODY_MERCURY, "2017-09-02T10:14Z", "2017-09-12T10:14Z", 17.90, VISIBLE_MORNING },
    { BODY_MERCURY, "2017-12-22T19:48Z", "2018-01-01T19:48Z", 22.70, VISIBLE_MORNING },
    { BODY_MERCURY, "2018-04-19T18:17Z", "2018-04-29T18:17Z", 27.00, VISIBLE_MORNING },
    { BODY_MERCURY, "2018-08-16T20:35Z", "2018-08-26T20:35Z", 18.30, VISIBLE_MORNING },
    { BODY_MERCURY, "2018-12-05T11:34Z", "2018-12-15T11:34Z", 21.30, VISIBLE_MORNING },
    { BODY_MERCURY, "2019-04-01T19:40Z", "2019-04-11T19:40Z", 27.70, VISIBLE_MORNING },
    { BODY_MERCURY, "2019-07-30T23:08Z", "2019-08-09T23:08Z", 19.00, VISIBLE_MORNING },
    { BODY_MERCURY, "2019-11-18T10:31Z", "2019-11-28T10:31Z", 20.10, VISIBLE_MORNING },
    { BODY_MERCURY, "2010-03-29T23:32Z", "2010-04-08T23:32Z", 19.40, VISIBLE_EVENING },
    { BODY_MERCURY, "2010-07-28T01:03Z", "2010-08-07T01:03Z", 27.40, VISIBLE_EVENING },
    { BODY_MERCURY, "2010-11-21T15:42Z", "2010-12-01T15:42Z", 21.50, VISIBLE_EVENING },
    { BODY_MERCURY, "2011-03-13T01:07Z", "2011-03-23T01:07Z", 18.60, VISIBLE_EVENING },
    { BODY_MERCURY, "2011-07-10T04:56Z", "2011-07-20T04:56Z", 26.80, VISIBLE_EVENING },
    { BODY_MERCURY, "2011-11-04T08:40Z", "2011-11-14T08:40Z", 22.70, VISIBLE_EVENING },
    { BODY_MERCURY, "2012-02-24T09:39Z", "2012-03-05T09:39Z", 18.20, VISIBLE_EVENING },
    { BODY_MERCURY, "2012-06-21T02:00Z", "2012-07-01T02:00Z", 25.70, VISIBLE_EVENING },
    { BODY_MERCURY, "2012-10-16T21:59Z", "2012-10-26T21:59Z", 24.10, VISIBLE_EVENING },
    { BODY_MERCURY, "2013-02-06T21:24Z", "2013-02-16T21:24Z", 18.10, VISIBLE_EVENING },
    { BODY_MERCURY, "2013-06-02T16:45Z", "2013-06-12T16:45Z", 24.30, VISIBLE_EVENING },
    { BODY_MERCURY, "2013-09-29T09:59Z", "2013-10-09T09:59Z", 25.30, VISIBLE_EVENING },
    { BODY_MERCURY, "2014-01-21T10:00Z", "2014-01-31T10:00Z", 18.40, VISIBLE_EVENING },
    { BODY_MERCURY, "2014-05-15T07:06Z", "2014-05-25T07:06Z", 22.70, VISIBLE_EVENING },
    { BODY_MERCURY, "2014-09-11T22:20Z", "2014-09-21T22:20Z", 26.40, VISIBLE_EVENING },
    { BODY_MERCURY, "2015-01-04T20:26Z", "2015-01-14T20:26Z", 18.90, VISIBLE_EVENING },
    { BODY_MERCURY, "2015-04-27T04:46Z", "2015-05-07T04:46Z", 21.20, VISIBLE_EVENING },
    { BODY_MERCURY, "2015-08-25T10:20Z", "2015-09-04T10:20Z", 27.10, VISIBLE_EVENING },
    { BODY_MERCURY, "2015-12-19T03:11Z", "2015-12-29T03:11Z", 19.70, VISIBLE_EVENING },
    { BODY_MERCURY, "2016-04-08T14:00Z", "2016-04-18T14:00Z", 19.90, VISIBLE_EVENING },
    { BODY_MERCURY, "2016-08-06T21:24Z", "2016-08-16T21:24Z", 27.40, VISIBLE_EVENING },
    { BODY_MERCURY, "2016-12-01T04:36Z", "2016-12-11T04:36Z", 20.80, VISIBLE_EVENING },
    { BODY_MERCURY, "2017-03-22T10:24Z", "2017-04-01T10:24Z", 19.00, VISIBLE_EVENING },
    { BODY_MERCURY, "2017-07-20T04:34Z", "2017-07-30T04:34Z", 27.20, VISIBLE_EVENING },
    { BODY_MERCURY, "2017-11-14T00:32Z", "2017-11-24T00:32Z", 22.00, VISIBLE_EVENING },
    { BODY_MERCURY, "2018-03-05T15:07Z", "2018-03-15T15:07Z", 18.40, VISIBLE_EVENING },
    { BODY_MERCURY, "2018-07-02T05:24Z", "2018-07-12T05:24Z", 26.40, VISIBLE_EVENING },
    { BODY_MERCURY, "2018-10-27T15:25Z", "2018-11-06T15:25Z", 23.30, VISIBLE_EVENING },
    { BODY_MERCURY, "2019-02-17T01:23Z", "2019-02-27T01:23Z", 18.10, VISIBLE_EVENING },
    { BODY_MERCURY, "2019-06-13T23:14Z", "2019-06-23T23:14Z", 25.20, VISIBLE_EVENING },
    { BODY_MERCURY, "2019-10-10T04:00Z", "2019-10-20T04:00Z", 24.60, VISIBLE_EVENING },
    { BODY_VENUS,   "2010-12-29T15:57Z", "2011-01-08T15:57Z", 47.00, VISIBLE_MORNING },
    { BODY_VENUS,   "2012-08-05T08:59Z", "2012-08-15T08:59Z", 45.80, VISIBLE_MORNING },
    { BODY_VENUS,   "2014-03-12T19:25Z", "2014-03-22T19:25Z", 46.60, VISIBLE_MORNING },
    { BODY_VENUS,   "2015-10-16T06:57Z", "2015-10-26T06:57Z", 46.40, VISIBLE_MORNING },
    { BODY_VENUS,   "2017-05-24T13:09Z", "2017-06-03T13:09Z", 45.90, VISIBLE_MORNING },
    { BODY_VENUS,   "2018-12-27T04:24Z", "2019-01-06T04:24Z", 47.00, VISIBLE_MORNING },
    { BODY_VENUS,   "2010-08-10T03:19Z", "2010-08-20T03:19Z", 46.00, VISIBLE_EVENING },
    { BODY_VENUS,   "2012-03-17T08:03Z", "2012-03-27T08:03Z", 46.00, VISIBLE_EVENING },
    { BODY_VENUS,   "2013-10-22T08:00Z", "2013-11-01T08:00Z", 47.10, VISIBLE_EVENING },
    { BODY_VENUS,   "2015-05-27T18:46Z", "2015-06-06T18:46Z", 45.40, VISIBLE_EVENING },
    { BODY_VENUS,   "2017-01-02T13:19Z", "2017-01-12T13:19Z", 47.10, VISIBLE_EVENING },
    { BODY_VENUS,   "2018-08-07T17:02Z", "2018-08-17T17:02Z", 45.90, VISIBLE_EVENING }
};

static const int ElongTestCount = sizeof(ElongTestData) / sizeof(ElongTestData[0]);

static int ParseDate(const char *text, astro_time_t *time)
{
    char zcheck;
    int year, month, day, hour, minute, nscanned;
    double second;

    nscanned = sscanf(text, "%d-%d-%dT%d:%d:%lf%c", &year, &month, &day, &hour, &minute, &second, &zcheck);
    if (nscanned != 7 || zcheck != 'Z')
    {
        second = 0.0;
        nscanned = sscanf(text, "%d-%d-%dT%d:%d%c", &year, &month, &day, &hour, &minute, &zcheck);
        if (nscanned != 6 || zcheck != 'Z')
        {
            fprintf(stderr, "C %s: Invalid date text '%s'\n", __func__, text);
            time->ut = time->tt = NAN;
            return 1;
        }
    }

    *time = Astronomy_MakeTime(year, month, day, hour, minute, second);
    return 0;
}

static int TestMaxElong(const elong_test_t *test)
{
    int error;
    astro_time_t searchTime, eventTime;
    astro_elongation_t evt;
    double hour_diff, arcmin_diff;
    const char *name = NULL;
    const char *vis = NULL;

    switch (test->body)
    {
    case BODY_MERCURY:  name = "Mercury";   break;
    case BODY_VENUS:    name = "Venus";     break;
    default:
        FFAIL("invalid body %d in test data.\n", test->body);
    }

    switch (test->visibility)
    {
    case VISIBLE_MORNING:   vis = "morning";    break;
    case VISIBLE_EVENING:   vis = "evening";    break;
    default:
        FFAIL("invalid visibility %d in test data.\n", test->visibility);
    }

    CHECK(ParseDate(test->searchDate, &searchTime));
    CHECK(ParseDate(test->eventDate,  &eventTime));

    evt = Astronomy_SearchMaxElongation(test->body, searchTime);
    if (evt.status != ASTRO_SUCCESS)
        FAIL("C %s(%s %s): SearchMaxElongation returned %d\n", __func__, name, test->searchDate, evt.status);

    hour_diff = 24.0 * ABS(evt.time.tt - eventTime.tt);
    arcmin_diff = 60.0 * ABS(evt.elongation - test->angle);

    FDEBUG("%-7s %-7s elong=%5.2lf (%4.2lf arcmin, %5.3lf hours)\n", name, vis, evt.elongation, arcmin_diff, hour_diff);

    if (hour_diff > 0.6)
        FAIL("C %s(%s %s): excessive hour error.\n", __func__, name, test->searchDate);

    if (arcmin_diff > 3.4)
        FAIL("C %s(%s %s): excessive arcmin error.\n", __func__, name, test->searchDate);

fail:
    return error;
}

static int SearchElongTest()
{
    int error, i;

    for (i=0; i < ElongTestCount; ++i)
        CHECK(TestMaxElong(&ElongTestData[i]));

    FPASSA("%d rows\n", ElongTestCount);
fail:
    return error;
}

static int TestPlanetLongitudes(
    astro_body_t body,
    const char *outFileName,
    const char *zeroLonEventName)
{
    int error = 1;
    const int startYear = 1700;
    const int stopYear  = 2200;
    astro_time_t time, stopTime;
    double rlon = 0.0;
    const char *event;
    astro_search_result_t search_result;
    int count = 0;
    double day_diff, min_diff = 1.0e+99, max_diff = 1.0e+99;
    astro_vector_t geo;
    double dist;
    FILE *outfile = NULL;
    const char *name;
    double ratio, thresh;

    name = Astronomy_BodyName(body);
    if (!name[0])
        FFAIL("Invalid body code %d\n", body);

    outfile = fopen(outFileName, "wt");
    if (outfile == NULL)
        FFAIL("Cannot open output file: %s\n", outFileName);

    time = Astronomy_MakeTime(startYear, 1, 1, 0, 0, 0.0);
    stopTime = Astronomy_MakeTime(stopYear, 1, 1, 0, 0, 0.0);
    while (time.tt < stopTime.tt)
    {
        ++count;
        event = (rlon == 0.0) ? zeroLonEventName : "sup";
        search_result = Astronomy_SearchRelativeLongitude(body, rlon, time);
        if (search_result.status != ASTRO_SUCCESS)
            FAIL("C %s(%s): SearchRelativeLongitude returned %d\n", __func__, name, search_result.status);

        if (count >= 2)
        {
            /* Check for consistent intervals. */
            /* Mainly I don't want to skip over an event! */
            day_diff = search_result.time.tt - time.tt;
            if (count == 2)
            {
                min_diff = max_diff = day_diff;
            }
            else
            {
                if (day_diff < min_diff)
                    min_diff = day_diff;

                if (day_diff > max_diff)
                    max_diff = day_diff;
            }
        }

        geo = Astronomy_GeoVector(body, search_result.time, ABERRATION);
        if (geo.status != ASTRO_SUCCESS)
            FAIL("C %s(%s): GeoVector returned %d\n", __func__, name, geo.status);

        dist = Astronomy_VectorLength(geo);
        fprintf(outfile, "e %s %s %0.16lf %0.16lf\n", name, event, search_result.time.tt, dist);

        /* Search for the opposite longitude event next time. */
        time = search_result.time;
        rlon = 180.0 - rlon;
    }

    switch (body)
    {
    case BODY_MERCURY:  thresh = 1.65;  break;
    case BODY_MARS:     thresh = 1.30;  break;
    default:            thresh = 1.07;  break;
    }

    ratio = max_diff / min_diff;
    DEBUG("C %s(%-7s): %5d events, ratio=%5.3lf, file: %s\n", __func__, name, count, ratio, outFileName);

    if (ratio > thresh)
        FAIL("C %s(%s): excessive event interval ratio.\n", __func__, name);

    error = 0;
fail:
    if (outfile != NULL) fclose(outfile);
    return error;
}

static int ElongationTest(void)
{
    int error;

    CHECK(TestElongFile("longitude/opposition_2018.txt", 0.0));

    CHECK(TestPlanetLongitudes(BODY_MERCURY, "temp/c_longitude_Mercury.txt", "inf"));
    CHECK(TestPlanetLongitudes(BODY_VENUS,   "temp/c_longitude_Venus.txt",   "inf"));
    CHECK(TestPlanetLongitudes(BODY_MARS,    "temp/c_longitude_Mars.txt",    "opp"));
    CHECK(TestPlanetLongitudes(BODY_JUPITER, "temp/c_longitude_Jupiter.txt", "opp"));
    CHECK(TestPlanetLongitudes(BODY_SATURN,  "temp/c_longitude_Saturn.txt",  "opp"));
    CHECK(TestPlanetLongitudes(BODY_URANUS,  "temp/c_longitude_Uranus.txt",  "opp"));
    CHECK(TestPlanetLongitudes(BODY_NEPTUNE, "temp/c_longitude_Neptune.txt", "opp"));
    CHECK(TestPlanetLongitudes(BODY_PLUTO,   "temp/c_longitude_Pluto.txt",   "opp"));

    CHECK(SearchElongTest());

fail:
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int RiseSet(void)
{
    const char *filename = "riseset/riseset.txt";
    int error = 1;
    FILE *infile = NULL;
    char line[100];
    char name[20];
    double longitude, latitude;
    int year, month, day, hour, minute;
    char kind[2];       /* "r" or "s" for rise or set */
    int direction;      /* +1 for rise, -1 for set */
    int lnum, nscanned;
    astro_time_t correct_date;
    astro_body_t body, current_body;
    astro_observer_t observer;
    astro_time_t r_search_date;
    astro_search_result_t r_evt, s_evt;     /* rise event, set event: search results */
    astro_search_result_t a_evt, b_evt;     /* chronologically first and second events */
    astro_time_t s_search_date;
    int a_dir = 0, b_dir = 0;
    double error_minutes, rms_minutes;
    double sum_minutes = 0.0;
    double max_minutes = 0.0;
    const double nudge_days = 1.0e-5;    /* just under 1 second */
    extern int _AltitudeDiffCallCount;          /* undocumented global var used for performance testing only */
    extern int _FindAscentMaxRecursionDepth;    /* undocumented global var used for performance testing only */

    _AltitudeDiffCallCount = 0;
    _FindAscentMaxRecursionDepth = -1;

    observer.latitude = observer.longitude = observer.height = NAN;
    current_body = BODY_INVALID;
    a_evt.status = b_evt.status = r_evt.status = s_evt.status = ASTRO_NOT_INITIALIZED;
    b_evt.time.tt = b_evt.time.ut = a_evt.time.tt = a_evt.time.ut = NAN;

    infile = fopen(filename, "rt");
    if (infile == NULL)
        FFAIL("cannot open input file: %s\n", filename);

    lnum = 0;
    while (ReadLine(line, sizeof(line), infile, filename, lnum))
    {
        ++lnum;

        /* Moon  103 -61 1944-01-02T17:08Z s */
        /* Moon  103 -61 1944-01-03T05:47Z r */
        nscanned = sscanf(line, "%9[A-Za-z] %lf %lf %d-%d-%dT%d:%dZ %1[rs]",
            name, &longitude, &latitude, &year, &month, &day, &hour, &minute, kind);

        if (nscanned != 9)
            FLNFAIL("invalid format\n");

        correct_date = Astronomy_MakeTime(year, month, day, hour, minute, 0.0);

        if (!strcmp(kind, "r"))
            direction = +1;
        else if (!strcmp(kind, "s"))
            direction = -1;
        else
            FLNFAIL("invalid kind '%s'\n", kind);

        body = Astronomy_BodyCode(name);
        if (body == BODY_INVALID)
            FLNFAIL("invalid body name '%s'", name);

        /* Every time we see a new geographic location, start a new iteration */
        /* of finding all rise/set times for that UTC calendar year. */
        if (observer.latitude != latitude || observer.longitude != longitude || current_body != body)
        {
            current_body = body;
            observer = Astronomy_MakeObserver(latitude, longitude, 0.0);
            r_search_date = s_search_date = Astronomy_MakeTime(year, 1, 1, 0, 0, 0.0);
            b_evt.time.tt = b_evt.time.ut = NAN;
            b_evt.status = ASTRO_NOT_INITIALIZED;
            FDEBUG("%-7s lat=%0.1lf lon=%0.1lf\n", name, latitude, longitude);
        }

        if (b_evt.status == ASTRO_SUCCESS)      /* has b_evt been initialized? (does it contain a valid event?) */
        {
            /* Recycle the second event from the previous iteration as the first event. */
            a_evt = b_evt;
            a_dir = b_dir;
            b_evt.status = ASTRO_NOT_INITIALIZED;   /* invalidate b_evt for the next time around the loop */
        }
        else
        {
            r_evt = Astronomy_SearchRiseSet(body, observer, DIRECTION_RISE, r_search_date, 366.0);
            if (r_evt.status != ASTRO_SUCCESS)
                FLNFAIL("did not find %s rise event: status = %d\n", name, r_evt.status);

            s_evt = Astronomy_SearchRiseSet(body, observer, DIRECTION_SET, s_search_date, 366.0);
            if (s_evt.status != ASTRO_SUCCESS)
                FLNFAIL("did not find %s set event: status = %d\n", name, s_evt.status);

            /* Expect the current event to match the earlier of the found dates. */
            if (r_evt.time.tt < s_evt.time.tt)
            {
                a_evt = r_evt;
                b_evt = s_evt;
                a_dir = +1;
                b_dir = -1;
            }
            else
            {
                a_evt = s_evt;
                b_evt = r_evt;
                a_dir = -1;
                b_dir = +1;
            }

            /* Nudge the event times forward a tiny amount. */
            r_search_date = Astronomy_AddDays(r_evt.time, nudge_days);
            s_search_date = Astronomy_AddDays(s_evt.time, nudge_days);
        }

        error_minutes = (24.0 * 60.0) * ABS(a_evt.time.tt - correct_date.tt);
        sum_minutes += error_minutes * error_minutes;
        if (error_minutes > max_minutes)
            max_minutes = error_minutes;

        if (error_minutes > 1.18)
            FLNFAIL("excessive prediction time error = %lg minutes.\n", error_minutes);

        if (a_dir != direction)
            FLNFAIL("expected dir=%d but found %d\n", direction, a_dir);
    }

    rms_minutes = V(sqrt(sum_minutes / lnum));
    FPASSA("%d lines: time errors in minutes: rms=%0.4lf, max=%0.4lf, recur=%d, altcount=%d\n", lnum, rms_minutes, max_minutes, _FindAscentMaxRecursionDepth, _AltitudeDiffCallCount);
fail:
    if (infile != NULL) fclose(infile);
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int RiseSetSlot(double ut1, double ut2, astro_direction_t dir, astro_observer_t observer)
{
    int error = 1;
    const int nslots = 100;
    astro_time_t time;
    astro_search_result_t result;
    int i;
    double ut, diff, maxDiff = 0.0;

    for (i = 1; i < nslots; ++i)
    {
        ut = ut1 + ((double)i / nslots)*(ut2 - ut1);
        time = Astronomy_TimeFromDays(ut);

        result = Astronomy_SearchRiseSet(BODY_SUN, observer, dir, time, -1.0);
        CHECK_STATUS(result);
        diff = SECONDS_PER_DAY * ABS(result.time.ut - ut1);
        if (diff > maxDiff) maxDiff = diff;

        result = Astronomy_SearchRiseSet(BODY_SUN, observer, dir, time, +1.0);
        CHECK_STATUS(result);
        diff = SECONDS_PER_DAY * ABS(result.time.ut - ut2);
        if (diff > maxDiff) maxDiff = diff;
    }
    if (maxDiff > 0.13)
        FAIL("C %s(dir=%d): EXCESSIVE slot-test discrepancy = %0.6lf seconds.\n", __func__, dir, maxDiff);
    DEBUG("C %s(dir=%d): slot discrepancy = %0.6lf seconds.\n", __func__, dir, maxDiff);
    error = 0;
fail:
    return error;
}


static int RiseSetReverse(void)
{
    /* Verify that the rise/set search works equally well forwards and backwards in time. */

    const int nsamples = 5000;
    const double nudge = 0.1;
    int error = 1;
    int i;
    double *utList = NULL;
    astro_observer_t observer = Astronomy_MakeObserver(30.5, -90.7, 0.0);
    astro_direction_t dir;
    astro_time_t time;
    astro_search_result_t result;
    double dt, dtMin = +1000.0, dtMax = -1000.0;
    double diff, maxDiff = 0.0;
    int k;

    utList = (double *)calloc(nsamples, sizeof(utList[0]));
    if (utList == NULL)
        FFAIL("out of memory!\n");

    /* Find alternating sunrise/sunset events in forward chronological order. */
    dir = DIRECTION_RISE;
    time = Astronomy_MakeTime(2022, 1, 1, 0, 0, 0.0);
    for (i = 0; i < nsamples; ++i)
    {
        result = Astronomy_SearchRiseSet(BODY_SUN, observer, dir, time, +1.0);
        CHECK_STATUS(result);
        utList[i] = result.time.ut;
        if (i > 0)
        {
            /* Check the time between consecutive sunrise/sunset events. */
            /* These will vary considerably with the seasons, so just make sure we don't miss any entirely. */
            dt = V(utList[i] - utList[i-1]);
            if (dt < dtMin) dtMin = dt;
            if (dt > dtMax) dtMax = dt;
        }
        dir *= -1;      /* Toggle searching sunrise/sunset. */
        time = Astronomy_AddDays(result.time, +nudge);
    }

    FDEBUG("dtMin=%0.6lf days, dtMax=%0.6lf days.\n", dtMin, dtMax);
    if (dtMin < 0.411 || dtMax > 0.589)
        FFAIL("Invalid intervals between sunrise/sunset.\n");

    /* Perform the same search in reverse. Verify we get consistent rise/set times. */
    for (int i = nsamples-1; i >= 0; --i)
    {
        dir *= -1;      /* Toggle searching sunrise/sunset. */
        result = Astronomy_SearchRiseSet(BODY_SUN, observer, dir, time, -1.0);
        CHECK_STATUS(result);
        diff = SECONDS_PER_DAY * ABS(utList[i] - result.time.ut);
        if (diff > maxDiff) maxDiff = diff;
        time = Astronomy_AddDays(result.time, -nudge);
    }
    if (maxDiff > 0.1)
        FFAIL("EXCESSIVE forward/backward discrepancy = %0.6lf seconds.\n", maxDiff);
    FDEBUG("forward/backward discrepancy = %0.6lf seconds.\n", maxDiff);

    /* All even indexes in utList hold sunrise times. */
    /* All odd indexes in utList hold sunset times. */
    /* Verify that forward/backward searches for consecutive sunrises/sunsets */
    /* resolve correctly for 100 time slots between them. */
    k = (nsamples / 2) & ~1;
    CHECK(RiseSetSlot(utList[k+0], utList[k+2], DIRECTION_RISE, observer));
    CHECK(RiseSetSlot(utList[k+1], utList[k+3], DIRECTION_SET,  observer));

    FPASS();
fail:
    free(utList);
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int CheckMagnitudeData(astro_body_t body, const char *filename)
{
    int error = 1;
    int lnum, count;
    FILE *infile = NULL;
    char line[200];
    const char *rest;
    astro_time_t time;
    astro_illum_t illum;
    int nscanned;
    double mag, sbrt, dist, rdot, delta, deldot, phase_angle;
    double diff, diff_lo = NAN, diff_hi = NAN, sum_squared_diff = 0.0, rms;
    const double limit = 0.012;

    infile = fopen(filename, "rt");
    if (infile == NULL)
        FFAIL("cannot open input file: %s\n", filename);

    count = lnum = 0;
    while (ReadLine(line, sizeof(line), infile, filename, lnum))
    {
        ++lnum;
        rest = ParseJplHorizonsDateTime(line, &time);
        /* Ignore non-data rows and data rows that contain "n.a." */
        if (rest && !strstr(rest, "n.a."))
        {
            nscanned = sscanf(rest, "%lf %lf %lf %lf %lf %lf %lf",
                &mag, &sbrt, &dist, &rdot, &delta, &deldot, &phase_angle);

            if (nscanned != 7)
                FLNFAIL("invalid data format\n");

            illum = Astronomy_Illumination(body, time);
            if (illum.status != ASTRO_SUCCESS)
                FLNFAIL("Astronomy_Illumination returned %d\n", illum.status);

            diff = illum.mag - mag;
            if (ABS(diff) > limit)
                FLNFAIL("EXCESSIVE ERROR: correct mag=%lf, calc mag=%lf, diff=%lf\n", mag, illum.mag, diff);

            sum_squared_diff += diff * diff;
            if (count == 0)
            {
                diff_lo = diff_hi = diff;
            }
            else
            {
                if (diff < diff_lo)
                    diff_lo = diff;

                if (diff > diff_hi)
                    diff_hi = diff;
            }

            ++count;
        }
    }

    if (count == 0)
        FFAIL("Did not find any data in file: %s\n", filename);

    rms = V(sqrt(sum_squared_diff / count));
    FDEBUG("%-21s %5d rows diff_lo=%0.4lf diff_hi=%0.4lf rms=%0.4lf\n", filename, count, diff_lo, diff_hi, rms);
    error = 0;

fail:
    if (infile != NULL) fclose(infile);
    return error;
}

static int CheckSaturn()
{
    /* JPL Horizons does not include Saturn's rings in its magnitude models. */
    /* I still don't have authoritative test data for Saturn's magnitude. */
    /* For now, I just test for consistency with Paul Schlyter's formulas at: */
    /* http://www.stjarnhimlen.se/comp/ppcomp.html#15 */

    static const struct test_case
    {
        const char *date;
        double mag;
        double tilt;
    }
    data[] =
    {
        { "1972-01-01T00:00Z", -0.31725492,  +24.43386475 },
        { "1980-01-01T00:00Z", +0.85796177,   -1.72627324 },
        { "2009-09-04T00:00Z", +1.01932560,   +0.01834451 },
        { "2017-06-15T00:00Z", -0.12303373,  -26.60068380 },
        { "2019-05-01T00:00Z", +0.33124502,  -23.47173574 },
        { "2025-09-25T00:00Z", +0.50543708,   +1.69118986 },
        { "2032-05-15T00:00Z", -0.04649573,  +26.95238680 }
    };

    static const int ncases = sizeof(data) / sizeof(data[0]);

    int i;
    astro_illum_t illum;
    astro_time_t time;
    double mag_diff, tilt_diff;

    for (i=0; i < ncases; ++i)
    {
        int error = ParseDate(data[i].date, &time);
        if (error)
            return 1;

        illum = Astronomy_Illumination(BODY_SATURN, time);
        if (illum.status != ASTRO_SUCCESS)
            FAILRET("C %s(%d): Illumination returned %d\n", __func__, i, illum.status);

        FDEBUG("date=%s  calc mag=%12.8lf  ring_tilt=%12.8lf\n", data[i].date, illum.mag, illum.ring_tilt);

        mag_diff = ABS(illum.mag - data[i].mag);
        if (mag_diff > 1.0e-8)
            FAILRET("C %s: ERROR: Excessive magnitude error %lg\n", __func__, mag_diff);

        tilt_diff = ABS(illum.ring_tilt - data[i].tilt);
        if (tilt_diff > 1.0e-8)
            FAILRET("C %s: ERROR: Excessive ring tilt error %lg\n", __func__, tilt_diff);
    }

    return 0;
}

static int MoonTest(void)
{
    astro_time_t time = Astronomy_MakeTime(2019, 6, 24, 15, 45, 37.0);
    astro_vector_t vec = Astronomy_GeoMoon(time);
    double dx, dy, dz, diff;

    if (vec.status != ASTRO_SUCCESS)
        FAILRET("C %s: ERROR: vec.status = %d\n", __func__, vec.status);

    printf("C %s: %0.16lg %0.16lg %0.16lg\n", __func__, vec.x, vec.y, vec.z);


    dx = vec.x - (+0.002674037026701135);
    dy = vec.y - (-0.0001531610316600666);
    dz = vec.z - (-0.0003150159927069429);
    diff = V(sqrt(dx*dx + dy*dy + dz*dz));
    printf("C %s: diff = %lg\n", __func__, diff);
    if (diff > 4.34e-19)
        FAILRET("C %s: EXCESSIVE ERROR\n", __func__);

    return 0;
}


typedef enum
{
    COORD_EQJ,
    COORD_ECL
}
coord_sys_t;


static int MoonVectorFile(const char *filename, coord_sys_t coords, double rms_angle_limit, double rms_length_limit)
{
    int error = 1;
    FILE *infile = NULL;
    int lnum = 0;
    int count = 0;      /* number of test cases processed */
    int toggle = 0;
    int found_start = 0;
    char line[200];
    double jd = 0.0;
    astro_vector_t expected;
    astro_vector_t calculated;
    astro_angle_result_t result;
    double angle_square_sum = 0.0;
    double expected_length;
    double calculated_length;
    double length_error;
    double length_error_square_sum = 0.0;
    double rms_length, rms_angle;
    double max_length = 0.0, max_angle = 0.0;
    astro_rotation_t rot;

    rot = Astronomy_Rotation_EQJ_ECL();
    CHECK_STATUS(rot);

    infile = fopen(filename, "rt");
    if (infile == NULL)
        FFAIL("cannot open file: %s\n", filename);

    while (ReadLine(line, sizeof(line), infile, filename, lnum))
    {
        ++lnum;
        if (!found_start)
        {
            if (strlen(line) > 5 && !memcmp(line, "$$SOE", 5))
                found_start = 1;
        }
        else
        {
            if (strlen(line) > 5 && !memcmp(line, "$$EOE", 5))
                break;

            if (strlen(line) < 62)
                FLNFAIL("line is too short.\n");

            /*
                2444239.500000000 = A.D. 1980-Jan-01 00:00:00.0000 TDB [del_T=     50.183943 s]
                X = 2.933885115037269E-04 Y = 2.421938721618651E-03 Z = 8.195641195651288E-04
            */
            if (toggle == 0)
            {
                if (1 != sscanf(line, "%lf", &jd))
                    FLNFAIL("cannot parse julian date.\n");
                V(jd);
            }
            else
            {
                /* Complete the expected Moon position vector for this time. */
                if (3 != sscanf(line, " X =%lf Y =%lf Z =%lf", &expected.x, &expected.y, &expected.z))
                    FLNFAIL("cannot parse position vector.\n");
                expected.t = Astronomy_TerrestrialTime(jd - JD_2000);
                expected.status = ASTRO_SUCCESS;

                switch (coords)
                {
                case COORD_EQJ:
                    calculated = Astronomy_GeoMoon(expected.t);
                    CHECK_STATUS(calculated);
                    break;

                case COORD_ECL:
                    calculated = Astronomy_GeoMoon(expected.t);
                    CHECK_STATUS(calculated);
                    calculated = Astronomy_RotateVector(rot, calculated);
                    CHECK_STATUS(calculated);
                    break;

                default:
                    FLNFAIL("unknown coord system %d\n", coords);
                }

                /* Compare distances and angles between vectors. */
                result = Astronomy_AngleBetween(expected, calculated);
                CHECK_STATUS(result);
                result.angle = ABS(result.angle);
                if (result.angle > max_angle)
                    max_angle = result.angle;
                angle_square_sum += (result.angle * result.angle);

                expected_length = Astronomy_VectorLength(expected);
                V(expected_length);
                calculated_length = Astronomy_VectorLength(calculated);
                V(calculated_length);

                length_error = ABS(calculated_length - expected_length);
                if (length_error > max_length)
                    max_length = length_error;
                length_error_square_sum += (length_error * length_error);

                ++count;
            }

            toggle ^= 1;
        }
    }

    if (count != 73050)
        FAIL("C %s(%s): unexpected count = %d!\n", __func__, filename, count);

    rms_angle = V(3600.0 * sqrt(angle_square_sum / count));
    rms_length = V(KM_PER_AU * sqrt(length_error_square_sum / count));
    max_angle = V(3600.0 * max_angle);
    max_length = V(KM_PER_AU * max_length);

    if (rms_angle > rms_angle_limit)
        FAIL("C %s(%s): EXCESSIVE rms angular error = %0.6lf arcsec\n", __func__, filename, rms_angle);

    if (rms_length > rms_length_limit)
        FAIL("C %s(%s): EXCESSIVE rms distance error = %0.3lf km\n", __func__, filename, rms_length);

    printf("C %s(%s): PASS (%d lines, %d cases, angular error rms=%0.6lf arcsec, max=%0.6lf arcsec; distance error rms=%0.3lf km, max=%0.3lf km)\n",
        __func__, filename, lnum, count, rms_angle, max_angle, rms_length, max_length);

    error = 0;
fail:
    if (infile != NULL) fclose(infile);
    return error;
}


static int MoonVector()
{
    int error;
    CHECK(MoonVectorFile("moonphase/moon_eqj.txt", COORD_EQJ, 1.16, 10.633));
    CHECK(MoonVectorFile("moonphase/moon_ecl.txt", COORD_ECL, 1.16, 10.633));
fail:
    return error;
}

static int MoonEcliptic(void)
{
    int error = 1;
    FILE *infile = NULL;
    int lnum = 0;
    int count = 0;
    int found_start = 0;
    const char *filename = "moonphase/moon_ecm.txt";
    char line[100];
    double jdut, tt, deltat, lon, lat;
    double diff_lat, diff_lon, max_lat = 0.0, max_lon = 0.0;
    astro_time_t time;
    astro_spherical_t sphere;

    /* Verify Moon's ecliptic coordinates relative to mean equator of date. */

    infile = fopen(filename, "rt");
    if (infile == NULL)
        FFAIL("Cannot open input file: %s\n", filename);

    while (ReadLine(line, sizeof(line), infile, filename, lnum))
    {
        ++lnum;
        if (!found_start)
        {
            if (strlen(line) > 5 && !memcmp(line, "$$SOE", 5))
                found_start = 1;
        }
        else
        {
            if (strlen(line) > 5 && !memcmp(line, "$$EOE", 5))
                break;

            if (strlen(line) < 79)
                FLNFAIL("line is too short.\n");

            /* Parse reference data from JPL Horizons. */

            /* " Date__(UT)__HR:MN Date_________JDUT           TDB-UT     ObsEcLon    ObsEcLat" */
            /* " 1900-Jan-01 00:00 2415020.500000000        -1.944461  272.4162663   1.1082671" */
            if (4 != sscanf(&line[19], "%lf %lf %lf %lf", &jdut, &deltat, &lon, &lat))
                FLNFAIL("cannot parse data.\n");
            V(jdut);
            V(deltat);
            V(lon);
            V(lat);

            /* Use JPL Horizons delta-t value to match their TT value as close as possible. */
            /* We do this because their Delta-T formula differs from Espenak-Meeus, especially in the future. */
            /* TT-UT = deltat ==> TT = UT + deltat */

            tt = V((jdut - JD_2000) + (deltat / SECONDS_PER_DAY));
            time = Astronomy_TerrestrialTime(tt);

            /* Calculate Moon's ecliptic coordinates relative to mean equator of date. */
            sphere = Astronomy_EclipticGeoMoon(time);
            CHECK_STATUS(sphere);
            V(sphere.lat);
            V(sphere.lon);

            /* Find maximum arcsecond discrepancy between latitudes and longitudes. */
            diff_lat = 3600.0 * DegreeDiff(sphere.lat, lat);
            diff_lon = 3600.0 * DegreeDiff(sphere.lon, lon);
            if (diff_lat > max_lat) max_lat = diff_lat;
            if (diff_lon > max_lon) max_lon = diff_lon;
            ++count;
        }
    }

    if (count != 73050)
        FFAIL("Unexpected count = %d\n", count);

    if (max_lat > 1.826)
        FFAIL("EXCESSIVE ecliptic latitude discrepancy = %0.6lf arcsec.\n", max_lat);

    if (max_lon > 5.928)
        FFAIL("EXCESSIVE ecliptic longitude discrepancy = %0.6lf arcsec.\n", max_lon);

    FPASSA("count=%d, max lat=%0.3lf arcsec, max lon=%0.3lf arcsec.\n", count, max_lat, max_lon);
fail:
    if (infile != NULL) fclose(infile);
    return error;
}


static int CheckIlluminationInvalidBody(astro_body_t body)
{
    astro_illum_t illum;
    astro_time_t time = Astronomy_MakeTime(2021, 11, 10, 0, 8, 16.0);

    /* Verify that invalid bodies return errors. */

    illum = Astronomy_Illumination(body, time);
    if (illum.status == ASTRO_SUCCESS)
        FAILRET("C %s: FAILURE -- incorrect success status for body %d\n", __func__, (int)body);

    return 0;   /* Correct behavior. */
}

static int MagnitudeTest(void)
{
    int nfailed = 0;

    nfailed += CheckIlluminationInvalidBody(BODY_INVALID);
    nfailed += CheckIlluminationInvalidBody(BODY_EARTH);
    nfailed += CheckIlluminationInvalidBody(BODY_EMB);
    nfailed += CheckIlluminationInvalidBody(BODY_SSB);
    nfailed += CheckIlluminationInvalidBody((astro_body_t)2112);

    nfailed += CheckMagnitudeData(BODY_SUN,     "magnitude/Sun.txt");
    nfailed += CheckMagnitudeData(BODY_MOON,    "magnitude/Moon.txt");
    nfailed += CheckMagnitudeData(BODY_MERCURY, "magnitude/Mercury.txt");
    nfailed += CheckMagnitudeData(BODY_VENUS,   "magnitude/Venus.txt");
    nfailed += CheckMagnitudeData(BODY_MARS,    "magnitude/Mars.txt");
    nfailed += CheckMagnitudeData(BODY_JUPITER, "magnitude/Jupiter.txt");
    nfailed += CheckSaturn();
    nfailed += CheckMagnitudeData(BODY_URANUS,  "magnitude/Uranus.txt");
    nfailed += CheckMagnitudeData(BODY_NEPTUNE, "magnitude/Neptune.txt");
    nfailed += CheckMagnitudeData(BODY_PLUTO,   "magnitude/Pluto.txt");

    nfailed += TestMaxMag(BODY_VENUS, "magnitude/maxmag_Venus.txt");

    if (nfailed == 0)
        printf("C %s: PASS\n", __func__);
    else
        fprintf(stderr, "C %s: FAILED %d test(s).\n", __func__, nfailed);

    return nfailed;
}

static int TestMaxMag(astro_body_t body, const char *filename)
{
    int error = 1;
    FILE *infile = NULL;
    int lnum, nscanned;
    int year1, month1, day1, hour1, minute1;
    int year2, month2, day2, hour2, minute2;
    double correct_mag, correct_angle1, correct_angle2;
    char line[100];
    astro_time_t search_time, time1, time2, center_time;
    astro_illum_t illum;
    double mag_diff, hours_diff;

    /*
        Example of input data:

        2001-02-21T08:00Z 2001-02-27T08:00Z 23.17 19.53 -4.84

        JPL Horizons test data has limited floating point precision in the magnitude values.
        There is a pair of dates for the beginning and end of the max magnitude period,
        given the limited precision. We pick the point halfway between as the supposed max magnitude time.
    */

    infile = fopen(filename, "rt");
    if (infile == NULL)
        FFAIL("Cannot open input file: %s\n", filename);

    lnum = 0;
    search_time = Astronomy_MakeTime(2001, 1, 1, 0, 0, 0.0);
    while (ReadLine(line, sizeof(line), infile, filename, lnum))
    {
        ++lnum;

        nscanned = sscanf(line, "%d-%d-%dT%d:%dZ %d-%d-%dT%d:%dZ %lf %lf %lf\n",
            &year1, &month1, &day1, &hour1, &minute1,
            &year2, &month2, &day2, &hour2, &minute2,
            &correct_angle1, &correct_angle2, &correct_mag);

        if (nscanned != 13)
            FLNFAIL("invalid data format.\n");

        time1 = Astronomy_MakeTime(year1, month1, day1, hour1, minute1, 0.0);
        time2 = Astronomy_MakeTime(year2, month2, day2, hour2, minute2, 0.0);
        center_time = Astronomy_AddDays(time1, 0.5*(time2.ut - time1.ut));

        illum = Astronomy_SearchPeakMagnitude(body, search_time);
        if (illum.status != ASTRO_SUCCESS)
            FLNFAIL("SearchPeakMagnitude returned %d\n", illum.status);

        mag_diff = ABS(illum.mag - correct_mag);
        hours_diff = 24.0 * ABS(illum.time.ut - center_time.ut);
        FDEBUG("mag_diff=%0.3lf, hours_diff=%0.3lf\n", mag_diff, hours_diff);
        if (hours_diff > 7.1)
            FLNFAIL("EXCESSIVE TIME DIFFERENCE.\n");

        if (mag_diff > 0.005)
            FLNFAIL("EXCESSIVE MAGNITUDE DIFFERENCE.\n");

        search_time = time2;
    }

    FPASSA("processed %d lines from file %s\n", lnum, filename);
fail:
    if (infile != NULL) fclose(infile);
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int ParseMonthName(const char *mtext, int *month)
{
    if (month == NULL)
        return 1;

    *month = -1;

    if (mtext == NULL)
        return 2;

    if (!strcmp(mtext, "Jan"))
        *month = 1;
    else if (!strcmp(mtext, "Feb"))
        *month = 2;
    else if (!strcmp(mtext, "Mar"))
        *month = 3;
    else if (!strcmp(mtext, "Apr"))
        *month = 4;
    else if (!strcmp(mtext, "May"))
        *month = 5;
    else if (!strcmp(mtext, "Jun"))
        *month = 6;
    else if (!strcmp(mtext, "Jul"))
        *month = 7;
    else if (!strcmp(mtext, "Aug"))
        *month = 8;
    else if (!strcmp(mtext, "Sep"))
        *month = 9;
    else if (!strcmp(mtext, "Oct"))
        *month = 10;
    else if (!strcmp(mtext, "Nov"))
        *month = 11;
    else if (!strcmp(mtext, "Dec"))
        *month = 12;
    else
        return 3;

    return 0;   /* success */
}

static const char *ParseJplHorizonsDateTime(const char *text, astro_time_t *time)
{
    int year, month, day, hour, minute;
    int nscanned;
    char mtext[4];
    char verify[30];
    size_t length;

    time->ut = time->tt = NAN;

    while (*text == ' ' && *text != '\0')
        ++text;

    nscanned = sscanf(text, "%d-%3[A-Za-z]-%d %d:%d", &year, mtext, &day, &hour, &minute);
    if (nscanned != 5)
        return NULL;

    if (year < 1000 || year > 9999)
        return NULL;       /* if not a 4-digit year, we are skipping wrong number of chars */

    if (day < 1 || day > 31)
        return NULL;

    if (hour < 0 || hour > 23)
        return NULL;

    if (minute < 0 || minute > 59)
        return NULL;

    if (!strcmp(mtext, "Jan"))
        month = 1;
    else if (!strcmp(mtext, "Feb"))
        month = 2;
    else if (!strcmp(mtext, "Mar"))
        month = 3;
    else if (!strcmp(mtext, "Apr"))
        month = 4;
    else if (!strcmp(mtext, "May"))
        month = 5;
    else if (!strcmp(mtext, "Jun"))
        month = 6;
    else if (!strcmp(mtext, "Jul"))
        month = 7;
    else if (!strcmp(mtext, "Aug"))
        month = 8;
    else if (!strcmp(mtext, "Sep"))
        month = 9;
    else if (!strcmp(mtext, "Oct"))
        month = 10;
    else if (!strcmp(mtext, "Nov"))
        month = 11;
    else if (!strcmp(mtext, "Dec"))
        month = 12;
    else
        return NULL;

    /* Make absolutely sure we know how many characters the date text is. */
    /* If anything is fishy, fail! */
    snprintf(verify, sizeof(verify), "%04d-%s-%02d %02d:%02d", year, mtext, day, hour, minute);
    length = strlen(verify);
    if (length != 17)
        return NULL;

    if (memcmp(verify, text, length))
        return NULL;

    *time = Astronomy_MakeTime(year, month, day, hour, minute, 0.0);
    return text + length;   /* return remaining portion of string */
}

/*-----------------------------------------------------------------------------------------------------------*/

static int LunarApsis(void)
{
    const char *filename = "apsides/moon.txt";
    int error = 1;
    FILE *infile = NULL;
    int lnum, nscanned;
    char line[100];
    int kind, year, month, day, hour, minute;
    double dist_km;
    astro_time_t start_time, correct_time;
    astro_apsis_t apsis;
    double diff_minutes, diff_km;
    double max_minutes = 0.0, max_km = 0.0;

    infile = fopen(filename, "rt");
    if (infile == NULL)
        FFAIL("Cannot open input file: %s\n", filename);

    /*
        0 2001-01-10T08:59Z 357132
        1 2001-01-24T19:02Z 406565
    */

    start_time = Astronomy_MakeTime(2001, 1, 1, 0, 0, 0.0);
    lnum = 0;
    while (ReadLine(line, sizeof(line), infile, filename, lnum))
    {
        ++lnum;

        if (lnum == 1)
            apsis = Astronomy_SearchLunarApsis(start_time);
        else
            apsis = Astronomy_NextLunarApsis(apsis);

        if (apsis.status != ASTRO_SUCCESS)
            FLNFAIL("Failed to find apsis.\n");

        nscanned = sscanf(line, "%d %d-%d-%dT%d:%dZ %lf", &kind, &year, &month, &day, &hour, &minute, &dist_km);
        if (nscanned != 7)
            FLNFAIL("invalid data format\n");

        if (kind != apsis.kind)
            FLNFAIL("expected apsis kind %d but found %d\n", kind, apsis.kind);

        correct_time = Astronomy_MakeTime(year, month, day, hour, minute, 0.0);

        diff_minutes = (24.0 * 60.0) * ABS(apsis.time.ut - correct_time.ut);
        diff_km = ABS(apsis.dist_km - dist_km);

        if (diff_minutes > 35.0)
            FLNFAIL("Excessive time error: %lf minutes.\n", diff_minutes);

        if (diff_km > 25.0)
            FLNFAIL("Excessive distance error: %lf km.\n", diff_km);

        if (diff_minutes > max_minutes)
            max_minutes = diff_minutes;

        if (diff_km > max_km)
            max_km = diff_km;
    }

    FPASSA("found %d events, max time error = %0.3lf minutes, max distance error = %0.3lf km.\n", lnum, max_minutes, max_km);
fail:
    if (infile != NULL) fclose(infile);
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int EarthApsis(void)
{
    const char *filename = "apsides/earth.txt";
    int error = 1;
    FILE *infile = NULL;
    int lnum, nscanned;
    char line[100];
    int kind, year, month, day, hour, minute;
    double dist_au;
    astro_time_t start_time, correct_time;
    astro_apsis_t apsis;
    double diff_minutes, diff_au;
    double max_minutes = 0.0, max_au = 0.0;

    infile = fopen(filename, "rt");
    if (infile == NULL)
        FFAIL("Cannot open input file: %s\n", filename);

    /*
        0 2001-01-04T08:52Z 0.9832860
        1 2001-07-04T13:37Z 1.0166426
    */

    start_time = Astronomy_MakeTime(2001, 1, 1, 0, 0, 0.0);
    lnum = 0;
    while (ReadLine(line, sizeof(line), infile, filename, lnum))
    {
        ++lnum;

        if (lnum == 1)
            apsis = Astronomy_SearchPlanetApsis(BODY_EARTH, start_time);
        else
            apsis = Astronomy_NextPlanetApsis(BODY_EARTH, apsis);

        if (apsis.status != ASTRO_SUCCESS)
            FLNFAIL("Failed to find apsis.\n");

        nscanned = sscanf(line, "%d %d-%d-%dT%d:%dZ %lf", &kind, &year, &month, &day, &hour, &minute, &dist_au);
        if (nscanned != 7)
            FLNFAIL("invalid data format\n");

        if (kind != apsis.kind)
            FLNFAIL("expected apsis kind %d but found %d\n", kind, apsis.kind);

        correct_time = Astronomy_MakeTime(year, month, day, hour, minute, 0.0);

        diff_minutes = (24.0 * 60.0) * ABS(apsis.time.ut - correct_time.ut);
        diff_au = ABS(apsis.dist_au - dist_au);

        if (diff_minutes > 120.58)
            FLNFAIL("Excessive time error: %lf minutes.\n", diff_minutes);

        if (diff_au > 1.2e-5)
            FLNFAIL("Excessive distance error: %lg AU.\n", diff_au);

        if (diff_minutes > max_minutes)
            max_minutes = diff_minutes;

        if (diff_au > max_au)
            max_au = diff_au;
    }

    FPASSA("found %d events, max time error = %0.3lf minutes, max distance error = %0.3lg AU.\n", lnum, max_minutes, max_au);
fail:
    if (infile != NULL) fclose(infile);
    return error;
}


static int PlanetApsis(void)
{
    int error;
    astro_body_t body;
    astro_time_t start_time, prev_time;
    astro_apsis_t apsis;
    astro_utc_t utc;
    int count;
    double interval, min_interval, max_interval;
    FILE *infile = NULL;
    char filename[100];
    char line[100];
    char expected_time_text[100];
    astro_time_t expected_time;
    int expected_kind;
    double expected_distance;
    double period;
    double diff_days, diff_degrees, max_degrees=0.0, diff_dist_ratio;
    const double degree_threshold = 0.1;
    double max_diff_days, max_dist_ratio;

    start_time = Astronomy_MakeTime(1700, 1, 1, 0, 0, 0.0);

    for (body = BODY_MERCURY; body <= BODY_PLUTO; ++body)
    {
        period = Astronomy_PlanetOrbitalPeriod(body);
        max_dist_ratio = 0.0;
        max_diff_days = 0.0;
        snprintf(filename, sizeof(filename)-1, "apsides/apsis_%d.txt", (int)body);
        if (infile) fclose(infile);
        infile = fopen(filename, "rt");
        if (infile == NULL)
            FFAIL("ERROR - cannot open input file: %s\n", filename);

        min_interval = max_interval = -1.0;
        apsis = Astronomy_SearchPlanetApsis(body, start_time);
        if (apsis.status != ASTRO_SUCCESS)
            FFAIL("ERROR %d finding first apsis for %s\n", apsis.status, Astronomy_BodyName(body));

        count = 1;
        while (ReadLine(line, sizeof(line), infile, filename, count-1))
        {
            /* Parse the line of test data. */
            if (   (3 != sscanf(line, "%d %s %lf", &expected_kind, expected_time_text, &expected_distance))
                || (expected_kind & ~1)     /* must be either 0=perihelion or 1=aphelion */
                || (expected_distance <= 0.0)
                || ParseDate(expected_time_text, &expected_time))
            {
                FFAIL("INPUT SYNTAX ERROR (%s line %d): '%s'\n", filename, count, line);
            }

            /* Compare computed values against expected values. */
            if (apsis.kind != expected_kind)
                FFAIL("WRONG APSIS KIND (%s line %d)\n", filename, count);

            diff_days = ABS(expected_time.tt - apsis.time.tt);
            if (diff_days > max_diff_days) max_diff_days = diff_days;
            diff_degrees = (diff_days / period) * 360.0;
            if (diff_degrees > max_degrees)
                max_degrees = diff_degrees;

            if (diff_degrees > degree_threshold)
                FFAIL("FAIL - %s exceeded angular threshold (%lg versus %lg degrees)\n", Astronomy_BodyName(body), max_degrees, degree_threshold);

            diff_dist_ratio = ABS(expected_distance - apsis.dist_au) / expected_distance;
            if (diff_dist_ratio > max_dist_ratio) max_dist_ratio = diff_dist_ratio;
            if (diff_dist_ratio > 1.05e-4)
            {
                FFAIL("EXCESSIVE DISTANCE ERROR for %s (%s line %d): expected=%0.16lf, calculated=%0.16lf, error ratio=%lg\n",
                    Astronomy_BodyName(body), filename, count, expected_distance, apsis.dist_au, diff_dist_ratio);
            }

            /* Calculate the next apsis. */
            prev_time = apsis.time;
            utc = Astronomy_UtcFromTime(apsis.time);
            apsis = Astronomy_NextPlanetApsis(body, apsis);
            if (apsis.status != ASTRO_SUCCESS)
            {
                FFAIL("ERROR %d finding apsis for %s after %04d-%02d-%02d\n",
                    apsis.status, Astronomy_BodyName(body), utc.year, utc.month, utc.day);
            }

            /* Update statistics. */
            ++count;
            interval = apsis.time.tt - prev_time.tt;
            if (min_interval < 0.0)
            {
                min_interval = max_interval = interval;
            }
            else
            {
                if (interval < min_interval)
                    min_interval = interval;
                if (interval > max_interval)
                    max_interval = interval;
            }
        }

        if (count < 2)
            FFAIL("FAILED to find apsides for %s\n", Astronomy_BodyName(body));

        FDEBUG("%5d apsides for %-9s -- intervals: min=%9.2lf, max=%9.2lf, ratio=%8.6lf; max day=%lg, degrees=%0.3lf, dist ratio=%lg\n",
            count, Astronomy_BodyName(body),
            min_interval, max_interval, max_interval / min_interval,
            max_diff_days,
            (max_diff_days / period) * 360.0,
            max_dist_ratio);
    }

    FPASS();
fail:
    if (infile) fclose(infile);
    return error;
}


/*-----------------------------------------------------------------------------------------------------------*/

static int CheckUnitVector(int lnum, const char *name, astro_rotation_t r, int i0, int j0, int di, int dj)
{
    int k;
    double x;
    double sum = 0.0;

    for (k=0; k<3; ++k)
    {
        x = r.rot[i0+k*di][j0+k*dj];
        sum += x*x;
    }
    x = ABS(sum - 1.0);
    if (x > 1.8e-15)
        FAILRET("C CheckUnitVector ERROR(%s line %d): unit error = %lg for i0=%d, j0=%d, di=%d, dj=%d\n", name, lnum, x, i0, j0, di, dj);

    return 0;
}

static int CheckRotationMatrix(int lnum, const char *name, astro_rotation_t r)
{
    if (r.status != ASTRO_SUCCESS)
        FAILRET("C CheckRotationMatrix ERROR(%s line %d): status = %d\n", name, lnum, r.status);

    /* Verify that every row and every column is a unit vector. */
    if (CheckUnitVector(lnum, name, r, 0, 0, 1, 0)) return 1;
    if (CheckUnitVector(lnum, name, r, 0, 1, 1, 0)) return 1;
    if (CheckUnitVector(lnum, name, r, 0, 2, 1, 0)) return 1;
    if (CheckUnitVector(lnum, name, r, 0, 0, 0, 1)) return 1;
    if (CheckUnitVector(lnum, name, r, 1, 0, 0, 1)) return 1;
    if (CheckUnitVector(lnum, name, r, 2, 0, 0, 1)) return 1;
    return 0;
}

#define CHECK_ROTMAT(r)   CHECK(CheckRotationMatrix(__LINE__, #r, (r)))

static int CompareVectors(const char *caller, astro_vector_t a, astro_vector_t b, double tolerance)
{
    double diff;

    if (a.status != ASTRO_SUCCESS)
        FAILRET("C CompareVectors ERROR(%s): a.status = %d\n", caller, a.status);

    if (b.status != ASTRO_SUCCESS)
        FAILRET("C CompareVectors ERROR(%s): b.status = %d\n", caller, b.status);

    diff = ABS(a.x - b.x);
    if (diff > tolerance)
        FAILRET("C CompareVectors ERROR(%s): x=%lg, expected %lg, diff %lg\n", caller, a.x, b.x, diff);

    diff = ABS(a.y - b.y);
    if (diff > tolerance)
        FAILRET("C CompareVectors ERROR(%s): y=%lg, expected %lg, diff %lg\n", caller, a.y, b.y, diff);

    diff = ABS(a.z - b.z);
    if (diff > tolerance)
        FAILRET("C CompareVectors ERROR(%s): z=%lg, expected %lg, diff %lg\n", caller, a.z, b.z, diff);

    return 0;
}

static int CompareMatrices(const char *caller, astro_rotation_t a, astro_rotation_t b, double tolerance)
{
    int i, j;

    if (a.status != ASTRO_SUCCESS)
        FAILRET("C CompareMatrices ERROR(%s): a.status = %d\n", caller, a.status);

    if (b.status != ASTRO_SUCCESS)
        FAILRET("C CompareMatrices ERROR(%s): b.status = %d\n", caller, b.status);

    for (i=0; i < 3; ++i)
    {
        for (j=0; j < 3; ++j)
        {
            double diff = ABS(a.rot[i][j] - b.rot[i][j]);
            if (diff > tolerance)
                FAILRET("C CompareMatrices ERROR(%s): matrix[%d][%d]=%lg, expected %lg, diff %lg\n", caller, i, j, a.rot[i][j], b.rot[i][j], diff);
        }
    }

    return 0;
}

static int Rotation_MatrixInverse(void)
{
    astro_rotation_t a, b, v;
    int error;

    a.status = ASTRO_SUCCESS;
    a.rot[0][0] = 1.0; a.rot[1][0] = 2.0; a.rot[2][0] = 3.0;
    a.rot[0][1] = 4.0; a.rot[1][1] = 5.0; a.rot[2][1] = 6.0;
    a.rot[0][2] = 7.0; a.rot[1][2] = 8.0; a.rot[2][2] = 9.0;

    v.status = ASTRO_SUCCESS;
    v.rot[0][0] = 1.0; v.rot[1][0] = 4.0; v.rot[2][0] = 7.0;
    v.rot[0][1] = 2.0; v.rot[1][1] = 5.0; v.rot[2][1] = 8.0;
    v.rot[0][2] = 3.0; v.rot[1][2] = 6.0; v.rot[2][2] = 9.0;

    b = Astronomy_InverseRotation(a);
    CHECK(CompareMatrices("Rotation_MatrixInverse", b, v, 0.0));

    FPASS();
fail:
    return error;
}

static int Rotation_Pivot(void)
{
    int error;
    astro_rotation_t ident;
    astro_rotation_t r;
    astro_rotation_t a;
    astro_vector_t v1, v2, ve;
    const double tolerance = 1.0e-15;

    /* Test #1 */

    /* Start with an identity matrix. */
    ident = Astronomy_IdentityMatrix();

    /* Pivot 90 degrees counterclockwise around the z-axis. */
    r = Astronomy_Pivot(ident, 2, +90.0);
    CHECK_STATUS(r);

    /* Put the expected answer in 'a'. */
    a.status = ASTRO_SUCCESS;
    a.rot[0][0] =  0.0;  a.rot[1][0] = -1.0;  a.rot[2][0] =  0.0;
    a.rot[0][1] = +1.0;  a.rot[1][1] =  0.0;  a.rot[2][1] =  0.0;
    a.rot[0][2] =  0.0;  a.rot[1][2] =  0.0;  a.rot[2][2] =  1.0;

    /* Compare actual 'r' with expected 'a'. */
    CHECK(CompareMatrices("Rotation_Pivot #1", r, a, tolerance));

    /* Test #2. */

    /* Pivot again, -30 degrees around the x-axis. */
    r = Astronomy_Pivot(r, 0, -30.0);
    CHECK_STATUS(r);

    /* Pivot a third time, 180 degrees around the y-axis. */
    r = Astronomy_Pivot(r, 1, +180.0);

    /* Use the 'r' matrix to rotate a vector. */
    v1.status = ASTRO_SUCCESS;
    v1.t = Astronomy_MakeTime(2000, 1, 1, 0, 0, 0.0);
    v1.x = 1.0;
    v1.y = 2.0;
    v1.z = 3.0;

    v2 = Astronomy_RotateVector(r, v1);
    CHECK_STATUS(v2);

    /* Initialize the expected vector 've'. */
    ve.status = ASTRO_SUCCESS;
    ve.t = v1.t;
    ve.x = +2.0;
    ve.y = +2.3660254037844390;
    ve.z = -2.0980762113533156;

    CHECK(CompareVectors("Rotation_Pivot #2", v2, ve, tolerance));

    FPASS();
fail:
    return error;
}

static int Rotation_MatrixMultiply(void)
{
    astro_rotation_t  a, b, c, v;
    int error;

    a.status = ASTRO_SUCCESS;
    a.rot[0][0] = 1.0; a.rot[1][0] = 2.0; a.rot[2][0] = 3.0;
    a.rot[0][1] = 4.0; a.rot[1][1] = 5.0; a.rot[2][1] = 6.0;
    a.rot[0][2] = 7.0; a.rot[1][2] = 8.0; a.rot[2][2] = 9.0;

    b.status = ASTRO_SUCCESS;
    b.rot[0][0] = 10.0; b.rot[1][0] = 11.0; b.rot[2][0] = 12.0;
    b.rot[0][1] = 13.0; b.rot[1][1] = 14.0; b.rot[2][1] = 15.0;
    b.rot[0][2] = 16.0; b.rot[1][2] = 17.0; b.rot[2][2] = 18.0;

    v.status = ASTRO_SUCCESS;
    v.rot[0][0] =  84.0; v.rot[1][0] =  90.0; v.rot[2][0] =  96.0;
    v.rot[0][1] = 201.0; v.rot[1][1] = 216.0; v.rot[2][1] = 231.0;
    v.rot[0][2] = 318.0; v.rot[1][2] = 342.0; v.rot[2][2] = 366.0;

    /* Let c = a*b. The order looks backwards in the call to make rotation semantics more intuitive. */
    c = Astronomy_CombineRotation(b, a);

    /* Verify that c = v. */
    CHECK(CompareMatrices("Rotation_MatrixMultiply", c, v, 0.0));

    FPASS();
fail:
    return error;
}

static int TestVectorFromAngles(double lat, double lon, double x, double y, double z)
{
    astro_spherical_t sphere;
    astro_vector_t vector;
    astro_time_t time;
    double diff, dx, dy, dz;

    /* Confirm the expected vector really is a unit vector. */
    diff = ABS((x*x + y*y + z*z) - 1.0);
    if (diff > 1.0e-16)
        FAILRET("C %s: EXCESSIVE unit error = %lg\n", __func__, diff);

    sphere.status = ASTRO_SUCCESS;
    sphere.lat = lat;
    sphere.lon = lon;
    sphere.dist = 1.0;

    time = Astronomy_MakeTime(2015, 3, 5, 12, 0, 0.0);
    vector = Astronomy_VectorFromSphere(sphere, time);

    if (vector.status != ASTRO_SUCCESS)
        FAILRET("C ERROR(TestVectorFromAngles): vector.status = %d\n", vector.status);

    dx = x - vector.x;
    dy = y - vector.y;
    dz = z - vector.z;
    diff = V(sqrt(dx*dx + dy*dy + dz*dz));

    DEBUG("C TestVectorFromAngles(%lf, %lf): diff = %lg\n", lat, lon, diff);
    if (diff > 2.0e-16)
        FAILRET("C %s: EXCESSIVE ERROR.\n", __func__);

    return 0;
}

static int TestAnglesFromVector(double lat, double lon, double x, double y, double z)
{
    astro_vector_t vector;
    astro_spherical_t sphere;
    double diff, latdiff, londiff;

    /* Confirm the expected vector really is a unit vector. */
    diff = ABS((x*x + y*y + z*z) - 1.0);
    if (diff > 1.0e-16)
        FAILRET("C TestAnglesFromVector(lat=%lf, lon=%lf, x=%lf, y=%lf, z=%lf): EXCESSIVE unit error = %lg\n", lat, lon, x, y, z, diff);

    vector.status = ASTRO_SUCCESS;
    vector.t = Astronomy_MakeTime(2015, 3, 5, 12, 0, 0.0);
    vector.x = x;
    vector.y = y;
    vector.z = z;

    sphere = Astronomy_SphereFromVector(vector);
    if (sphere.status != ASTRO_SUCCESS)
        FAILRET("C %s(lat=%lf, lon=%lf, x=%lf, y=%lf, z=%lf): sphere.status = %d\n", __func__, lat, lon, x, y, z, sphere.status);

    latdiff = ABS(sphere.lat - lat);
    londiff = ABS(sphere.lon - lon);
    DEBUG("TestAnglesFromVector(x=%lf, y=%lf, z=%lf): latdiff=%lg, londiff=%lg\n", x, y, z, latdiff, londiff);
    if (latdiff > 8.0e-15 || londiff > 8.0e-15)
        FAILRET("C %s: EXCESSIVE ERROR\n", __func__);

    return 0;
}

static astro_rotation_t RotateX(double rot)
{
    astro_rotation_t m;
    double ca, sa;

    /* https://en.wikipedia.org/wiki/Rotation_matrix */

    ca = cos(rot * DEG2RAD);
    sa = sin(rot * DEG2RAD);
    m.status = ASTRO_SUCCESS;
    m.rot[0][0] = 1.0;  m.rot[1][0] = 0.0;  m.rot[2][0] = 0.0;
    m.rot[0][1] = 0.0;  m.rot[1][1] = +ca;  m.rot[2][1] = -sa;
    m.rot[0][2] = 0.0;  m.rot[1][2] = +sa;  m.rot[2][2] = +ca;

    return m;
}

static astro_rotation_t RotateY(double rot)
{
    astro_rotation_t m;
    double ca, sa;

    /* https://en.wikipedia.org/wiki/Rotation_matrix */

    ca = cos(rot * DEG2RAD);
    sa = sin(rot * DEG2RAD);
    m.status = ASTRO_SUCCESS;
    m.rot[0][0] = +ca;  m.rot[1][0] = 0.0;  m.rot[2][0] = +sa;
    m.rot[0][1] = 0.0;  m.rot[1][1] = 1.0;  m.rot[2][1] = 0.0;
    m.rot[0][2] = -sa;  m.rot[1][2] = 0.0;  m.rot[2][2] = +ca;

    return m;
}

static astro_rotation_t RotateZ(double rot)
{
    astro_rotation_t m;
    double ca, sa;

    /* https://en.wikipedia.org/wiki/Rotation_matrix */

    ca = cos(rot * DEG2RAD);
    sa = sin(rot * DEG2RAD);
    m.status = ASTRO_SUCCESS;
    m.rot[0][0] = +ca;  m.rot[1][0] = -sa;  m.rot[2][0] = 0.0;
    m.rot[0][1] = +sa;  m.rot[1][1] = +ca;  m.rot[2][1] = 0.0;
    m.rot[0][2] = 0.0;  m.rot[1][2] = 0.0;  m.rot[2][2] = 1.0;

    return m;
}

static int TestSpin(
    double xrot, double yrot, double zrot,
    double sx, double sy, double sz,
    double tx, double ty, double tz)
{
    astro_rotation_t mx, my, mz, m;
    astro_vector_t sv, tv;
    double diff, dx, dy, dz;
    int error;

    /* https://en.wikipedia.org/wiki/Rotation_matrix */

    mx = RotateX(xrot);
    CHECK_ROTMAT(mx);
    my = RotateY(yrot);
    CHECK_ROTMAT(my);
    mz = RotateZ(zrot);
    CHECK_ROTMAT(mz);
    m = Astronomy_CombineRotation(mx, my);
    CHECK_ROTMAT(m);
    m = Astronomy_CombineRotation(m, mz);
    CHECK_ROTMAT(m);

    sv.status = ASTRO_SUCCESS;
    sv.x = sx;
    sv.y = sy;
    sv.z = sz;
    sv.t = Astronomy_MakeTime(2019, 5, 5, 12, 30, 0.0);

    tv = Astronomy_RotateVector(m, sv);
    if (tv.status != ASTRO_SUCCESS)
        FAIL("C ERROR(TestSpin): tv.status = %d\n", tv.status);

    if (tv.t.ut != sv.t.ut)
        FAIL("C ERROR(TestSpin): tv time != sv time\n");

    dx = tx - tv.x;
    dy = ty - tv.y;
    dz = tz - tv.z;
    diff = V(sqrt(dx*dx + dy*dy + dz*dz));
    DEBUG("C TestSpin(xrot=%0.0lf, yrot=%0.0lf, zrot=%0.0lf, sx=%0.0lf, sy=%0.0lf, sz=%0.0lf): diff = %lg\n", xrot, yrot, zrot, sx, sy, sz, diff);
    if (diff > 1.0e-15)
        FFAIL("EXCESSIVE ERROR\n");

    error = 0;
fail:
    return error;
}

static int Test_EQJ_GAL_NOVAS(const char *filename)
{
    /* Compare against ICRS/GAL calculated by NOVAS C 3.1. */
    const double THRESHOLD_SECONDS = 8.8;
    int error;
    double ra, dec, glon, glat;
    double dlon, dlat, diff, max_diff;
    FILE *infile = NULL;
    int lnum, nscanned;
    char line[100];
    astro_rotation_t rot, inv;
    astro_time_t time;
    astro_spherical_t eqj_sphere, gal_sphere;
    astro_vector_t eqj_vec, gal_vec;

    time = Astronomy_MakeTime(2000, 1, 1, 0, 0, 0.0);   /* placeholder time - value does not matter */

    rot = Astronomy_Rotation_EQJ_GAL();
    CHECK_STATUS(rot);

    inv = Astronomy_Rotation_GAL_EQJ();
    CHECK_STATUS(inv);
    CHECK_INVERSE(rot, inv);

    infile = fopen(filename, "rt");
    if (infile == NULL)
        FFAIL("Cannot open input file: %s\n", filename);

    max_diff = 0.0;
    lnum = 0;
    while (ReadLine(line, sizeof(line), infile, filename, lnum))
    {
        ++lnum;
        nscanned = sscanf(line, "%lf %lf %lf %lf", &ra, &dec, &glon, &glat);
        if (nscanned != 4)
            FAIL("C Test_EQJ_GAL_NOVAS(%s line %d): bad input format.\n", filename, lnum);
        V(ra);
        V(dec);
        V(glon);
        V(glat);

        /* Use Astronomy Engine to do the same EQJ/GAL conversion. */
        eqj_sphere.status = ASTRO_SUCCESS;
        eqj_sphere.dist = 1.0;
        eqj_sphere.lon = 15.0 * ra;     /* Convert from sidereal hours to degrees. */
        eqj_sphere.lat = dec;

        eqj_vec = Astronomy_VectorFromSphere(eqj_sphere, time);
        CHECK_STATUS(eqj_vec);

        gal_vec = Astronomy_RotateVector(rot, eqj_vec);
        CHECK_STATUS(gal_vec);

        gal_sphere = Astronomy_SphereFromVector(gal_vec);
        CHECK_STATUS(gal_sphere);

        dlat = V(gal_sphere.lat - glat);
        dlon = cos(DEG2RAD * glat) * V(gal_sphere.lon - glon);
        diff = V(3600.0 * sqrt(dlon*dlon + dlat*dlat));     /* error in arcseconds */
        if (diff > THRESHOLD_SECONDS)
            FAIL("C Test_EQJ_GAL_NOVAS(%s line %d): EXCESSIVE ERROR = %0.3lf arcseconds\n", filename, lnum, diff);

        if (diff > max_diff)
            max_diff = diff;
    }

    FDEBUG("PASS. max_diff = %0.3lf arcseconds.\n", max_diff);
    error = 0;
fail:
    if (infile != NULL) fclose(infile);
    return error;
}

static int Test_EQJ_GAL_JPL(const char *filename)
{
    /*
        Compare against ICRS/GAL calculated by JPL Horizons tool.
        We get a fairly large error of 23 arcseconds due because I
        can't figure out how to make JPL Horizons use the same
        aberration settings in equatorial and galactic coordinates.
    */
    const double THRESHOLD_SECONDS = 23.0;
    int error, lnum, nscanned;
    int found_begin = 0;
    int found_end = 0;
    double ra, dec, glon, glat;
    FILE *infile = NULL;
    char line[100];
    astro_rotation_t rot;
    astro_time_t time;
    astro_spherical_t eqj_sphere, gal_sphere;
    astro_vector_t eqj_vec, gal_vec;
    double dlon, dlat, diff, max_diff = 0.0;

    time = Astronomy_MakeTime(2000, 1, 1, 0, 0, 0.0);   /* placeholder time - value does not matter */

    rot = Astronomy_Rotation_EQJ_GAL();
    CHECK_STATUS(rot);

    infile = fopen(filename, "rt");
    if (infile == NULL)
        FFAIL("Cannot open input file: %s\n", filename);

    lnum = 0;
    while (!found_end && ReadLine(line, sizeof(line), infile, filename, lnum))
    {
        ++lnum;
        if (!found_begin)
        {
            if (strlen(line) >= 5 && !memcmp(line, "$$SOE", 5))
                found_begin = 1;
        }
        else if (strlen(line) >= 5 && !memcmp(line, "$$EOE", 5))
        {
            found_end = 1;
        }
        else
        {
            if (strlen(line) < 18)
                FAIL("C Test_EQJ_GAL_JPL(%s line %d): line is too short.\n", filename, lnum);

            nscanned = sscanf(line+18, "%lf %lf %lf %lf", &ra, &dec, &glon, &glat);
            if (nscanned != 4)
                FAIL("C Test_EQJ_GAL_JPL(%s line %d): invalid data format.\n", filename, lnum);

            V(ra);
            V(dec);
            V(glon);
            V(glat);

            /* Use Astronomy Engine to do the same EQJ/GAL conversion. */
            eqj_sphere.status = ASTRO_SUCCESS;
            eqj_sphere.dist = 1.0;
            eqj_sphere.lon = ra;        /* RA is already in degrees in the JPL Horizons input file. */
            eqj_sphere.lat = dec;

            eqj_vec = Astronomy_VectorFromSphere(eqj_sphere, time);
            CHECK_STATUS(eqj_vec);

            gal_vec = Astronomy_RotateVector(rot, eqj_vec);
            CHECK_STATUS(gal_vec);

            gal_sphere = Astronomy_SphereFromVector(gal_vec);
            CHECK_STATUS(gal_sphere);

            dlat = V(gal_sphere.lat - glat);
            dlon = cos(DEG2RAD * glat) * V(gal_sphere.lon - glon);
            diff = V(3600.0 * sqrt(dlon*dlon + dlat*dlat));     /* error in arcseconds */
            if (diff > THRESHOLD_SECONDS)
                FAIL("C Test_EQJ_GAL_JPL(%s line %d): EXCESSIVE ERROR = %0.3lf arcseconds\n", filename, lnum, diff);

            if (diff > max_diff)
                max_diff = diff;
        }
    }

    if (!found_begin)
        FFAIL("did not find begin-data marker in: %s\n", filename);

    if (!found_end)
        FFAIL("did not find end-data marker in: %s\n", filename);

    FDEBUG("PASS. max_diff = %0.3lf arcseconds.\n", max_diff);
    error = 0;
fail:
    if (infile != NULL) fclose(infile);
    return error;
}

static int Test_EQJ_EQD(astro_body_t body)
{
    astro_time_t time;
    astro_observer_t observer;
    astro_equatorial_t eq2000, eqdate, eqcheck;
    astro_vector_t v2000, vdate, t2000;
    astro_rotation_t r;
    double ra_diff, dec_diff, dist_diff, diff;
    int error;

    /* Verify conversion of equatorial J2000 to equatorial of-date, and back. */

    /* Use established functions to calculate spherical coordinates for the body, in both EQJ and EQD. */
    time = Astronomy_MakeTime(2019, 12, 8, 20, 50, 0.0);
    observer = Astronomy_MakeObserver(35, -85, 0);
    eq2000 = Astronomy_Equator(body, &time, observer, EQUATOR_J2000, ABERRATION);
    CHECK_STATUS(eq2000);
    eqdate = Astronomy_Equator(body, &time, observer, EQUATOR_OF_DATE, ABERRATION);
    CHECK_STATUS(eqdate);

    /* Convert EQJ angular coordinates to vector. */
    v2000 = eq2000.vec;
    CHECK_STATUS(v2000);

    /* Find rotation matrix. */
    r = Astronomy_Rotation_EQJ_EQD(&time);
    CHECK_ROTMAT(r);

    /* Rotate EQJ vector to EQD vector. */
    vdate = Astronomy_RotateVector(r, v2000);
    CHECK_STATUS(vdate);

    /* Convert vector back to angular coordinates. */
    eqcheck = Astronomy_EquatorFromVector(vdate);
    CHECK_STATUS(eqcheck);

    /* Compare the result with the eqdate. */
    ra_diff = ABS(eqcheck.ra - eqdate.ra);
    dec_diff = ABS(eqcheck.dec - eqdate.dec);
    dist_diff = ABS(eqcheck.dist - eqdate.dist);
    FDEBUG("%s ra=%0.3lf, dec=%0.3lf, dist=%0.3lf, ra_diff=%lg, dec_diff=%lg, dist_diff=%lg\n", Astronomy_BodyName(body), eqdate.ra, eqdate.dec, eqdate.dist, ra_diff, dec_diff, dist_diff);
    if (ra_diff > 1.0e-14 || dec_diff > 1.0e-14 || dist_diff > 4.0e-15)
        FFAIL("EXCESSIVE ERROR\n");

    /* Perform the inverse conversion back to equatorial J2000 coordinates. */
    r = Astronomy_Rotation_EQD_EQJ(&time);
    CHECK_ROTMAT(r);
    t2000 = Astronomy_RotateVector(r, vdate);
    CHECK_STATUS(t2000);
    CHECK(VectorDiff(t2000, v2000, &diff));
    FDEBUG("%s inverse diff = %lg\n", Astronomy_BodyName(body), diff);
    if (diff > 5.0e-15)
        FFAIL("EXCESSIVE INVERSE ERROR\n");

    error = 0;
fail:
    return error;
}

static int Test_EQD_HOR(astro_body_t body)
{
    astro_time_t time;
    astro_observer_t observer;
    astro_equatorial_t eqd, eqj;
    astro_horizon_t hor;
    astro_rotation_t rot;
    astro_spherical_t sphere;
    astro_vector_t vec_eqd, vec_eqj, vec_hor, check_eqd, check_eqj, check_hor;
    double diff_az, diff_alt, diff;
    int error;

    /* Use existing functions to calculate horizontal coordinates of the body for the time+observer. */
    time = Astronomy_MakeTime(1970, 12, 13, 5, 15, 0.0);
    observer = Astronomy_MakeObserver(-37.0, +45.0, 0.0);
    CHECK_EQU(eqd, Astronomy_Equator(body, &time, observer, EQUATOR_OF_DATE, ABERRATION));
    DEBUG("C Test_EQD_HOR %s: OFDATE ra=%0.3lf, dec=%0.3lf\n", Astronomy_BodyName(body), eqd.ra, eqd.dec);
    hor = Astronomy_Horizon(&time, observer, eqd.ra, eqd.dec, REFRACTION_NORMAL);

    /* Calculate the position of the body as an equatorial vector of date. */
    CHECK_VECTOR(vec_eqd, eqd.vec);

    /* Calculate rotation matrix to convert equatorial J2000 vector to horizontal vector. */
    rot = Astronomy_Rotation_EQD_HOR(&time, observer);
    CHECK_ROTMAT(rot);

    /* Rotate the equator of date vector to a horizontal vector. */
    CHECK_VECTOR(vec_hor, Astronomy_RotateVector(rot, vec_eqd));

    /* Convert the horizontal vector to horizontal angular coordinates. */
    sphere = Astronomy_HorizonFromVector(vec_hor, REFRACTION_NORMAL);
    CHECK_STATUS(sphere);

    diff_alt = ABS(sphere.lat - hor.altitude);
    diff_az = ABS(sphere.lon - hor.azimuth);

    DEBUG("C Test_EQD_HOR %s: trusted alt=%0.3lf, az=%0.3lf; test alt=%0.3lf, az=%0.3lf; diff_alt=%lg, diff_az=%lg\n",
        Astronomy_BodyName(body), hor.altitude, hor.azimuth, sphere.lat, sphere.lon, diff_alt, diff_az);

    if (diff_alt > 3.6e-14 || diff_az > 1.2e-13)
        FFAIL("EXCESSIVE HORIZONTAL ERROR.\n");

    /* Confirm that we can convert back to horizontal vector. */
    CHECK_VECTOR(check_hor, Astronomy_VectorFromHorizon(sphere, time, REFRACTION_NORMAL));
    CHECK(VectorDiff(check_hor, vec_hor, &diff));
    DEBUG("C Test_EQD_HOR %s: horizontal recovery: diff = %lg\n", Astronomy_BodyName(body), diff);
    if (diff > 3.0e-15)
        FFAIL("EXCESSIVE ERROR IN HORIZONTAL RECOVERY.\n");

    /* Verify the inverse translation from horizontal vector to equatorial of-date vector. */
    rot = Astronomy_Rotation_HOR_EQD(&time, observer);
    CHECK_ROTMAT(rot);
    CHECK_VECTOR(check_eqd, Astronomy_RotateVector(rot, vec_hor));
    CHECK(VectorDiff(check_eqd, vec_eqd, &diff));
    DEBUG("C Test_EQD_HOR %s: OFDATE inverse rotation diff = %lg\n", Astronomy_BodyName(body), diff);
    if (diff > 2.67e-15)
        FFAIL("EXCESSIVE OFDATE INVERSE HORIZONTAL ERROR.\n");

    /* Exercise HOR to EQJ translation. */
    CHECK_EQU(eqj, Astronomy_Equator(body, &time, observer, EQUATOR_J2000, ABERRATION));
    CHECK_VECTOR(vec_eqj, eqj.vec);

    rot = Astronomy_Rotation_HOR_EQJ(&time, observer);
    CHECK_ROTMAT(rot);
    CHECK_VECTOR(check_eqj, Astronomy_RotateVector(rot, vec_hor));
    CHECK(VectorDiff(check_eqj, vec_eqj, &diff));
    DEBUG("C Test_EQD_HOR %s: J2000 inverse rotation diff = %lg\n", Astronomy_BodyName(body), diff);
    if (diff > 6.0e-15)
        FFAIL("EXCESSIVE J2000 INVERSE HORIZONTAL ERROR.\n");

    /* Verify the inverse translation: EQJ to HOR. */
    rot = Astronomy_Rotation_EQJ_HOR(&time, observer);
    CHECK_ROTMAT(rot);
    CHECK_VECTOR(check_hor, Astronomy_RotateVector(rot, vec_eqj));
    CHECK(VectorDiff(check_hor, vec_hor, &diff));
    DEBUG("C Test_EQD_HOR %s: EQJ inverse rotation diff = %lg\n", Astronomy_BodyName(body), diff);
    if (diff > 5.0e-15)
        FFAIL("EXCESSIVE EQJ INVERSE HORIZONTAL ERROR.\n");

    error = 0;
fail:
    return error;
}

static int Test_EQD_ECT(void)
{
    int error = 1;
    int count = 0;
    astro_time_t time = Astronomy_MakeTime(1900, 1, 1, 0, 0, 0.0);
    astro_time_t stopTime = Astronomy_MakeTime(2100, 1, 1, 0, 0, 0.0);
    astro_vector_t eqj, eqd, ect, check_ect;
    astro_rotation_t eqj_eqd, eqd_ect;
    astro_spherical_t sphere;
    double diff, max_diff = 0.0;

    while (time.ut <= stopTime.ut)
    {
        /* Get Moon's geocentric position in EQJ. */
        eqj = Astronomy_GeoMoon(time);
        CHECK_STATUS(eqj);

        /* Convert EQJ to EQD. */
        eqj_eqd = Astronomy_Rotation_EQJ_EQD(&time);
        CHECK_ROTMAT(eqj_eqd);
        eqd = Astronomy_RotateVector(eqj_eqd, eqj);
        CHECK_STATUS(eqd);

        /* Convert EQD to ECT. */
        eqd_ect = Astronomy_Rotation_EQD_ECT(&time);
        CHECK_ROTMAT(eqd_ect);
        ect = Astronomy_RotateVector(eqd_ect, eqd);
        CHECK_STATUS(ect);

        /* Independently get the Moon's spherical coordinates in ECT. */
        sphere = Astronomy_EclipticGeoMoon(time);
        CHECK_STATUS(sphere);

        /* Convert spherical coordinates to ECT vector. */
        check_ect = Astronomy_VectorFromSphere(sphere, time);
        CHECK_STATUS(check_ect);

        /* Verify the two ECT vectors are identical, within tolerance. */
        CHECK(VectorDiff(ect, check_ect, &diff));
        if (diff > max_diff) max_diff = diff;

        time = Astronomy_AddDays(time, 10.0);
        ++count;
    }

    if (max_diff > 3.743e-18)
        FFAIL("excessive vector diff = %0.6le AU.\n", max_diff);

    FDEBUG("PASS: count = %d, max_diff = %0.6le AU\n", count, max_diff);
    error = 0;
fail:
    return error;
}


static int EclipticTest(void)
{
    int error = 1;
    int count = 0;
    astro_time_t time = Astronomy_MakeTime(1900, 1, 1, 0, 0, 0.0);
    astro_time_t stopTime = Astronomy_MakeTime(2100, 1, 1, 0, 0, 0.0);
    astro_vector_t eqj, check_ect;
    astro_spherical_t sphere;
    astro_ecliptic_t eclip;
    double diff, max_vec_diff = 0.0, max_angle_diff = 0.0;

    while (time.ut <= stopTime.ut)
    {
        /* Get Moon's geocentric position in EQJ. */
        eqj = Astronomy_GeoMoon(time);
        CHECK_STATUS(eqj);

        /* Convert EQJ to ECT. */
        eclip = Astronomy_Ecliptic(eqj);
        CHECK_STATUS(eclip);

        /* Confirm that the ecliptic angles and ecliptic vector are consistent.*/
        CHECK_STATUS(eclip.vec);
        V(eclip.vec.x);
        V(eclip.vec.y);
        V(eclip.vec.z);
        sphere.dist = V(Astronomy_VectorLength(eclip.vec));
        sphere.lat = V(eclip.elat);
        sphere.lon = V(eclip.elon);
        sphere.status = ASTRO_SUCCESS;
        check_ect = Astronomy_VectorFromSphere(sphere, time);
        CHECK(VectorDiff(eclip.vec, check_ect, &diff));
        if (diff > max_angle_diff) max_angle_diff = diff;

        /* Independently get the Moon's spherical coordinates in ECT. */
        sphere = Astronomy_EclipticGeoMoon(time);
        CHECK_STATUS(sphere);

        /* Convert spherical coordinates to ECT vector. */
        check_ect = Astronomy_VectorFromSphere(sphere, time);
        CHECK_STATUS(check_ect);

        /* Verify the two ECT vectors are identical, within tolerance. */
        CHECK(VectorDiff(eclip.vec, check_ect, &diff));
        if (diff > max_vec_diff) max_vec_diff = diff;

        time = Astronomy_AddDays(time, 10.0);
        ++count;
    }

    if (max_angle_diff > 2.910e-18)
        FFAIL("EXCESSIVE ANGLE DIFF = %0.6le AU.\n", max_angle_diff);

    if (max_vec_diff > 3.388e-18)
        FFAIL("EXCESSIVE VECTOR DIFF = %0.6le AU.\n", max_vec_diff);

    FPASSA("PASS: count = %d, max_vec_diff = %0.6le AU, max_angle_diff = %0.6le AU.\n", count, max_vec_diff, max_angle_diff);
fail:
    return error;
}


static int CheckInverse(const char *aname, const char *bname, astro_rotation_t arot, astro_rotation_t brot)
{
    int error;
    astro_rotation_t crot;
    astro_rotation_t identity;

    CHECK(CheckRotationMatrix(__LINE__, aname, arot));
    CHECK(CheckRotationMatrix(__LINE__, bname, brot));

    crot = Astronomy_CombineRotation(arot, brot);
    CHECK_ROTMAT(crot);

    identity.status = ASTRO_SUCCESS;
    identity.rot[0][0] = 1; identity.rot[1][0] = 0; identity.rot[2][0] = 0;
    identity.rot[0][1] = 0; identity.rot[1][1] = 1; identity.rot[2][1] = 0;
    identity.rot[0][2] = 0; identity.rot[1][2] = 0; identity.rot[2][2] = 1;

    CHECK(CompareMatrices("CheckInverse", crot, identity, 2.0e-15));

    error = 0;
fail:
    return error;
}

static int CheckCycle(
    const char *cyclename,
    astro_rotation_t arot, astro_rotation_t brot, astro_rotation_t crot)
{
    int error;
    astro_rotation_t xrot;

    xrot = Astronomy_CombineRotation(arot, brot);
    CHECK_ROTMAT(xrot);
    xrot = Astronomy_InverseRotation(xrot);
    CHECK(CompareMatrices(cyclename, crot, xrot, 2.0e-15));
    error = 0;
fail:
    return error;
}

#define CHECK_CYCLE(a,b,c)  CHECK(CheckCycle(("CheckCycle: " #a ", " #b ", " #c), a, b, c))

static int Test_RotRoundTrip(void)
{
    int error;
    astro_time_t time;
    astro_observer_t observer;
    astro_rotation_t eqj_ecl, ecl_eqj;
    astro_rotation_t eqj_hor, hor_eqj;
    astro_rotation_t eqj_eqd, eqd_eqj;
    astro_rotation_t hor_eqd, eqd_hor;
    astro_rotation_t eqd_ecl, ecl_eqd;
    astro_rotation_t hor_ecl, ecl_hor;
    astro_rotation_t eqd_ect, ect_eqd;
    astro_rotation_t eqj_ect, ect_eqj;

    time = Astronomy_MakeTime(2067, 5, 30, 14, 45, 0.0);
    observer = Astronomy_MakeObserver(+28.0, -82.0, 0.0);

    /*
        In each round trip, calculate a forward rotation and a backward rotation.
        Verify the two are inverse matrices.
    */

    /* Round trip #1: EQJ <==> EQD. */
    eqj_eqd = Astronomy_Rotation_EQJ_EQD(&time);
    eqd_eqj = Astronomy_Rotation_EQD_EQJ(&time);
    CHECK_INVERSE(eqj_eqd, eqd_eqj);

    /* Round trip #2: EQJ <==> ECL. */
    eqj_ecl = Astronomy_Rotation_EQJ_ECL();
    ecl_eqj = Astronomy_Rotation_ECL_EQJ();
    CHECK_INVERSE(eqj_ecl, ecl_eqj);

    /* Round trip #3: EQJ <==> HOR. */
    eqj_hor = Astronomy_Rotation_EQJ_HOR(&time, observer);
    hor_eqj = Astronomy_Rotation_HOR_EQJ(&time, observer);
    CHECK_INVERSE(eqj_hor, hor_eqj);

    /* Round trip #4: EQD <==> HOR. */
    eqd_hor = Astronomy_Rotation_EQD_HOR(&time, observer);
    hor_eqd = Astronomy_Rotation_HOR_EQD(&time, observer);
    CHECK_INVERSE(eqd_hor, hor_eqd);

    /* Round trip #5: EQD <==> ECL. */
    eqd_ecl = Astronomy_Rotation_EQD_ECL(&time);
    ecl_eqd = Astronomy_Rotation_ECL_EQD(&time);
    CHECK_INVERSE(eqd_ecl, ecl_eqd);

    /* Round trip #6: HOR <==> ECL. */
    hor_ecl = Astronomy_Rotation_HOR_ECL(&time, observer);
    ecl_hor = Astronomy_Rotation_ECL_HOR(&time, observer);
    CHECK_INVERSE(hor_ecl, ecl_hor);

    /* Round trip #7: EQD <==> ECT. */
    eqd_ect = Astronomy_Rotation_EQD_ECT(&time);
    ect_eqd = Astronomy_Rotation_ECT_EQD(&time);
    CHECK_INVERSE(eqd_ect, ect_eqd);

    /* Round trip #8: EQJ <==> ECT. */
    eqj_ect = Astronomy_Rotation_EQJ_ECT(&time);
    ect_eqj = Astronomy_Rotation_ECT_EQJ(&time);
    CHECK_INVERSE(eqj_ect, ect_eqj);

    /*
        Verify that combining different sequences of rotations result
        in the expected combination.
        For example, (EQJ ==> HOR ==> ECL) must be the same matrix as (EQJ ==> ECL).
    */
    CHECK_CYCLE(eqj_ecl, ecl_eqd, eqd_eqj);
    CHECK_CYCLE(eqj_hor, hor_ecl, ecl_eqj);
    CHECK_CYCLE(eqj_hor, hor_eqd, eqd_eqj);
    CHECK_CYCLE(ecl_eqd, eqd_hor, hor_ecl);
    CHECK_CYCLE(eqj_eqd, eqd_ect, ect_eqj);

    FDEBUG("PASS\n");
    error = 0;
fail:
    return error;
}

static int RotationTest(void)
{
    int error;
    CHECK(Rotation_MatrixInverse());
    CHECK(Rotation_MatrixMultiply());
    CHECK(Rotation_Pivot());

    /* Verify conversion of spherical coordinates to vector. */
    CHECK(TestVectorFromAngles(0.0, 0.0, 1.0, 0.0, 0.0));
    CHECK(TestVectorFromAngles(0.0, 90.0, 0.0, 1.0, 0.0));
    CHECK(TestVectorFromAngles(0.0, 180.0, -1.0, 0.0, 0.0));
    CHECK(TestVectorFromAngles(0.0, 270.0, 0.0, -1.0, 0.0));
    CHECK(TestVectorFromAngles(+90.0, 0.0, 0.0, 0.0, 1.0));
    CHECK(TestVectorFromAngles(-90.0, 0.0, 0.0, 0.0, -1.0));
    CHECK(TestVectorFromAngles(-30.0, +60.0, 0.43301270189221946, 0.75, -0.5));

    /* Verify conversion of vector to spherical coordinates. */
    CHECK(TestAnglesFromVector(0.0, 0.0, 1.0, 0.0, 0.0));
    CHECK(TestAnglesFromVector(0.0, 90.0, 0.0, 1.0, 0.0));
    CHECK(TestAnglesFromVector(0.0, 180.0, -1.0, 0.0, 0.0));
    CHECK(TestAnglesFromVector(0.0, 270.0, 0.0, -1.0, 0.0));
    CHECK(TestAnglesFromVector(+90.0, 0.0, 0.0, 0.0, 1.0));
    CHECK(TestAnglesFromVector(-90.0, 0.0, 0.0, 0.0, -1.0));
    CHECK(TestAnglesFromVector(-30.0, +60.0, 0.43301270189221946, 0.75, -0.5));

    /* Verify rotation of vectors */
    CHECK(TestSpin(0.0, 0.0, 90.0, +1, +2, +3, -2, +1, +3));
    CHECK(TestSpin(0.0, 0.0, 0.0, 1.0, 2.0, -3.0, 1.0, 2.0, -3.0));
    CHECK(TestSpin(90.0, 0.0, 0.0, +1, +2, +3, +1, -3, +2));
    CHECK(TestSpin(0.0, 90.0, 0.0, +1, +2, +3, +3, +2, -1));
    CHECK(TestSpin(180.0, 180.0, 180.0, +1, +2, +3, +1, +2, +3));
    CHECK(TestSpin(0.0, 0.0, -45.0, +1, 0, 0, +0.7071067811865476, -0.7071067811865476, 0));

    CHECK(Test_EQJ_GAL_NOVAS("temp/galeqj.txt"));
    CHECK(Test_EQJ_GAL_JPL("galactic/mars.txt"));

    CHECK(Test_EQJ_EQD(BODY_MERCURY));
    CHECK(Test_EQJ_EQD(BODY_VENUS));
    CHECK(Test_EQJ_EQD(BODY_MARS));
    CHECK(Test_EQJ_EQD(BODY_JUPITER));
    CHECK(Test_EQJ_EQD(BODY_SATURN));

    CHECK(Test_EQD_HOR(BODY_MERCURY));
    CHECK(Test_EQD_HOR(BODY_VENUS));
    CHECK(Test_EQD_HOR(BODY_MARS));
    CHECK(Test_EQD_HOR(BODY_JUPITER));
    CHECK(Test_EQD_HOR(BODY_SATURN));

    CHECK(Test_EQD_ECT());

    CHECK(Test_RotRoundTrip());

    FPASS();
fail:
    return error;
}

static int VectorDiff(astro_vector_t a, astro_vector_t b, double *diff)
{
    double dx, dy, dz;

    *diff = NAN;

    if (a.status != ASTRO_SUCCESS)
        FAILRET("C %s: ERROR - first vector has status %d\n", __func__, a.status);

    if (b.status != ASTRO_SUCCESS)
        FAILRET("C %s: ERROR - second vector has status %d\n", __func__, b.status);

    dx = a.x - b.x;
    dy = a.y - b.y;
    dz = a.z - b.z;
    *diff = V(sqrt(dx*dx + dy*dy + dz*dz));
    return 0;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int RefractionTest(void)
{
    int error;
    double alt, corrected, refr, inv_refr, check_alt, diff;

    for (alt = -90.1; alt <= +90.1; alt += 0.001)
    {
        refr = Astronomy_Refraction(REFRACTION_NORMAL, alt);
        corrected = alt + refr;
        inv_refr = Astronomy_InverseRefraction(REFRACTION_NORMAL, corrected);
        check_alt = corrected + inv_refr;
        diff = ABS(check_alt - alt);
        if (diff > 2.0e-14)
            FFAIL("EXCESSIVE diff: alt=%8.3lf, refr=%10.6lf, diff=%lg\n", alt, refr, diff);
    }

    FPASS();
fail:
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int ConstellationTest(void)
{
    int error = 1;
    FILE *infile = NULL;
    const char *inFileName = "constellation/test_input.txt";
    char line[100];
    double ra, dec;
    char symbol[4];
    int lnum, failcount, id;
    astro_constellation_t constel;

    infile = fopen(inFileName, "rt");
    if (infile == NULL)
        FFAIL("Cannot open input file: %s\n", inFileName);

    lnum = 0;
    failcount = 0;
    while (ReadLine(line, sizeof(line), infile, inFileName, lnum))
    {
        ++lnum;
        if (4 != sscanf(line, "%d %lf %lf %3s", &id, &ra, &dec, symbol) || 3 != strlen(symbol))
            FFAIL("Invalid test data in %s line %d\n", inFileName, lnum);

        constel = Astronomy_Constellation(ra, dec);
        if (constel.status != ASTRO_SUCCESS)
            FFAIL("FAILED star %d with status %d: %s line %d\n", id, constel.status, inFileName, lnum);

        if (constel.symbol == NULL || constel.name == NULL)
            FFAIL("UNEXPECTED NULL name/symbol: star %d, %s line %d\n", id, inFileName, lnum);

        if (strcmp(constel.symbol, symbol))
        {
            fprintf(stderr, "Star %6d: expected %s, found %s at B1875 RA=%10.6lf, DEC=%10.6lf\n", id, symbol, constel.symbol, constel.ra_1875, constel.dec_1875);
            ++failcount;
        }
    }

    if (failcount > 0)
        FFAIL("%d failures\n", failcount);

    FPASSA("(verified %d)\n", lnum);
fail:
    if (infile != NULL) fclose(infile);
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static void PrintTime(astro_time_t time)
{
    astro_utc_t utc = Astronomy_UtcFromTime(time);
    printf("%04d-%02d-%02dT%02d:%02d:%06.3lfZ", utc.year, utc.month, utc.day, utc.hour, utc.minute, utc.second);
}

static int LunarEclipseIssue78(void)
{
    int error;
    astro_lunar_eclipse_t eclipse;
    astro_time_t search_time   = Astronomy_MakeTime(2020, 12, 19, 0, 0, 0.0);
    astro_time_t expected_peak = Astronomy_MakeTime(2021, 5, 26, 11, 18, 42);  /* https://www.timeanddate.com/eclipse/lunar/2021-may-26 */
    double dt_seconds;

    eclipse = Astronomy_SearchLunarEclipse(search_time);
    CHECK_STATUS(eclipse);

    dt_seconds = (24.0 * 3600.0) * ABS(eclipse.peak.tt - expected_peak.tt);
    if (dt_seconds > 40.0)
        FFAIL("Excessive prediction error = %lf seconds.\n", dt_seconds);

    if (eclipse.kind != ECLIPSE_TOTAL)
        FFAIL("Expected total eclipse; found %d\n", eclipse.kind);

    FPASS();
fail:
    return error;
}

static int LunarEclipseTest(void)
{
    const char *filename = "eclipse/lunar_eclipse.txt";
    const char *statsFileName = "eclipse/c_le_stats.csv";
    FILE *infile = NULL;
    FILE *outfile = NULL;
    int lnum = 0;
    int error = 1;
    char line[100];
    astro_time_t peak_time;
    astro_lunar_eclipse_t eclipse;
    double partial_minutes, total_minutes;
    double diff_minutes, max_diff_minutes = 0.0, sum_diff_minutes = 0.0;
    int diff_count = 0;
    double diff_days;
    int sd_valid = 0;
    int frac_valid = 0;
    int skip_count = 0;
    const double diff_limit = 2.0;
    extern int _CalcMoonCount;      /* incremented by Astronomy Engine every time expensive CalcMoon() is called */

    _CalcMoonCount = 0;

    infile = fopen(filename, "rt");
    if (infile == NULL)
        FFAIL("Cannot open input file: %s\n", filename);

    outfile = fopen(statsFileName, "wt");
    if (outfile == NULL)
        FFAIL("Cannot open output stats file: %s\n", statsFileName);

    eclipse = Astronomy_SearchLunarEclipse(Astronomy_MakeTime(1701, 1, 1, 0, 0, 0.0));
    CHECK_STATUS(eclipse);

    fprintf(outfile, "\"utc\",\"center\",\"partial\",\"total\"\n");
    while (ReadLine(line, sizeof(line), infile, filename, lnum))
    {
        ++lnum;

        /* Make sure numeric data are finite numbers. */
        V(eclipse.obscuration);
        V(eclipse.sd_partial);
        V(eclipse.sd_penum);
        V(eclipse.sd_total);

        /* scan test data */

        /* 2021-05-26T11:19Z  94   9 */
        if (strlen(line) < 17)
            FAIL("C LunarEclipseTest(%s line %d): line is too short.\n", filename, lnum);

        line[17] = '\0';
        CHECK(ParseDate(line, &peak_time));
        if (2 != sscanf(line+18, "%lf %lf", &partial_minutes, &total_minutes))
            FAIL("C LunarEclipseTest(%s line %d): invalid data format.\n", filename, lnum);

        /* Verify that the calculated eclipse semi-durations are consistent with the kind. */
        /* Verify that obscuration values also make sense for the kind. */

        switch (eclipse.kind)
        {
        case ECLIPSE_PENUMBRAL:
            sd_valid = (eclipse.sd_penum > 0.0) && (eclipse.sd_partial == 0.0) && (eclipse.sd_total == 0.0);
            frac_valid = (eclipse.obscuration == 0.0);
            break;

        case ECLIPSE_PARTIAL:
            sd_valid = (eclipse.sd_penum > 0.0) && (eclipse.sd_partial > 0.0) && (eclipse.sd_total == 0.0);
            frac_valid = (eclipse.obscuration > 0.0) && (eclipse.obscuration < 1.0);
            break;

        case ECLIPSE_TOTAL:
            sd_valid = (eclipse.sd_penum > 0.0) && (eclipse.sd_partial > 0.0) && (eclipse.sd_total > 0.0);
            frac_valid = (eclipse.obscuration == 1.0);
            break;

        default:
            FAIL("C LunarEclipseTest(%s line %d): invalid eclipse kind %d.\n", filename, lnum, eclipse.kind);
        }

        if (!sd_valid)
        {
            FAIL("C LunarEclipseTest(%s line %d): invalid semiduration(s) for kind %d: penum=%lf, partial=%lf, total=%lf.\n",
                filename, lnum, eclipse.kind, eclipse.sd_penum, eclipse.sd_partial, eclipse.sd_total);
        }

        if (!frac_valid)
        {
            FAIL("C LunarEclipseTest(%s line %d): invalid obscuration = %0.16lf for kind %d\n",
                filename, lnum, eclipse.obscuration, eclipse.kind);
        }

        /* check eclipse peak time */

        diff_days = eclipse.peak.ut - peak_time.ut;
        /* tolerate missing penumbral eclipses - skip to next input line without calculating next eclipse. */
        if (partial_minutes == 0.0 && diff_days > 20.0)
        {
            ++skip_count;
            continue;
        }

        fprintf(outfile, "\"%s\",%lf,%lf,%lf\n",
            line,   /* was already truncated at offset [17] above, so now has only UTC */
            diff_days * (24.0 * 60.0),
            eclipse.sd_partial - partial_minutes,
            eclipse.sd_total - total_minutes
        );

        diff_minutes = (24.0 * 60.0) * ABS(diff_days);
        sum_diff_minutes += diff_minutes;
        ++diff_count;

        if (diff_minutes > diff_limit)
        {
            printf("C LunarEclipseTest expected peak: ");
            PrintTime(peak_time);
            printf("\n");
            printf("C LunarEclipseTest found    peak: ");
            PrintTime(eclipse.peak);
            printf("\n");
            FAIL("C LunarEclipseTest(%s line %d): EXCESSIVE peak time error = %lf minutes (%lf days).\n", filename, lnum, diff_minutes, diff_days);
        }

        if (diff_minutes > max_diff_minutes)
            max_diff_minutes = diff_minutes;

        /* check partial eclipse duration */

        diff_minutes = ABS(partial_minutes - eclipse.sd_partial);
        sum_diff_minutes += diff_minutes;
        ++diff_count;

        if (diff_minutes > diff_limit)
            FAIL("C LunarEclipseTest(%s line %d): EXCESSIVE partial eclipse semiduration error: %lf minutes\n", filename, lnum, diff_minutes);

        if (diff_minutes > max_diff_minutes)
            max_diff_minutes = diff_minutes;

        /* check total eclipse duration */

        diff_minutes = ABS(total_minutes - eclipse.sd_total);
        sum_diff_minutes += diff_minutes;
        ++diff_count;

        if (diff_minutes > diff_limit)
            FAIL("C LunarEclipseTest(%s line %d): EXCESSIVE total eclipse semiduration error: %lf minutes\n", filename, lnum, diff_minutes);

        if (diff_minutes > max_diff_minutes)
            max_diff_minutes = diff_minutes;

        /* calculate for next iteration */

        eclipse = Astronomy_NextLunarEclipse(eclipse.peak);
        if (eclipse.status != ASTRO_SUCCESS)
            FAIL("C LunarEclipseTest(%s line %d): Astronomy_NextLunarEclipse returned status %d\n", filename, lnum, eclipse.status);
    }

    sum_diff_minutes /= diff_count;     /* convert to average error in minutes */
    if (sum_diff_minutes > 0.274)
        FFAIL("EXCESSIVE AVERAGE TIME ERROR: %lf\n", sum_diff_minutes);

    if (skip_count > 9)
        FFAIL("Skipped too many penumbral eclipses: %d\n", skip_count);

    FPASSA("(verified %d, skipped %d, %d CalcMoons, max_diff_minutes = %0.3lf, avg_diff_minutes = %0.3lf)\n", lnum, skip_count, _CalcMoonCount, max_diff_minutes, sum_diff_minutes);
fail:
    if (infile != NULL) fclose(infile);
    if (outfile != NULL) fclose(outfile);
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int LunarFractionCase(int year, int month, int day, double obscuration)
{
    int error;
    astro_time_t time;
    astro_lunar_eclipse_t eclipse;
    double dt, diff;

    /* Search for the first lunar eclipse to occur after the given date. */
    /* It should always happen within 24 hours of the given date. */
    time = Astronomy_MakeTime(year, month, day, 0, 0, 0.0);
    eclipse = Astronomy_SearchLunarEclipse(time);
    CHECK_STATUS(eclipse);

    if (eclipse.kind != ECLIPSE_PARTIAL)
        FAIL("C LunarFractionCase(%04d-%02d-%02d): expected partial eclipse, but found %d.\n", year, month, day, eclipse.kind);

    dt = V(eclipse.peak.ut - time.ut);
    if (dt < 0.0 || dt > 1.0)
        FAIL("C LunarFractionCase(%04d-%02d-%02d): eclipse occurs %0.4lf days after predicted date.\n", year, month, day, dt);

    diff = V(eclipse.obscuration - obscuration);
    if (ABS(diff) > 0.00763)
        FAIL("C LunarFractionCase(%04d-%02d-%02d) FAIL: obscuration error = %0.8lf, expected = %0.3lf, actual = %0.8lf\n", year, month, day, diff, obscuration, eclipse.obscuration);
    DEBUG("C LunarFractionCase(%04d-%02d-%02d) fraction error = %0.8lf\n", year, month, day, diff);

fail:
    return error;
}


static int LunarFractionTest(void)
{
    int error;

    /* Verify calculation of the fraction of the Moon's disc covered by the Earth's umbra during a partial eclipse. */
    /* Data for this is more tedious to gather, because Espenak data does not contain it. */
    /* We already verify fraction=0.0 for penumbral eclipses and fraction=1.0 for total eclipses in LunarEclipseTest. */

    CHECK(LunarFractionCase(2010,  6, 26, 0.506));  /* https://www.timeanddate.com/eclipse/lunar/2010-june-26 */
    CHECK(LunarFractionCase(2012,  6,  4, 0.304));  /* https://www.timeanddate.com/eclipse/lunar/2012-june-4 */
    CHECK(LunarFractionCase(2013,  4, 25, 0.003));  /* https://www.timeanddate.com/eclipse/lunar/2013-april-25 */
    CHECK(LunarFractionCase(2017,  8,  7, 0.169));  /* https://www.timeanddate.com/eclipse/lunar/2017-august-7 */
    CHECK(LunarFractionCase(2019,  7, 16, 0.654));  /* https://www.timeanddate.com/eclipse/lunar/2019-july-16 */
    CHECK(LunarFractionCase(2021, 11, 19, 0.991));  /* https://www.timeanddate.com/eclipse/lunar/2021-november-19 */
    CHECK(LunarFractionCase(2023, 10, 28, 0.060));  /* https://www.timeanddate.com/eclipse/lunar/2023-october-28 */
    CHECK(LunarFractionCase(2024,  9, 18, 0.035));  /* https://www.timeanddate.com/eclipse/lunar/2024-september-18 */
    CHECK(LunarFractionCase(2026,  8, 28, 0.962));  /* https://www.timeanddate.com/eclipse/lunar/2026-august-28 */
    CHECK(LunarFractionCase(2028,  1, 12, 0.024));  /* https://www.timeanddate.com/eclipse/lunar/2028-january-12 */
    CHECK(LunarFractionCase(2028,  7,  6, 0.325));  /* https://www.timeanddate.com/eclipse/lunar/2028-july-6 */
    CHECK(LunarFractionCase(2030,  6, 15, 0.464));  /* https://www.timeanddate.com/eclipse/lunar/2030-june-15 */

    FPASS();
fail:
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int GlobalSolarEclipseTest(void)
{
    const int expected_count = 1180;
    int error = 1;
    FILE *infile = NULL;
    const char *inFileName = "eclipse/solar_eclipse.txt";
    int lnum;
    char line[100];
    char typeChar;
    double deltaT, lat, lon;
    astro_time_t peak;
    astro_global_solar_eclipse_t eclipse;
    astro_eclipse_kind_t expected_kind;
    double diff_days, diff_minutes, max_minutes=0.0;
    double diff_angle, max_angle=0.0;
    int skip_count = 0;
    extern int _CalcMoonCount;      /* incremented by Astronomy Engine every time expensive CalcMoon() is called */

    _CalcMoonCount = 0;

    infile = fopen(inFileName, "rt");
    if (infile == NULL)
        FFAIL("Cannot open input file: %s\n", inFileName);

    eclipse = Astronomy_SearchGlobalSolarEclipse(Astronomy_MakeTime(1701, 1, 1, 0, 0, 0.0));

    lnum = 0;
    while (ReadLine(line, sizeof(line), infile, inFileName, lnum))
    {
        ++lnum;

        if (eclipse.status != ASTRO_SUCCESS)
            FAIL("C GlobalSolarEclipseTest(%s line %d): error %d searching for solar eclipse.\n", inFileName, lnum, eclipse.status);

        /* 1889-12-22T12:54:15Z   -6 T   -12.7   -12.8 */
        if (strlen(line) < 20)
            FAIL("C GlobalSolarEclipseTest(%s line %d): line is too short.\n", inFileName, lnum);

        line[20] = '\0';        /* terminate the date/time string */
        if (ParseDate(line, &peak))
            FAIL("C GlobalSolarEclipseTest(%s line %d): invalid date/time format: '%s'\n", inFileName, lnum, line);

        if (4 != sscanf(line+21, "%lf %c %lf %lf", &deltaT, &typeChar, &lat, &lon))
            FAIL("C GlobalSolarEclipseTest(%s line %d): invalid data format.\n", inFileName, lnum);

        /*
            The test data allows for "hybrid" eclipses, which means that some
            observers see a total eclipse, others see an annular eclipse.
            The global solar eclipse predictor determines the eclipse kind
            for the peak observer only. Therefore, hybrid in the test data
            matches a total eclipse from our predictor.
        */
        switch (typeChar)
        {
        case 'P':   expected_kind = ECLIPSE_PARTIAL;    break;
        case 'A':   expected_kind = ECLIPSE_ANNULAR;    break;
        case 'T':   expected_kind = ECLIPSE_TOTAL;      break;
        case 'H':   expected_kind = ECLIPSE_TOTAL;      break;
        default:
            FAIL("C GlobalSolarEclipseTest(%s line %d): invalid eclipse kind in test data.\n", inFileName, lnum);
        }

        diff_days = V(eclipse.peak.ut - peak.ut);

        /* Sometimes we find marginal eclipses that aren't listed in the test data. */
        /* Ignore them if the distance between the Sun/Moon shadow axis and the Earth's center is large. */
        while (diff_days < -25.0 && eclipse.distance > 9000.0)
        {
            ++skip_count;
            eclipse = Astronomy_NextGlobalSolarEclipse(eclipse.peak);
            CHECK_STATUS(eclipse);
            diff_days = V(eclipse.peak.ut - peak.ut);
        }

        /* Validate the eclipse prediction. */
        diff_minutes = (24 * 60) * ABS(diff_days);
        if (diff_minutes > 7.56)
        {
            printf("Expected: ");
            PrintTime(peak);
            printf("\nFound:    ");
            PrintTime(eclipse.peak);
            printf("\n");
            FAIL("C GlobalSolarEclipseTest(%s line %d): EXCESSIVE TIME ERROR = %0.2lf minutes\n", inFileName, lnum, diff_minutes);
        }

        if (diff_minutes > max_minutes)
            max_minutes = diff_minutes;

        /* Validate the eclipse kind, but only when it is not a "glancing" eclipse. */
        if ((V(eclipse.distance) < 6360) && (eclipse.kind != expected_kind))
            FAIL("C GlobalSolarEclipseTest(%s line %d): WRONG ECLIPSE KIND: expected %d, found %d\n", inFileName, lnum, expected_kind, eclipse.kind);

        if (eclipse.kind == ECLIPSE_TOTAL || eclipse.kind == ECLIPSE_ANNULAR)
        {
            /*
                When the distance between the Moon's shadow ray and the Earth's center is beyond 6100 km,
                it creates a glancing blow whose geographic coordinates are excessively sensitive to
                slight changes in the ray. Therefore, it is unreasonable to count large errors there.
            */
            if (eclipse.distance < 6100.0)
            {
                diff_angle = AngleDiff(lat, lon, eclipse.latitude, eclipse.longitude);
                if (diff_angle > 0.247)
                    FAIL("C GlobalSolarEclipseTest(%s line %d): EXCESSIVE GEOGRAPHIC LOCATION ERROR = %0.6lf degrees\n", inFileName, lnum, diff_angle);
                if (diff_angle > max_angle)
                    max_angle = diff_angle;
            }
        }

        /* Verify that the obscuration value is consistent with the eclipse kind. */
        switch (eclipse.kind)
        {
        case ECLIPSE_PARTIAL:
            if (!isnan(eclipse.obscuration))
                FAIL("C GlobalSolarEclipseTest(%s line %d): expected NAN obscuration for partial eclipse, but found %lf\n", inFileName, lnum, eclipse.obscuration);
            break;

        case ECLIPSE_ANNULAR:
            if (!isfinite(eclipse.obscuration))
                FAIL("C GlobalSolarEclipseTest(%s line %d): expected finite obscuration for annular eclipse.\n", inFileName, lnum);
            if (eclipse.obscuration < 0.8 || eclipse.obscuration >= 1.0)
                FAIL("C GlobalSolarEclipseTest(%s line %d): obscuration = %0.6lf is out of range for annular eclipse.\n", inFileName, lnum, eclipse.obscuration);
            break;

        case ECLIPSE_TOTAL:
            if (!isfinite(eclipse.obscuration))
                FAIL("C GlobalSolarEclipseTest(%s line %d): expected finite obscuration for total eclipse.\n", inFileName, lnum);
            if (eclipse.obscuration != 1.0)
                FAIL("C GlobalSolarEclipseTest(%s line %d): obscuration = %0.6lf should have been 1.0 for total eclipse.\n", inFileName, lnum, eclipse.obscuration);
            break;

        default:
            FAIL("C GlobalSolarEclipseTest(%s line %d): Unhandled eclipse kind %d for obscuration check.\n", inFileName, lnum, eclipse.kind);
        }

        eclipse = Astronomy_NextGlobalSolarEclipse(eclipse.peak);
    }

    if (lnum != expected_count)
        FFAIL("WRONG LINE COUNT = %d, expected %d\n", lnum, expected_count);

    if (skip_count > 2)
        FFAIL("EXCESSIVE SKIP COUNT = %d\n", skip_count);

    FPASSA("(%d verified, %d skipped, %d CalcMoons, max minutes = %0.3lf, max angle = %0.3lf)\n", lnum, skip_count, _CalcMoonCount, max_minutes, max_angle);
fail:
    if (infile != NULL) fclose(infile);
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static void VectorFromAngles(double v[3], double lat, double lon)
{
    double coslat = cos(DEG2RAD * lat);
    v[0] = cos(DEG2RAD * lon) * coslat;
    v[1] = sin(DEG2RAD * lon) * coslat;
    v[2] = sin(DEG2RAD * lat);
}


static double AngleDiff(double alat, double alon, double blat, double blon)
{
    double a[3];
    double b[3];
    double dot;

    /* Convert angles to vectors on a unit sphere. */
    VectorFromAngles(a, alat, alon);
    VectorFromAngles(b, blat, blon);

    dot = V(a[0]*b[0] + a[1]*b[1] + a[2]*b[2]);
    if (dot <= -1.0)
        return 180.0;

    if (dot >= +1.0)
        return 0.0;

    return V(RAD2DEG * acos(dot));
}

/*-----------------------------------------------------------------------------------------------------------*/

static int LocalSolarEclipseTest(void)
{
    int error;
    CHECK(LocalSolarEclipseTest1());
    CHECK(LocalSolarEclipseTest2());
fail:
    return error;
}

static int LocalSolarEclipseTest1(void)
{
    /*
        Re-use the test data for global solar eclipses, only feed the given coordinates
        into the local solar eclipse predictor as the observer's location.
        In each case, start the search 20 days before the expected eclipse.
        Then verify that the peak time and eclipse type is correct in each case.
    */

    int error = 1;
    FILE *infile = NULL;
    const char *inFileName = "eclipse/solar_eclipse.txt";
    int lnum;
    char line[100];
    char typeChar;
    double deltaT;
    astro_observer_t observer;
    astro_time_t peak, search_start;
    astro_local_solar_eclipse_t eclipse;
    double diff_days, diff_minutes, max_minutes=0.0;
    int skip_count = 0;
    extern int _CalcMoonCount;      /* incremented by Astronomy Engine every time expensive CalcMoon() is called */

    _CalcMoonCount = 0;

    infile = fopen(inFileName, "rt");
    if (infile == NULL)
        FFAIL("Cannot open input file: %s\n", inFileName);

    lnum = 0;
    observer.height = 0.0;
    while (ReadLine(line, sizeof(line), infile, inFileName, lnum))
    {
        ++lnum;

        /* 1889-12-22T12:54:15Z   -6 T   -12.7   -12.8 */
        if (strlen(line) < 20)
            FAIL("C LocalSolarEclipseTest1(%s line %d): line is too short.\n", inFileName, lnum);

        line[20] = '\0';        /* terminate the date/time string */
        if (ParseDate(line, &peak))
            FAIL("C LocalSolarEclipseTest1(%s line %d): invalid date/time format: '%s'\n", inFileName, lnum, line);

        if (4 != sscanf(line+21, "%lf %c %lf %lf", &deltaT, &typeChar, &observer.latitude, &observer.longitude))
            FAIL("C LocalSolarEclipseTest1(%s line %d): invalid data format.\n", inFileName, lnum);

        /* Start the search 20 days before we know the eclipse should peak. */
        search_start = Astronomy_AddDays(peak, -20.0);
        eclipse = Astronomy_SearchLocalSolarEclipse(search_start, observer);
        if (eclipse.status != ASTRO_SUCCESS)
            FAIL("C LocalSolarEclipseTest1(%s line %d): Astronomy_SearchLocalSolarEclipse returned %d\n", inFileName, lnum, eclipse.status);

        /* Validate the predicted eclipse peak time. */
        diff_days = eclipse.peak.time.ut - peak.ut;
        if (diff_days > 20.0)
        {
            ++skip_count;
            continue;
        }

        diff_minutes = (24 * 60) * ABS(diff_days);
        if (diff_minutes > 7.737)
        {
            printf("Expected: ");
            PrintTime(peak);
            printf("\nFound:    ");
            PrintTime(eclipse.peak.time);
            printf("  ut=%0.6lf, tt=%0.6lf\n", eclipse.peak.time.ut, eclipse.peak.time.tt);
            FAIL("C LocalSolarEclipseTest1(%s line %d): EXCESSIVE TIME ERROR = %0.6lf minutes\n", inFileName, lnum, diff_minutes);
        }

        if (diff_minutes > max_minutes)
            max_minutes = diff_minutes;

        /* Verify obscuration makes sense for this kind of eclipse. */

        if (!isfinite(eclipse.obscuration))
            FAIL("C LocalSolarEclipseTest1(%s line %d): obscuration is not finite\n", inFileName, lnum);

        switch (eclipse.kind)
        {
        case ECLIPSE_ANNULAR:
        case ECLIPSE_PARTIAL:
            if (eclipse.obscuration <= 0.0 || eclipse.obscuration >= 1.0)
                FAIL("C LocalSolarEclipseTest1(%s line %d): eclipse kind %d has invalid obscuration = %0.8lf\n", inFileName, lnum, eclipse.kind, eclipse.obscuration);
            break;

        case ECLIPSE_TOTAL:
            if (eclipse.obscuration != 1.0)
                FAIL("C LocalSolarEclipseTest1(%s line %d): total eclipse has invalid obscuration = %0.8lf\n", inFileName, lnum, eclipse.obscuration);
            break;

        default:
            FAIL("C LocalSolarEclipseTest1(%s line %d): Invalid eclipse kind %d\n", inFileName, lnum, eclipse.kind);
        }
    }

    if (skip_count > 6)
        FFAIL("EXCESSIVE SKIP COUNT %d\n", skip_count);

    FPASSA("(%d verified, %d skipped, %d CalcMoons, max minutes = %0.3lf)\n", lnum, skip_count, _CalcMoonCount, max_minutes);
    error = 0;
fail:
    if (infile != NULL) fclose(infile);
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int ParseEvent(const char *str, int required, astro_time_t *time)
{
    if (required)
        return ParseDate(str, time);

    if (strcmp(str, "-"))
    {
        fprintf(stderr, "ParseEvent: Expected '-' but found '%s'\n", str);
        return 1;
    }

    time->ut = time->tt = NAN;
    return 0;
}


static int IgnoreLine(char *line)
{
    int col, empty;

    /* Treat '#' as a comment character. */
    empty = 1;
    for (col=0; line[col] != '\0' && line[col] != '#'; ++col)
        if (!isspace(line[col]))
            empty = 0;

    line[col] = '\0';
    return empty;
}


static int CheckEvent(
    const char *inFileName,
    int lnum,
    const char *name,
    astro_time_t expected_time,
    double expected_altitude,
    astro_eclipse_event_t evt,
    double *max_minutes,
    double *max_degrees)
{
    int error = 1;
    double diff_minutes, diff_alt;

    diff_minutes = (24 * 60) * ABS(expected_time.ut - evt.time.ut);

    if (diff_minutes > *max_minutes)
        *max_minutes = diff_minutes;

    if (diff_minutes > 1.0)
        FAIL("CheckEvent(%s line %d): EXCESSIVE TIME ERROR: %0.3lf minutes\n", inFileName, lnum, diff_minutes);

    /* Ignore discrepancies for negative altitudes, because of quirky and irrelevant differences in refraction models. */
    if (expected_altitude >= 0.0)
    {
        diff_alt = ABS(expected_altitude - evt.altitude);
        if (diff_alt > *max_degrees) *max_degrees = diff_alt;
        if (diff_alt > 0.5)
            FAIL("CheckEvent(%s line %d): EXCESSIVE ALTITUDE ERROR: %0.6lf degrees (expected = %0.6lf, calc = %0.6lf)\n", inFileName, lnum, diff_alt, expected_altitude, evt.altitude);
    }

    error = 0;
fail:
    return error;
}


#define TIMESTRSIZE 31

static int LocalSolarEclipseTest2(void)
{
    /*
        Test ability to calculate local solar eclipse conditions away from
        the peak position on the Earth.
    */

    int error = 1;
    FILE *infile = NULL;
    const char *inFileName = "eclipse/local_solar_eclipse.txt";
    int lnum, nscanned;
    char line[300];
    char p1str[TIMESTRSIZE], p2str[TIMESTRSIZE], t1str[TIMESTRSIZE], t2str[TIMESTRSIZE], peakstr[TIMESTRSIZE];
    double p1alt, p2alt, t1alt, t2alt, peakalt;
    astro_time_t p1, p2, t1, t2, peak, search_time;
    char typeChar;
    astro_observer_t observer;
    double max_minutes=0.0, max_degrees=0.0;
    int verify_count = 0;
    astro_local_solar_eclipse_t eclipse;
    astro_eclipse_kind_t expected_kind;
    extern int _CalcMoonCount;      /* incremented by Astronomy Engine every time expensive CalcMoon() is called */

    _CalcMoonCount = 0;

    infile = fopen(inFileName, "rt");
    if (infile == NULL)
        FFAIL("Cannot open input file: %s\n", inFileName);

    lnum = 0;
    observer.height = 0.0;
    while (ReadLine(line, sizeof(line), infile, inFileName, lnum))
    {
        ++lnum;
        if (IgnoreLine(line)) continue;

        nscanned = sscanf(line, "%lf %lf %c %30s %lf %30s %lf %30s %lf %30s %lf %30s %lf",
            &observer.latitude,   &observer.longitude,
            &typeChar,
            p1str,      &p1alt,
            t1str,      &t1alt,
            peakstr,    &peakalt,
            t2str,      &t2alt,
            p2str,      &p2alt);

        if (nscanned != 13)
            FAIL("C LocalSolarEclipseTest2(%s line %d): Incorrect token count (scanned %d)\n", inFileName, lnum, nscanned);

        switch (typeChar)
        {
        case 'P': expected_kind = ECLIPSE_PARTIAL; break;
        case 'A': expected_kind = ECLIPSE_ANNULAR; break;
        case 'T': expected_kind = ECLIPSE_TOTAL;   break;
        default:
            FAIL("C LocalSolarEclipseTest2(%s line %d): invalid eclipse type '%c'\n", inFileName, lnum, typeChar);
        }

        CHECK(ParseEvent(p1str, 1, &p1));
        CHECK(ParseEvent(p2str, 1, &p2));
        CHECK(ParseEvent(peakstr, 1, &peak));
        CHECK(ParseEvent(t1str, (typeChar != 'P'), &t1));
        CHECK(ParseEvent(t2str, (typeChar != 'P'), &t2));

        search_time = Astronomy_AddDays(p1, -20.0);
        eclipse = Astronomy_SearchLocalSolarEclipse(search_time, observer);
        if (eclipse.status != ASTRO_SUCCESS)
            FAIL("C LocalSolarEclipseTest2(%s line %d): error %d searching for solar eclipse.\n", inFileName, lnum, eclipse.status);

        if (eclipse.kind != expected_kind)
            FAIL("C LocalSolarEclipseTest2(%s line %d): expected eclipse kind %d, found %d\n", inFileName, lnum, expected_kind, eclipse.kind);

        CHECK(CheckEvent(inFileName, lnum, "peak", peak, peakalt, eclipse.peak, &max_minutes, &max_degrees));
        CHECK(CheckEvent(inFileName, lnum, "partial_begin", p1, p1alt, eclipse.partial_begin, &max_minutes, &max_degrees));
        CHECK(CheckEvent(inFileName, lnum, "partial_end", p2, p2alt, eclipse.partial_end, &max_minutes, &max_degrees));
        if (typeChar != 'P')
        {
            CHECK(CheckEvent(inFileName, lnum, "total_begin", t1, t1alt, eclipse.total_begin, &max_minutes, &max_degrees));
            CHECK(CheckEvent(inFileName, lnum, "total_end", t2, t2alt, eclipse.total_end, &max_minutes, &max_degrees));
        }

        ++verify_count;
    }

    FPASSA("(%d verified, %d CalcMoons, max minutes = %0.3lf, max alt degrees = %0.3lf)\n", verify_count, _CalcMoonCount, max_minutes, max_degrees);
fail:
    if (infile != NULL) fclose(infile);
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int GlobalAnnularCase(int year, int month, int day, double obscuration)
{
    int error;
    astro_time_t time;
    astro_global_solar_eclipse_t eclipse;
    double dt, diff;

    /* Search for the first solar eclipse that occurs after the given date. */
    time = Astronomy_MakeTime(year, month, day, 0, 0, 0.0);
    eclipse = Astronomy_SearchGlobalSolarEclipse(time);
    CHECK_STATUS(eclipse);

    /* Verify the eclipse is within 1 day after the search basis time. */
    dt = V(eclipse.peak.ut - time.ut);
    if (dt < 0.0 || dt > 1.0)
        FAIL("C GlobalAnnularCase(%04d-%02d-%02d) FAIL: found eclipse %0.8lf days after search time.\n", year, month, day, dt);

    /* Verify we found an annular solar eclipse. */
    if (eclipse.kind != ECLIPSE_ANNULAR)
        FAIL("C GlobalAnnularCase(%04d-%02d-%02d) FAIL: expected annular eclipse, found %d.\n", year, month, day, eclipse.kind);

    /* Check how accurately we calculated obscuration. */
    diff = V(eclipse.obscuration - obscuration);
    if (ABS(diff) > 0.0000904)
        FAIL("C GlobalAnnularCase(%04d-%02d-%02d) FAIL: excessive obscuration error = %0.8lf. expected = %0.8lf, calculated = %0.8lf\n", year, month, day, diff, obscuration, eclipse.obscuration);
    DEBUG("C GlobalAnnularCase(%04d-%02d-%02d) obscuration error = %0.8lf, expected = %0.8lf, calculated = %0.8lf\n", year, month, day, diff, obscuration, eclipse.obscuration);

fail:
    return error;
}


static int LocalSolarCase(
    int year,
    int month,
    int day,
    double latitude,
    double longitude,
    astro_eclipse_kind_t kind,
    double obscuration,
    double tolerance)
{
    int error;
    astro_time_t time;
    astro_observer_t observer;
    astro_local_solar_eclipse_t eclipse;
    double dt, diff;

    observer = Astronomy_MakeObserver(latitude, longitude, 0.0);
    time = Astronomy_MakeTime(year, month, day, 0, 0, 0.0);
    eclipse = Astronomy_SearchLocalSolarEclipse(time, observer);
    CHECK_STATUS(eclipse);

    dt = V(eclipse.peak.time.ut - time.ut);
    if (dt < 0.0 || dt > 1.0)
        FAIL("C LocalSolarCase(%04d-%02d-%02d) FAIL: elapsed time = %0.6lf days\n", year, month, day, dt);

    if (eclipse.kind != kind)
        FAIL("C LocalSolarCase(%04d-%02d-%02d) FAIL: expected kind %d, found %d\n", year, month, day, kind, eclipse.kind);

    diff = V(eclipse.obscuration - obscuration);
    if (ABS(diff) > tolerance)
        FAIL("C LocalSolarCase(%04d-%02d-%02d) FAIL: obscuration diff = %11.8lf, expected = %11.8lf, calculated = %11.8lf\n", year, month, day, diff, obscuration, eclipse.obscuration);
    DEBUG("C LocalSolarCase(%04d-%02d-%02d) obscuration diff = %11.8lf, expected = %11.8lf, calculated = %11.8lf\n", year, month, day, diff, obscuration, eclipse.obscuration);

fail:
    return error;
}


static int SolarFractionTest(void)
{
    int error;

    /* Verify global solar eclipse obscurations for annular eclipses only. */
    /* This is because they are the only nontrivial values for global solar eclipses. */
    /* The trivial values are all validated exactly by GlobalSolarEclipseTest(). */

    CHECK(GlobalAnnularCase(2023, 10, 14, 0.90638));    /* https://www.eclipsewise.com/solar/SEprime/2001-2100/SE2023Oct14Aprime.html */
    CHECK(GlobalAnnularCase(2024, 10,  2, 0.86975));    /* https://www.eclipsewise.com/solar/SEprime/2001-2100/SE2024Oct02Aprime.html */
    CHECK(GlobalAnnularCase(2027,  2,  6, 0.86139));    /* https://www.eclipsewise.com/solar/SEprime/2001-2100/SE2027Feb06Aprime.html */
    CHECK(GlobalAnnularCase(2028,  1, 26, 0.84787));    /* https://www.eclipsewise.com/solar/SEprime/2001-2100/SE2028Jan26Aprime.html */
    CHECK(GlobalAnnularCase(2030,  6,  1, 0.89163));    /* https://www.eclipsewise.com/solar/SEprime/2001-2100/SE2030Jun01Aprime.html */

    /* Verify obscuration values for specific locations on the Earth. */
    /* Local solar eclipse calculations include obscuration for all types of eclipse, not just annular and total. */
    CHECK(LocalSolarCase(2023, 10, 14,  11.3683,  -83.1017, ECLIPSE_ANNULAR, 0.90638, 0.000080));   /* https://www.eclipsewise.com/solar/SEprime/2001-2100/SE2023Oct14Aprime.html */
    CHECK(LocalSolarCase(2023, 10, 14,  25.78,    -80.22,   ECLIPSE_PARTIAL, 0.578,   0.000023));   /* https://aa.usno.navy.mil/calculated/eclipse/solar?eclipse=22023&lat=25.78&lon=-80.22&label=Miami%2C+FL&height=0&submit=Get+Data */
    CHECK(LocalSolarCase(2023, 10, 14,  30.2666,  -97.7000, ECLIPSE_PARTIAL, 0.8867,  0.001016));   /* http://astro.ukho.gov.uk/eclipse/0332023/Austin_TX_United_States_2023Oct14.png */
    CHECK(LocalSolarCase(2024,  4,  8,  25.2900, -104.1383, ECLIPSE_TOTAL,   1.0,     0.0));        /* https://www.eclipsewise.com/solar/SEprime/2001-2100/SE2024Apr08Tprime.html */
    CHECK(LocalSolarCase(2024,  4,  8,  37.76,   -122.44,   ECLIPSE_PARTIAL, 0.340,   0.000604));   /* https://aa.usno.navy.mil/calculated/eclipse/solar?eclipse=12024&lat=37.76&lon=-122.44&label=San+Francisco%2C+CA&height=0&submit=Get+Data */
    CHECK(LocalSolarCase(2024, 10,  2, -21.9533, -114.5083, ECLIPSE_ANNULAR, 0.86975, 0.000061));   /* https://www.eclipsewise.com/solar/SEprime/2001-2100/SE2024Oct02Aprime.html */
    CHECK(LocalSolarCase(2024, 10,  2, -33.468,   -70.636,  ECLIPSE_PARTIAL, 0.436,   0.000980));   /* https://aa.usno.navy.mil/calculated/eclipse/solar?eclipse=22024&lat=-33.468&lon=-70.636&label=Santiago%2C+Chile&height=0&submit=Get+Data */
    CHECK(LocalSolarCase(2030,  6,  1,  56.525,    80.0617, ECLIPSE_ANNULAR, 0.89163, 0.000067));   /* https://www.eclipsewise.com/solar/SEprime/2001-2100/SE2030Jun01Aprime.html */
    CHECK(LocalSolarCase(2030,  6,  1,  40.388,    49.914,  ECLIPSE_PARTIAL, 0.67240, 0.000599));   /* http://xjubier.free.fr/en/site_pages/SolarEclipseCalc_Diagram.html */
    CHECK(LocalSolarCase(2030,  6,  1,  40.3667,   49.8333, ECLIPSE_PARTIAL, 0.6736,  0.001464));   /* http://astro.ukho.gov.uk/eclipse/0132030/Baku_Azerbaijan_2030Jun01.png */

    FPASS();
fail:
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int PlotDeltaT(const char *outFileName)
{
    int error = 1;
    FILE *outfile = NULL;
    int year;
    double dt;
    astro_time_t time;

    outfile = fopen(outFileName, "wt");
    if (outfile == NULL)
        FAIL("ctest PlotDeltaT: Cannot open output file: %s\n", outFileName);

    fprintf(outfile, "\"year\",\"delta_t\"\n");
    for (year = 1500; year <= 2500; ++year)
    {
        time = Astronomy_MakeTime(year, 1, 1, 0, 0, 0.0);
        dt = (24.0 * 3600.0) * (time.tt - time.ut);
        fprintf(outfile, "%d,%lf\n", year, dt);
    }

    printf("ctest PlotDeltaT: Wrote file %s\n", outFileName);
    error = 0;
fail:
    if (outfile != NULL) fclose(outfile);
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int TransitFile(astro_body_t body, const char *filename, double limit_minutes, double limit_sep)
{
    int error = 1;
    FILE *infile = NULL;
    char line[100];
    int lnum;
    astro_time_t time1, time2, timep;
    astro_transit_t transit;
    double separation;
    double diff_start, diff_peak, diff_finish, diff_sep;
    double max_minutes = 0.0, max_sep = 0.0;
    const int START_YEAR = 1600;

    infile = fopen(filename, "rt");
    if (infile == NULL)
        FFAIL("Cannot open input file: %s\n", filename);

    transit = Astronomy_SearchTransit(body, Astronomy_MakeTime(START_YEAR, 1, 1, 0, 0, 0.0));
    if (transit.status != ASTRO_SUCCESS)
        FAIL("C TransitFile(%s): SearchTransit returned %d\n", filename, transit.status);

    DEBUG("\n----------------------------------------------------------------\n\n");
    FDEBUG("STARTING %s\n\n", filename);

    lnum = 0;
    while (ReadLine(line, sizeof(line), infile, filename, lnum))
    {
        ++lnum;

        /* 22:17 1881-11-08T00:57Z 03:38  3.8633 */

        if (strlen(line) < 37)
            FAIL("C TransitFile(%s line %d): line is too short.\n", filename, lnum);

        if (line[5] != ' ' || line[23] != ' ' || line[29] != ' ')
            FAIL("C TransitFile(%s line %d): invalid data format.\n", filename, lnum);

        if (1 != sscanf(line+30, "%lf", &separation))
            FAIL("C TransitFile(%s line %d): unable to parse angular separation.\n", filename, lnum);

        line[23] = '\0';
        if (ParseDate(line+6, &timep))
            FAIL("C TransitFile(%s line %d): invalid date/time '%s'\n", filename, lnum, line+6);

        /* Patch the start time and reparse. */
        line[17] = line[0];
        line[18] = line[1];
        line[20] = line[3];
        line[21] = line[4];
        if (ParseDate(line+6, &time1))
            FAIL("C TransitFile(%s line %d): failed to parse patched start time '%s'\n", filename, lnum, line+6);

        /* If the start time is after the peak time, it really starts on the previous day. */
        if (time1.ut > timep.ut)
            time1 = Astronomy_AddDays(time1, -1.0);

        /* Patch the finish time and reparse. */
        line[17] = line[24];
        line[18] = line[25];
        line[20] = line[27];
        line[21] = line[28];
        if (ParseDate(line+6, &time2))
            FAIL("C TransitFile(%s line %d): failed to parse patched finish time '%s'\n", filename, lnum, line+6);

        /* If the finish time is before the peak time, it really starts on the next day. */
        if (time2.ut < timep.ut)
            time2 = Astronomy_AddDays(time2, +1.0);

        /* Measure result errors. */
        diff_start  = (24.0 * 60.0) * (time1.ut - transit.start.ut );
        diff_peak   = (24.0 * 60.0) * (timep.ut - transit.peak.ut  );
        diff_finish = (24.0 * 60.0) * (time2.ut - transit.finish.ut);
        diff_sep    = separation - transit.separation;

        if (Verbose)
        {
            printf("Expected: ");
            PrintTime(time1);
            printf("    ");
            PrintTime(timep);
            printf("    ");
            PrintTime(time2);
            printf(" %10.4lf\n", separation);

            printf("Found:    ");
            PrintTime(transit.start);
            printf("    ");
            PrintTime(transit.peak);
            printf("    ");
            PrintTime(transit.finish);
            printf(" %10.4lf\n", transit.separation);

            printf("Diff: %24.3lf    %24.3lf    %24.3lf     %10.4lf\n\n", diff_start, diff_peak, diff_finish, diff_sep);
        }

        diff_start  = ABS(diff_start);
        diff_peak   = ABS(diff_peak);
        diff_finish = ABS(diff_finish);
        if (diff_start  > max_minutes)  max_minutes = diff_start;
        if (diff_peak   > max_minutes)  max_minutes = diff_peak;
        if (diff_finish > max_minutes)  max_minutes = diff_finish;
        if (max_minutes > limit_minutes)
            FAIL("C TransitFile(%s line %d): EXCESSIVE TIME ERROR = %0.3lf minutes. (Run again with -v to debug.)\n", filename, lnum, max_minutes);

        diff_sep = ABS(diff_sep);
        if (diff_sep > max_sep)  max_sep = diff_sep;
        if (max_sep > limit_sep)
            FAIL("C TransitFile(%s line %d): EXCESSIVE SEPARATION ERROR = %0.4lf arcminutes. (Run again with -v to debug.)\n", filename, lnum, max_sep);

        /* Search for the next transit. */
        transit = Astronomy_NextTransit(body, transit.finish);
        if (transit.status != ASTRO_SUCCESS)
            FAIL("C TransitFile(%s line %d): NextTransit returned %d\n", filename, lnum, transit.status);
    }

    printf("C TransitFile(%-20s): PASS - verified %2d, max minutes = %6.3lf, max sep arcmin = %6.4lf\n", filename, lnum, max_minutes, max_sep);
    error = 0;

fail:
    if (infile != NULL) fclose(infile);
    return error;
}

static int Transit(void)
{
    int error;

    CHECK(TransitFile(BODY_MERCURY, "eclipse/mercury.txt", 10.710, 0.2121));
    CHECK(TransitFile(BODY_VENUS,   "eclipse/venus.txt",    9.109, 0.6772));
fail:
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int DistancePlot(astro_body_t body, double ut1, double ut2, const char *filename)
{
    const int npoints = 100000;
    int error = 1;
    int i;
    double ut;
    astro_time_t time, j2000;
    astro_func_result_t dist;
    FILE *outfile = NULL;

    outfile = fopen(filename, "wt");
    if (outfile == NULL)
        FFAIL("Cannot open output file: %s\n", filename);

    j2000 = Astronomy_MakeTime(2000, 1, 1, 12, 0, 0.0);

    fprintf(outfile, "\"tt\",\"distance\"\n");
    for (i=0; i < npoints; ++i)
    {
        ut = ut1 + (((double)i)/((double)(npoints-1)) * (ut2 - ut1));
        time = Astronomy_AddDays(j2000, ut);
        dist = Astronomy_HelioDistance(body, time);
        CHECK_STATUS(dist);
        fprintf(outfile, "%0.16lf,%0.16lg\n", time.tt, dist.value);
    }

    error = 0;
fail:
    if (outfile != NULL) fclose(outfile);
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int PlutoCheckDate(double ut, double arcmin_tolerance, double x, double y, double z)
{
    int error = 1;
    astro_time_t time;
    astro_vector_t vector;
    double dx, dy, dz, diff, dist, arcmin;

    time = Astronomy_TimeFromDays(ut);

    DEBUG("C %s: ", __func__);
    if (Verbose) PrintTime(time);
    DEBUG(" = %0.6lf UT = %0.6lf TT\n", time.ut, time.tt);

    vector = Astronomy_HelioVector(BODY_PLUTO, time);
    if (vector.status != ASTRO_SUCCESS)
        FFAIL("FAIL - Astronomy_HelioVector returned status = %d\n", vector.status);

    dx = V(vector.x) - x;
    dy = V(vector.y) - y;
    dz = V(vector.z) - z;
    diff = sqrt(dx*dx + dy*dy + dz*dz);
    dist = (sqrt(x*x + y*y + z*z) - 1.0);       /* worst-case distance between Pluto and Earth */
    arcmin = (diff / dist) * (180.0 * 60.0 / PI);
    FDEBUG("calc pos = [%20.16lf, %20.16lf, %20.16lf]\n", vector.x, vector.y, vector.z);
    FDEBUG("ref  pos = [%20.16lf, %20.16lf, %20.16lf]\n", x, y, z);
    FDEBUG("del  pos = [%20.16lf, %20.16lf, %20.16lf]\n", vector.x - x, vector.y - y, vector.z - z);
    FDEBUG("diff = %le AU, %0.3lf arcmin\n", diff, arcmin);
    DEBUG("\n");

    if (V(arcmin) > arcmin_tolerance)
        FFAIL("EXCESSIVE ERROR\n");

    error = 0;
fail:
    return error;
}


static int PlutoCheck(void)
{
    int error;
    CHECK(PlutoCheckDate(  +18250.0, 0.089, +37.4377303523676090, -10.2466292454075898, -14.4773101310875809));
    CHECK(PlutoCheckDate( -856493.0, 4.067, +23.4292113199166252, +42.1452685817740829,  +6.0580908436642940));
    CHECK(PlutoCheckDate( +435633.0, 0.016, -27.3178902095231813, +18.5887022581070305, +14.0493896259306936));
    CHECK(PlutoCheckDate(       0.0, 8.e-9,  -9.8753673425269000, -27.9789270580402771,  -5.7537127596369588));
    CHECK(PlutoCheckDate( +800916.0, 2.286, -29.5266052645301365, +12.0554287322176474, +12.6878484911631091));

    FPASS();
fail:
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int GeoidTestCase(FILE *outfile, astro_time_t time, astro_observer_t observer, astro_equator_date_t equdate)
{
    int error;
    astro_state_vector_t state;
    astro_vector_t surface;
    astro_vector_t geo_moon;
    astro_equatorial_t topo_moon;
    double dx, dy, dz, diff, lat_diff, lon_diff, h_diff;
    astro_observer_t vobs;

    topo_moon = Astronomy_Equator(BODY_MOON, &time, observer, equdate, NO_ABERRATION);
    CHECK_STATUS(topo_moon);

    surface = Astronomy_ObserverVector(&time, observer, equdate);
    CHECK_STATUS(surface);

    state = Astronomy_ObserverState(&time, observer, equdate);
    CHECK_STATUS(state);

    /* Verify state vector's position exactly matches the lone position vector. */
    if (state.x != surface.x) FFAIL("state vector x = %0.16le, but position vector x = %0.16le\n", state.x, surface.x);
    if (state.y != surface.y) FFAIL("state vector x = %0.16le, but position vector x = %0.16le\n", state.y, surface.y);
    if (state.z != surface.z) FFAIL("state vector x = %0.16le, but position vector x = %0.16le\n", state.z, surface.z);

    if (outfile != NULL)
    {
        const double speedConvert = KM_PER_AU / 24.0;   /* converts AU/day into km/hour */

        fprintf(
            outfile,
            "%d %22.12lf %6.1lf %7.1lf %7.1lf %12.6lf %12.6lf %12.6lf %12.6lf %12.6lf %12.6lf\n",
            (int)equdate,
            time.ut,
            observer.latitude,
            observer.longitude,
            observer.height,
            KM_PER_AU * state.x,
            KM_PER_AU * state.y,
            KM_PER_AU * state.z,
            speedConvert * state.vx,
            speedConvert * state.vy,
            speedConvert * state.vz
        );
    }

    geo_moon = Astronomy_GeoMoon(time);
    CHECK_STATUS(geo_moon);

    if (equdate == EQUATOR_OF_DATE)
    {
        /* Astronomy_GeoMoon() returns J2000 coordinates. Convert to equator-of-date coordinates. */
        astro_rotation_t rot = Astronomy_Rotation_EQJ_EQD(&time);
        CHECK_STATUS(rot);
        geo_moon = Astronomy_RotateVector(rot, geo_moon);
    }

    dx = KM_PER_AU * V((geo_moon.x - surface.x) - topo_moon.vec.x);
    dy = KM_PER_AU * V((geo_moon.y - surface.y) - topo_moon.vec.y);
    dz = KM_PER_AU * V((geo_moon.z - surface.z) - topo_moon.vec.z);
    diff = sqrt(dx*dx + dy*dy + dz*dz);
    FDEBUG("equ=%d, tt=%14.5lf, lat=%5.1lf, lon=%6.1lf, ht=%6.1lf, surface=(%12.6lf, %12.6lf, %12.6lf), diff = %9.6lf km\n",
        (int)equdate,
        time.tt,
        observer.latitude,
        observer.longitude,
        observer.height,
        KM_PER_AU * surface.x,
        KM_PER_AU * surface.y,
        KM_PER_AU * surface.z,
        diff);

    /* Require 1 millimeter accuracy! (one millionth of a kilometer). */
    if (diff > 1.0e-6)
        FFAIL("EXCESSIVE POSITION ERROR.\n");

    /* Verify that we can convert the surface vector back to an observer. */
    vobs = Astronomy_VectorObserver(&surface, equdate);
    lat_diff = ABS(vobs.latitude - observer.latitude);

    /* Longitude is meaningless at the poles, so don't bother checking it there. */
    if (-89.99 <= observer.latitude && observer.latitude <= +89.99)
    {
        lon_diff = ABS(vobs.longitude - observer.longitude);
        if (lon_diff > 180.0)
            lon_diff = 360.0 - lon_diff;
        lon_diff = ABS(lon_diff * cos(DEG2RAD * observer.latitude));
        if (lon_diff > 1.0e-6)
            FFAIL("EXCESSIVE longitude check error = %lf\n", lon_diff);
    }
    else
    {
        lon_diff = 0.0;
    }

    h_diff = ABS(vobs.height - observer.height);
    FDEBUG("vobs=(lat=%lf, lon=%lf, height=%lf), lat_diff=%lg, lon_diff=%lg, h_diff=%lg\n",
        vobs.latitude, vobs.longitude, vobs.height, lat_diff, lon_diff, h_diff);

    if (lat_diff > 1.0e-6)
        FFAIL("EXCESSIVE latitude check error = %lf\n", lat_diff);

    if (h_diff > 0.001)
        FFAIL("EXCESSIVE height check error = %lf\n", h_diff);

    error = 0;
fail:
    return error;
}


static int GeoidTest(void)
{
    int error;
    int tindex, oindex;
    astro_time_t time;
    astro_vector_t vec;
    astro_observer_t observer;
    int lat, lon;
    FILE *outfile = NULL;
    const char *outFileName = "temp/c_geoid.txt";

    const astro_time_t time_list[] =
    {
        Astronomy_MakeTime(1066,  9, 27, 18,  0,  0.0),
        Astronomy_MakeTime(1970, 12, 13, 15, 42,  0.0),
        Astronomy_MakeTime(1970, 12, 13, 15, 43,  0.0),
        Astronomy_MakeTime(2015,  3,  5,  2, 15, 45.0)
    };
    const int ntimes = sizeof(time_list) / sizeof(time_list[0]);

    const astro_observer_t observer_list[] =
    {
        Astronomy_MakeObserver( +1.5,   +2.7,    7.4),
        Astronomy_MakeObserver(-53.7, +141.7, +100.0),
        Astronomy_MakeObserver(+30.0,  -85.2,  -50.0),
        Astronomy_MakeObserver(+90.0,  +45.0,  -50.0),
        Astronomy_MakeObserver(-90.0, -180.0,    0.0)
    };
    const int nobs = sizeof(observer_list) / sizeof(observer_list[0]);

    /* Make sure Astronomy_ObserverVector() checks for invalid equdate parameter. */
    time = time_list[0];
    vec = Astronomy_ObserverVector(&time, observer_list[0], (astro_equator_date_t)42);
    if (vec.status != ASTRO_INVALID_PARAMETER)
        FFAIL("Expected ASTRO_INVALID_PARAMETER (%d) but found %d\n", (int)ASTRO_INVALID_PARAMETER, (int)vec.status);
    FDEBUG("Astronomy_ObserverVector correctly detected invalid astro_equator_date_t parameter.\n");

    /* Test a variety of times and locations, in both supported orientation systems. */
    /* Also write reference data for comparing with other language implementations. */
    outfile = fopen(outFileName, "wt");
    if (outfile == NULL)
        FFAIL("FAIL - cannot open output file: %s\n", outFileName);

    for (oindex = 0; oindex < nobs; ++oindex)
    {
        for (tindex = 0; tindex < ntimes; ++tindex)
        {
            CHECK(GeoidTestCase(outfile, time_list[tindex], observer_list[oindex], EQUATOR_J2000));
            CHECK(GeoidTestCase(outfile, time_list[tindex], observer_list[oindex], EQUATOR_OF_DATE));
        }
    }

    /* More exhaustive tests for a single time value across many different geographic coordinates. */
    time = Astronomy_MakeTime(2021, 6, 20, 15, 8, 0.0);
    for (lat = -90; lat <= +90; lat += 1)
    {
        for (lon = -175; lon <= +180; lon += 5)
        {
            observer = Astronomy_MakeObserver(lat, lon, 0.0);
            CHECK(GeoidTestCase(NULL, time, observer, EQUATOR_OF_DATE));
        }
    }

    FPASS();
fail:
    if (outfile) fclose(outfile);
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static void TrimWhiteSpace(char *line)
{
    int len = (int)strlen(line);
    while (len > 0 && isspace(line[len-1]))
        line[--len] = '\0';
}


static int StringStartsWith(const char *text, const char *prefix)
{
    int i;

    for (i = 0; prefix[i] != '\0'; ++i)
        if (text[i] != prefix[i])
            return 0;

    return 1;
}


static astro_state_vector_t SelectJupiterMoon(const astro_jupiter_moons_t *jm, int mindex)
{
    switch (mindex)
    {
        case 0: return jm->io;
        case 1: return jm->europa;
        case 2: return jm->ganymede;
        case 3: return jm->callisto;
        default:
            fprintf(stderr, "C FATAL(%s): Invalid mindex = %d\n", __func__, mindex);
            exit(1);
    }
}


static int JupiterMoons_CheckJpl(int mindex, double tt, double pos[3], double vel[3])
{
    int error;
    astro_jupiter_moons_t jm;
    astro_state_vector_t moon;
    astro_time_t time;
    double dx, dy, dz, mag, diff;
    const double pos_tolerance = 9.0e-4;
    const double vel_tolerance = 9.0e-4;

    time = Astronomy_TerrestrialTime(tt);

    jm = Astronomy_JupiterMoons(time);
    CHECK_STATUS(jm.io);
    CHECK_STATUS(jm.europa);
    CHECK_STATUS(jm.ganymede);
    CHECK_STATUS(jm.callisto);
    moon = SelectJupiterMoon(&jm, mindex);

    dx = V(pos[0] - moon.x);
    dy = V(pos[1] - moon.y);
    dz = V(pos[2] - moon.z);
    mag = V(sqrt(pos[0]*pos[0] + pos[1]*pos[1] + pos[2]*pos[2]));
    diff = V(sqrt(dx*dx + dy*dy + dz*dz) / mag);
    if (diff > pos_tolerance)
        FFAIL("(mindex=%d, tt=%0.1lf): excessive position error %le\n", mindex, tt, diff);

    dx = V(vel[0] - moon.vx);
    dy = V(vel[1] - moon.vy);
    dz = V(vel[2] - moon.vz);
    mag = V(sqrt(vel[0]*vel[0] + vel[1]*vel[1] + vel[2]*vel[2]));
    diff = V(sqrt(dx*dx + dy*dy + dz*dz) / mag);
    if (diff > vel_tolerance)
        FFAIL("(mindex=%d, tt=%0.1lf): excessive velocity error %le\n", mindex, tt, diff);

    error = 0;
fail:
    return error;
}


static int JupiterMoonsTest(void)
{
    int error, mindex, check_mindex, lnum, found, part;
    double tt = NAN, pos[3] = {NAN, NAN, NAN}, vel[3] = {NAN, NAN, NAN};
    FILE *infile = NULL;
    char filename[100];
    char line[100];
    const int expected_count = 5001;
    int count;

    for (mindex = 0; mindex < 4; ++mindex)
    {
        snprintf(filename, sizeof(filename), "jupiter_moons/horizons/jm%d.txt", mindex);
        infile = fopen(filename, "rt");
        if (infile == NULL)
            FFAIL("Cannot open input file: %s\n", filename);

        FDEBUG("Comparing against %s\n", filename);

        lnum = 0;
        found = 0;
        part = -1;
        count = 0;
        while (ReadLine(line, sizeof(line), infile, filename, lnum))
        {
            ++lnum;
            TrimWhiteSpace(line);
            if (!found)
            {
                if (!strcmp(line, "$$SOE"))
                {
                    found = 1;
                    part = 0;
                }
                else if (StringStartsWith(line, "Revised:"))
                {
                    if (strlen(line) != 79)
                        FAIL("C JupiterMoonsTest(%s line %d): unexpected line length.\n", filename, lnum);
                    check_mindex = atoi(&line[76]) - 501;
                    if (mindex != check_mindex)
                        FAIL("C JupiterMoonsTest(%s line %d): moon index does not match: check=%d, mindex=%d.\n", filename, lnum, check_mindex, mindex);
                }
            }
            else if (!strcmp(line, "$$EOE"))
            {
                break;
            }
            else
            {
                switch (part)
                {
                    case 0:
                        /* 2446545.000000000 = A.D. 1986-Apr-24 12:00:00.0000 TDB */
                        if (1 != sscanf(line, "%lf", &tt))
                            FAIL("C JupiterMoonsTest(%s line %d): failed to read time stamp.\n", filename, lnum);
                        tt -= JD_2000;    /* convert JD to J2000 TT */
                        break;

                    case 1:
                        /* X = 1.134408131605554E-03 Y =-2.590904586750408E-03 Z =-7.490427225904720E-05 */
                        if (3 != sscanf(line, " X =%lf Y =%lf Z =%lf", &pos[0], &pos[1], &pos[2]))
                            FAIL("C JupiterMoonsTest(%s line %d): cannot parse position vector.\n", filename, lnum);
                        break;

                    case 2:
                        /* VX= 9.148038778472862E-03 VY= 3.973823407182510E-03 VZ= 2.765660368640458E-04 */
                        if (3 != sscanf(line, " VX=%lf VY=%lf VZ=%lf", &vel[0], &vel[1], &vel[2]))
                            FAIL("C JupiterMoonsTest(%s line %d): cannot parse velocity vector.\n", filename, lnum);
                        if (JupiterMoons_CheckJpl(mindex, tt, pos, vel))
                            FAIL("C JupiterMoonsTest(%s line %d): FAILED VERIFICATION.\n", filename, lnum);
                        ++count;
                        break;

                    default:
                        FAIL("C JupiterMoonsTest(%s line %d): unexpected part = %d.\n", filename, lnum, part);
                }
                part = (part + 1) % 3;
            }
        }

        fclose(infile);
        infile = NULL;

        if (count != expected_count)
            FAIL("C JupiterMoonsTest(%s): expected %d test cases, found %d\n", filename, expected_count, count);
    }

    FPASS();
fail:
    if (infile != NULL) fclose(infile);
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int Issue103(void)
{
    /* https://github.com/cosinekitty/astronomy/issues/103 */

    astro_observer_t observer = Astronomy_MakeObserver(29.0, -81.0, 10.0);
    double ut = -8.817548982869034808e+04;
    astro_time_t time = Astronomy_TimeFromDays(ut);
    astro_body_t body = BODY_VENUS;
    astro_equatorial_t ofdate = Astronomy_Equator(body, &time, observer, EQUATOR_OF_DATE, ABERRATION);
    astro_horizon_t hor;

    printf("tt  = %23.16lf\n", time.tt);
    printf("ra  = %23.16lf\n", ofdate.ra);
    printf("dec = %23.16lf\n", ofdate.dec);
    hor = Astronomy_Horizon(&time, observer, ofdate.ra, ofdate.dec, REFRACTION_NONE);
    printf("az  = %23.16lf\n", hor.azimuth);
    printf("alt = %23.16lf\n", hor.altitude);

    return 0;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int AberrationTest(void)
{
    int error, lnum, nscanned;
    int found_begin = 0;
    int found_end = 0;
    int count = 0;
    FILE *infile = NULL;
    const char *filename = "equatorial/Mars_j2000_ofdate_aberration.txt";
    char line[100];
    double jd, jra, jdec, dra, ddec, xra, xdec;
    astro_time_t time;
    astro_rotation_t rot;
    astro_spherical_t eqj_sphere, eqd_sphere;
    astro_vector_t eqj_vec, eqd_vec;
    double factor, diff_seconds, max_diff_seconds = 0.0;
    astro_state_vector_t eqj_earth;
    const double THRESHOLD_SECONDS = 0.453;

    infile = fopen(filename, "rt");
    if (infile == NULL)
        FFAIL("Cannot open input file: %s\n", filename);

    lnum = 0;
    while (!found_end && ReadLine(line, sizeof(line), infile, filename, lnum))
    {
        ++lnum;
        if (!found_begin)
        {
            if (strlen(line) >= 5 && !memcmp(line, "$$SOE", 5))
                found_begin = 1;
        }
        else if (strlen(line) >= 5 && !memcmp(line, "$$EOE", 5))
        {
            found_end = 1;
        }
        else
        {
            /* 2459371.500000000 *   118.566080210  22.210647456 118.874086738  22.155784122 */
            nscanned = sscanf(line+22, "%lf %lf %lf %lf", &jra, &jdec, &dra, &ddec);
            if (nscanned != 4)
                FAIL("C AberrationTest(%s line %d): invalid coordinates\n", filename, lnum);

            nscanned = sscanf(line, "%lf", &jd);
            if (nscanned != 1)
                FAIL("C AberrationTest(%s line %d): invalid julian date\n", filename, lnum);

            /* Convert julian day value to astro_time_t. */
            time = Astronomy_TimeFromDays(jd - JD_2000);

            /* Convert EQJ angular coordinates (jra, jdec) to an EQJ vector. */
            eqj_sphere.status = ASTRO_SUCCESS;
            eqj_sphere.lat = jdec;
            eqj_sphere.lon = jra;       /* JPL Horizons RA is already in degrees, not hours. */
            eqj_sphere.dist = C_AUDAY;  /* scale to the speed of light, to prepare for aberration correction. */
            eqj_vec = Astronomy_VectorFromSphere(eqj_sphere, time);
            CHECK_STATUS(eqj_vec);

            /* Aberration correction: calculate the Earth's barycentric velocity vector in EQJ coordinates. */
            eqj_earth = Astronomy_BaryState(BODY_EARTH, time);
            CHECK_STATUS(eqj_earth);

            /* Use non-relativistic approximation: add light vector to Earth velocity vector. */
            /* This gives aberration-corrected apparent position of the star in EQJ. */
            eqj_vec.x += eqj_earth.vx;
            eqj_vec.y += eqj_earth.vy;
            eqj_vec.z += eqj_earth.vz;

            /* Calculate the rotation matrix that converts J2000 coordinates to of-date coordinates. */
            rot = Astronomy_Rotation_EQJ_EQD(&time);
            CHECK_STATUS(rot);

            /* Use the rotation matrix to re-orient the EQJ vector to a EQD vector. */
            eqd_vec = Astronomy_RotateVector(rot, eqj_vec);
            CHECK_STATUS(eqd_vec);

            /* Convert the EQD vector back to spherical angular coordinates. */
            eqd_sphere = Astronomy_SphereFromVector(eqd_vec);
            CHECK_STATUS(eqd_sphere);

            /* Calculate the differences in RA and DEC between expected and calculated values. */
            factor = cos(V(eqd_sphere.lat * DEG2RAD));  /* RA errors are less important toward the poles */
            xra = factor * ABS(eqd_sphere.lon - dra);
            xdec = ABS(eqd_sphere.lat - ddec);
            diff_seconds = V(3600.0 * sqrt(xra*xra + xdec*xdec));
            DEBUG("C AberrationTest(%s line %d): xra=%0.6lf deg, xdec=%0.6lf deg, diff_seconds=%0.3lf.\n", filename, lnum, xra, xdec, diff_seconds);
            if (diff_seconds > THRESHOLD_SECONDS)
                FAIL("C AberrationTest(%s line %d): EXCESSIVE ANGULAR ERROR = %0.3lf seconds\n", filename, lnum, diff_seconds);
            if (diff_seconds > max_diff_seconds)
                max_diff_seconds = diff_seconds;

            /* We have completed one more test case. */
            ++count;
        }
    }

    printf("C AberrationTest(%s): PASS - Tested %d cases. max_diff_seconds = %0.3lf\n", filename, count, max_diff_seconds);
    error = 0;
fail:
    if (infile != NULL) fclose(infile);
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static double StateVectorDiff(int relative, const double vec[3], double x, double y, double z)
{
    double dx = V(vec[0] - x);
    double dy = V(vec[1] - y);
    double dz = V(vec[2] - z);
    double diff_squared = dx*dx + dy*dy + dz*dz;
    if (relative)
        diff_squared /= (vec[0]*vec[0] + vec[1]*vec[1] + vec[2]*vec[2]);
    return V(sqrt(diff_squared));
}

struct _verify_state_context_t;

typedef astro_state_vector_t (* state_func_t) (struct _verify_state_context_t *, astro_body_t, astro_time_t);

typedef struct _verify_state_context_t
{
    state_func_t    func;

    /* The following are used for Lagrange point testing only. */
    astro_body_t    major;
    int             point;      /* 1=L1, 2=L2, ..., 5=L5. */
}
verify_state_context_t;

static double ArcCos(double x)
{
    if (x <= -1.0)
        return 180.0;

    if (x >= +1.0)
        return 0.0;

    return RAD2DEG * acos(x);
}

static void PrintDiagnostic(double x, double y, double z, const double ref[3])
{
    double calc_mag, ref_mag, angle;

    calc_mag = sqrt(x*x + y*y + z*z);
    ref_mag = sqrt(ref[0]*ref[0] + ref[1]*ref[1] + ref[2]*ref[2]);
    angle = 60.0 * ArcCos((x*ref[0] + y*ref[1] + z*ref[2])/(calc_mag * ref_mag));
    fprintf(stderr, "CALCULATED x = %22.16le, y = %22.16le, z = %22.16le [mag = %22.16le]\n", x, y, z, calc_mag);
    fprintf(stderr, "REFERENCE  x = %22.16le, y = %22.16le, z = %22.16le [mag = %22.16le]\n", ref[0], ref[1], ref[2], ref_mag);
    fprintf(stderr, "ANGLE ERROR = %0.6lf arcmin, MAG ERROR = %0.6lf\n", angle, (calc_mag-ref_mag)/ref_mag);
}

static int VerifyState(
    verify_state_context_t *context,
    double *max_rdiff,
    double *max_vdiff,
    astro_body_t body,
    const char *filename,
    int lnum,
    astro_time_t time,
    const double pos[3],
    const double vel[3],
    double r_thresh,
    double v_thresh)
{
    int error;
    astro_state_vector_t state;
    double rdiff, vdiff;

    state = context->func(context, body, time);
    if (state.status != ASTRO_SUCCESS)
        FAIL("C VerifyState(%s line %d): state function returned error %d\n", filename, lnum, state.status);

    rdiff = StateVectorDiff((r_thresh > 0.0), pos, state.x, state.y, state.z);
    if (rdiff > *max_rdiff)
        *max_rdiff = rdiff;

    vdiff = StateVectorDiff((v_thresh > 0.0), vel, state.vx, state.vy, state.vz);
    if (vdiff > *max_vdiff)
        *max_vdiff = vdiff;

    if (rdiff > fabs(r_thresh))
    {
        PrintDiagnostic(state.x, state.y, state.z, pos);
        FAIL("C VerifyState(%s line %d): EXCESSIVE position error = %0.4le\n", filename, lnum, rdiff);
    }

    if (vdiff > fabs(v_thresh))
    {
        PrintDiagnostic(state.vx, state.vy, state.vz, vel);
        FAIL("C VerifyState(%s line %d): EXCESSIVE velocity error = %0.4le\n", filename, lnum, vdiff);
    }

    error = 0;
fail:
    return error;
}

static int VerifyStateBody(
    verify_state_context_t *context,
    astro_body_t body,
    const char *filename,
    double r_thresh,
    double v_thresh)
{
    int error, lnum, nscanned;
    int found_begin = 0;
    int found_end = 0;
    int count = 0;
    int part = 0;
    FILE *infile = NULL;
    char line[100];
    astro_time_t time;
    double jd, pos[3], vel[3];
    double max_rdiff = 0.0, max_vdiff = 0.0;

    infile = fopen(filename, "rt");
    if (infile == NULL)
        FFAIL("Cannot open input file: %s\n", filename);

    lnum = 0;
    while (!found_end && ReadLine(line, sizeof(line), infile, filename, lnum))
    {
        ++lnum;
        if (!found_begin)
        {
            if (strlen(line) >= 5 && !memcmp(line, "$$SOE", 5))
                found_begin = 1;
        }
        else
        {
            /*
                Input comes in triplets of lines:

                2444249.500000000 = A.D. 1980-Jan-11 00:00:00.0000 TDB
                 X =-3.314860345089456E-01 Y = 8.463418210972562E-01 Z = 3.667227830514760E-01
                 VX=-1.642704711077836E-02 VY=-5.494770742558920E-03 VZ=-2.383170237527642E-03

                Track which of these 3 cases we are in using the 'part' variable...
            */

            switch (part)
            {
            case 0:
                if (strlen(line) >= 5 && !memcmp(line, "$$EOE", 5))
                {
                    found_end = 1;
                }
                else
                {
                    nscanned = sscanf(line, "%lf", &jd);
                    if (nscanned != 1)
                        FAIL("C VerifyStateBody(%s line %d) ERROR reading Julian date.\n", filename, lnum);
                    V(jd);

                    /* Convert julian TT day value to astro_time_t. */
                    time = Astronomy_TerrestrialTime(jd - JD_2000);
                }
                break;

            case 1:
                nscanned = sscanf(line, " X =%lf Y =%lf Z =%lf", &pos[0], &pos[1], &pos[2]);
                if (nscanned != 3)
                    FAIL("C VerifyStateBody(%s line %d) ERROR reading position vector.\n", filename, lnum);
                V(pos[0]);
                V(pos[1]);
                V(pos[2]);
                break;

            case 2:
                nscanned = sscanf(line, " VX=%lf VY=%lf VZ=%lf", &vel[0], &vel[1], &vel[2]);
                if (nscanned != 3)
                    FAIL("C VerifyStateBody(%s line %d) ERROR reading velocity vector.\n", filename, lnum);
                V(vel[0]);
                V(vel[1]);
                V(vel[2]);
                CHECK(VerifyState(context, &max_rdiff, &max_vdiff, body, filename, lnum, time, pos, vel, r_thresh, v_thresh));
                ++count;
                break;

            default:
                FFAIL("INTERNAL ERROR : part=%d\n", part);
            }

            part = (part + 1) % 3;
        }
    }

    DEBUG("C VerifyStateBody(%-22s): PASS - Tested %d cases. max rdiff=%0.3le, vdiff=%0.3le\n", filename, count, max_rdiff, max_vdiff);
    error = 0;
fail:
    if (infile != NULL) fclose(infile);
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

/* Constants for use inside unit tests only; they doesn't make sense for public consumption. */
#define BODY_GEOMOON    ((astro_body_t)(-100))
#define BODY_GEO_EMB    ((astro_body_t)(-101))

static astro_state_vector_t BaryState(verify_state_context_t *context, astro_body_t body, astro_time_t time)
{
    (void)context;

    if (body == BODY_GEOMOON)
        return Astronomy_GeoMoonState(time);

    if (body == BODY_GEO_EMB)
        return Astronomy_GeoEmbState(time);

    return Astronomy_BaryState(body, time);
}

static int BaryStateTest(void)
{
    int error;  /* set as a side-effect of CHECK macro */
    verify_state_context_t context;

    memset(&context, 0, sizeof(context));
    context.func = BaryState;

    CHECK(VerifyStateBody(&context, BODY_SUN,     "barystate/Sun.txt",      -1.224e-05, -1.134e-07));
    CHECK(VerifyStateBody(&context, BODY_MERCURY, "barystate/Mercury.txt",   1.672e-04,  2.698e-04));
    CHECK(VerifyStateBody(&context, BODY_VENUS,   "barystate/Venus.txt",     4.123e-05,  4.308e-05));
    CHECK(VerifyStateBody(&context, BODY_EARTH,   "barystate/Earth.txt",     2.296e-05,  6.359e-05));
    CHECK(VerifyStateBody(&context, BODY_MARS,    "barystate/Mars.txt",      3.107e-05,  5.550e-05));
    CHECK(VerifyStateBody(&context, BODY_JUPITER, "barystate/Jupiter.txt",   7.389e-05,  2.471e-04));
    CHECK(VerifyStateBody(&context, BODY_SATURN,  "barystate/Saturn.txt",    1.067e-04,  3.220e-04));
    CHECK(VerifyStateBody(&context, BODY_URANUS,  "barystate/Uranus.txt",    9.035e-05,  2.519e-04));
    CHECK(VerifyStateBody(&context, BODY_NEPTUNE, "barystate/Neptune.txt",   9.838e-05,  4.446e-04));
    CHECK(VerifyStateBody(&context, BODY_PLUTO,   "barystate/Pluto.txt",     4.259e-05,  7.827e-05));
    CHECK(VerifyStateBody(&context, BODY_MOON,    "barystate/Moon.txt",      2.354e-05,  6.604e-05));
    CHECK(VerifyStateBody(&context, BODY_EMB,     "barystate/EMB.txt",       2.353e-05,  6.511e-05));
    CHECK(VerifyStateBody(&context, BODY_GEOMOON, "barystate/GeoMoon.txt",   4.086e-05,  5.347e-05));
    CHECK(VerifyStateBody(&context, BODY_GEO_EMB, "barystate/GeoEMB.txt",    4.076e-05,  5.335e-05));

    FPASS();
fail:
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static astro_state_vector_t HelioStateFunc(verify_state_context_t *context, astro_body_t body, astro_time_t time)
{
    (void)context;
    return Astronomy_HelioState(body, time);
}

static int HelioStateTest(void)
{
    int error;  /* set as a side-effect of CHECK macro */
    verify_state_context_t context;

    memset(&context, 0, sizeof(context));
    context.func = HelioStateFunc;

    CHECK(VerifyStateBody(&context, BODY_SSB,     "heliostate/SSB.txt",     -1.209e-05, -1.125e-07));
    CHECK(VerifyStateBody(&context, BODY_MERCURY, "heliostate/Mercury.txt",  1.481e-04,  2.756e-04));
    CHECK(VerifyStateBody(&context, BODY_VENUS,   "heliostate/Venus.txt",    3.528e-05,  4.485e-05));
    CHECK(VerifyStateBody(&context, BODY_EARTH,   "heliostate/Earth.txt",    1.476e-05,  6.105e-05));
    CHECK(VerifyStateBody(&context, BODY_MARS,    "heliostate/Mars.txt",     3.154e-05,  5.603e-05));
    CHECK(VerifyStateBody(&context, BODY_JUPITER, "heliostate/Jupiter.txt",  7.455e-05,  2.562e-04));
    CHECK(VerifyStateBody(&context, BODY_SATURN,  "heliostate/Saturn.txt",   1.066e-04,  3.150e-04));
    CHECK(VerifyStateBody(&context, BODY_URANUS,  "heliostate/Uranus.txt",   9.034e-05,  2.712e-04));
    CHECK(VerifyStateBody(&context, BODY_NEPTUNE, "heliostate/Neptune.txt",  9.834e-05,  4.534e-04));
    CHECK(VerifyStateBody(&context, BODY_PLUTO,   "heliostate/Pluto.txt",    4.271e-05,  1.198e-04));
    CHECK(VerifyStateBody(&context, BODY_MOON,    "heliostate/Moon.txt",     1.477e-05,  6.195e-05));
    CHECK(VerifyStateBody(&context, BODY_EMB,     "heliostate/EMB.txt",      1.476e-05,  6.106e-05));

    FPASS();
fail:
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static astro_state_vector_t LagrangeFunc(verify_state_context_t *context, astro_body_t minor_body, astro_time_t time)
{
    return Astronomy_LagrangePoint(
        context->point,
        time,
        context->major,
        minor_body
    );
}


static int VerifyStateLagrange(
    astro_body_t majorBody,
    astro_body_t minorBody,
    int point,
    const char *filename,
    double r_thresh,
    double v_thresh)
{
    verify_state_context_t context;
    memset(&context, 0, sizeof(context));
    context.func = LagrangeFunc;
    context.major = majorBody;
    context.point = point;
    return VerifyStateBody(&context, minorBody, filename, r_thresh, v_thresh);
}


static int VerifyEquilateral(
    const char *kind,
    astro_body_t major_body,
    astro_body_t minor_body,
    int point,
    astro_time_t time,
    double mx,
    double my,
    double mz,
    double px,
    double py,
    double pz,
    double *max_diff,
    double *max_arcmin)
{
    int error = 0;
    const double LENGTH_TOLERANCE = 1.0e-15;
    const double ARCMIN_TOLERANCE = 3.0e-12;
    double dx, dy, dz, dlength;
    double mlength, plength, diff, dotprod;
    double arcmin;
    char tag[100];

    snprintf(tag, sizeof(tag),
        "C VerifyEquilateral(%s,%s,%s,L%d,tt=%0.3lf)",
        kind,
        Astronomy_BodyName(major_body),
        Astronomy_BodyName(minor_body),
        point,
        time.tt
    );

    /* Verify minor body vector and Lagrange point vector are the same length. */
    mlength = sqrt(mx*mx + my*my + mz*mz);
    plength = sqrt(px*px + py*py + pz*pz);
    diff = ABS((plength - mlength) / mlength);
    if (diff > LENGTH_TOLERANCE)
        FAIL("%s: FAIL mlength = %0.6le, plength = %0.6le, diff = %0.6le\n", tag, mlength, plength, diff);
    if (diff > *max_diff)
        *max_diff = diff;

    /* Verify the third triangle leg (minor body to Lagrange point) is also the same length. */
    dx = px - mx;
    dy = py - my;
    dz = pz - mz;
    dlength = sqrt(dx*dx + dy*dy + dz*dz);
    diff = ABS((dlength - mlength) / mlength);
    if (diff > LENGTH_TOLERANCE)
        FAIL("%s: FAIL mlength = %0.6le, dlength = %0.6le, diff = %0.6le\n", tag, mlength, dlength, diff);
    if (diff > *max_diff)
        *max_diff = diff;

    /* The 3 mutual angles should all be 60 degrees. */
    dotprod = (mx*px + my*py + mz*pz) / (mlength * plength);
    arcmin = ABS(60.0 * (60.0 - ArcCos(dotprod)));
    if (arcmin > ARCMIN_TOLERANCE)
        FAIL("%s: FAIL m/p angle error = %0.6le arcmin.\n", tag, arcmin);
    if (arcmin > *max_arcmin)
        *max_arcmin = arcmin;

    dotprod = -(mx*dx + my*dy + mz*dz) / (mlength * dlength);       /* negate because pointing opposite dirs */
    arcmin = ABS(60.0 * (60.0 - ArcCos(dotprod)));
    if (arcmin > ARCMIN_TOLERANCE)
        FAIL("%s: FAIL m/d angle error = %0.6le arcmin.\n", tag, arcmin);
    if (arcmin > *max_arcmin)
        *max_arcmin = arcmin;

    dotprod = (px*dx + py*dy + pz*dz) / (plength * dlength);       /* negate because pointing opposite dirs */
    arcmin = ABS(60.0 * (60.0 - ArcCos(dotprod)));
    if (arcmin > ARCMIN_TOLERANCE)
        FAIL("%s: FAIL p/d angle error = %0.6le arcmin.\n", tag, arcmin);
    if (arcmin > *max_arcmin)
        *max_arcmin = arcmin;

fail:
    if (error != 0) printf("%s: returning %d\n", tag, error);
    return error;
}


static int VerifyLagrangeTriangle(astro_body_t major_body, astro_body_t minor_body, int point)
{
    int error, count;
    char tag[100];
    astro_state_vector_t major_state, minor_state, point_state;
    double major_mass, minor_mass;
    const double tt1 = 7335.5;      /* 2020-02-01T00:00Z */
    const double tt2 = 7425.5;      /* 2020-05-01T00:00Z */
    const double dt = 0.125;        /* 1/8 is exactly represented in binary */
    astro_time_t time;
    double max_pos_diff = 0.0, max_pos_arcmin = 0.0;
    double max_vel_diff = 0.0, max_vel_arcmin = 0.0;

    snprintf(tag, sizeof(tag),
        "C VerifyLagrangeTriangle(%s,%s,L%d)",
        Astronomy_BodyName(major_body),
        Astronomy_BodyName(minor_body),
        point
    );

    if (point != 4 && point != 5)
        FAIL("%s: Invalid Lagrange point %d\n", tag, point);

    major_mass = Astronomy_MassProduct(major_body);
    if (major_mass <= 0.0)
        FAIL("%s: Invalid mass product for major body.\n", tag);

    minor_mass = Astronomy_MassProduct(minor_body);
    if (minor_mass <= 0.0)
        FAIL("%s: Invalid mass product for minor body.\n", tag);

    count = 0;
    time = Astronomy_TerrestrialTime(tt1);
    while (time.tt <= tt2)
    {
        ++count;

        major_state = Astronomy_HelioState(major_body, time);
        if (major_state.status != ASTRO_SUCCESS)
            FAIL("%s: HelioState failed for major body.\n", tag);

        minor_state = Astronomy_HelioState(minor_body, time);
        if (minor_state.status != ASTRO_SUCCESS)
            FAIL("%s: HelioState failed for minor body.\n", tag);

        point_state = Astronomy_LagrangePointFast(point, major_state, major_mass, minor_state, minor_mass);
        if (point_state.status != ASTRO_SUCCESS)
            FAIL("%s: Astronomy_LagrangePoint returned status = %d\n", tag, point_state.status);

        /* Verify the (major, minor, L4/L5) triangle is equilateral, both in position and velocity. */
        CHECK(VerifyEquilateral(
            "position",
            major_body,
            minor_body,
            point,
            time,
            minor_state.x - major_state.x,
            minor_state.y - major_state.y,
            minor_state.z - major_state.z,
            point_state.x,
            point_state.y,
            point_state.z,
            &max_pos_diff,
            &max_pos_arcmin
        ));

        CHECK(VerifyEquilateral(
            "velocity",
            major_body,
            minor_body,
            point,
            time,
            minor_state.vx - major_state.vx,
            minor_state.vy - major_state.vy,
            minor_state.vz - major_state.vz,
            point_state.vx,
            point_state.vy,
            point_state.vz,
            &max_vel_diff,
            &max_vel_arcmin
        ));

        time = Astronomy_TerrestrialTime(time.tt + dt);
    }

    if (Verbose)
    {
        printf("%s: PASS (%d cases)\n", tag, count);
        printf("    max_pos_diff = %0.3le, max_vel_diff = %0.3le\n", max_pos_diff, max_vel_diff);
        printf("    max_pos_arcmin = %0.3le, max_vel_arcmin = %0.3le\n", max_pos_arcmin, max_vel_arcmin);
    }
    error = 0;
fail:
    return error;
}


static int VerifyGeoMoon(const char *filename)
{
    verify_state_context_t context;

    memset(&context, 0, sizeof(context));
    context.func = BaryState;
    return VerifyStateBody(&context, BODY_GEOMOON, filename, 3.777e-5, 5.047e-5);
}


static double ErrorArcmin(
    double ax, double ay, double az,
    double bx, double by, double bz)
{
    double dx = ax - bx;
    double dy = ay - by;
    double dz = az - bz;
    double error = sqrt(dx*dx + dy*dy + dz*dz);
    double mag = sqrt(ax*ax + ay*ay + az*az);
    return (error / mag) * (RAD2DEG * 60.0);
}


static int LagrangeJplGeoMoon(const char *mb_filename, const char *lp_filename, int point)
{
    int error, i;
    double major_mass, minor_mass;
    astro_state_vector_t major;
    astro_state_vector_t m, p, q;
    state_vector_batch_t mb = EmptyStateVectorBatch();
    state_vector_batch_t lp = EmptyStateVectorBatch();
    double arcmin;
    double max_pos_arcmin = 0.0;
    double max_vel_arcmin = 0.0;

    /* The major body is the Earth, and we do everything geocentrically. */
    major.x = major.y = major.z = 0.0;
    major.vx = major.vy = major.vz = 0.0;
    memset(&major.t, 0, sizeof(major.t));
    major.status = ASTRO_SUCCESS;

    major_mass = Astronomy_MassProduct(BODY_EARTH);
    if (major_mass <= 0.0)
        FAIL("LagrangeJplGeoMoon: cannot find Earth mass\n");

    minor_mass = Astronomy_MassProduct(BODY_MOON);
    if (minor_mass <= 0.0)
        FAIL("LagrangeJplGeoMoon: cannot find Moon mass\n");

    /* Use state vectors provided by JPL to calculate Lagrange points. */
    /* Compare calculated Lagrange points against JPL Lagrange points. */

    CHECK(LoadStateVectors(&mb, mb_filename));
    CHECK(LoadStateVectors(&lp, lp_filename));
    if (mb.length != lp.length)
        FFAIL("%s has %d states, but %s has %d\n", mb_filename, mb.length, lp_filename, lp.length);

    for (i = 0; i < mb.length; ++i)
    {
        m = mb.array[i];
        p = lp.array[i];
        if (m.t.tt != p.t.tt)
            FAIL("C LagrangeJplGeoMoon(%s): mismatching time: %0.16lf != %0.16lf.\n", lp_filename, m.t.tt, p.t.tt);

        major.t = m.t;
        q = Astronomy_LagrangePointFast(point, major, major_mass, m, minor_mass);
        if (q.status != ASTRO_SUCCESS)
            FAIL("C LagrangeJplGeoMoon(%s): Astronomy_LagrangePoint returned status %d.\n", lp_filename, q.status);

        arcmin = ErrorArcmin(p.x, p.y, p.z, q.x, q.y, q.z);
        if (arcmin > max_pos_arcmin)
            max_pos_arcmin = arcmin;
        if (arcmin > 4.9e-5)
            FAIL("C LagrangeJplGeoMoon(%s, %d): EXCESSIVE position error = %0.6lf arcmin.\n", lp_filename, i, arcmin);

        arcmin = ErrorArcmin(p.vx, p.vy, p.vz, q.vx, q.vy, q.vz);
        if (arcmin > max_vel_arcmin)
            max_vel_arcmin = arcmin;
        if (arcmin > 5.45)      /* !!! REPORT TO JPL !!! */
            FAIL("C LagrangeJplGeoMoon(%s, %d): EXCESSIVE velocity error = %0.6lf arcmin.\n", lp_filename, i, arcmin);
    }

    DEBUG("C LagrangeJplGeoMoon(%s): PASS: %d cases, max pos arcmin = %0.16lf, max vel arcmin = %0.16lf\n", lp_filename, mb.length, max_pos_arcmin, max_vel_arcmin);
fail:
    FreeStateVectorBatch(&mb);
    FreeStateVectorBatch(&lp);
    return error;
}


static int LagrangeTest(void)
{
    int error;  /* set as a side-effect of CHECK macro */

    /* Before verifying against JPL values, do self-consistency checks for L4/L5. */
    CHECK(VerifyLagrangeTriangle(BODY_EARTH, BODY_MOON, 4));
    CHECK(VerifyLagrangeTriangle(BODY_EARTH, BODY_MOON, 5));

    /* Make sure our geocentric moon calculations match JPL's. */
    CHECK(VerifyGeoMoon("lagrange/geo_moon.txt"));

    /* Try feeding JPL states into Astronomy_LagrangePoint(). */
    CHECK(LagrangeJplGeoMoon("lagrange/geo_moon.txt", "lagrange/em_L4.txt", 4));
    CHECK(LagrangeJplGeoMoon("lagrange/geo_moon.txt", "lagrange/em_L5.txt", 5));

    /* NOTE: JPL Horizons does not provide L3 calculations. */

    CHECK(VerifyStateLagrange(BODY_SUN, BODY_EMB, 1, "lagrange/semb_L1.txt",   1.33e-5, 6.13e-5));
    CHECK(VerifyStateLagrange(BODY_SUN, BODY_EMB, 2, "lagrange/semb_L2.txt",   1.33e-5, 6.13e-5));
    CHECK(VerifyStateLagrange(BODY_SUN, BODY_EMB, 4, "lagrange/semb_L4.txt",   3.75e-5, 5.28e-5));
    CHECK(VerifyStateLagrange(BODY_SUN, BODY_EMB, 5, "lagrange/semb_L5.txt",   3.75e-5, 5.28e-5));

    CHECK(VerifyStateLagrange(BODY_EARTH, BODY_MOON, 1, "lagrange/em_L1.txt",  3.79e-5, 5.06e-5));
    CHECK(VerifyStateLagrange(BODY_EARTH, BODY_MOON, 2, "lagrange/em_L2.txt",  3.79e-5, 5.06e-5));
    CHECK(VerifyStateLagrange(BODY_EARTH, BODY_MOON, 4, "lagrange/em_L4.txt",  3.79e-5, 1.59e-3));
    CHECK(VerifyStateLagrange(BODY_EARTH, BODY_MOON, 5, "lagrange/em_L5.txt",  3.79e-5, 1.59e-3));

    FPASS();
fail:
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int LoadStateVectors(
    state_vector_batch_t *batch,
    const char *filename)
{
    int error, lnum, nscanned;
    int found_begin = 0;
    int found_end = 0;
    int part = 0;
    FILE *infile = NULL;
    char line[100];
    double jd;
    astro_state_vector_t state;

    memset(&state, 0, sizeof(state));

    infile = fopen(filename, "rt");
    if (infile == NULL)
        FFAIL("Cannot open input file: %s\n", filename);

    lnum = 0;
    while (!found_end && ReadLine(line, sizeof(line), infile, filename, lnum))
    {
        ++lnum;
        if (!found_begin)
        {
            if (strlen(line) >= 5 && !memcmp(line, "$$SOE", 5))
                found_begin = 1;
        }
        else
        {
            /*
                Input comes in triplets of lines:

                2444249.500000000 = A.D. 1980-Jan-11 00:00:00.0000 TDB
                 X =-3.314860345089456E-01 Y = 8.463418210972562E-01 Z = 3.667227830514760E-01
                 VX=-1.642704711077836E-02 VY=-5.494770742558920E-03 VZ=-2.383170237527642E-03

                Track which of these 3 cases we are in using the 'part' variable...
            */

            switch (part)
            {
            case 0:
                if (strlen(line) >= 5 && !memcmp(line, "$$EOE", 5))
                {
                    found_end = 1;
                }
                else
                {
                    nscanned = sscanf(line, "%lf", &jd);
                    if (nscanned != 1)
                        FAIL("C LoadStateVectors(%s line %d) ERROR reading Julian date.\n", filename, lnum);
                    V(jd);

                    /* Convert julian TT day value to astro_time_t. */
                    state.t = Astronomy_TerrestrialTime(jd - JD_2000);
                }
                break;

            case 1:
                nscanned = sscanf(line, " X =%lf Y =%lf Z =%lf", &state.x, &state.y, &state.z);
                if (nscanned != 3)
                    FAIL("C LoadStateVectors(%s line %d) ERROR reading position vector.\n", filename, lnum);
                V(state.x);
                V(state.y);
                V(state.z);
                break;

            case 2:
                nscanned = sscanf(line, " VX=%lf VY=%lf VZ=%lf", &state.vx, &state.vy, &state.vz);
                if (nscanned != 3)
                    FAIL("C LoadStateVectors(%s line %d) ERROR reading velocity vector.\n", filename, lnum);
                V(state.vx);
                V(state.vy);
                V(state.vz);
                state.status = ASTRO_SUCCESS;
                CHECK(AppendStateVector(batch, state));
                break;

            default:
                FFAIL("INTERNAL ERROR : part=%d\n", part);
            }

            part = (part + 1) % 3;
        }
    }

    error = 0;
fail:
    if (infile != NULL) fclose(infile);
    return error;
}


static astro_vector_t CrossProduct(astro_vector_t a, astro_vector_t b)
{
    astro_vector_t c;

    c.status = ASTRO_SUCCESS;
    c.t = a.t;
    c.x = a.y*b.z - a.z*b.y;
    c.y = a.z*b.x - a.x*b.z;
    c.z = a.x*b.y - a.y*b.x;

    return c;
}


static int LagrangeJplAnalyzeFiles(
    const char *mb_filename,        /* filename containing minor body state vectors relative to major body */
    const char *lp_filename,        /* filename containing Lagrange point state vectors relative to major body */
    int point)                      /* Lagrange point 1..5 */
{
    int error, i;
    state_vector_batch_t mb = EmptyStateVectorBatch();
    state_vector_batch_t lp = EmptyStateVectorBatch();
    double pos_mag_ratio, vel_mag_ratio, angle;
    double pos_dev, vel_dev, angle_dev;
    double dr, dv, da;
    double m_mag, p_mag;
    astro_state_vector_t m, p;

    /*
        [Don Cross - 2022-02-12]
        I am trying to understand how JPL Horizons calculates L4 and L5.
        So I generated data for heliocentric EMB state vectors
        and geocentric Moon vectors. This should be enough to run statistics
        on distance ratios, angles, and planes of alignment (normal vectors).

        Quantities I would like to find means and standard deviations for:
        1. Distance from the major and minor bodies.
        2. Angle away from the vector from the major body toward the minor body.
        3. (L4/L5 only) Normal vector to the plane that contains both L4/L5 vector and minor body vector.
    */

    CHECK(LoadStateVectors(&mb, mb_filename));
    CHECK(LoadStateVectors(&lp, lp_filename));
    if (mb.length != lp.length)
        FFAIL("%d state vectors in %s, but %d in %s\n", mb.length, mb_filename, lp.length, lp_filename);

    if (mb.length < 10)
        FAIL("C LagrangeJplAnalyzeFiles(%s): %d state vectors is not enough for statistical analysis.\n", mb_filename, mb.length);

    /* Calculate mean values. */
    angle = pos_mag_ratio = vel_mag_ratio = 0.0;
    for (i = 0; i < mb.length; ++i)
    {
        m = mb.array[i];
        p = lp.array[i];

        /* Sanity check that the time offsets match between the two files. */
        if (m.t.tt != p.t.tt)
            FAIL("C LagrangeJplAnalyzeFiles(%d, %s): time mismatch", i, lp_filename);

        m_mag = sqrt(m.x*m.x + m.y*m.y + m.z*m.z);
        p_mag = sqrt(p.x*p.x + p.y*p.y + p.z*p.z);
        pos_mag_ratio += p_mag/m_mag;
        vel_mag_ratio += sqrt((p.vx*p.vx + p.vy*p.vy + p.vz*p.vz) / (m.vx*m.vx + m.vy*m.vy + m.vz*m.vz));
        angle += ArcCos((m.x*p.x + m.y*p.y + m.z*p.z) / (m_mag * p_mag));
    }
    pos_mag_ratio /= mb.length;
    vel_mag_ratio /= mb.length;
    angle /= mb.length;

    /* Calculate standard deviations. */
    pos_dev = vel_dev = angle_dev = 0.0;
    for (i = 0; i < mb.length; ++i)
    {
        m = mb.array[i];
        p = lp.array[i];
        m_mag = sqrt(m.x*m.x + m.y*m.y + m.z*m.z);
        p_mag = sqrt(p.x*p.x + p.y*p.y + p.z*p.z);
        dr = pos_mag_ratio - p_mag/m_mag;
        dv = vel_mag_ratio - sqrt((p.vx*p.vx + p.vy*p.vy + p.vz*p.vz) / (m.vx*m.vx + m.vy*m.vy + m.vz*m.vz));
        da = angle - ArcCos((m.x*p.x + m.y*p.y + m.z*p.z) / (m_mag * p_mag));
        pos_dev += (dr * dr);
        vel_dev += (dv * dv);
        angle_dev += (da * da);
    }
    pos_dev = sqrt(pos_dev / mb.length);
    vel_dev = sqrt(vel_dev / mb.length);
    angle_dev = sqrt(angle_dev / mb.length);

    printf("C LagrangeJplAnalyzeFiles(%s): %d samples\n", lp_filename, mb.length);
    printf("    mag [mean = %0.8lf, dev = %0.8lf]; vel [mean = %0.8lf, dev = %0.8lf]\n",
        pos_mag_ratio, pos_dev,
        vel_mag_ratio, vel_dev);
    printf("    angle [mean = %0.8lf, dev = %0.8lf]\n",
        angle, angle_dev);

    /* Special case for L4, L5: try to understand the instantaneous co-orbital plane. */
    if (point == 4 || point == 5)
    {
        double max_pole_diff = 0.0;
        astro_angle_result_t pole_diff;
        double v_mag;
        astro_vector_t m_unit, p_unit, mp_norm;
        astro_vector_t v_unit, mv_norm;
        m_unit.status = p_unit.status = v_unit.status = ASTRO_SUCCESS;

        for (i = 0; i < mb.length; ++i)
        {
            m = mb.array[i];
            p = lp.array[i];

            m_mag = sqrt(m.x*m.x + m.y*m.y + m.z*m.z);
            m_unit.x = m.x / m_mag;
            m_unit.y = m.y / m_mag;
            m_unit.z = m.z / m_mag;
            m_unit.t = m.t;

            p_mag = sqrt(p.x*p.x + p.y*p.y + p.z*p.z);
            p_unit.x = p.x / p_mag;
            p_unit.y = p.y / p_mag;
            p_unit.z = p.z / p_mag;
            p_unit.t = p.t;

            v_mag = sqrt(m.vx*m.vx + m.vy*m.vy + m.vz*m.vz);
            v_unit.x = m.vx / v_mag;
            v_unit.y = m.vy / v_mag;
            v_unit.z = m.vz / v_mag;
            v_unit.t = m.t;

            if (point == 4)
                mp_norm = CrossProduct(m_unit, p_unit);
            else
                mp_norm = CrossProduct(p_unit, m_unit);

            mv_norm = CrossProduct(m_unit, v_unit);
            pole_diff = Astronomy_AngleBetween(mp_norm, mv_norm);
            CHECK_STATUS(pole_diff);
            if (pole_diff.angle > max_pole_diff)
                max_pole_diff = pole_diff.angle;
        }
        printf("    max L%d pole diff angle = %0.6lf degrees.\n", point, max_pole_diff);
    }

    /* Special case for L4, L5: confirm velocity vector would leave distances as an equilateral triangle. */
    if (point == 4 || point == 5)
    {
        double a, b, c;      /* distances before v*dt increment */
        double dx, dy, dz;
        double ratio;
        double min_ratio_before = NAN, max_ratio_before = NAN;
        double min_ratio_after  = NAN, max_ratio_after  = NAN;
        const double dt = 1.0;

        for (i = 0; i < mb.length; ++i)
        {
            m = mb.array[i];
            p = lp.array[i];

            /* a = distance from major body to minor body */
            a = sqrt(m.x*m.x + m.y*m.y + m.z*m.z);

            /* b = distance from major body to L4/L5 */
            b = sqrt(p.x*p.x + p.y*p.y + p.z*p.z);

            /* c = distance from minor body to L4/L5 */
            dx = p.x - m.x;
            dy = p.y - m.y;
            dz = p.z - m.z;
            c = sqrt(dx*dx + dy*dy + dz*dz);

            ratio = b / a;
            if (i == 0)
            {
                min_ratio_before = max_ratio_before = ratio;
            }
            else
            {
                if (ratio < min_ratio_before)
                    min_ratio_before = ratio;
                if (ratio > max_ratio_before)
                    max_ratio_before = ratio;
            }

            ratio = c / a;
            if (ratio < min_ratio_before)
                min_ratio_before = ratio;
            if (ratio > max_ratio_before)
                max_ratio_before = ratio;

            ratio = c / b;
            if (ratio < min_ratio_before)
                min_ratio_before = ratio;
            if (ratio > max_ratio_before)
                max_ratio_before = ratio;

            /* simulate a small movement of the body over a time period dt */
            dx = m.x + dt*m.vx;
            dy = m.y + dt*m.vy;
            dz = m.z + dt*m.vz;
            a = sqrt(dx*dx + dy*dy + dz*dz);

            /* simulate straight-line movement of Lagrange point over dt */
            dx = p.x + dt*p.vx;
            dy = p.y + dt*p.vy;
            dz = p.z + dt*p.vz;
            b = sqrt(dx*dx + dy*dy + dz*dz);

            /* measure extrapolated distance between minor body and Lagrange point */
            dx -= m.x + dt*m.vx;
            dy -= m.y + dt*m.vy;
            dz -= m.z + dt*m.vz;
            c = sqrt(dx*dx + dy*dy + dz*dz);

            /* All 3 distances (a, b, c) should be the same. */

            ratio = b / a;
            if (i == 0)
            {
                min_ratio_after = max_ratio_after = ratio;
            }
            else
            {
                if (ratio < min_ratio_after)
                    min_ratio_after = ratio;
                if (ratio > max_ratio_after)
                    max_ratio_after = ratio;
            }

            ratio = c / a;
            if (ratio < min_ratio_after)
                min_ratio_after = ratio;
            if (ratio > max_ratio_after)
                max_ratio_after = ratio;

            ratio = c / b;
            if (ratio < min_ratio_after)
                min_ratio_after = ratio;
            if (ratio > max_ratio_after)
                max_ratio_after = ratio;

            if (ABS(1.0 - min_ratio_after) > 1.0e-7)
                FAIL("LagrangeJplAnalyzeFiles(%s): BAD min_ratio_after = %0.15lf\n", lp_filename, min_ratio_after);

            if (ABS(1.0 - max_ratio_after) > 1.0e-7)
                FAIL("LagrangeJplAnalyzeFiles(%s): BAD max_ratio_after = %0.15lf\n", lp_filename, max_ratio_after);
        }

        printf("    length ratios before: min = %0.15lf, max = %0.15lf\n", min_ratio_before, max_ratio_before);
        printf("    length ratios after : min = %0.15lf, max = %0.15lf\n", min_ratio_after,  max_ratio_after );
    }

    /* Special case for L4/L5: confirm velocity vectors are 60 degrees apart. */
    if (point == 4 || point == 5)
    {
        double speed, angle, arcmin_error;
        double mx, my, mz;
        double px, py, pz;
        double min_angle = NAN;
        double max_angle = NAN;

        for (i = 0; i < mb.length; ++i)
        {
            m = mb.array[i];
            p = lp.array[i];

            /* Calculate unit vector in direction of the minor body's velocity. */
            speed = sqrt(m.vx*m.vx + m.vy*m.vy + m.vz*m.vz);
            mx = m.vx / speed;
            my = m.vy / speed;
            mz = m.vz / speed;

            /* Calculate unit vector in the direction of the Lagrange point's velocity. */
            speed = sqrt(p.vx*p.vx + p.vy*p.vy + p.vz*p.vz);
            px = p.vx / speed;
            py = p.vy / speed;
            pz = p.vz / speed;

            /* The dot product should always be very close to 0.5 (an angle of 60 degrees). */
            angle = ArcCos(px*mx + py*my + pz*mz);
            arcmin_error = 60.0 * ABS(angle - 60.0);
            if (arcmin_error > 0.0026)
                FAIL("C LagrangeJplAnalyzeFiles(%d, %s): velocity angle is out of bounds: %0.16lf (error = %0.4le arcmin)\n", i, lp_filename, angle, arcmin_error);

            if (i == 0)
            {
                min_angle = max_angle = angle;
            }
            else
            {
                if (angle < min_angle)
                    min_angle = angle;
                if (angle > max_angle)
                    max_angle = angle;
            }
        }

        printf("    speed angles: min_angle = %0.15lf, max_angle = %0.15lf, spread = %0.4le\n", min_angle, max_angle, max_angle - min_angle);
    }

fail:
    FreeStateVectorBatch(&mb);
    FreeStateVectorBatch(&lp);
    return error;
}


static int LagrangeJplAnalysis(void)
{
    int error;

    CHECK(LagrangeJplAnalyzeFiles("lagrange/helio_emb.txt", "lagrange/semb_L1.txt", 1));
    CHECK(LagrangeJplAnalyzeFiles("lagrange/helio_emb.txt", "lagrange/semb_L2.txt", 2));
    CHECK(LagrangeJplAnalyzeFiles("lagrange/helio_emb.txt", "lagrange/semb_L4.txt", 4));
    CHECK(LagrangeJplAnalyzeFiles("lagrange/helio_emb.txt", "lagrange/semb_L5.txt", 5));

    CHECK(LagrangeJplAnalyzeFiles("lagrange/geo_moon.txt", "lagrange/em_L1.txt", 1));
    CHECK(LagrangeJplAnalyzeFiles("lagrange/geo_moon.txt", "lagrange/em_L2.txt", 2));
    CHECK(LagrangeJplAnalyzeFiles("lagrange/geo_moon.txt", "lagrange/em_L4.txt", 4));
    CHECK(LagrangeJplAnalyzeFiles("lagrange/geo_moon.txt", "lagrange/em_L5.txt", 5));
fail:
    return error;
}


/*-----------------------------------------------------------------------------------------------------------*/

static astro_state_vector_t TopoStateFunc(verify_state_context_t *context, astro_body_t body, astro_time_t time)
{
    astro_observer_t        observer;
    astro_state_vector_t    observer_state, state;

    (void)context;

    observer.latitude = 30.0;
    observer.longitude = -80.0;
    observer.height = 1000.0;

    observer_state = Astronomy_ObserverState(&time, observer, EQUATOR_J2000);
    if (observer_state.status != ASTRO_SUCCESS)
    {
        fprintf(stderr, "C TopoStateFunc: Astronomy_ObserverState returned error %d\n", observer_state.status);
        return observer_state;
    }

    if (body == BODY_GEO_EMB)
    {
        state = Astronomy_GeoEmbState(time);
    }
    else if (body == BODY_EARTH)
    {
        state.status = ASTRO_SUCCESS;
        state.t = time;
        state.x = state.y = state.z = 0.0;
        state.vx = state.vy = state.vz = 0.0;
    }
    else
    {
        fprintf(stderr, "C TopoStateFunc: Unsupported body %d\n", body);
        memset(&state, 0, sizeof(state));
        state.status = ASTRO_INVALID_BODY;
        return state;
    }

    if (state.status != ASTRO_SUCCESS)
    {
        fprintf(stderr, "C TopoStateFunc: body %d resulted in error %d\n", body, state.status);
        return state;
    }

    state.x  -= observer_state.x;
    state.y  -= observer_state.y;
    state.z  -= observer_state.z;
    state.vx -= observer_state.vx;
    state.vy -= observer_state.vy;
    state.vz -= observer_state.vz;

    return state;
}

static int TopoStateTest(void)
{
    int error;  /* set as a side-effect of CHECK macro */
    verify_state_context_t context;

    context.func = TopoStateFunc;
    CHECK(VerifyStateBody(&context, BODY_EARTH,   "topostate/Earth_N30_W80_1000m.txt",  2.108e-04, 2.430e-04));
    CHECK(VerifyStateBody(&context, BODY_GEO_EMB, "topostate/EMB_N30_W80_1000m.txt",    7.197e-04, 2.497e-04));

    FPASS();
fail:
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int Twilight(void)
{
    int error, lnum, nscanned, i;
    char line[200];
    char search_text[20];
    char token[6][20];
    astro_time_t search_time;
    astro_time_t expected_time[6];
    astro_search_result_t calc[6];
    FILE *infile;
    astro_observer_t observer;
    double diff, max_diff = 0.0;
    const double tolerance_seconds = 60.0;
    const char *filename = "riseset/twilight.txt";

    infile = fopen(filename, "rt");
    if (infile == NULL)
        FFAIL("Cannot open input file: %s\n", filename);

    lnum = 0;
    observer.height = 0.0;
    while (ReadLine(line, sizeof(line), infile, filename, lnum))
    {
        ++lnum;

        nscanned = sscanf(line, "%lf %lf %17s %17s %17s %17s %17s %17s %17s",
            &observer.latitude,
            &observer.longitude,
            search_text,
            token[0], token[1], token[2], token[3], token[4], token[5]);

        if (nscanned != 9)
            FAIL("C Twilight(%s line %d): expected 9 tokens but found %d\n", filename, lnum, nscanned);

        if (ParseDate(search_text, &search_time))
            FAIL("C Twilight(%s line %d): BAD SEARCH TIME FORMAT.", filename, lnum);

        for (i = 0; i < 6; ++i)
            if (ParseDate(token[i], &expected_time[i]))
                FAIL("C Twilight(%s line %d): BAD EXPECTED TIME [%d] FORMAT.\n", filename, lnum, i);

        search_time = expected_time[0];
        calc[0] = Astronomy_SearchAltitude(BODY_SUN, observer, DIRECTION_RISE, search_time, 1.0, -18.0);   // astronomical dawn
        calc[1] = Astronomy_SearchAltitude(BODY_SUN, observer, DIRECTION_RISE, search_time, 1.0, -12.0);   // nautical dawn
        calc[2] = Astronomy_SearchAltitude(BODY_SUN, observer, DIRECTION_RISE, search_time, 1.0,  -6.0);   // civil dawn
        calc[3] = Astronomy_SearchAltitude(BODY_SUN, observer, DIRECTION_SET,  search_time, 1.0,  -6.0);   // civil dusk
        calc[4] = Astronomy_SearchAltitude(BODY_SUN, observer, DIRECTION_SET,  search_time, 1.0, -12.0);   // nautical dusk
        calc[5] = Astronomy_SearchAltitude(BODY_SUN, observer, DIRECTION_SET,  search_time, 1.0, -18.0);   // astronomical dusk

        for (i = 0; i < 6; ++i)
        {
            if (calc[i].status != ASTRO_SUCCESS)
                FAIL("C Twilight(%s line %d): error %d in search[%d]\n", filename, lnum, calc[i].status, i);

            diff = 86400.0 * ABS(calc[i].time.ut - expected_time[i].ut);
            if (diff > tolerance_seconds)
                FAIL("C TwilightTest(%s line %d): EXCESSIVE ERROR = %0.3lf seconds for test %d.\n", filename, lnum, diff, i);

            if (diff > max_diff)
                max_diff = diff;
        }
    }

    FPASSA("(%d test cases, max error = %0.3lf seconds)\n", lnum, max_diff);
fail:
    if (infile != NULL) fclose(infile);
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int Libration(const char *filename, int *ndata, double *var_lon, double *var_lat)
{
    int error;
    FILE *infile;
    int lnum, count, nscanned;
    char line[200];
    int day, month, year, hour, minute;
    char mtext[4];
    double phase, age, diam, dist, ra, dec, slon, slat, elon, elat, axisa;
    astro_time_t time;
    astro_libration_t lib;
    double diff_elon, diff_elat, diff_distance, diff_diam;
    double max_diff_elon = 0.0, max_diff_elat = 0.0, max_diff_distance = 0.0, max_diff_diam = 0.0;
    double max_eclip_lon = -900.0;

    infile = fopen(filename, "rt");
    if (infile == NULL)
        FFAIL("cannot open input file: %s\n", filename);

    lnum = 0;
    count = 0;
    while (ReadLine(line, sizeof(line), infile, filename, lnum))
    {
        ++lnum;
        if (lnum == 1)
        {
            if (strcmp(line, "   Date       Time    Phase    Age    Diam    Dist     RA        Dec      Slon      Slat     Elon     Elat   AxisA\n"))
                FAIL("C Libration(%s line %d): unexpected header line\n", filename, lnum);
        }
        else
        {
            /* 01 Jan 2020 00:00 UT  29.95   5.783  1774.5  403898  23.2609  -10.0824   114.557   -0.045   0.773    6.360  336.353 */
            nscanned = sscanf(line,
                "%d %3s %d %d:%d UT %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
                &day, mtext, &year, &hour, &minute,
                &phase, &age, &diam, &dist, &ra, &dec, &slon, &slat, &elon, &elat, &axisa);

            if (nscanned != 16)
                FAIL("C Libration(%s line %d): expected 16 tokens, found %d\n", filename, lnum, nscanned);

            /* Calculate the astronomy time value for this calendar date/time. */
            if (ParseMonthName(mtext, &month))
                FAIL("Libration(%s line %d): invalid month symbol '%s'\n", filename, lnum, mtext);

            time = Astronomy_MakeTime(year, month, day, hour, minute, 0.0);
            lib = Astronomy_Libration(time);

            diff_elon = 60.0 * ABS(lib.elon - elon);
            if (diff_elon > max_diff_elon)
                max_diff_elon = diff_elon;

            diff_elat = 60.0 * ABS(lib.elat - elat);
            if (diff_elat > max_diff_elat)
                max_diff_elat = diff_elat;

            diff_distance = ABS(lib.dist_km - dist);
            if (diff_distance > max_diff_distance)
                max_diff_distance = diff_distance;

            diff_diam = ABS(lib.diam_deg - diam/3600.0);
            if (diff_diam > max_diff_diam)
                max_diff_diam = diff_diam;

            if (lib.mlon > max_eclip_lon)
                max_eclip_lon = lib.mlon;

            if (diff_elon > 0.1304)
                FAIL("C Libration(%s line %d): EXCESSIVE diff_elon = %0.4lf arcmin\n", filename, lnum, diff_elon);

            if (diff_elat > 1.6476)
                FAIL("C Libration(%s line %d): EXCESSIVE diff_elat = %0.4lf arcmin\n", filename, lnum, diff_elat);

            if (diff_distance > 54.377)
                FAIL("C Libration(%s line %d): EXCESSIVE diff_distance = %0.3lf km\n", filename, lnum, diff_distance);

            if (diff_diam > 0.00009)
                FAIL("C Libration(%s line %d): EXCESSIVE diff_diam = %0.3le degrees\n", filename, lnum, diff_diam);

            /* Update sum-of-squared-errors. */
            *var_lon += diff_elon * diff_elon;
            *var_lat += diff_elat * diff_elat;

            ++count;
        }
    }

    if (max_eclip_lon < 359.0 || max_eclip_lon > 360.0)
        FAIL("C Libration(%s): INVALID max ecliptic longitude %0.3lf degrees.\n", filename, max_eclip_lon);

    printf("C Libration(%s): PASS (%d test cases, max_diff_elon = %0.4lf arcmin, max_diff_elat = %0.4lf arcmin, max_diff_distance = %0.3lf km, max_diff_diam = %0.12lf deg)\n",
        filename, count, max_diff_elon, max_diff_elat, max_diff_distance, max_diff_diam);

    *ndata += count;
    error = 0;
fail:
    if (infile != NULL) fclose(infile);
    return error;
}

static int LibrationTest(void)
{
    int error;
    int ndata = 0;
    double var_lat = 0.0;
    double var_lon = 0.0;
    double dev_lat, dev_lon;

    CHECK(Libration("libration/mooninfo_2020.txt", &ndata, &var_lon, &var_lat));
    CHECK(Libration("libration/mooninfo_2021.txt", &ndata, &var_lon, &var_lat));
    CHECK(Libration("libration/mooninfo_2022.txt", &ndata, &var_lon, &var_lat));

    dev_lon = sqrt(var_lon / ndata);
    dev_lat = sqrt(var_lat / ndata);
    printf("C LibrationTest PASS: %d data points, dev_lon = %0.4lf arcmin, dev_lat = %0.4lf arcmin\n", ndata, dev_lon, dev_lat);
fail:
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int DE405_Check(void)
{
    int error, lnum, nscanned;
    const char *filename = "barystate/de405_state.txt";
    FILE *infile;
    double jd;
    astro_time_t time;
    char line[200];
    char name[10];
    double pos[3], vel[3];
    astro_body_t body;
    astro_state_vector_t state;
    double dx, dy, dz, dr, dv;
    double scale;

    infile = fopen(filename, "rt");
    if (infile == NULL)
        FFAIL("cannot open input file: %s\n", filename);

    lnum = 0;
    while (ReadLine(line, sizeof(line), infile, filename, 0))
    {
        ++lnum;
        if (line[0] == '*')
            break;

        if (lnum == 1)
        {
            if (1 != sscanf(line, "%lf", &jd))
                FAIL("DE405_Check(%s line %d): cannot scan Julian Date\n", filename, lnum);
            time = Astronomy_TerrestrialTime(jd - JD_2000);
            Astronomy_FormatTime(time, TIME_FORMAT_MILLI, line, sizeof(line));
            FDEBUG("time = %s\n", line);
        }
        else
        {
            nscanned = sscanf(line, "%10[A-Za-z] %lf %lf %lf %lf %lf %lf", name, &pos[0], &pos[1], &pos[2], &vel[0], &vel[1], &vel[2]);
            if (nscanned != 7)
                FAIL("C DE405_Check(%s line %d): expected 7 tokens, found %d\n", filename, lnum, nscanned);
            body = Astronomy_BodyCode(name);
            if (body == BODY_INVALID)
                FAIL("C DE405_Check(%s line %d): unrecognized body name '%s'\n", filename, lnum, name);

            switch (body)
            {
            case BODY_MOON:
                /* geocentric Moon */
                state = Astronomy_GeoMoonState(time);
                scale = sqrt(pos[0]*pos[0] + pos[1]*pos[1] + pos[2]*pos[2]);
                break;

            case BODY_SUN:
                /* barycentric Sun */
                state = Astronomy_BaryState(body, time);
                scale = 1.0;
                break;

            default:
                /* heliocentric planet */
                state = Astronomy_HelioState(body, time);
                scale = sqrt(pos[0]*pos[0] + pos[1]*pos[1] + pos[2]*pos[2]);
                break;
            }

            if (state.status != ASTRO_SUCCESS)
                FAIL("C DE405_Check(%s line %d): status = %d\n", filename, lnum, state.status);

            dx = pos[0] - state.x;
            dy = pos[1] - state.y;
            dz = pos[2] - state.z;
            dr = V(sqrt(dx*dx + dy*dy + dz*dz) / scale);

            dx = vel[0] - state.vx;
            dy = vel[1] - state.vy;
            dz = vel[2] - state.vz;
            dv = V(sqrt(dx*dx + dy*dy + dz*dz));

            FDEBUG("%-10s dr=%0.4le dv=%0.4le\n", name, dr, dv);
            if (dr > 8.7e-5)
                FLNFAIL("EXCESSIVE POSITION ERROR\n");

            if (dv > 7.3e-6)
                FLNFAIL("EXCESSIVE VELOCITY ERROR\n");
        }
    }

    FPASS();
fail:
    if (infile != NULL) fclose(infile);
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int AxisTestBody(astro_body_t body, const char *filename, double arcmin_tolerance)
{
    int error, lnum, nscanned, found_data, count;
    astro_axis_t axis;
    astro_time_t time;
    astro_spherical_t sphere;
    astro_vector_t north;
    astro_angle_result_t diff;
    double jd, ra, dec, arcmin, max_arcmin;
    FILE *infile;
    char line[100];

    infile = fopen(filename, "rt");
    if (infile == NULL)
        FAIL("AxisTestBody: cannot open input file: %s\n", filename);

    count = 0;
    lnum = 0;
    found_data = 0;
    max_arcmin = 0.0;
    while (ReadLine(line, sizeof(line), infile, filename, lnum))
    {
        ++lnum;
        if (!found_data)
        {
            if (0 == memcmp(line, "$$SOE", 5))
                found_data = 1;
        }
        else
        {
            if (0 == memcmp(line, "$$EOE", 5))
                break;

            /* [ 1979-Jun-13 00:00 2444037.500000000     181.44164   89.88493] */
            if (strlen(line) < 61)
                FAIL("C AxisTestBody(%s line %d): line is too short\n", filename, lnum);

            nscanned = sscanf(&line[19], "%lf %lf %lf", &jd, &ra, &dec);
            if (nscanned != 3)
                FAIL("C AxisTestBody(%s line %d): could not scan data.\n", filename, lnum);

            time = Astronomy_TimeFromDays(jd - JD_2000);
            axis = Astronomy_RotationAxis(body, &time);
            if (axis.status != ASTRO_SUCCESS)
                FAIL("C AxisTestBody(%s line %d): Astronomy_Axis returned error %d\n", filename, lnum, axis.status);

            /* Convert the reference angles to a reference north pole vector. */
            sphere.status = ASTRO_SUCCESS;
            sphere.dist = 1.0;
            sphere.lat = dec;
            sphere.lon = ra;    /* tricky: RA is in degrees, not sidereal hours */
            north = Astronomy_VectorFromSphere(sphere, time);
            if (north.status != ASTRO_SUCCESS)
                FAIL("C AxisTestBody(%s line %d): VectorFromSphere error %d\n", filename, lnum, north.status);

            /* Find angle between two versions of the north pole. Use that as the measure of error. */
            diff = Astronomy_AngleBetween(north, axis.north);
            if (diff.status != ASTRO_SUCCESS)
                FAIL("C AxisTestBody(%s line %d): AngleBetween error %d\n", filename, lnum, diff.status);

            arcmin = diff.angle * 60.0;     /* convert error degrees to arcminutes */
            if (arcmin > max_arcmin)
                max_arcmin = arcmin;

            ++count;
        }
    }

    DEBUG("C AxisTestBody(%s): %d test cases, max arcmin error = %0.6lf.\n", filename, count, max_arcmin);
    if (max_arcmin > arcmin_tolerance)
        FAIL("C AxisTestBody(%s): EXCESSIVE ERROR = %lf arcmin\n", filename, max_arcmin);
    error = 0;
fail:
    if (infile != NULL) fclose(infile);
    return error;
}

static int AxisTest(void)
{
    int error;
    CHECK(AxisTestBody(BODY_SUN,      "axis/Sun.txt",       0.0));
    CHECK(AxisTestBody(BODY_MERCURY,  "axis/Mercury.txt",   0.074340));
    CHECK(AxisTestBody(BODY_VENUS,    "axis/Venus.txt",     0.0));
    CHECK(AxisTestBody(BODY_EARTH,    "axis/Earth.txt",     0.002032));
    CHECK(AxisTestBody(BODY_MOON,     "axis/Moon.txt",      0.264845));
    CHECK(AxisTestBody(BODY_MARS,     "axis/Mars.txt",      0.075323));
    CHECK(AxisTestBody(BODY_JUPITER,  "axis/Jupiter.txt",   0.000324));
    CHECK(AxisTestBody(BODY_SATURN,   "axis/Saturn.txt",    0.000304));
    CHECK(AxisTestBody(BODY_URANUS,   "axis/Uranus.txt",    0.0));
    CHECK(AxisTestBody(BODY_NEPTUNE,  "axis/Neptune.txt",   0.000462));
    CHECK(AxisTestBody(BODY_PLUTO,    "axis/Pluto.txt",     0.0));
    FPASS();
fail:
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int MapPerformanceTest(void)
{
    int error;
    int count;
    astro_observer_t observer;
    astro_time_t time;
    astro_vector_t ovec;
    astro_rotation_t rot;

    time = Astronomy_MakeTime(2022, 1, 22, 12, 30, 0.0);

    count = 0;
    observer.height = 0.0;
    for (observer.longitude = -180.0; observer.longitude < +180.0; observer.longitude += 0.01)
    {
        for (observer.latitude = -85.0; observer.latitude <= +85.0; observer.latitude += 0.01)
        {
            ovec = Astronomy_ObserverVector(&time, observer, EQUATOR_OF_DATE);
            CHECK_STATUS(ovec);
            rot = Astronomy_Rotation_EQD_HOR(&time, observer);
            CHECK_STATUS(rot);
            ++count;
        }
    }

    /*
        612,017,000 geographic locations.

        Before sidereal time optimization:
        Trial #1: 236.420 seconds.
        Trial #2: 235.906 seconds.
        Trial #3: 238.809 seconds.

        After sidereal time optimization:
        Trial #1: 103.452 seconds.
        Trial #2: 104.249 seconds.
        Trial #3: 103.657 seconds.

        Mean performance improvement ratio = 2.284.
    */

    printf("MapPerformanceTest: PASS (%d geographic locations)\n", count);
    error = 0;
fail:
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int GeoMoonPerformance(void)
{
    /* Call EclipticGeoMoon for many different times to measure its speed. */

    int error = 1;
    int count = 0;
    astro_time_t time = Astronomy_MakeTime(1800, 1, 1, 0, 0, 0.0);
    astro_time_t stopTime = Astronomy_MakeTime(2200, 1, 1, 0, 0, 0.0);
    while (time.ut <= stopTime.ut)
    {
        astro_spherical_t sphere = Astronomy_EclipticGeoMoon(time);
        if (sphere.status != ASTRO_SUCCESS)
            FAIL("GeoMoonPerformance: EclipticGeoMoon returned error %d.\n", sphere.status);
        time = Astronomy_AddDays(time, 0.01);
        ++count;
    }
    printf("GeoMoonPerformance: PASS (called %d times)\n", count);
    error = 0;
fail:
    return error;
}


/*-----------------------------------------------------------------------------------------------------------*/

static int NutationPerformance(void)
{
    /* Call SiderealTime for many different times to gauge speed of iau2000b nutation model. */

    int count = 0;
    astro_time_t time = Astronomy_MakeTime(1800, 1, 1, 0, 0, 0.0);
    astro_time_t stopTime = Astronomy_MakeTime(2200, 1, 1, 0, 0, 0.0);
    while (time.ut <= stopTime.ut)
    {
        Astronomy_SiderealTime(&time);
        time = Astronomy_AddDays(time, 0.01);
        ++count;
    }
    printf("NutationPerformance: PASS (called %d times)\n", count);
    return 0;
}

/*-----------------------------------------------------------------------------------------------------------*/


static int MoonNodes(void)
{
    const char *filename = "moon_nodes/moon_nodes.txt";
    int error;
    int lnum, nscanned;
    char kind, prev_kind;
    astro_time_t time;
    double ra, dec;
    FILE *infile;
    char line[100];
    char date[20];
    astro_spherical_t ecl;
    astro_vector_t vec_eqj, vec_eqd, vec_test;
    double diff_lat, max_lat = 0.0;
    double max_node_lat = 0.0;
    double diff_angle, max_angle = 0.0;
    double diff_minutes, max_minutes = 0.0;
    double dt, min_dt = 1.0e+99, max_dt = 0.0;
    astro_rotation_t rot;
    astro_spherical_t sphere;
    astro_angle_result_t result;
    astro_node_event_t node;

    memset(&node, 0, sizeof(node));
    node.status = ASTRO_NOT_INITIALIZED;

    infile = fopen(filename, "rt");
    if (infile == NULL)
        FAIL("MoonNodes: Cannot open input file: %s\n", filename);

    lnum = 0;
    prev_kind = '?';
    while (ReadLine(line, sizeof(line), infile, filename, lnum))
    {
        ++lnum;

        /* Parse the line from the test data file. */
        /* A 2001-01-09T13:53Z    7.1233   22.5350 */
        /* D 2001-01-22T22:22Z   19.1250  -21.4683 */

        if (strlen(line) < 40)
            FAIL("C MoonNodes(%s line %d): line is too short\n", filename, lnum);

        nscanned = sscanf(line, "%c %20s %lf %lf", &kind, date, &ra, &dec);
        if (nscanned != 4)
            FAIL("C MoonNodes(%s line %d): syntax error\n", filename, lnum);

        if (kind != 'A' && kind != 'D')
            FAIL("C MoonNodes(%s line %d): invalid kind character.\n", filename, lnum);

        if (kind == prev_kind)
            FAIL("C MoonNodes(%s line %d): duplicate ascending/descending kind.\n", filename, lnum);

        if (ParseDate(date, &time))
            FAIL("C MoonNodes(%s line %d): invalid date/time.\n", filename, lnum);

        if (!isfinite(ra) || ra < 0.0 || ra > 24.0)
            FAIL("C MoonNodes(%s line %d): invalid right ascension.\n", filename, lnum);

        if (!isfinite(dec) || dec < -90.0 || dec > +90.0)
            FAIL("C MoonNodes(%s line %d): invalid declination.\n", filename, lnum);

        /* Convert RA/DEC to a vector. */
        sphere.status = ASTRO_SUCCESS;
        sphere.lat = dec;
        sphere.lon = 15.0 * ra;
        sphere.dist = 1.0;
        vec_test = Astronomy_VectorFromSphere(sphere, time);
        CHECK_STATUS(vec_test);

        /* Calculate the Moon's ecliptic angles. Verify latitude is very close to zero. */
        ecl = Astronomy_EclipticGeoMoon(time);
        CHECK_STATUS(ecl);
        diff_lat = 60.0 * ABS(ecl.lat);  /* calculate arcminute latitude error */
        V(ecl.lon);
        V(ecl.dist);

        if (diff_lat > max_lat)
            max_lat = diff_lat;

        /* Calculate EQD coordinates. Verify against input file. */
        vec_eqj = Astronomy_GeoMoon(time);
        CHECK_STATUS(vec_eqj);

        rot = Astronomy_Rotation_EQJ_EQD(&time);
        CHECK_STATUS(rot);
        vec_eqd = Astronomy_RotateVector(rot, vec_eqj);
        CHECK_STATUS(vec_eqd);

        /* Measure the error angle between the correct vector and the calculated vector. */
        result = Astronomy_AngleBetween(vec_test, vec_eqd);
        CHECK_STATUS(result);
        diff_angle = 60.0 * ABS(result.angle);
        if (diff_angle > max_angle)
            max_angle = diff_angle;
        if (diff_angle > 1.54)
            FAIL("C MoonNodes(%s line %d): EXCESSIVE equatorial error = %0.3lf arcmin\n", filename, lnum, diff_angle);

        /* Test the Astronomy Engine moon node searcher. */
        if (lnum == 1)
        {
            /* The very first time, so search for the first node in the series. */
            /* Back up a few days to make sure we really are finding it ourselves. */
            astro_time_t earlier = Astronomy_AddDays(time, -6.5472);    /* pick a weird amount of time */
            node = Astronomy_SearchMoonNode(earlier);
        }
        else
        {
            double prev_tt = node.time.tt;

            /* Use the previous node to find the next node. */
            node = Astronomy_NextMoonNode(node);

            /* Find the range of time intervals between consecutive nodes. */
            dt = node.time.tt - prev_tt;
            if (dt < min_dt)
                min_dt = dt;
            if (dt > max_dt)
                max_dt = dt;
        }

        if (node.status != ASTRO_SUCCESS)
            FAIL("C MoonNodes(%s line %d): error %d returned by node search\n", filename, lnum, node.status);

        /* Verify the ecliptic latitude is very close to zero at the alleged node. */
        ecl = Astronomy_EclipticGeoMoon(node.time);
        CHECK_STATUS(ecl);
        diff_lat = 60.0 * ABS(ecl.lat);
        if (diff_lat > 8.06e-4)
            FAIL("C MoonNodes(%s line %d): found node has excessive latitude = %0.4le arcmin\n", filename, lnum, diff_lat);
        if (diff_lat > max_node_lat)
            max_node_lat = diff_lat;

        /* Verify the time agrees with Espenak's time to within a few minutes. */
        diff_minutes = (24.0 * 60.0) * ABS(node.time.tt - time.tt);
        if (diff_minutes > max_minutes)
            max_minutes = diff_minutes;

        /* Verify the kind of node matches what Espenak says (ascending or descending). */
        if (kind == 'A' && node.kind != ASCENDING_NODE)
            FAIL("C MoonNodes(%s line %d): did not find ascending node as expected.\n", filename, lnum);

        if (kind == 'D' && node.kind != DESCENDING_NODE)
            FAIL("C MoonNodes(%s line %d): did not find descending node as expected.\n", filename, lnum);

        /* Prepare for the next iteration. */
        prev_kind = kind;
    }

    if (max_lat > 0.183)
        FFAIL("EXCESSIVE ecliptic latitude error = %0.3lf arcmin\n", max_lat);

    if (max_minutes > 3.681)
        FFAIL("EXCESSIVE time prediction error = %0.3lf minutes\n", max_minutes);

    FDEBUG("min_dt = %0.3lf days, max_dt = %0.3lf days.\n", min_dt, max_dt);
    FPASSA("(%d nodes, max lat error = %0.3lf arcmin, max equ error = %0.3lf arcmin, max time error = %0.3lf minutes, max node lat = %0.3le arcmin)\n", lnum, max_lat, max_angle, max_minutes, max_node_lat);
fail:
    if (infile != NULL) fclose(infile);
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int SiderealTimeTest(void)
{
    int error;
    astro_time_t time;
    double gast, diff;
    const double correct = 9.3983699280076483;

    time = Astronomy_MakeTime(2022, 3, 15, 21, 50, 0.0);
    gast = Astronomy_SiderealTime(&time);
    diff = ABS(gast - correct);
    printf("C %s: gast=%0.15f, correct=%0.15lf, diff=%0.3le hours.\n", __func__, gast, correct, diff);
    if (diff > 1.0e-15)
        FFAIL("EXCESSIVE ERROR\n");

    FPASS();
fail:
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static double ArcminPosError(astro_state_vector_t correct, astro_state_vector_t calc)
{
    double dx = V(calc.x - correct.x);
    double dy = V(calc.y - correct.y);
    double dz = V(calc.z - correct.z);
    double diff_squared = dx*dx + dy*dy + dz*dz;
    double mag_squared = correct.x*correct.x + correct.y*correct.y + correct.z*correct.z;
    double radians = V(sqrt(diff_squared / mag_squared));
    double arcmin = (RAD2DEG * 60.0) * radians;
    return arcmin;
}

static double ArcminVelError(astro_state_vector_t correct, astro_state_vector_t calc)
{
    double dx = V(calc.vx - correct.vx);
    double dy = V(calc.vy - correct.vy);
    double dz = V(calc.vz - correct.vz);
    double diff_squared = dx*dx + dy*dy + dz*dz;
    double mag_squared = correct.vx*correct.vx + correct.vy*correct.vy + correct.vz*correct.vz;
    double radians = V(sqrt(diff_squared / mag_squared));
    double arcmin = (RAD2DEG * 60.0) * radians;
    return arcmin;
}


static double SSB_ArcminPosError(astro_state_vector_t correct, astro_state_vector_t calc)
{
    /* Scale the SSB based on 1 AU, not on its absolute magnitude, which can become very close to zero. */
    double dx = V(calc.x - correct.x);
    double dy = V(calc.y - correct.y);
    double dz = V(calc.z - correct.z);
    double diff_squared = dx*dx + dy*dy + dz*dz;
    double radians = V(sqrt(diff_squared));
    double arcmin = (RAD2DEG * 60.0) * radians;
    return arcmin;
}


static int GravSimFile(
    const char *filename,
    astro_body_t originBody,
    int nsteps,
    double *r_score,
    double *v_score,
    double r_thresh,
    double v_thresh)
{
    int error;
    int i, k;
    double rdiff, vdiff, tdiff;
    double max_rdiff = 0.0, max_vdiff = 0.0;
    astro_grav_sim_t *sim = NULL;
    astro_state_vector_t state;
    astro_status_t status;
    state_vector_batch_t batch = EmptyStateVectorBatch();
    astro_time_t time, checkTime;
    astro_body_t checkOrigin;

    if (nsteps < 1)
        FAIL("C GravSimFile(%s): invalid nsteps=%d\n", filename, nsteps);

    CHECK(LoadStateVectors(&batch, filename));

    if (batch.length <= 0)
        FAIL("C GravSimFile(%s): batch.length = %d is invalid.\n", filename, batch.length);

    /* Use the state vector and time in the batch to initialize the gravity simulator. */
    state = batch.array[0];

    status = Astronomy_GravSimInit(&sim, originBody, state.t, 1, &state);
    if (status != ASTRO_SUCCESS)
        FAIL("C GravSimFile(%s): Astronomy_GravSimInit returned error %d\n", filename, status);

    i = Astronomy_GravSimNumBodies(sim);
    if (i != 1)
        FAIL("C GravSimFile(%s): expected 1 body, found %d\n", filename, i);

    checkOrigin = Astronomy_GravSimOrigin(sim);
    if (checkOrigin != originBody)
        FAIL("C GravSimFile(%s): expected body %d, found %d\n", filename, originBody, checkOrigin);

    time = state.t;
    for (i = 1; i < batch.length; ++i)
    {
        /* Split each entry of the input batch into `nsteps` simulation steps. */
        double tt1 = batch.array[i-1].t.tt;
        double tt2 = batch.array[i].t.tt;
        double dt = (tt2 - tt1) / nsteps;
        for (k = 1; k <= nsteps; ++k)
        {
            time = Astronomy_TerrestrialTime(tt1 + k*dt);
            status = Astronomy_GravSimUpdate(sim, time, 1, &state);
            if (status != ASTRO_SUCCESS)
                FAIL("C GravSimFile(%s : i=%d): Astronomy_GravSimInit returned error %d\n", filename, i, status);

            checkTime = Astronomy_GravSimTime(sim);
            if (checkTime.tt != time.tt)
                FAIL("C GravSimFile(%s): Expected tt=%0.16lf, found tt=%0.16lf", filename, time.tt, checkTime.tt);
        }

        tdiff = SECONDS_PER_DAY * ABS(time.tt - batch.array[i].t.tt);
        if (tdiff > 1.0e-15)
            FAIL("C GravSimFile(%s): terrestrial time error = %0.16lf seconds.\n", filename, tdiff);

        /* Compare the simulated state with the reference state. */
        rdiff = ArcminPosError(batch.array[i], state);
        if (rdiff > max_rdiff)
            max_rdiff = rdiff;

        if (rdiff > r_thresh)
            FAIL("C GravSimFile(%s : i=%d): EXCESSIVE position error = %0.4lf arcmin\n", filename, i, rdiff);

        vdiff = ArcminVelError(batch.array[i], state);
        if (vdiff > max_vdiff)
            max_vdiff = vdiff;

        if (vdiff > v_thresh)
            FAIL("C GravSimFile(%s : i=%d): EXCESSIVE velocity error = %0.4lf arcmin\n", filename, i, vdiff);
    }

    if (max_rdiff > *r_score)
        *r_score = max_rdiff;

    if (max_vdiff > *v_score)
        *v_score = max_vdiff;

    DEBUG("C GravSimFile(%-22s): PASS (count = %d, pos error = %7.4lf arcmin, vel error = %7.4lf arcmin)\n", filename, batch.length, max_rdiff, max_vdiff);
    error = 0;
fail:
    Astronomy_GravSimFree(sim);
    FreeStateVectorBatch(&batch);
    return error;
}


static int GravSimEmpty(
    const char *filename,
    astro_body_t origin,
    astro_body_t body,
    double limit_rdiff,
    double limit_vdiff)
{
    int error;
    int i;
    astro_body_t checkOrigin;
    astro_status_t status;
    astro_grav_sim_t *sim = NULL;
    state_vector_batch_t batch = EmptyStateVectorBatch();
    astro_state_vector_t calc;
    double rdiff, vdiff;
    double max_rdiff = 0.0;
    double max_vdiff = 0.0;

    CHECK(LoadStateVectors(&batch, filename));
    if (batch.length < 1)
        FAIL("C GravSimEmpty(%s): invalid number of entries = %d\n", filename, batch.length);

    /* Verify that we can create an empty simulator, and that we can calculate the SSB from it. */
    status = Astronomy_GravSimInit(&sim, origin, batch.array[0].t, 0, NULL);
    if (status != ASTRO_SUCCESS)
        FAIL("C GravSimEmpty(%s): Astronomy_GravSimInit returned %d\n", filename, status);

    i = Astronomy_GravSimNumBodies(sim);
    if (i != 0)
        FAIL("C GravSimEmpty(%s): expected 0 bodies, found %d\n", filename, i);

    checkOrigin = Astronomy_GravSimOrigin(sim);
    if (checkOrigin != origin)
        FAIL("C GravSimEmpty(%s): expected body %d, found %d\n", filename, origin, checkOrigin);

    for (i = 0; i < batch.length; ++i)
    {
        status = Astronomy_GravSimUpdate(sim, batch.array[i].t, 0, NULL);
        if (status != ASTRO_SUCCESS)
            FAIL("C GravSimEmpty(%s): i=%d, Astronomy_GravSimUpdate returned %d\n", filename, i, status);

        calc = Astronomy_GravSimBodyState(sim, body);
        CHECK_STATUS(calc);

        if (origin==BODY_SSB && body==BODY_SUN)
            rdiff = SSB_ArcminPosError(batch.array[i], calc);
        else
            rdiff = ArcminPosError(batch.array[i], calc);

        if (rdiff > limit_rdiff)
            FAIL("C GravSimEmpty(%s): excessive position error = %0.6lf arcmin\n", filename, rdiff);

        if (rdiff > max_rdiff)
            max_rdiff = rdiff;

        vdiff = ArcminVelError(batch.array[i], calc);
        if (vdiff > max_vdiff)
            max_vdiff = vdiff;

        if (vdiff > limit_vdiff)
            FAIL("C GravSimEmpty(%s): excessive velocity error = %0.6lf arcmin\n", filename, vdiff);
    }

    DEBUG("C GravSimEmpty(%-22s): PASS - %d cases, pos error = %0.4lf arcmin, vel error = %0.4lf arcmin\n", filename, batch.length, max_rdiff, max_vdiff);
    error = 0;
fail:
    Astronomy_GravSimFree(sim);
    return error;
}


static int GravitySimulatorTest(void)
{
    int error;
    const int nsteps = 20;
    double rscore = 0.0;
    double vscore = 0.0;

    CHECK(GravSimEmpty("barystate/Sun.txt",      BODY_SSB, BODY_SUN,      0.0269, 1.9635));
    CHECK(GravSimEmpty("barystate/Mercury.txt",  BODY_SSB, BODY_MERCURY,  0.5725, 0.9332));
    CHECK(GravSimEmpty("barystate/Venus.txt",    BODY_SSB, BODY_VENUS,    0.1433, 0.1458));
    CHECK(GravSimEmpty("barystate/Earth.txt",    BODY_SSB, BODY_EARTH,    0.0651, 0.2098));
    CHECK(GravSimEmpty("barystate/Mars.txt",     BODY_SSB, BODY_MARS,     0.1150, 0.1896));
    CHECK(GravSimEmpty("barystate/Jupiter.txt",  BODY_SSB, BODY_JUPITER,  0.2546, 0.8831));
    CHECK(GravSimEmpty("barystate/Saturn.txt",   BODY_SSB, BODY_SATURN,   0.3660, 1.0818));
    CHECK(GravSimEmpty("barystate/Uranus.txt",   BODY_SSB, BODY_URANUS,   0.3107, 0.9321));
    CHECK(GravSimEmpty("barystate/Neptune.txt",  BODY_SSB, BODY_NEPTUNE,  0.3382, 1.5586));

    CHECK(GravSimEmpty("heliostate/Mercury.txt", BODY_SUN, BODY_MERCURY,  0.5088, 0.9473));
    CHECK(GravSimEmpty("heliostate/Venus.txt",   BODY_SUN, BODY_VENUS,    0.1214, 0.1543));
    CHECK(GravSimEmpty("heliostate/Earth.txt",   BODY_SUN, BODY_EARTH,    0.0508, 0.2099));
    CHECK(GravSimEmpty("heliostate/Mars.txt",    BODY_SUN, BODY_MARS,     0.1085, 0.1927));
    CHECK(GravSimEmpty("heliostate/Jupiter.txt", BODY_SUN, BODY_JUPITER,  0.2564, 0.8805));
    CHECK(GravSimEmpty("heliostate/Saturn.txt",  BODY_SUN, BODY_SATURN,   0.3664, 1.0826));
    CHECK(GravSimEmpty("heliostate/Uranus.txt",  BODY_SUN, BODY_URANUS,   0.3106, 0.9322));
    CHECK(GravSimEmpty("heliostate/Neptune.txt", BODY_SUN, BODY_NEPTUNE,  0.3381, 1.5584));

    DEBUG("\n");

    CHECK(GravSimFile("barystate/Ceres.txt",    BODY_SSB,   nsteps, &rscore, &vscore, 0.6640, 0.6226));
    CHECK(GravSimFile("barystate/Pallas.txt",   BODY_SSB,   nsteps, &rscore, &vscore, 0.4687, 0.3474));
    CHECK(GravSimFile("barystate/Vesta.txt",    BODY_SSB,   nsteps, &rscore, &vscore, 0.5806, 0.5462));
    CHECK(GravSimFile("barystate/Juno.txt",     BODY_SSB,   nsteps, &rscore, &vscore, 0.6760, 0.5750));
    CHECK(GravSimFile("barystate/Bennu.txt",    BODY_SSB,   nsteps, &rscore, &vscore, 3.7444, 2.6581));
    CHECK(GravSimFile("barystate/Halley.txt",   BODY_SSB,   nsteps, &rscore, &vscore, 0.0539, 0.0825));

    CHECK(GravSimFile("heliostate/Ceres.txt",   BODY_SUN,   nsteps, &rscore, &vscore, 0.0445, 0.0355));
    CHECK(GravSimFile("heliostate/Pallas.txt",  BODY_SUN,   nsteps, &rscore, &vscore, 0.1062, 0.0854));
    CHECK(GravSimFile("heliostate/Vesta.txt",   BODY_SUN,   nsteps, &rscore, &vscore, 0.1432, 0.1308));
    CHECK(GravSimFile("heliostate/Juno.txt",    BODY_SUN,   nsteps, &rscore, &vscore, 0.1554, 0.1328));

    CHECK(GravSimFile("geostate/Ceres.txt",     BODY_EARTH, nsteps, &rscore, &vscore, 6.5689, 6.4797));
    CHECK(GravSimFile("geostate/Pallas.txt",    BODY_EARTH, nsteps, &rscore, &vscore, 9.3288, 7.3533));
    CHECK(GravSimFile("geostate/Vesta.txt",     BODY_EARTH, nsteps, &rscore, &vscore, 3.2980, 3.8863));
    CHECK(GravSimFile("geostate/Juno.txt",      BODY_EARTH, nsteps, &rscore, &vscore, 6.0962, 7.7147));

    FPASSA("(pos score = %0.4lf arcmin, vel score = %0.4lf arcmin)\n", rscore, vscore);
fail:
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int CheckDecemberSolstice(int year, const char *expected)
{
    int error = 1;
    char actual[TIME_TEXT_BYTES];
    astro_seasons_t si = Astronomy_Seasons(year);
    astro_status_t status = Astronomy_FormatTime(si.dec_solstice, TIME_FORMAT_MILLI, actual, sizeof(actual));
    if (status != ASTRO_SUCCESS)
        FFAIL("FAIL: status=%d for year %d.\n", status, year);
    if (strcmp(actual, expected))
        FFAIL("FAIL: year %d, expected [%s], actual [%s]\n", year, expected, actual);
    error = 0;
fail:
    return error;
}

static int DatesIssue250(void)
{
    /* Make sure we can handle dates outside the range supported by System.DateTime. */
    /* https://github.com/cosinekitty/astronomy/issues/250 */
    int error;
    CHECK(CheckDecemberSolstice( 2022, "2022-12-21T21:47:54.455Z"));
    CHECK(CheckDecemberSolstice(-2300, "-002300-12-19T16:22:27.929Z"));
    CHECK(CheckDecemberSolstice(12345, "+012345-12-11T13:30:10.276Z"));
    FPASS();
fail:
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int StarRiseSetCulmCase(
    const char *starName,
    double ra,
    double dec,
    double distLy,
    astro_observer_t observer,
    int year,
    int month,
    int day,
    int riseHour,
    int riseMinute,
    int culmHour,
    int culmMinute,
    int setHour,
    int setMinute)
{
    int error;
    astro_status_t status;
    astro_time_t searchTime, expectedRiseTime, expectedSetTime, expectedCulmTime;
    astro_search_result_t rise, set;
    astro_hour_angle_t culm;
    double rdiff, cdiff, sdiff;

    /* Calculate expected event times. */

    expectedRiseTime = Astronomy_MakeTime(year, month, day, riseHour, riseMinute, 0.0);
    expectedCulmTime = Astronomy_MakeTime(year, month, day, culmHour, culmMinute, 0.0);
    expectedSetTime  = Astronomy_MakeTime(year, month, day, setHour,  setMinute,  0.0);

    /* Define a custom star object. */

    status = Astronomy_DefineStar(BODY_STAR1, ra, dec, distLy);
    if (status != ASTRO_SUCCESS)
        FAIL("StarRiseSetCulm(%s): Astronomy_DefineStar returned %d.\n", starName, status);

    /* Use Astronomy Engine to search for event times. */

    searchTime = Astronomy_MakeTime(year, month, day, 0, 0, 0.0);

    rise = Astronomy_SearchRiseSet(BODY_STAR1, observer, DIRECTION_RISE, searchTime, 1.0);
    CHECK_STATUS(rise);

    culm = Astronomy_SearchHourAngleEx(BODY_STAR1, observer, 0.0, searchTime, +1);
    CHECK_STATUS(culm);

    set = Astronomy_SearchRiseSet(BODY_STAR1, observer, DIRECTION_SET, searchTime, 1.0);
    CHECK_STATUS(set);

    /* Compare expected times with calculated times. */

    rdiff = MINUTES_PER_DAY * ABS(expectedRiseTime.ut - rise.time.ut);
    cdiff = MINUTES_PER_DAY * ABS(expectedCulmTime.ut - culm.time.ut);
    sdiff = MINUTES_PER_DAY * ABS(expectedSetTime.ut  - set.time.ut);

    DEBUG("C StarRiseSetCulmCase(%s): minutes rdiff = %0.4lf, cdiff = %0.4lf, sdiff = %0.4lf\n", starName, rdiff, cdiff, sdiff);

    if (rdiff > 0.5) FAIL("C StarRiseSetCulmCase(%s): exccessive rise time error = %0.4lf minutes.\n", starName, rdiff);
    if (cdiff > 0.5) FAIL("C StarRiseSetCulmCase(%s): exccessive culm time error = %0.4lf minutes.\n", starName, cdiff);
    if (sdiff > 0.5) FAIL("C StarRiseSetCulmCase(%s): exccessive set time error = %0.4lf minutes.\n", starName, sdiff);

    error = 0;
fail:
    return error;
}

static int StarRiseSetCulm(void)
{
    int error;
    astro_observer_t observer = Astronomy_MakeObserver(+25.77, -80.19, 0.0);

    CHECK(StarRiseSetCulmCase("Sirius",   6.7525, -16.7183,   8.6, observer, 2022, 11, 21,  2, 37,  8,  6, 13, 34));
    CHECK(StarRiseSetCulmCase("Sirius",   6.7525, -16.7183,   8.6, observer, 2022, 11, 25,  2, 22,  7, 50, 13, 18));
    CHECK(StarRiseSetCulmCase("Canopus",  6.3992, -52.6956, 310.0, observer, 2022, 11, 21,  4, 17,  7, 44, 11, 11));
    CHECK(StarRiseSetCulmCase("Canopus",  6.3992, -52.6956, 310.0, observer, 2022, 11, 25,  4,  1,  7, 28, 10, 56));

    FPASS();
fail:
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/

static int HourAngleCase(int year, int month, int day, double latitude, double longitude, double hourAngle, double *maxdiff)
{
    int error;
    astro_hour_angle_t search;
    astro_observer_t observer;
    astro_time_t startTime;
    astro_func_result_t ha;
    double diff;
    const double threshold = 0.1 / 3600.0;  /* Astronomy_SearchHourAngleEx() accuracy: 0.1 seconds converted to hours */

    observer = Astronomy_MakeObserver(latitude, longitude, 0.0);
    startTime = Astronomy_MakeTime(year, month, day, 0, 0, 0.0);

    search = Astronomy_SearchHourAngleEx(BODY_SUN, observer, hourAngle, startTime, +1);
    CHECK_STATUS(search);

    ha = Astronomy_HourAngle(BODY_SUN, &search.time, observer);
    CHECK_STATUS(ha);
    diff = ABS(ha.value - hourAngle);
    if (diff > 12.0)
        diff = 24.0 - diff;

    if (diff > *maxdiff)
        *maxdiff = diff;

    if (diff > threshold)
        FFAIL("EXCESSIVE ERROR = %0.6le, calc HA=%0.16lf for hourAngle=%0.1lf, longitude=%0.1lf\n", diff, ha.value, hourAngle, longitude);

    FDEBUG("Hour angle = %4.1lf, longitude = %6.1lf, diff = %7.4le hours\n", hourAngle, longitude, diff);
    error = 0;
fail:
    return error;
}


static int HourAngleTest(void)
{
    int error, hour, longitude;
    double maxdiff = 0.0;
    int cases = 0;

    for (longitude = -175; longitude <= 180; longitude += 5)
        for (hour = 0; hour < 24; ++hour, ++cases)
            CHECK(HourAngleCase(2023, 2, 11, 35.0, (double)longitude, (double)hour, &maxdiff));

    FPASSA("%d cases, maxdiff = %0.4le hours.\n", cases, maxdiff);
fail:
    return error;
}

/*-----------------------------------------------------------------------------------------------------------*/
