#include "private.h"

/* do not use within pcg, reuses memory */
void SCS(accum_by_atrans)(const ScsMatrix *A, ScsLinSysWork *p,
                          const scs_float *x, scs_float *y) {
  scs_float *v_m = p->tmp_m;
  scs_float *v_n = p->r;
  cudaMemcpy(v_m, x, A->m * sizeof(scs_float), cudaMemcpyHostToDevice);
  cudaMemcpy(v_n, y, A->n * sizeof(scs_float), cudaMemcpyHostToDevice);

  cusparseDnVecSetValues(p->dn_vec_m, (void *) v_m);
  cusparseDnVecSetValues(p->dn_vec_n, (void *) v_n);
  SCS(accum_by_atrans_gpu)(
    p->Ag, p->dn_vec_m, p->dn_vec_n, p->cusparse_handle,
    &p->buffer_size, &p->buffer
  );

  cudaMemcpy(y, v_n, A->n * sizeof(scs_float), cudaMemcpyDeviceToHost);
}

/* do not use within pcg, reuses memory */
void SCS(accum_by_a)(const ScsMatrix *A, ScsLinSysWork *p, const scs_float *x,
                     scs_float *y) {
  scs_float *v_m = p->tmp_m;
  scs_float *v_n = p->r;
  cudaMemcpy(v_n, x, A->n * sizeof(scs_float), cudaMemcpyHostToDevice);
  cudaMemcpy(v_m, y, A->m * sizeof(scs_float), cudaMemcpyHostToDevice);

  cusparseDnVecSetValues(p->dn_vec_m, (void *) v_m);
  cusparseDnVecSetValues(p->dn_vec_n, (void *) v_n);
#if GPU_TRANSPOSE_MAT > 0
  SCS(accum_by_atrans_gpu)(
    p->Agt, p->dn_vec_n, p->dn_vec_m, p->cusparse_handle,
    &p->buffer_size, &p->buffer
  );
#else
  SCS(accum_by_a_gpu)(
    p->Ag, p->dn_vec_n, p->dn_vec_m, p->cusparse_handle,
    &p->buffer_size, &p->buffer
  );
#endif

  cudaMemcpy(y, v_m, A->m * sizeof(scs_float), cudaMemcpyDeviceToHost);
}

char *SCS(get_lin_sys_method)(const ScsMatrix *A, const ScsMatrix *P) {
  char *str = (char *)scs_malloc(sizeof(char) * 128);
  sprintf(str,
          "lin-sys:  sparse-indirect GPU\n\t  nnz(A): %li, nnz(P): %li\n",
          (long)A->p[A->n], P ? (long)P->p[P->n] : 0l);
  return str;
}

char *SCS(get_lin_sys_summary)(ScsLinSysWork *p, const ScsInfo *info) {
  char *str = (char *)scs_malloc(sizeof(char) * 128);
  sprintf(str, "lin-sys: avg cg its: %2.2f\n",
          (scs_float)p->tot_cg_its / (info->iter + 1));
  p->tot_cg_its = 0;
  return str;
}

void SCS(free_lin_sys_work)(ScsLinSysWork *p) {
  if (p) {
    cudaFree(p->p);
    cudaFree(p->r);
    cudaFree(p->Gp);
    cudaFree(p->bg);
    cudaFree(p->tmp_m);
    cudaFree(p->z);
    cudaFree(p->M);
    if (p->Pg) {
      SCS(free_gpu_matrix)(p->Pg);
      scs_free(p->Pg);
    }
    if (p->Ag) {
      SCS(free_gpu_matrix)(p->Ag);
      scs_free(p->Ag);
    }
    if (p->Agt) {
      SCS(free_gpu_matrix)(p->Agt);
      scs_free(p->Agt);
    }
    if (p->buffer != SCS_NULL) {
      cudaFree(p->buffer);
    }
    cusparseDestroyDnVec(p->dn_vec_m);
    cusparseDestroyDnVec(p->dn_vec_n);
    cusparseDestroyDnVec(p->dn_vec_n_p);
    cusparseDestroy(p->cusparse_handle);
    cublasDestroy(p->cublas_handle);
    /* Don't reset because it interferes with other GPU programs. */
    /* cudaDeviceReset(); */
    scs_free(p);
  }
}

