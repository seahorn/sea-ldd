#include "util.h"
#include "tddInt.h"



/** 
 * Extrapolation for intervals. 
 * Extrapolates f by g. Assumes that f IMPLIES g
 * Untested, unproven, use at your own risk 
 */
tdd_node * 
tdd_box_extrapolate (tdd_manager *tdd, tdd_node *f, tdd_node *g)
{
  tdd_node *res;
  
  do 
    {
      CUDD->reordered = 0;
      res = tdd_box_extrapolate_recur (tdd, f, g);
    } while (CUDD->reordered == 1);
  return (res);
}

/**
 * Apply t1 <- [amin,amax]*t2 + [kmin,kmax], i.e, replace all
 * constraints on term t1 with a linear combination on constraints
 * with term t2
 */
tdd_node * 
tdd_term_replace (tdd_manager *tdd, tdd_node *f, linterm_t t1, 
		  linterm_t t2, constant_t amin, constant_t amax, 
		  constant_t kmin, constant_t kmax)
{
  tdd_node *res;
  DdHashTable * table;
  
  do 
    {
      CUDD->reordered = 0;
      table = cuddHashTableInit (CUDD, 1, 2);
      if (table == NULL) return NULL;
      
      res = tdd_term_replace_recur (tdd, f, t1, t2, 
				    amin, amax, kmin, kmax,table);
      if (res != NULL)
	cuddRef (res);
      cuddHashTableQuit (table);
    } while (CUDD->reordered == 1);
  
  if (res != NULL) cuddDeref (res);
  return (res);
}

/**
 * Approximates f by computing the min and max bounds for all terms.
 */
tdd_node * 
tdd_term_minmax_approx (tdd_manager *tdd, tdd_node *f)
{
  tdd_node *res;
  
  do 
    {
      CUDD->reordered = 0;
      res = tdd_term_minmax_approx_recur (tdd, f);
    } while (CUDD->reordered == 1);
  return (res);
}



