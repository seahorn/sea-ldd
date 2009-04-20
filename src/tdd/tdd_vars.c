#include "util.h"
#include "tddInt.h"

static tdd_node * tdd_assoc_node (tdd_manager *, tdd_node *, lincons_t);
static void tdd_update_cudd_mtr_tree (DdManager *, tdd_node *, tdd_node * );

#ifdef MTR_DEBUG_FINE
static void tdd_debug_print_mtr (MtrNode* tree);
#endif


tdd_node *tdd_get_true (tdd_manager *tdd)
{
  return DD_ONE (CUDD);
}

tdd_node *tdd_get_false (tdd_manager *tdd)
{
  return tdd_not (DD_ONE (CUDD));
}



tdd_node* tdd_new_var (tdd_manager * tdd, lincons_t l)
{
  tdd_node * n = Cudd_bddNewVar (CUDD);
  
  if (n == NULL)
    return NULL;
  
  
  n = tdd_assoc_node (tdd, n, l);

#ifdef MTR_DEBUG_FINE
  fprintf (stderr, "Create a new mtr with index %d and level %d\n", n->index, 
	   cuddI(CUDD, n->index));
#endif

  /* create a tree node to maintain fixed ordering of the 
   * nodes corresponding to constraints with the same term as l.
   * Since l is the first such constraint, the group is of size 1
   */
  Cudd_MakeTreeNode (CUDD, n->index, 1, MTR_FIXED);

#ifdef MTR_DEBUG_FINE
  tdd_debug_print_mtr (CUDD->tree);
#endif

  return n;
}


tdd_node * 
tdd_new_var_before (tdd_manager * tdd, tdd_node * v, lincons_t l)
{

  tdd_node * n;
  unsigned int vLevel;

  if (tdd->be_bddlike)
    return tdd_new_var (tdd, l);
  
  vLevel = cuddI (CUDD, v->index);

  n = Cudd_bddNewVarAtLevel (CUDD, vLevel);
  if (n == NULL) return NULL;

  n = tdd_assoc_node (tdd, n, l);

#ifdef MTR_DEBUG_FINE
  fprintf (stderr, "new_var_before: update with level %d\n", vLevel);
#endif

  tdd_update_cudd_mtr_tree (CUDD, v, n);

  return n;
  
}

tdd_node * 
tdd_new_var_after (tdd_manager * tdd, tdd_node *v, lincons_t l)
{
  
  tdd_node * n;
  unsigned int vLevel;

  if (tdd->be_bddlike)
    return tdd_new_var (tdd, l);

  
  vLevel = cuddI (CUDD, v->index);

  n = Cudd_bddNewVarAtLevel (CUDD, 1 + vLevel);
  if (n == NULL) return NULL;
  
  n = tdd_assoc_node (tdd, n, l);

#ifdef MTR_DEBUG_FINE
  fprintf (stderr, "new_var_after: update with level %d from index %d\n", 
	   vLevel, v->index);
#endif

  tdd_update_cudd_mtr_tree (CUDD, v, n);
  
  return n;
  
}


tdd_node * 
tdd_assoc_node (tdd_manager * tdd, tdd_node *n, lincons_t l)
{
  int idx;
  int i;
  
  idx = n->index;
  
  if (idx >= tdd->varsSize)
    {
      lincons_t* newDdVars = ALLOC (lincons_t, CUDD->maxSize);
      if (newDdVars == NULL) return NULL;
      
      for (i = 0; i < tdd->varsSize; i++)
	newDdVars [i] = tdd->ddVars [i];
      for (i = tdd->varsSize; i < CUDD->maxSize; i++)
	newDdVars [i] = NULL;
      
      FREE (tdd->ddVars);
      tdd->varsSize = CUDD->maxSize;
      tdd->ddVars = newDdVars;
    }
  
  tdd->ddVars [idx] = THEORY->dup_lincons (l);

  return n;
}


/**
 * Updates the group tree corresponding to addition of n into the
 * same group as v
 */
void 
tdd_update_cudd_mtr_tree (DdManager *cudd, tdd_node *v, tdd_node *n)
{
  MtrNode *tree;
  MtrNode *group;
  unsigned int vLevel;
  unsigned int nLevel;
  

  /* XXX We use potentially unsafe arithmetic that may lead to overflow.
     XXX Need to look into this later.
  */
  
  tree = cudd->tree;
  /* assume group tree is already created */
  assert (tree != NULL);
  /* assume the root has children */
  assert (tree->child != NULL);
  

  vLevel = cuddI (cudd, v->index);
  nLevel = cuddI (cudd, n->index);

#ifdef MTR_DEBUG_FINE
  fprintf (stderr, "Looking for a group at levels: (v=%d,n=%d)  v->index=%d, n->index=%d\n", 
	   vLevel, nLevel, v->index, n->index);

  fprintf (stderr, "BEFORE update_mtr\n");
  tdd_debug_print_mtr (tree);
  fprintf (stderr, "BEFORE update_mtr\n\n");
#endif


  /* Find group that contains either vLevel or nLevel. 
     Such a group must exist. There are 3 cases: 
     (a) vLevel and nLevel are contained in the same group
     (b) vLevel is in some group, but nLevel is not in any group
     (c) nLevel is in some group, but vLevel is not in any group
   */
  for (group = tree->child; group != NULL; group = group->younger)
    {

      
      if ((group->low <= vLevel && vLevel < group->low + group->size) ||
	  (group->low <= nLevel && nLevel < group->low + group->size))
	{	  
	  /* found the right group. increment its size */
	  group->size = group->size + 1;

	  /* if n was added to the front of the group, update
	     group index and low
	  */
	  if (group->low == vLevel && nLevel < vLevel)
	    {
	      group->low = nLevel;
	      group->index = n->index;
	    }
	  break;
	}
      
    }
  
  /* we must have found a group to which level belongs*/
  assert (group != NULL);
  
#ifdef MTR_DEBUG_FINE
  fprintf (stderr, "AFTER update_mtr\n");
  tdd_debug_print_mtr (tree);
  fprintf (stderr, "END update_mtr\n\n");
#endif

}



#ifdef MTR_DEBUG_FINE

void 
tdd_debug_print_mtr (MtrNode * tree)
{
  MtrNode *group;
  
  fprintf (stderr, "Root is (low=%d, index=%d, size=%d)\n", 
	   tree->low, tree->index, tree->size);

  for (group = tree->child; group != NULL; group = group->younger)
  fprintf (stderr, "\tgroup is (low=%d, index=%d, size=%d)\n", 
	   group->low, group->index, group->size);
}

#endif
