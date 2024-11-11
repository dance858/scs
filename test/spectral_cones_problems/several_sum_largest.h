#include "glbopts.h"
#include "linalg.h"
#include "minunit.h"
#include "problem_utils.h"
#include "rw.h"
#include "scs.h"
#include "scs_matrix.h"
#include "util.h"

// for SpectralSCS
static const char *several_sum_largest(void)
{
    ScsCone *k = (ScsCone *)scs_calloc(1, sizeof(ScsCone));
    ScsData *d = (ScsData *)scs_calloc(1, sizeof(ScsData));
    ScsSettings *stgs = (ScsSettings *)scs_calloc(1, sizeof(ScsSettings));
    ScsSolution *sol = (ScsSolution *)scs_calloc(1, sizeof(ScsSolution));
    ScsInfo info = {0};
    scs_int exitflag;
    scs_float perr, derr;
    scs_int success, read_status;
    const char *fail;

    /* data */
    scs_float Ax[] = {-1., -1., 1., -1., -1., 1., -1., -1., 1., -1., -1., 1., -1.,
                      -1., 1., -1., -1., 1., -1., -1., 1., -1., -1., 1., -1., -1.,
                      1., -1., -1., 1., -1., -1., 1., -1., -1., 1., -1., -1., 1.,
                      -1., -1., 1., -1., -1., 1., -1., -1., 1., -1., -1., 1., -1.,
                      -1., 1., -1., -1., 1., -1., -1., 1., -1., -1., 1., -1., -1.,
                      1., -1., -1., 1., -1., -1., 1., -1., -1., 1., -1., -1., 1.,
                      -1., -1., 1., -1., -1., 1., -1., -1., 1., -1., -1., 1., -1.,
                      -1., 1., -1., -1., 1., -1., -1., 1., -1., -1., 1., -1., -1.,
                      1., -1., -1., 1., -1., -1., 1., -1., -1., 1., -1., -1., 1.,
                      -1., -1., 1., -1., -1.};
    scs_int Ai[] = {1, 822, 0, 2, 823, 0, 42, 863, 0, 81, 902,
                    0, 119, 940, 0, 156, 977, 0, 192, 1013, 0, 227,
                    1048, 0, 261, 1082, 0, 294, 1115, 0, 326, 1147, 0,
                    357, 1178, 0, 387, 1208, 0, 416, 1237, 0, 444, 1265,
                    0, 471, 1292, 0, 497, 1318, 0, 522, 1343, 0, 546,
                    1367, 0, 569, 1390, 0, 591, 1412, 0, 612, 1433, 0,
                    632, 1453, 0, 651, 1472, 0, 669, 1490, 0, 686, 1507,
                    0, 702, 1523, 0, 717, 1538, 0, 731, 1552, 0, 744,
                    1565, 0, 756, 1577, 0, 767, 1588, 0, 777, 1598, 0,
                    786, 1607, 0, 794, 1615, 0, 801, 1622, 0, 807, 1628,
                    0, 812, 1633, 0, 816, 1637, 0, 819, 1640, 0, 821,
                    1642};
    scs_int Ap[] = {0, 1, 2, 5, 8, 11, 14, 17, 20, 23, 26, 29, 32,
                    35, 38, 41, 44, 47, 50, 53, 56, 59, 62, 65, 68, 71,
                    74, 77, 80, 83, 86, 89, 92, 95, 98, 101, 104, 107, 110,
                    113, 116, 119, 122};

    scs_float b[] = {0., 0., -1., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., 1.41421356, -0.,
                     -0., -0., -1., -0., 1.41421356,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -1., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., 1.41421356, -0., -6.,
                     -0., -0., -0., 1.41421356, -0.,
                     -0., -0., -0., -0., -0.,
                     1.41421356, 1.41421356, -0., -0., -0.,
                     -0., -0., -0., 1.41421356, -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., 1.41421356, -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -2., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., 1.41421356, -0., -0., 1.41421356,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -2., -0., 1.41421356,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., 1.41421356, -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -3., -0., -0.,
                     -0., -0., -0., -0., 1.41421356,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., 1.41421356, 1.41421356, -0.,
                     -0., -6., -0., -0., -0.,
                     1.41421356, -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., 1.41421356, -0., -0., -0.,
                     -0., -0., -0., 1.41421356, -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., 1.41421356, -0., -1.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., 1.41421356, -0., -0., -0.,
                     -0., -1., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     1.41421356, -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -3., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., 1.41421356, -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., 1.41421356,
                     -0., 1.41421356, -2., 1.41421356, -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -3., 1.41421356, -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., 1.41421356, -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -4.,
                     1.41421356, -0., -0., 1.41421356, -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -3., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., 1.41421356, -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -5., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., 1.41421356, -0.,
                     -0., 1.41421356, -0., -0., -0.,
                     -0., -0., -0., 1.41421356, -0.,
                     1.41421356, -0., -2., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., 1.41421356, -0., 1.41421356, -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -3., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., 1.41421356, -0., 1.41421356,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -2., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     1.41421356, -0., -0., 1.41421356, -0.,
                     -0., -0., -2., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     1.41421356, -0., -0., -0., -0.,
                     -0., -0., -1., -0., -0.,
                     -0., -0., 1.41421356, -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -2., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -2.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., 1.41421356,
                     -0., -0., 1.41421356, -0., -0.,
                     -0., -3., -0., -0., -0.,
                     1.41421356, 1.41421356, -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -1., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -5., -0., -0.,
                     -0., -0., -0., 1.41421356, -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -2., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -4.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., 1.41421356, -0., 1.41421356,
                     -0., -4., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -2., -0., -0.,
                     -0., -0., 1.41421356, 1.41421356, -0.,
                     -0., -0., -1., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -3., -0., 1.41421356, -0.,
                     -0., -0., -0., 1.41421356, -4.,
                     -0., -0., -0., -0., -0.,
                     -0., -1., -0., -0., -0.,
                     -0., -0., -4., -0., -0.,
                     -0., 1.41421356, -7., -0., -0.,
                     -0., -2., -0., -0., -4.,
                     -0., -3., 0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -2., -0.,
                     -0., -0., -0., 1.41421356, -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., 1.41421356, -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -4., -0., -0.,
                     -0., -0., -0., -0., 1.41421356,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., 1.41421356, -0.,
                     1.41421356, 1.41421356, -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -3., -0., -0., -0., -0.,
                     -0., -0., -0., -0., 1.41421356,
                     -0., -0., -0., 1.41421356, -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., 1.41421356, -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -4., -0., -0.,
                     1.41421356, -0., 1.41421356, -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     1.41421356, -0., 1.41421356, -0., -0.,
                     -0., -0., -0., -3., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., 1.41421356, -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., 1.41421356, -0., 1.41421356,
                     -0., -0., -0., -1., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -2., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., 1.41421356, -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -6., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     1.41421356, 1.41421356, -0., -0., -0.,
                     -0., -0., -0., -0., 1.41421356,
                     -0., -0., -0., -0., -0.,
                     1.41421356, -0., -0., 1.41421356, -0.,
                     -0., 1.41421356, -5., -0., -0.,
                     -0., 1.41421356, -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     1.41421356, -0., 1.41421356, -0., -0.,
                     -0., -0., -0., -2., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., 1.41421356, -0.,
                     -0., 1.41421356, -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -2., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., 1.41421356,
                     -0., 1.41421356, -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -3., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., 1.41421356, -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., 1.41421356, -0.,
                     -0., -0., -0., -0., -0.,
                     -3., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., 1.41421356, -0., -0., -0.,
                     1.41421356, -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -5., -0., -0.,
                     -0., 1.41421356, -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., 1.41421356, -0., 1.41421356,
                     -0., -0., -0., 1.41421356, -0.,
                     -0., 1.41421356, -0., -1., -0.,
                     -0., -0., -0., -0., 1.41421356,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -5., -0.,
                     -0., -0., -0., -0., 1.41421356,
                     -0., -0., -0., 1.41421356, -0.,
                     -0., -0., -0., -0., 1.41421356,
                     -0., 1.41421356, -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -3., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -1., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -1., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., 1.41421356, -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -2., -0.,
                     -0., -0., -0., 1.41421356, -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -2., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., 1.41421356,
                     -0., -0., -0., -0., -0.,
                     -2., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -2., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -4., -0.,
                     -0., 1.41421356, -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., 1.41421356, -4., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -2., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -3., -0., -0., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -3., -0., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -1., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -0., -1., -0.,
                     -0., -0., -0., -0., -0.,
                     -0., -0., -4., 1.41421356, -0.,
                     -0., -0., -0., -0., -0.,
                     -5., -0., 1.41421356, -0., 1.41421356,
                     -0., -0., -6., -0., -0.,
                     -0., -0., 1.41421356, -2., -0.,
                     -0., -0., -0., -2., -0.,
                     -0., -0., -1., -0., -0.,
                     -1., -0., -3.};
    scs_float c[] = {1., 2., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0.,
                     0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0.,
                     0., 0., 0., 0., 0., 0., 0., 0.};

    scs_int m = 1643;
    scs_int n = 42;

    scs_int z = 1;
    scs_int l = 0;
    scs_int *q = SCS_NULL;
    scs_int qsize = 0;
    scs_int *s = SCS_NULL;
    scs_int ssize = 0;
    scs_int ep = 0;
    scs_int ed = 0;
    scs_float *p = SCS_NULL;
    scs_int psize = 0;
    scs_int *d_array = SCS_NULL;
    scs_int dsize = 0;
    scs_int *nuc_m = SCS_NULL;
    scs_int *nuc_n = SCS_NULL;
    scs_int nucsize = 0;
    scs_int *ell1 = SCS_NULL;
    scs_int ell1_size = 0;
    scs_int sl_n[] = {40, 40};
    scs_int sl_k[] = {4, 7};
    scs_int sl_size = 2;

    // computed using mosek (the input of Ax is truncated, and mosek solved
    // the problem with the non-truncated data)
    scs_float opt = -6.8681703775862095;
    /* end data */

    d->m = m;
    d->n = n;
    d->b = b;
    d->c = c;

    d->A = (ScsMatrix *)scs_calloc(1, sizeof(ScsMatrix));

    d->A->m = m;
    d->A->n = n;
    d->A->x = Ax;
    d->A->i = Ai;
    d->A->p = Ap;

    k->z = z;
    k->l = l;
    k->q = q;
    k->qsize = qsize;
    k->s = s;
    k->ssize = ssize;
    k->ep = ep;
    k->ed = ed;
    k->p = p;
    k->psize = psize;
    k->d = d_array;
    k->dsize = dsize;
    k->nuc_m = nuc_m;
    k->nuc_n = nuc_n;
    k->nucsize = nucsize;
    k->ell1 = ell1;
    k->ell1_size = ell1_size;
    k->sl_n = sl_n;
    k->sl_k = sl_k;
    k->sl_size = sl_size;

    scs_set_default_settings(stgs);
    stgs->eps_abs = 1e-7;
    stgs->eps_rel = 1e-7;
    stgs->eps_infeas = 1e-9;

    exitflag = scs(d, k, stgs, sol, &info);

    perr = SCS(dot)(d->c, sol->x, d->n) - opt;
    derr = -SCS(dot)(d->b, sol->y, d->m) - opt;

    scs_printf("primal obj error %4e\n", perr);
    scs_printf("dual obj error %4e\n", derr);

    success = ABS(perr) < 1e-4 && ABS(derr) < 1e-4 && exitflag == SCS_SOLVED;

    mu_assert("several_sum_largest: SCS failed to produce outputflag SCS_SOLVED",
              success);

    fail = verify_solution_correct(d, k, stgs, &info, sol, exitflag);
    if (fail)
        return fail;

    /* kill data */
    scs_free(d->A);
    scs_free(k);
    scs_free(stgs);
    scs_free(d);
    SCS(free_sol)
    (sol);

    return fail;
}