tdd_node *
tdd_box_extrapolate_recur (tdd_manager *tdd,
			   tdd_node *f,
			   tdd_node *g)
{
  DdManager * manager;
  DdNode *F, *fv, *fnv, *G, *gv, *gnv, *Fnv, *Gnv;
  DdNode *one, *zero, *r, *t, *e;
  unsigned int topf, topg, index;

  lincons_t vCons;

  manager = CUDD;
  statLine(manager);
  one = DD_ONE(manager);
  zero = Cudd_Not (one);

  F = Cudd_Regular(f);
  G = Cudd_Regular(g);


  /* Terminal cases. */
  if (F == one || G == one || F == G) return g;

  /* At this point f and g are not constant. */
  /* widen is asymmetric. cannot switch operands */

  /* Check cache. */
  if (F->ref != 1 || G->ref != 1) {
    r = cuddCacheLookup2(manager, (DD_CTFP)tdd_box_extrapolate, f, g);
    if (r != NULL) return(r);
  }
  
  /* Get the levels */
  /* Here we can skip the use of cuddI, because the operands are known
  ** to be non-constant.
  */
  topf = manager->perm[F->index];
  topg = manager->perm[G->index];

  /* Compute cofactors. */
  if (topf <= topg) {
    index = F->index;
    fv = cuddT(F);
    fnv = cuddE(F);
    if (Cudd_IsComplement(f)) {
      fv = Cudd_Not(fv);
      fnv = Cudd_Not(fnv);
    }
  } else {
    index = G->index;
    fv = fnv = f;
  }
  
  if (topg <= topf) {
    gv = cuddT(G);
    gnv = cuddE(G);
    if (Cudd_IsComplement(g)) {
      gv = Cudd_Not(gv);
      gnv = Cudd_Not(gnv);
    }
  } else {
    gv = gnv = g;
  }


  /** 
   * Get the constraint of the root node 
   */
  vCons = tdd->ddVars [index];

  /** 
   * If f and g have the same term, simplify the THEN part of the
   * non-root diagram. This eliminates a redundant test. This assumes
   * that if a constraint A is less than in diagram ordering than B
   * then A implies B.
   * 
   * We check whether f and g have the same constraint using the
   * following facts: 
   *   vInfo is the constraint of the root diagram
   *   gv == g iff g is not the root diagram
   *   fv == f iff f is not the root diagram
   */
  if (gv == g)
    {
      lincons_t gCons = tdd->ddVars [G->index];
      
      if (THEORY->is_stronger_cons (vCons, gCons))
	{
	  gv = cuddT (G);
	  if (Cudd_IsComplement (g))
	    gv = Cudd_Not (gv);
	}
    }
  else if (fv == f)
    {
      lincons_t fCons = tdd->ddVars [F->index];
      
      if (THEORY->is_stronger_cons (vCons, fCons))
	{
	  fv = cuddT (F);
	  if (Cudd_IsComplement (f))
	    fv = Cudd_Not (fv);
	}
    }
  
  
  
  /* Here, fv, fnv are lhs and rhs of f, 
           gv, gnv are lhs and rhs of g,
	   index is the index of the  root variable 
  */

  t = tdd_box_extrapolate_recur (tdd, fv, gv);
  if (t == NULL) return (NULL);
  cuddRef (t);
  
  e = tdd_box_extrapolate_recur (tdd, fnv, gnv);
  if (e == NULL)
    {
      Cudd_IterDerefBdd (manager, t);
      return NULL;
    }
  cuddRef (e);
  
  r = NULL;

  Fnv = Cudd_Regular (fnv);
  Gnv = Cudd_Regular (gnv);

  if (t == e)
    {
      r = t;
      cuddRef (r);
    }
  /** 
   ** extrapolate to the right
   ** perm[F->index] < perm[G->index]  implies that F and G are different
   ** gv != g  implies that C(f) implies C(g)
   ** perm[G->index] < perm[Fnv->index] implies that C(g) implies C(fnv)
   **/
  else if ( (CUDD->perm[F->index] < CUDD->perm[G->index]) &&
	    (gv != g) &&
	    (Fnv->index == CUDD_CONST_INDEX ||
	     CUDD->perm[G->index] < CUDD->perm[Fnv->index]) &&
	    (fnv == zero || 
	     (Fnv != one &&
	      cuddT (Fnv) == ((fnv == Fnv) ? zero : one))))
    {      
      /* r = OR (t, e) */
      r = tdd_and_recur (tdd, Cudd_Not (t), Cudd_Not (e));
      r = Cudd_NotCond (r, r != NULL);

      if (r == NULL)
	{
	  Cudd_IterDerefBdd (CUDD, t);
	  Cudd_IterDerefBdd (CUDD, e);
	  return NULL;
	}
      cuddRef (r);

    }
  /** extrapolate to the left */
  else if (CUDD->perm[G->index] < CUDD->perm[F->index] && 
	   fv == zero &&
	   f != fv &&
	   (Gnv->index == CUDD_CONST_INDEX ||
	    CUDD->perm[F->index] < CUDD->perm[Gnv->index]))

    {
      DdNode *h, *k, *w;
      lincons_t fnvCons, gnvCons, fCons;
      
      fnv = cuddE (F);
      fnv = Cudd_NotCond (fnv, F != f);
      Fnv = Cudd_Regular (fnv);
      
      
      fnvCons = tdd->ddVars [Fnv->index];
      gnvCons = tdd->ddVars [Gnv->index];
      fCons = tdd->ddVars [F->index];
      
      if (THEORY->is_stronger_cons (fCons, fnvCons))
	h = Cudd_NotCond (cuddT(Fnv), Fnv != fnv);
      else
	h = fnv;
      
      if (THEORY->is_stronger_cons (vCons, gnvCons))
	k = Cudd_NotCond (cuddT (Gnv), Gnv != gnv);
      else
	k = gnv;
      
      /* clear out t and e since we will recompute them */
      Cudd_IterDerefBdd (CUDD, t);
      t = NULL;
      Cudd_IterDerefBdd (CUDD, e);
      e = NULL;
      
      w = tdd_box_extrapolate_recur (tdd, h, k);
      
      if (w == NULL) return NULL;      
      cuddRef (w);

      /* t = OR (gv, w) */
      t = tdd_and_recur (tdd, Cudd_Not (gv), Cudd_Not (w));
      t = Cudd_NotCond (t, t != NULL);

      if (t == NULL)
	{
	  Cudd_IterDerefBdd (CUDD, w);
	  return NULL;
	}
      cuddRef (t);
      Cudd_IterDerefBdd (CUDD, w);
      
      e = tdd_box_extrapolate_recur (tdd, fnv, gnv);
      if (e == NULL)
	{
	  Cudd_IterDerefBdd (CUDD, t);
	  return NULL;
	}
      cuddRef (e);

    }
  

  if (r == NULL)
    {
      DdNode *root;
      
      root = Cudd_bddIthVar (CUDD, index);

      if (root == NULL)
	{
	  Cudd_IterDerefBdd (CUDD, t);
	  Cudd_IterDerefBdd (CUDD, e);
	  return NULL;
	}
      cuddRef (root);

      /** XXX should use tdd_unique_inter instead */
      r = tdd_ite_recur (tdd, root, t, e);
      if (r == NULL)
	{
	  Cudd_IterDerefBdd (CUDD, t);
	  Cudd_IterDerefBdd (CUDD, e);
	  return NULL;
	}
      cuddRef (r);
      Cudd_IterDerefBdd (CUDD, root);
    }
  

  Cudd_IterDerefBdd (CUDD, t);
  Cudd_IterDerefBdd (CUDD, e);

  if (F->ref != 1 || G->ref != 1)
    cuddCacheInsert2(manager, (DD_CTFP)tdd_box_extrapolate, f, g, r);

  cuddDeref (r);
  return r;
  
}