/* z = M * z elementwise in place, assumes M, z on GPU */
static void scale_by_diag(cublasHandle_t cublas_handle, scs_float *M,
                          scs_float *z, scs_int n) {
  CUBLAS(tbmv)(cublas_handle, CUBLAS_FILL_MODE_LOWER, CUBLAS_OP_N,
               CUBLAS_DIAG_NON_UNIT, n, 0, M, 1, z, 1);
}

/* y = (rho_x * I + P + A' R A) x */
static void mat_vec(const ScsGpuMatrix *A, const ScsGpuMatrix *P,
                    ScsLinSysWork *p, const scs_float *x, scs_float *y) {
  /* x and y MUST already be loaded to GPU */
  scs_float *z= p->tmp_m; /* temp memory */
  cudaMemset(z, 0, A->m * sizeof(scs_float));
  cudaMemset(y, 0, A->n * sizeof(scs_float));
  if (P) {
    /* y = Px */
    cusparseDnVecSetValues(p->dn_vec_n_p, (void *) y);
    cusparseDnVecSetValues(p->dn_vec_n, (void *) x);

    SCS(accum_by_p_gpu)(
      P, p->dn_vec_n, p->dn_vec_n_p, p->cusparse_handle,
      &p->buffer_size, &p->buffer
    );
  }

  cusparseDnVecSetValues(p->dn_vec_m, (void *) z);
  cusparseDnVecSetValues(p->dn_vec_n, (void *) x);
  /* z = Ax */
#if GPU_TRANSPOSE_MAT > 0
  SCS(accum_by_atrans_gpu)(
    p->Agt, p->dn_vec_n, p->dn_vec_m, p->cusparse_handle,
    &p->buffer_size, &p->buffer
  );
#else
  SCS(accum_by_a_gpu)(
    A, p->dn_vec_n, p->dn_vec_m, p->cusparse_handle,
    &p->buffer_size, &p->buffer
  );
#endif

  /* z = R A x */
  scale_by_diag(p->cublas_handle, p->rho_y_vec_gpu, z, A->m);

  cusparseDnVecSetValues(p->dn_vec_m, (void *) z);
  cusparseDnVecSetValues(p->dn_vec_n, (void *) y);
  /* y += A'z => y = Px + A' R Ax */
  SCS(accum_by_atrans_gpu)(
    A, p->dn_vec_m, p->dn_vec_n, p->cusparse_handle,
    &p->buffer_size, &p->buffer
  );
  /* y += rho_x * x = rho_x * x + Px + A' R A x */
  CUBLAS(axpy)(p->cublas_handle, A->n, &(p->rho_x), x, 1, y, 1);
}


/* set M = inv ( diag ( rho_x * I + P + A' R A ) ) */
static void set_preconditioner(const ScsMatrix *A, const ScsMatrix *P,
                               ScsLinSysWork *p, scs_float * rho_y_vec) {
  scs_int i, k;
  scs_float *M = (scs_float *)scs_calloc(A->n, sizeof(scs_float));

#if VERBOSITY > 0
  scs_printf("getting pre-conditioner\n");
#endif

  for (i = 0; i < A->n; ++i) { /* cols */
    M[i] = p->rho_x;
    /* diag(A' R A) */
    for (k = A->p[i]; k < A->p[i + 1]; ++k) {
      /* A->i[k] is row of entry k with value A->x[k] */
      M[i] += rho_y_vec[A->i[k]] * A->x[k] * A->x[k];
    }
    if (P) {
      for (k = P->p[i]; k < P->p[i + 1]; k++) {
        /* diagonal element only */
        if (P->i[k] == i) { /* row == col */
          M[i] += P->x[k];
          break;
        }
      }
    }
    M[i] = 1. / M[i];
  }
  cudaMemcpy(p->M, M, A->n * sizeof(scs_float), cudaMemcpyHostToDevice);
  scs_free(M);
#if VERBOSITY > 0
  scs_printf("finished getting pre-conditioner\n");
#endif
}

/* P comes in upper triangular, expand to full */
static csc *fill_p_matrix(const csc *P) {
  scs_int i, j, k, kk;
  scs_int Pnzmax = 2 * P->p[P->n]; /* upper bound */
  csc * P_T = cs_spalloc(P->n, P->n, Pnzmax, 1, 1);
  kk = 0;
  for (j = 0; j < P->n; j++) { /* cols */
    for (k = P->p[j]; k < P->p[j + 1]; k++) {
      i = P->i[k]; /* row */
      if (i > j) { /* only upper triangular needed */
        break;
      }
      P_T->i[kk] = i;
      P_T->p[kk] = j;
      P_T->x[kk] = P->x[k];
      if (i != j) { /* not diagonal */
        P_T->i[kk+1] = j;
        P_T->p[kk+1] = i;
        P_T->x[kk+1] = P->x[k];
      }
      kk += 2;
    }
  }
  return cs_compress(P_T, SCS_NULL);
}


ScsLinSysWork *SCS(init_lin_sys_work)(const ScsMatrix *A, const ScsMatrix *P,
                                      scs_float *rho_y_vec, scs_float rho_x) {
  cudaError_t err;
  csc *P_full;
  ScsLinSysWork *p = (ScsLinSysWork *)scs_calloc(1, sizeof(ScsLinSysWork));
  ScsGpuMatrix *Ag = (ScsGpuMatrix *)scs_calloc(1, sizeof(ScsGpuMatrix));
  ScsGpuMatrix *Pg = SCS_NULL;

  /* Used for initializing dense vectors */
  scs_float *tmp_null_n = SCS_NULL;
  scs_float *tmp_null_m = SCS_NULL;

#if GPU_TRANSPOSE_MAT > 0
  size_t new_buffer_size = 0;
#endif

  p->rho_x = rho_x;
  p->cublas_handle = 0;
  p->cusparse_handle = 0;

  p->tot_cg_its = 0;

  p->buffer_size = 0;
  p->buffer = SCS_NULL;

  /* Get handle to the CUBLAS context */
  cublasCreate(&p->cublas_handle);

  /* Get handle to the CUSPARSE context */
  cusparseCreate(&p->cusparse_handle);

  Ag->n = A->n;
  Ag->m = A->m;
  Ag->nnz = A->p[A->n];
  Ag->descr = 0;
  cudaMalloc((void **)&Ag->i, (A->p[A->n]) * sizeof(scs_int));
  cudaMalloc((void **)&Ag->p, (A->n + 1) * sizeof(scs_int));
  cudaMalloc((void **)&Ag->x, (A->p[A->n]) * sizeof(scs_float));

  cudaMalloc((void **)&p->p, A->n * sizeof(scs_float));
  cudaMalloc((void **)&p->r, A->n * sizeof(scs_float));
  cudaMalloc((void **)&p->Gp, A->n * sizeof(scs_float));
  cudaMalloc((void **)&p->bg, (A->n + A->m) * sizeof(scs_float));
  cudaMalloc((void **)&p->tmp_m, A->m * sizeof(scs_float));
  cudaMalloc((void **)&p->z, A->n * sizeof(scs_float));
  cudaMalloc((void **)&p->M, A->n * sizeof(scs_float));
  cudaMalloc((void **)&p->rho_y_vec_gpu, A->m * sizeof(scs_float));

  cudaMemcpy(Ag->i, A->i, (A->p[A->n]) * sizeof(scs_int),
             cudaMemcpyHostToDevice);
  cudaMemcpy(Ag->p, A->p, (A->n + 1) * sizeof(scs_int),
             cudaMemcpyHostToDevice);
  cudaMemcpy(Ag->x, A->x, (A->p[A->n]) * sizeof(scs_float),
             cudaMemcpyHostToDevice);
  cudaMemcpy(p->rho_y_vec_gpu, rho_y_vec, A->m * sizeof(scs_float),
             cudaMemcpyHostToDevice);

  cusparseCreateCsr(&Ag->descr, Ag->n, Ag->m, Ag->nnz, Ag->p, Ag->i, Ag->x,
    SCS_CUSPARSE_INDEX, SCS_CUSPARSE_INDEX,
    CUSPARSE_INDEX_BASE_ZERO, SCS_CUDA_FLOAT);

  if (P) {
    P_full = fill_p_matrix(P);
    Pg->n = P->n;
    Pg->m = P->m;
    Pg->nnz = P->p[P->n];
    Pg->descr = 0;
    Pg = (ScsGpuMatrix *)scs_calloc(1, sizeof(ScsGpuMatrix));
    cudaMalloc((void **)&Pg->i, (P->p[P->n]) * sizeof(scs_int));
    cudaMalloc((void **)&Pg->p, (P->n + 1) * sizeof(scs_int));
    cudaMalloc((void **)&Pg->x, (P->p[P->n]) * sizeof(scs_float));

    cudaMemcpy(Pg->i, P->i, (P->p[P->n]) * sizeof(scs_int),
               cudaMemcpyHostToDevice);
    cudaMemcpy(Pg->p, P->p, (P->n + 1) * sizeof(scs_int),
               cudaMemcpyHostToDevice);
    cudaMemcpy(Pg->x, P->x, (P->p[P->n]) * sizeof(scs_float),
               cudaMemcpyHostToDevice);

    cusparseCreateCsr(&Pg->descr, Pg->n, Pg->m, Pg->nnz, Pg->p, Pg->i, Pg->x,
      SCS_CUSPARSE_INDEX, SCS_CUSPARSE_INDEX,
      CUSPARSE_INDEX_BASE_ZERO, SCS_CUDA_FLOAT);

    SCS(cs_spfree)(P_full);
  } else {
    Pg = SCS_NULL;
  }

  p->Ag = Ag;
  p->Pg = Pg;
  p->Agt = SCS_NULL;

  // XXX
  //cudaMalloc((void **)&tmp_null_n, A->n * sizeof(scs_float));
  //cudaMalloc((void **)&tmp_null_m, A->m * sizeof(scs_float));
  //cusparseCreateDnVec(&p->dn_vec_n, Ag->n, tmp_null_n, SCS_CUDA_FLOAT);
  //cusparseCreateDnVec(&p->dn_vec_m, Ag->m, tmp_null_m, SCS_CUDA_FLOAT);
  cusparseCreateDnVec(&p->dn_vec_n, Ag->n, SCS_NULL, SCS_CUDA_FLOAT);
  cusparseCreateDnVec(&p->dn_vec_n_p, Ag->n, SCS_NULL, SCS_CUDA_FLOAT);
  cusparseCreateDnVec(&p->dn_vec_m, Ag->m, SCS_NULL, SCS_CUDA_FLOAT);
  //cudaFree(tmp_null_n);
  //cudaFree(tmp_null_m);

  set_preconditioner(A, P, p, rho_y_vec);

#if GPU_TRANSPOSE_MAT > 0
  p->Agt = (ScsGpuMatrix *)scs_malloc(sizeof(ScsGpuMatrix));
  p->Agt->n = A->m;
  p->Agt->m = A->n;
  p->Agt->nnz = A->p[A->n];
  p->Agt->descr = 0;
  /* Matrix description */

  cudaMalloc((void **)&p->Agt->i, (A->p[A->n]) * sizeof(scs_int));
  cudaMalloc((void **)&p->Agt->p, (A->m + 1) * sizeof(scs_int));
  cudaMalloc((void **)&p->Agt->x, (A->p[A->n]) * sizeof(scs_float));
  /* transpose Ag into Agt for faster multiplies */
  /* TODO: memory intensive, could perform transpose in CPU and copy to GPU */
  cusparseCsr2cscEx2_bufferSize
  (p->cusparse_handle, A->n, A->m, A->p[A->n],
    Ag->x, Ag->p, Ag->i,
    p->Agt->x, p->Agt->p, p->Agt->i,
    SCS_CUDA_FLOAT, CUSPARSE_ACTION_NUMERIC,
    CUSPARSE_INDEX_BASE_ZERO, SCS_CSR2CSC_ALG,
    &new_buffer_size);

  if (new_buffer_size > p->buffer_size) {
    if (p->buffer != SCS_NULL) {
      cudaFree(p->buffer);
    }
    cudaMalloc(&p->buffer, new_buffer_size);
    p->buffer_size = new_buffer_size;
  }

  cusparseCsr2cscEx2
  (p->cusparse_handle, A->n, A->m, A->p[A->n],
    Ag->x, Ag->p, Ag->i,
    p->Agt->x, p->Agt->p, p->Agt->i,
    SCS_CUDA_FLOAT, CUSPARSE_ACTION_NUMERIC,
    CUSPARSE_INDEX_BASE_ZERO, SCS_CSR2CSC_ALG,
    p->buffer);

  cusparseCreateCsr
  (&p->Agt->descr, p->Agt->n, p->Agt->m, p->Agt->nnz,
    p->Agt->p, p->Agt->i, p->Agt->x,
    SCS_CUSPARSE_INDEX, SCS_CUSPARSE_INDEX,
    CUSPARSE_INDEX_BASE_ZERO, SCS_CUDA_FLOAT);
#endif

  err = cudaGetLastError();
  if (err != cudaSuccess) {
    printf("%s:%d:%s\nERROR_CUDA: %s\n", __FILE__, __LINE__, __func__,
           cudaGetErrorString(err));
    SCS(free_lin_sys_work)(p);
    return SCS_NULL;
  }
  return p;
}