tdd_node * 
tdd_term_replace_recur (tdd_manager * tdd, tdd_node *f, 
			linterm_t t1, linterm_t t2, 
			constant_t amin, constant_t amax, 
			constant_t kmin, constant_t kmax,
			DdHashTable* table)
{
  DdNode *F, *res, *fv, *fnv, *t, *e;
  DdNode *rootT, *rootE;
  
  lincons_t fCons;
  linterm_t fTerm;
  

  F = Cudd_Regular (f);
  
  if (F == DD_ONE (CUDD)) 
    {
      if (t2 == NULL && f == DD_ONE(CUDD))
	{
	  lincons_t maxCons;
	  lincons_t minCons;
	  DdNode *tmp1, *tmp2;
	  
	  maxCons = THEORY->create_cons (THEORY->dup_term (t1), 0,
					 THEORY->dup_cst (kmax));
	  if (maxCons == NULL) return NULL;

	  res = to_tdd (tdd, maxCons);
	  THEORY->destroy_lincons (maxCons);
	  if (res == NULL) return NULL;
	  cuddRef (res);

	  minCons = THEORY->create_cons (THEORY->negate_term (t1), 0,
					 THEORY->negate_cst (kmin));
	  if (minCons == NULL) 
	    {
	      Cudd_IterDerefBdd (CUDD, res);
	      return NULL;
	    }
	  tmp1 = to_tdd (tdd, minCons);
	  if (tmp1 == NULL)
	    {
	      Cudd_IterDerefBdd (CUDD, res);
	      return NULL;
	    }
	  cuddRef (tmp1);

	  tmp2 = tdd_and_recur (tdd, res, tmp1);
	  if (tmp2 != NULL) cuddRef (tmp2);
	  Cudd_IterDerefBdd (CUDD, res);
	  Cudd_IterDerefBdd (CUDD, tmp1);
	  if (tmp2 == NULL) return NULL;
	  res = tmp2;

	  cuddDeref (res);
	  return res;
	}
      
      return f;  
    }


  /* XXX if terms t1 and t2 do not appear in f, can stop here */

  /* check cache */
  if (F->ref != 1 && ((res = cuddHashTableLookup1 (table, f)) != NULL))
    return res;

  /* cofactors */
  fv = Cudd_NotCond (cuddT(F), F != f);
  fnv = Cudd_NotCond (cuddE(F), F != f);
  

  /* roots of the result diagram */
  rootT = NULL;
  rootE = NULL;
  
  fCons = tdd->ddVars [F->index];
  fTerm = THEORY->get_term (fCons);
  
  /* remove top term if it is t1 */
  if (THEORY->term_equals (fTerm, t1))
    {
      rootT = DD_ONE(CUDD);
      cuddRef (rootT);
      
      rootE = DD_ONE(CUDD);
      cuddRef (rootE);
    }
  /* keep top term if it is not t1 */
  else
    {
      rootT = Cudd_bddIthVar (CUDD, F->index);
      if (rootT == NULL) return NULL;
      cuddRef (rootT);
      
      rootE = Cudd_Not (rootT);
      cuddRef (rootE);
    }
  
  
  /* if top constraint is  t2 < c, add t1 < amax*c + kmax to the THEN branch
   * and !(t1 < amin*c + kmin) to the ELSE branch of the result
   */
  if (THEORY->term_equals (fTerm, t2))
    {
      DdNode *tmp, *d;
      constant_t fCst, tmpCst, nCst;
      lincons_t newCons;
      
      fCst = THEORY->get_constant (fCons);


      /* nCst = amax * fCst + kmax */
      tmpCst = THEORY->mul_cst (amax, fCst);
      nCst = THEORY->add_cst (tmpCst, kmax);
      THEORY->destroy_cst (tmpCst);
      tmpCst = NULL;
      
      newCons = THEORY->create_cons (THEORY->dup_term (t1), 
				     THEORY->is_strict (fCons), 
				     nCst);
      /* nCst is now managed by createCons */

      d = to_tdd (tdd, newCons);
      THEORY->destroy_lincons (newCons);
      newCons = NULL;
      if (d == NULL)
	{
	  Cudd_IterDerefBdd (CUDD, rootT);
	  Cudd_IterDerefBdd (CUDD, rootE);
	  return NULL;
	}
      cuddRef (d);

      /* add d to the THEN branch */
      tmp = tdd_and_recur (tdd, rootT, d);
      if (tmp != NULL) cuddRef (tmp);
      Cudd_IterDerefBdd (CUDD, rootT);
      if (tmp == NULL)
	{
	  Cudd_IterDerefBdd (CUDD, rootE);
	  Cudd_IterDerefBdd (CUDD, d);
	  return NULL;
	}
      rootT = tmp;
      tmp = NULL;

      /* compute d for the ELSE branch */
      if (amin != amax || kmin != kmax)
	{
	  Cudd_IterDerefBdd (CUDD, d);
	  d = NULL;
	  
	  /* nCst = amin * fCst + kmin */
	  tmpCst = THEORY->mul_cst (amin, fCst);
	  nCst = THEORY->add_cst (tmpCst, kmin);
	  THEORY->destroy_cst (tmpCst);
	  tmpCst = NULL;
      
	  newCons = THEORY->create_cons (THEORY->dup_term (t1), 
					 THEORY->is_strict (fCons), 
					 nCst);
	  /* nCst is now managed by createCons */

	  d = to_tdd (tdd, newCons);
	  THEORY->destroy_lincons (newCons);
	  newCons = NULL;
	  if (d == NULL)
	    {
	      Cudd_IterDerefBdd (CUDD, rootT);
	      Cudd_IterDerefBdd (CUDD, rootE);
	      return NULL;
	    }
	  cuddRef (d);
	}

      /* add d to the ELSE branch */
      tmp = tdd_and_recur (tdd, rootE, Cudd_Not (d));
      if (tmp != NULL) cuddRef (tmp);
      Cudd_IterDerefBdd (CUDD, rootE);
      if (tmp == NULL)
	{
	  Cudd_IterDerefBdd (CUDD, rootT);
	  Cudd_IterDerefBdd (CUDD, d);
	  return NULL;
	}
      rootE = tmp;

      Cudd_IterDerefBdd (CUDD, d);

    }
  
      
  /* recursive step */

  t = tdd_term_replace_recur (tdd, fv, t1, t2, amin, amax, kmin, kmax, table);
  if (t == NULL)
    {
      Cudd_IterDerefBdd (CUDD, rootT);
      Cudd_IterDerefBdd (CUDD, rootE);
      return NULL;
    }
  cuddRef (t);
  
  e = tdd_term_replace_recur (tdd, fnv, t1, t2, amin, amax, kmin, kmax, table);
  if (e == NULL)
    {
      Cudd_IterDerefBdd (CUDD, rootT);
      Cudd_IterDerefBdd (CUDD, rootE);
      Cudd_IterDerefBdd (CUDD, t);
      return NULL;
    }
  cuddRef (e);

  /* Add rootT to the THEN branch of the result */
  if (rootT != DD_ONE(CUDD))
    {
      DdNode *tmp;
      tmp = tdd_and_recur (tdd, rootT, t);
      if (tmp != NULL) cuddRef (tmp);
      Cudd_IterDerefBdd (CUDD, rootT);
      Cudd_IterDerefBdd (CUDD, t);
      if (tmp == NULL)
	{
	  Cudd_IterDerefBdd (CUDD, rootE);
	  Cudd_IterDerefBdd (CUDD, e);
	  return NULL;
	}
      t = tmp; 
    }
  else
    /* rootT == DD_ONE(CUDD) */
    cuddDeref (rootT);
  
  /* Add rootE to the ELSE branch of the result */
  if (rootE != DD_ONE(CUDD))
    {
      DdNode *tmp;
      tmp = tdd_and_recur (tdd, rootE, e);
      if (t != NULL) cuddRef (tmp);
      Cudd_IterDerefBdd (CUDD, rootE);
      Cudd_IterDerefBdd (CUDD, e);
      if (tmp == NULL)
	{
	  Cudd_IterDerefBdd (CUDD, t);
	  return NULL;
	}
      e = tmp; 
    }
  else
    /* rootE == DD_ONE(CUDD) */
    cuddDeref (rootE);


  /* res = OR (t, e) */
  res = tdd_and_recur (tdd, Cudd_Not (t), Cudd_Not (e));
  if (res != NULL)
    {
      res = Cudd_Not (res);
      cuddRef (res);
    }
  Cudd_IterDerefBdd (CUDD, t);
  Cudd_IterDerefBdd (CUDD, e);
  if (res == NULL) return NULL;
  
  /* update cache */
  if (F->ref != 1)
    {
      ptrint fanout = (ptrint) F->ref;
      cuddSatDec (fanout);
      if (!cuddHashTableInsert1 (table, f, res, fanout))
	{
	  Cudd_IterDerefBdd (CUDD, res);
	  return NULL;
	}
    }

  cuddDeref (res);
  return res;
    
}