/* solves (rho_x * I + P + A' R A)x = b, s warm start, solution stored in b */
/* on GPU */
static scs_int pcg(const ScsGpuMatrix *A, const ScsMatrix *P,
                   ScsLinSysWork *pr, const scs_float *s, scs_float *bg,
                   scs_int max_its, scs_float tol) {
  scs_int i, n = A->n;
  scs_float ztr, ztr_prev, alpha, ptGp, beta, nrm_r, neg_alpha;
  scs_float onef = 1.0, neg_onef = -1.0;
  scs_float *p = pr->p;   /* cg direction */
  scs_float *Gp = pr->Gp; /* updated CG direction */
  scs_float *r = pr->r;   /* cg residual */
  scs_float *z = pr->z;   /* preconditioned */
  cublasHandle_t cublas_handle = pr->cublas_handle;

  if (!s) {
    /* take s = 0 */
    /* r = b */
    cudaMemcpy(r, bg, n * sizeof(scs_float), cudaMemcpyDeviceToDevice);
    /* b = 0 */
    cudaMemset(bg, 0, n * sizeof(scs_float));
  } else {
    /* p contains bg temporarily */
    cudaMemcpy(p, bg, n * sizeof(scs_float), cudaMemcpyDeviceToDevice);
    /* bg = s */
    cudaMemcpy(bg, s, n * sizeof(scs_float), cudaMemcpyHostToDevice);
    /* r = Mat * s */
    mat_vec(A, P, pr, bg, r);
    /* r = Mat * s - b */
    CUBLAS(axpy)(cublas_handle, n, &neg_onef, p, 1, r, 1);
    /* r = b - Mat * s  */
    CUBLAS(scal)(cublas_handle, n, &neg_onef, r, 1);
  }

  CUBLAS(nrm2)(cublas_handle, n, r, 1, &nrm_r); // TODO XXX
  nrm_r = SQRTF(nrm_r);
  /* check to see if we need to run CG at all */
  if (nrm_r < MAX(tol, 1e-12)) {
    return 0;
  }

  /* z = M r */
  cudaMemcpy(z, r, n * sizeof(scs_float), cudaMemcpyDeviceToDevice);
  scale_by_diag(cublas_handle, pr->M, z, n);
  /* ztr = z'r */
  CUBLAS(dot)(cublas_handle, n, r, 1, z, 1, &ztr);
  /* p = z */
  cudaMemcpy(p, z, n * sizeof(scs_float), cudaMemcpyDeviceToDevice);

  for (i = 0; i < max_its; ++i) {
    /* Gp = Mat * p */
    mat_vec(A, P, pr, p, Gp);
    /* ptGp = p'Gp */
    CUBLAS(dot)(cublas_handle, n, p, 1, Gp, 1, &ptGp);
    /* alpha = z'r / p'G p */
    alpha = ztr / ptGp;
    neg_alpha = -alpha;
    /* b += alpha * p */
    CUBLAS(axpy)(cublas_handle, n, &alpha, p, 1, bg, 1);
    /* r -= alpha * G p */
    CUBLAS(axpy)(cublas_handle, n, &neg_alpha, Gp, 1, r, 1);

    CUBLAS(nrm2)(cublas_handle, n, r, 1, &nrm_r); // TODO XXX
    nrm_r = SQRTF(nrm_r);
    if (nrm_r < tol) {
      return i + 1;
    }
    /* z = M r */
    cudaMemcpy(z, r, n * sizeof(scs_float), cudaMemcpyDeviceToDevice);
    scale_by_diag(cublas_handle, pr->M, z, n);
    ztr_prev = ztr;
    /* ztr = z'r */
    CUBLAS(dot)(cublas_handle, n, r, 1, z, 1, &ztr);
    beta = ztr / ztr_prev;
    /* p = beta * p, where beta = ztr / ztr_prev */
    CUBLAS(scal)(cublas_handle, n, &beta, p, 1);
    /* p = z + beta * p */
    CUBLAS(axpy)(cublas_handle, n, &onef, z, 1, p, 1);
  }
  return i;
}