tdd_node *
tdd_term_minmax_approx_recur (tdd_manager *tdd,
			      tdd_node *f)
{
  DdManager * manager;
  DdNode *F, *fv, *fnv, *h, *k;
  DdNode *one, *zero, *r, *t;

  unsigned int minIndex, maxIndex;
  lincons_t minCons, maxCons;
  
  manager = CUDD;
  statLine(manager);
  one = DD_ONE(manager);
  zero = Cudd_Not (one);

  F = Cudd_Regular(f);


  /* Terminal cases. */
  if (F == one) return f;


  /* Check cache. */
  if (F->ref != 1) {
    r = cuddCacheLookup1(manager, (DD_CTFP1)tdd_term_minmax_approx, f);
    if (r != NULL) return(r);
  }
  
  /* Get the levels */
  /* Here we can skip the use of cuddI, because the operands are known
  ** to be non-constant.
  */
  minIndex = manager->perm[F->index];

  fv = Cudd_NotCond (cuddT(F), F != f);
  fnv = Cudd_NotCond (cuddE(F), F != f);

  /** 
   * Get the constraint of the root node 
   */
  minCons = tdd->ddVars [minIndex];

  maxCons = minCons;
  maxIndex = minIndex;

  k = fv;
  cuddRef (k);
  
  h = fnv;
  
  while (!cuddIsConstant (h))
    {
      lincons_t hCons;
      DdNode *H, *tmp;
      
      H = Cudd_Regular (h);
      hCons = tdd->ddVars [H->index];
      
      if (!THEORY->is_stronger_cons (maxCons, hCons)) break;
      
      maxCons = hCons;
      maxIndex = H->index;

      /* k = OR (k, h) */
      tmp = tdd_and_recur (tdd, Cudd_Not (k), Cudd_Not (h));
      if (tmp != NULL) cuddRef (tmp);
      Cudd_IterDerefBdd (CUDD, k);
      if (tmp == NULL) return NULL;
      k = Cudd_Not (tmp);

      h = Cudd_NotCond (cuddE (H), H != h);
    }
  
  if (h != zero)
    {
      DdNode *tmp;
      
      tmp = tdd_and_recur (tdd, Cudd_Not (k), Cudd_Not (h));
      if (tmp != NULL) cuddRef (tmp);
      Cudd_IterDerefBdd (CUDD, k);
      if (tmp == NULL) return NULL;
      k = Cudd_Not (tmp);
    }
  
  t = tdd_term_minmax_approx_recur (tdd, k);
  if (t != NULL) cuddRef (t);
  Cudd_IterDerefBdd (CUDD, k);
  if (t == NULL) return NULL;
  k = t; t = NULL;
  

  if (h != zero)
    {
      r = k; k = NULL;
    }
  
  else
    {
      DdNode *root;
      
      root = Cudd_bddIthVar (CUDD, maxIndex);
      if (root == NULL)
	{
	  Cudd_IterDerefBdd (CUDD, k);
	  return NULL;
	}
      cuddRef (root);
      
      r = tdd_ite_recur (tdd, root, k, zero);
      if (r != NULL) cuddRef (r);
      Cudd_IterDerefBdd (CUDD, root);
      Cudd_IterDerefBdd (CUDD, k);
      k = NULL;
      if (r == NULL) return NULL;
    }
  
  /* k is dead here, r is referenced once */

  if (fv == zero)
    {
      DdNode *root, *tmp;
      
      root = Cudd_bddIthVar (CUDD, minIndex);
      if (root == NULL)
	{
	  Cudd_IterDerefBdd (CUDD, r);
	  return NULL;
	}
      cuddRef (root);
      
      tmp = tdd_ite_recur (tdd, root, zero, r);
      if (tmp != NULL) cuddRef (tmp);
      Cudd_IterDerefBdd (CUDD, r);
      if (tmp == NULL) return NULL;
      r = tmp;
    }
  
  if (F->ref != 1)
    cuddCacheInsert1(CUDD, (DD_CTFP1)tdd_term_minmax_approx, f, r);
  
  cuddDeref (r);
  return r;
}