/* solves Mx = b, for x but stores result in b */
/* s contains warm-start (if available) */
/*
 * [x] = [rho_x I + P     A' ]^{-1} [rx]
 * [y]   [     A        -R^-1]      [ry]
 *
 * R = diag(rho_y_vec)
 *
 * becomes:
 *
 * x = (rho_x I + P + A' R A)^{-1} (rx + A' R ry)
 * y = R (Ax - ry)
 *
 */
scs_int SCS(solve_lin_sys)(const ScsMatrix *A, const ScsMatrix *P,
                           ScsLinSysWork *p, scs_float *b, const scs_float *s,
                           scs_float tol) {
  scs_int cg_its, max_iters;
  scs_float neg_onef = -1.0;

  /* these are on GPU */
  scs_float *bg = p->bg;
  scs_float *tmp_m = p->tmp_m;
  ScsGpuMatrix *Ag = p->Ag;
  ScsGpuMatrix *Pg = p->Pg;

  if (CG_NORM(b, A->n + A->m) <= 1e-12) {
    memset(b, 0, (A->n + A->m) * sizeof(scs_float));
    return 0;
  }

  /* bg = b = [rx; ry] */
  cudaMemcpy(bg, b, (Ag->n + Ag->m) * sizeof(scs_float), cudaMemcpyHostToDevice);
  /* tmp = ry */
  cudaMemcpy(tmp_m, &(b[Ag->n]), Ag->m * sizeof(scs_float),
             cudaMemcpyDeviceToDevice);
  /* tmp = R * tmp = R * ry */
  scale_by_diag(p->cublas_handle, p->rho_y_vec_gpu, tmp_m, p->Ag->m);

  cusparseDnVecSetValues(p->dn_vec_m, (void *) tmp_m); /* R * ry */
  cusparseDnVecSetValues(p->dn_vec_n, (void *) bg); /* rx */
  /* bg[:n] = rx + A' R ry */
  SCS(accum_by_atrans_gpu)(
    Ag, p->dn_vec_m, p->dn_vec_n, p->cusparse_handle,
    &p->buffer_size, &p->buffer
  );

  /* set max_iters to 10 * n (though in theory n is enough for any tol) */
  max_iters = 10 * Ag->n;

  /* solves (rho_x I + P + A' R A)x = bg, s warm start, solution stored in bg */
  cg_its = pcg(Ag, Pg, p, s, bg, max_iters, tol); /* bg[:n] = x */

  /* bg[n:] = -ry */
  CUBLAS(scal)(p->cublas_handle, Ag->m, &neg_onef, &(bg[Ag->n]), 1);
  cusparseDnVecSetValues(p->dn_vec_m, (void *) &(bg[Ag->n])); /* -ry */
  cusparseDnVecSetValues(p->dn_vec_n, (void *) bg); /* x */

  /* b[n:] = Ax - ry */
#if GPU_TRANSPOSE_MAT > 0
  SCS(accum_by_atrans_gpu)(
    p->Agt, p->dn_vec_n, p->dn_vec_m, p->cusparse_handle,
    &p->buffer_size, &p->buffer
  );
#else
  SCS(accum_by_a_gpu)(
    Ag, p->dn_vec_n, p->dn_vec_m, p->cusparse_handle,
    &p->buffer_size, &p->buffer
  );
#endif

  /* bg[n:] = R bg[n:] = R (Ax - ry) = y */
  scale_by_diag(p->cublas_handle, p->rho_y_vec_gpu, &(bg[A->n]), p->Ag->m);

  /* copy bg = [x; y] back to b */
  cudaMemcpy(b, bg, (Ag->n + Ag->m) * sizeof(scs_float), cudaMemcpyDeviceToHost);
  p->tot_cg_its += cg_its;
#if VERBOSITY > 1
  scs_printf("tol %.3e\n", tol);
  scs_printf("cg_its %i\n", (int)cg_its);
#endif
  return 0;
}
